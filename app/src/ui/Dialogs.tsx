import { useState } from 'react';
import { addSource, copyToScene, CopyMode, KIND_INFO, placeSource } from '../model/doc';
import { commit, getState, setUi, toast, useStore } from '../model/store';
import { SourceKind } from '../model/types';
import { connect, useBridge } from '../obs/bridge';
import { Modal } from './common';

export function CopyDialog() {
  const doc = useStore((s) => s.doc);
  const ui = useStore((s) => s.ui);
  const ids = ui.copyDialog?.ids ?? [];
  const from = ids.length ? doc.instances[ids[0]]?.sceneId : null;
  const [targets, setTargets] = useState<string[]>(() => ui.copyDialog?.targets ?? []);
  const [mode, setMode] = useState<CopyMode>(ui.copyDefault ?? 'independent');
  const [remember, setRemember] = useState(!!ui.copyDefault);
  if (!ui.copyDialog) return null;
  const close = () => setUi({ copyDialog: null });
  const names = ids.map((id) => doc.instances[id]?.name).filter(Boolean);

  const go = () => {
    commit(`Copy ${names.length > 1 ? names.length + ' layers' : names[0]} (${mode}) to ${targets.length} scene(s)`, (d) => {
      for (const t of targets) copyToScene(d, ids, t, mode);
    });
    setUi({ copyDialog: null, copyDefault: remember ? mode : null });
    toast(`Copied to ${targets.length} scene(s) as ${mode}`);
  };

  return (
    <Modal title={`Copy ${names.length > 1 ? names.length + ' layers' : '“' + names[0] + '”'} to…`} onClose={close}>
      <div className="mb">
        <div>
          <div className="muted small" style={{ marginBottom: 6 }}>Scenes</div>
          <div style={{ maxHeight: 180, overflow: 'auto', border: '1px solid var(--line-2)', borderRadius: 6, padding: 4 }}>
            {doc.sceneOrder.map((sid) => (
              <label key={sid} className="row" style={{ cursor: 'pointer' }}>
                <input type="checkbox" checked={targets.includes(sid)} onChange={() => setTargets(targets.includes(sid) ? targets.filter((t) => t !== sid) : [...targets, sid])} />
                <span className="name">{doc.scenes[sid].name}</span>
                {sid === from && <span className="muted small">(this scene)</span>}
              </label>
            ))}
          </div>
        </div>
        <div className={'choice' + (mode === 'linked' ? ' on' : '')} onClick={() => setMode('linked')}>
          <span style={{ fontSize: 18 }}>🔗</span>
          <div><b>Linked</b><span className="muted small">Shares transform, crop, filters and visibility. Edit one, they all change. You can unlink any section later.</span></div>
        </div>
        <div className={'choice' + (mode === 'independent' ? ' on' : '')} onClick={() => setMode('independent')}>
          <span style={{ fontSize: 18 }}>⧉</span>
          <div><b>Independent</b><span className="muted small">A snapshot copy. Same source, but changes stay in each scene.</span></div>
        </div>
      </div>
      <div className="mf">
        <label className="small muted" style={{ marginRight: 'auto', display: 'flex', gap: 6, alignItems: 'center' }}>
          <input type="checkbox" checked={remember} onChange={() => setRemember(!remember)} /> Remember for paste
        </label>
        <button onClick={close}>Cancel</button>
        <button className="primary" disabled={!targets.length} onClick={go}>Copy</button>
      </div>
    </Modal>
  );
}

const KINDS: SourceKind[] = ['camera', 'display', 'window', 'game', 'image', 'media', 'browser', 'color', 'text', 'mic', 'desktop_audio'];

export function AddSourceDialog() {
  const open = useStore((s) => s.ui.addSourceDialog);
  const sceneId = useStore((s) => s.ui.sceneId);
  const [kind, setKind] = useState<SourceKind>('camera');
  const [name, setName] = useState('');
  const [extra, setExtra] = useState('');
  const [place, setPlace] = useState(true);
  if (!open) return null;
  const info = KIND_INFO[kind];
  const close = () => (setUi({ addSourceDialog: false }), setName(''), setExtra(''));
  const extraField: Partial<Record<SourceKind, [string, string]>> = {
    text: ['text', 'Text'],
    browser: ['url', 'URL'],
    image: ['file', 'File path'],
    media: ['file', 'File path'],
    color: ['color', 'Color'],
  };
  const ef = extraField[kind];

  const create = () => {
    let sid = '', iid = '';
    const settings: Record<string, unknown> = ef ? { [ef[0]]: extra || (kind === 'color' ? '#1d1b2e' : '') } : {};
    commit(`Add source ${name || info.label}`, (d) => {
      sid = addSource(d, kind, name.trim() || info.label, settings).id;
      if (place && sceneId && info.video) iid = placeSource(d, sceneId, sid).id;
    });
    setUi(iid ? { selection: [iid], poolSelection: null } : { poolSelection: sid, selection: [], rightTab: info.video ? 'inspector' : 'audio' });
    close();
  };

  return (
    <Modal title="Add to Media Pool" onClose={close} wide>
      <div className="mb">
        <div className="kinds">
          {KINDS.map((k) => (
            <div key={k} className={'kind' + (k === kind ? ' on' : '')} onClick={() => setKind(k)}>
              <span className="i">{KIND_INFO[k].icon}</span>
              {KIND_INFO[k].label}
            </div>
          ))}
        </div>
        <div className="kv">
          <label>Name</label>
          <input autoFocus placeholder={info.label} value={name} onChange={(e) => setName(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && create()} />
          {ef && (
            <>
              <label>{ef[1]}</label>
              {kind === 'color' ? <input type="color" value={extra || '#1d1b2e'} onChange={(e) => setExtra(e.target.value)} /> : <input value={extra} onChange={(e) => setExtra(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && create()} />}
            </>
          )}
        </div>
        <div className="muted small">
          Added once, used anywhere. {info.video ? 'Each scene gets its own size, crop and filters.' : 'Audio sources are mixed in the Audio tab.'}
          {['camera', 'display', 'window', 'mic', 'desktop_audio'].includes(kind) && ' Pick the device in the inspector after connecting to OBS.'}
        </div>
      </div>
      <div className="mf">
        {info.video && sceneId && (
          <label className="small muted" style={{ marginRight: 'auto', display: 'flex', gap: 6, alignItems: 'center' }}>
            <input type="checkbox" checked={place} onChange={() => setPlace(!place)} /> Also place on “{getState().doc.scenes[sceneId]?.name}”
          </label>
        )}
        <button onClick={close}>Cancel</button>
        <button className="primary" onClick={create}>Add</button>
      </div>
    </Modal>
  );
}

export function ConnectDialog() {
  const open = useStore((s) => s.ui.connectDialog);
  const b = useBridge((x) => x);
  const [url, setUrl] = useState(b.url);
  const [pw, setPw] = useState('');
  if (!open) return null;
  const close = () => setUi({ connectDialog: false });
  const go = async () => {
    await connect(url, pw);
    setPw('');
  };
  return (
    <Modal title="Connect to OBS" onClose={close}>
      <div className="mb">
        <div className="muted small">
          In OBS, open <b>Tools → WebSocket Server Settings</b>, tick <b>Enable WebSocket server</b>, and copy the port and password here.
          LOBS will create and manage its scenes in OBS; scenes it doesn’t know about are left alone.
        </div>
        <div className="kv">
          <label>Address</label>
          <input value={url} onChange={(e) => setUrl(e.target.value)} />
          <label>Password</label>
          <input type="password" value={pw} onChange={(e) => setPw(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && void go()} autoComplete="off" />
        </div>
        {b.status === 'error' && <div className="warn">⚠ {b.error}</div>}
        {b.status === 'connected' && <div style={{ color: 'var(--ok)' }}>● Connected to OBS {b.obsVersion}</div>}
      </div>
      <div className="mf">
        <button onClick={close}>{b.status === 'connected' ? 'Done' : 'Cancel'}</button>
        {b.status !== 'connected' && <button className="primary" disabled={b.status === 'connecting'} onClick={() => void go()}>{b.status === 'connecting' ? 'Connecting…' : 'Connect'}</button>}
      </div>
    </Modal>
  );
}
