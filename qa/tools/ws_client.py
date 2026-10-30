"""An obs-websocket v5 client with no dependencies, for _probe-websocket.js.

Waits for <dir>/ws-ready.json (port, password, scene and layer names), connects to LOBS like
Stream Deck or VNyan would, runs a set of requests, and writes <dir>/ws-result.json.
    python -I qa/tools/ws_client.py <dir>
"""
import base64, hashlib, json, os, socket, struct, sys, time

d = sys.argv[1]
ready = os.path.join(d, 'ws-ready.json')
os.makedirs(d, exist_ok=True)
for _ in range(240):
    if os.path.exists(ready):
        break
    time.sleep(0.5)
cfg = json.load(open(ready, encoding='utf-8'))
checks, details = {}, {}


def check(name, ok, detail=''):
    checks[name] = bool(ok)
    if not ok:
        details[name] = str(detail)[:400]


class Ws:
    def __init__(self, port):
        self.s = socket.create_connection(('127.0.0.1', port), timeout=5)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall(('GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                        'Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: obswebsocket.json\r\n\r\n' % key).encode())
        head = b''
        while b'\r\n\r\n' not in head:
            head += self.s.recv(1)
        self.head = head.decode()
        accept = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
        self.accepted = '101' in self.head.split('\r\n')[0] and accept in self.head
        self.pending = []

    def send(self, obj):
        data = json.dumps(obj).encode()
        mask = os.urandom(4)
        n = len(data)
        h = bytes([0x81]) + (bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack('>H', n) if n < 65536 else bytes([0x80 | 127]) + struct.pack('>Q', n))
        self.s.sendall(h + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def _exact(self, n):
        b = b''
        while len(b) < n:
            c = self.s.recv(n - len(b))
            if not c:
                raise ConnectionError('closed')
            b += c
        return b

    def frame(self):
        b0, b1 = self._exact(2)
        n = b1 & 0x7f
        if n == 126:
            n = struct.unpack('>H', self._exact(2))[0]
        elif n == 127:
            n = struct.unpack('>Q', self._exact(8))[0]
        payload = self._exact(n)
        return b0 & 0x0f, payload

    def recv(self):
        op, p = self.frame()
        if op == 8:
            raise ConnectionError('closed %d %s' % (struct.unpack('>H', p[:2])[0] if len(p) >= 2 else 0, p[2:].decode(errors='replace')))
        return json.loads(p)

    def until(self, pred, timeout=5):
        self.s.settimeout(timeout)
        for m in self.pending:
            if pred(m):
                self.pending.remove(m)
                return m
        while True:
            m = self.recv()
            if pred(m):
                return m
            self.pending.append(m)

    def request(self, rtype, data=None, rid=None):
        rid = rid or rtype
        self.send({'op': 6, 'd': {'requestType': rtype, 'requestId': rid, 'requestData': data or {}}})
        return self.until(lambda m: m['op'] == 7 and m['d']['requestId'] == rid)['d']


def auth(hello, password):
    a = hello['d'].get('authentication')
    if not a:
        return None
    secret = base64.b64encode(hashlib.sha256((password + a['salt']).encode()).digest()).decode()
    return base64.b64encode(hashlib.sha256((secret + a['challenge']).encode()).digest()).decode()


try:
    # A wrong password is turned away (close code 4008).
    w = Ws(cfg['port'])
    hello = w.recv()
    w.send({'op': 1, 'd': {'rpcVersion': 1, 'authentication': auth(hello, 'wrong password')}})
    try:
        w.recv()
        check('wrong password is refused', False, 'got a reply')
    except ConnectionError as e:
        check('wrong password is refused', '4008' in str(e), e)

    w = Ws(cfg['port'])
    check('WebSocket upgrade', w.accepted, w.head)
    hello = w.recv()
    check('Hello asks for a password', hello['op'] == 0 and 'authentication' in hello['d'], hello)
    w.send({'op': 1, 'd': {'rpcVersion': 1, 'authentication': auth(hello, cfg['password']), 'eventSubscriptions': 0x7ff}})
    ident = w.until(lambda m: m['op'] == 2)
    check('Identified', ident['d'].get('negotiatedRpcVersion') == 1, ident)

    v = w.request('GetVersion')
    check('GetVersion', v['requestStatus']['result'] and v['responseData']['rpcVersion'] == 1, v)
    sl = w.request('GetSceneList')
    names = [s['sceneName'] for s in sl.get('responseData', {}).get('scenes', [])]
    check('GetSceneList names the scenes', cfg['a'] in names and cfg['b'] in names, names)
    check('program is Alpha', sl['responseData']['currentProgramSceneName'] == cfg['a'], sl)

    r = w.request('SetCurrentProgramScene', {'sceneName': cfg['b']})
    check('SetCurrentProgramScene', r['requestStatus']['result'], r)
    ev = w.until(lambda m: m['op'] == 5 and m['d']['eventType'] == 'CurrentProgramSceneChanged', 8)
    check('CurrentProgramSceneChanged event', ev['d']['eventData']['sceneName'] == cfg['b'], ev)

    items = w.request('GetSceneItemList', {'sceneName': cfg['a']})
    layer = [i for i in items.get('responseData', {}).get('sceneItems', []) if i['sourceName'] == cfg['layer']]
    check('GetSceneItemList lists the layer', len(layer) == 1, items)
    if layer:
        iid = layer[0]['sceneItemId']
        idr = w.request('GetSceneItemId', {'sceneName': cfg['a'], 'sourceName': cfg['layer']})
        check('GetSceneItemId', idr.get('responseData', {}).get('sceneItemId') == iid, idr)
        r = w.request('SetSceneItemEnabled', {'sceneName': cfg['a'], 'sceneItemId': iid, 'sceneItemEnabled': False})
        check('SetSceneItemEnabled', r['requestStatus']['result'], r)
        ev = w.until(lambda m: m['op'] == 5 and m['d']['eventType'] == 'SceneItemEnableStateChanged', 8)
        check('SceneItemEnableStateChanged event', ev['d']['eventData']['sceneItemEnabled'] is False, ev)
        r = w.request('GetSceneItemEnabled', {'sceneName': cfg['a'], 'sceneItemId': iid})
        check('GetSceneItemEnabled', r.get('responseData', {}).get('sceneItemEnabled') is False, r)

    inputs = w.request('GetInputList').get('responseData', {}).get('inputs', [])
    check('GetInputList', len(inputs) > 0, inputs)
    audio = [i for i in inputs if 'wasapi' in i['inputKind']]
    if audio:
        name = audio[0]['inputName']
        before = w.request('GetInputMute', {'inputName': name})['responseData']['inputMuted']
        t = w.request('ToggleInputMute', {'inputName': name})
        check('ToggleInputMute', t.get('responseData', {}).get('inputMuted') == (not before), t)
        ev = w.until(lambda m: m['op'] == 5 and m['d']['eventType'] == 'InputMuteStateChanged', 8)
        check('InputMuteStateChanged event', ev['d']['eventData']['inputMuted'] == (not before), ev)
        w.request('SetInputMute', {'inputName': name, 'inputMuted': before})
        r = w.request('SetInputVolume', {'inputName': name, 'inputVolumeDb': -6})
        g = w.request('GetInputVolume', {'inputName': name})
        check('SetInputVolume in dB', r['requestStatus']['result'] and abs(g['responseData']['inputVolumeDb'] + 6) < 0.1, g)
        w.request('SetInputVolume', {'inputName': name, 'inputVolumeMul': 1})

    st = w.request('GetStreamStatus')
    check('GetStreamStatus', st['requestStatus']['result'] and st['responseData']['outputActive'] is False, st)
    u = w.request('NoSuchRequest')
    check('an unknown request says so (204)', u['requestStatus']['code'] == 204, u)

    w.send({'op': 8, 'd': {'requestId': 'batch', 'requests': [{'requestType': 'GetVersion'}, {'requestType': 'GetCurrentProgramScene'}]}})
    b = w.until(lambda m: m['op'] == 9)['d']
    check('RequestBatch', len(b['results']) == 2 and b['results'][1]['responseData']['sceneName'] == cfg['b'], b)
except Exception as e:  # noqa
    check('ran without errors', False, repr(e))

json.dump({'checks': checks, 'details': details}, open(os.path.join(d, 'ws-result.json'), 'w', encoding='utf-8'))
print(json.dumps({'checks': checks, 'details': details}, indent=1))
