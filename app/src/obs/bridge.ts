// OBS bridge: one-way reconciler from the LOBS document into a running OBS (obs-websocket v5).
//
// Mapping:
//   Source    -> one OBS input, created in the hidden scene "[LOBS] Pool"
//   Instance  -> a private OBS scene "[LOBS] <instance id>" holding the shared input.
//                Crop goes on the inner item; filters + opacity go on the private scene,
//                so each instance has its own filter chain while the input is captured once.
//   Scene     -> an OBS scene with one item per visible leaf instance (groups are flattened).
import OBSWebSocket from 'obs-websocket-js';
import { useSyncExternalStore } from 'react';
import { flatten, getProp, KIND_INFO } from '../model/doc';
import { getState, onDocChange, setUi, toast } from '../model/store';
import { Filter, LobsDoc, MonitorType, Source, SourceKind } from '../model/types';

export interface BridgeState {
  status: 'disconnected' | 'connecting' | 'connected' | 'error';
  error: string | null;
  url: string;
  syncing: boolean;
  lastSync: number | null;
  syncError: string | null;
  streaming: boolean;
  recording: boolean;
  studioMode: boolean;
  transitions: string[];
  transition: string;
  thumbs: Record<string, string>; // obs source name -> data url
  sizes: Record<string, [number, number]>; // obs input name -> natural size
  obsVersion: string;
  pluginMode: boolean; // LOBS OBS plugin found: layers are lobs_instance inputs, not scenes
}

let bs: BridgeState = {
  status: 'disconnected',
  error: null,
  url: localStorage.getItem('lobs.obs.url') ?? 'ws://127.0.0.1:4455',
  syncing: false,
  lastSync: null,
  syncError: null,
  streaming: false,
  recording: false,
  studioMode: false,
  transitions: [],
  transition: '',
  thumbs: {},
  sizes: {},
  obsVersion: '',
  pluginMode: false,
};
const ls = new Set<() => void>();
const set = (p: Partial<BridgeState>) => {
  bs = { ...bs, ...p };
  for (const l of ls) l();
};
export const useBridge = <T>(sel: (s: BridgeState) => T) =>
  useSyncExternalStore((l) => (ls.add(l), () => ls.delete(l)), () => sel(bs));

const obs = new OBSWebSocket();
if (import.meta.env.DEV) (window as any).__lobsObs = obs; // debugging hook in dev builds only
const POOL = '[LOBS] Pool';
const PREFIX = '[LOBS] ';
export const privateSceneName = (instId: string) => PREFIX + instId;
const INSTANCE_PREFIX = '[LOBS] layer ';
const instanceInputName = (instId: string) => INSTANCE_PREFIX + instId;

// Remember the OBS names we last pushed, so renames become SetInputName/SetSceneName.
const NAMES_KEY = 'lobs.obs.names';
let pushedNames: { inputs: Record<string, string>; scenes: Record<string, string> } = (() => {
  try {
    return JSON.parse(localStorage.getItem(NAMES_KEY) ?? '') ?? { inputs: {}, scenes: {} };
  } catch {
    return { inputs: {}, scenes: {} };
  }
})();
const saveNames = () => localStorage.setItem(NAMES_KEY, JSON.stringify(pushedNames));

// ---------- connection ----------

export async function connect(url: string, password?: string) {
  set({ status: 'connecting', error: null, url });
  localStorage.setItem('lobs.obs.url', url);
  try {
    const hello = await obs.connect(url, password || undefined);
    const ver = await obs.call('GetVersion');
    set({ status: 'connected', obsVersion: ver.obsVersion });
    void hello;
    await refreshLive();
    await refreshTransitions();
    const studio = await obs.call('GetStudioModeEnabled');
    set({ studioMode: studio.studioModeEnabled });
    toast(`Connected to OBS ${ver.obsVersion}`);
    await push(getState().doc);
    startThumbs();
  } catch (e: any) {
    set({ status: 'error', error: e?.message ?? String(e) });
  }
}

export async function disconnect() {
  stopThumbs();
  await obs.disconnect();
}

obs.on('ConnectionClosed', () => {
  stopThumbs();
  if (bs.status === 'connected') toast('Disconnected from OBS');
  set({ status: bs.status === 'error' ? 'error' : 'disconnected', streaming: false, recording: false });
  setUi({ live: false });
});
obs.on('StreamStateChanged', (e) => {
  set({ streaming: e.outputActive });
  syncLive();
});
obs.on('RecordStateChanged', (e) => {
  set({ recording: e.outputActive });
  syncLive();
});
obs.on('CurrentProgramSceneChanged', (e) => {
  const doc = getState().doc;
  const id = doc.sceneOrder.find((s) => doc.scenes[s].name === e.sceneName) ?? null;
  setUi({ programSceneId: id });
});
obs.on('StudioModeStateChanged', (e) => set({ studioMode: e.studioModeEnabled }));
obs.on('CurrentSceneTransitionChanged', (e) => set({ transition: e.transitionName }));

function syncLive() {
  setUi({ live: bs.streaming || bs.recording });
}

async function refreshLive() {
  const [st, rec, prog] = await Promise.all([obs.call('GetStreamStatus'), obs.call('GetRecordStatus'), obs.call('GetCurrentProgramScene')]);
  set({ streaming: st.outputActive, recording: rec.outputActive });
  const doc = getState().doc;
  setUi({ programSceneId: doc.sceneOrder.find((s) => doc.scenes[s].name === prog.currentProgramSceneName) ?? null });
  syncLive();
}

async function refreshTransitions() {
  const t = await obs.call('GetSceneTransitionList');
  set({ transitions: t.transitions.map((x: any) => x.transitionName as string), transition: (t.currentSceneTransitionName as string) ?? '' });
}

// ---------- live controls ----------

const connected = () => bs.status === 'connected';

export async function take(sceneId: string) {
  const doc = getState().doc;
  const name = doc.scenes[sceneId]?.name;
  if (!name) return;
  if (!connected()) {
    setUi({ programSceneId: sceneId });
    return;
  }
  await flushPush();
  await obs.call('SetCurrentProgramScene', { sceneName: name });
}

export async function setTransition(name: string) {
  if (connected()) await obs.call('SetCurrentSceneTransition', { transitionName: name });
}

export async function toggleStream() {
  if (connected()) await obs.call('ToggleStream');
}
export async function toggleRecord() {
  if (connected()) await obs.call('ToggleRecord');
}

/** Device/monitor/window choices for a source, read from OBS. */
export async function listItems(source: Source): Promise<{ name: string; value: string }[]> {
  const prop = DEVICE_PROP[source.kind];
  if (!connected() || !prop) return [];
  try {
    await flushPush();
    const r = await obs.call('GetInputPropertiesListPropertyItems', { inputName: obsInputName(source), propertyName: prop });
    return r.propertyItems.filter((i: any) => i.itemEnabled !== false).map((i: any) => ({ name: String(i.itemName), value: String(i.itemValue) }));
  } catch {
    return [];
  }
}

// ---------- kind mapping ----------

const KIND_CANDIDATES: Record<SourceKind, string[]> = {
  camera: ['dshow_input', 'av_capture_input', 'macos-avcapture', 'v4l2_input'],
  display: ['monitor_capture', 'screen_capture', 'display_capture', 'xshm_input', 'pipewire-desktop-capture-source'],
  window: ['window_capture', 'xcomposite_input', 'pipewire-window-capture-source'],
  game: ['game_capture'],
  image: ['image_source'],
  media: ['ffmpeg_source'],
  browser: ['browser_source'],
  color: ['color_source'],
  text: ['text_gdiplus', 'text_ft2_source'],
  mic: ['wasapi_input_capture', 'coreaudio_input_capture', 'pulse_input_capture'],
  desktop_audio: ['wasapi_output_capture', 'coreaudio_output_capture', 'pulse_output_capture'],
};

export const DEVICE_PROP: Partial<Record<SourceKind, string>> = {
  camera: 'video_device_id',
  display: 'monitor_id',
  window: 'window',
  mic: 'device_id',
  desktop_audio: 'device_id',
};

let kindList: string[] = [];
function obsKind(kind: SourceKind): string {
  for (const base of KIND_CANDIDATES[kind]) {
    // Prefer the newest versioned kind, e.g. color_source_v3.
    const matches = kindList.filter((k) => k === base || k.startsWith(base + '_v')).sort();
    if (matches.length) return matches[matches.length - 1];
  }
  return KIND_CANDIDATES[kind][0];
}

function hexToAbgr(hex: string) {
  const m = /^#?([0-9a-f]{6})$/i.exec(hex);
  if (!m) return 0xff000000;
  const n = parseInt(m[1], 16);
  const r = (n >> 16) & 255, g = (n >> 8) & 255, b = n & 255;
  return (0xff000000 + (b << 16) + (g << 8) + r) >>> 0;
}

function obsSettings(doc: LobsDoc, s: Source): Record<string, unknown> {
  const st = s.settings as any;
  switch (s.kind) {
    case 'color':
      return { color: hexToAbgr(st.color ?? '#000000'), width: doc.canvas.w, height: doc.canvas.h };
    case 'text':
      return { text: st.text ?? '' , color: hexToAbgr(st.textColor ?? '#ffffff') & 0xffffff, font: { face: 'Arial', size: Number(st.size ?? 96), style: 'Bold', flags: 1 } };
    case 'browser':
      return { url: st.url ?? 'about:blank', width: Number(st.width ?? 800), height: Number(st.height ?? 600) };
    case 'image':
      return { file: st.file ?? '' };
    case 'media':
      return { local_file: st.file ?? '', looping: !!st.loop };
    default: {
      const prop = DEVICE_PROP[s.kind];
      return prop && st.device ? { [prop]: st.device } : {};
    }
  }
}

const MONITOR: Record<MonitorType, string> = {
  off: 'OBS_MONITORING_TYPE_NONE',
  monitor_only: 'OBS_MONITORING_TYPE_MONITOR_ONLY',
  monitor_and_output: 'OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT',
};

function obsFilter(f: Filter): { kind: string; settings: Record<string, unknown> } | null {
  const s = f.settings as any;
  switch (f.type) {
    case 'color':
      return { kind: 'color_filter_v2', settings: { brightness: s.brightness, contrast: s.contrast, saturation: s.saturation, gamma: s.gamma, hue_shift: s.hue } };
    case 'chroma_key':
      return { kind: 'chroma_key_filter_v2', settings: { key_color_type: 'custom', key_color: hexToAbgr(s.key_color) & 0xffffff, similarity: s.similarity, smoothness: s.smoothness } };
    case 'sharpness':
      return { kind: 'sharpness_filter_v2', settings: { sharpness: s.amount } };
    case 'lut':
      return { kind: 'clut_filter', settings: { image_path: s.path, clut_amount: s.amount } };
    case 'blur':
      return null; // needs a third-party plugin (e.g. Composite Blur); kept in the doc, skipped on push
  }
}

// ---------- reconciler ----------

let pushing: Promise<void> | null = null;
let dirty = false;
let lastSettings: Record<string, string> = {};

onDocChange((doc) => {
  if (!connected()) return;
  clearTimeout(pushTimer);
  pushTimer = window.setTimeout(() => void push(doc), 350);
});
let pushTimer: number | undefined;

async function flushPush() {
  clearTimeout(pushTimer);
  if (pushing) await pushing;
  await push(getState().doc);
}

export async function push(doc: LobsDoc): Promise<void> {
  if (!connected()) return;
  if (pushing) {
    dirty = true;
    return pushing;
  }
  set({ syncing: true });
  pushing = reconcile(doc)
    .then(() => set({ lastSync: Date.now(), syncError: null }))
    .catch((e) => set({ syncError: e?.message ?? String(e) }))
    .finally(() => {
      pushing = null;
      set({ syncing: false });
      if (dirty) {
        dirty = false;
        void push(getState().doc);
      }
    });
  return pushing;
}

async function call(req: string, data?: any): Promise<any> {
  try {
    return await (obs.call as any)(req, data);
  } catch (e: any) {
    const what = data?.inputName ?? data?.sourceName ?? data?.sceneName ?? '';
    throw new Error(`${req}${what ? ` (${what})` : ''}: ${e?.message ?? e}`);
  }
}

/** The OBS input name a pool source was pushed under (may differ from its LOBS name on a clash). */
export const obsInputName = (s: Source) => pushedNames.inputs[s.id] ?? s.name;

async function ensureScene(name: string, existing: Set<string>) {
  if (!existing.has(name)) {
    await call('CreateScene', { sceneName: name });
    existing.add(name);
  }
}

async function reconcile(doc: LobsDoc) {
  if (!kindList.length) kindList = (await call('GetInputKindList', { unversioned: false })).inputKinds;
  const video = await call('GetVideoSettings');
  const W = video.baseWidth as number, H = video.baseHeight as number;

  const sceneNames = new Set<string>((await call('GetSceneList')).scenes.map((s: any) => s.sceneName));
  const inputs = new Map<string, string>((await call('GetInputList')).inputs.map((i: any) => [i.inputName, i.inputKind]));
  const audioWarnings: string[] = [];

  // 1. Inputs
  await ensureScene(POOL, sceneNames);
  for (const id of doc.sourceOrder) {
    const s = doc.sources[id];
    // LOBS only manages inputs it created. If the name belongs to an input LOBS didn't make,
    // push under "<name> [LOBS]" so the user's own input is never changed or deleted.
    const clash = inputs.has(s.name) && pushedNames.inputs[id] !== s.name;
    const name = clash ? `${s.name} [LOBS]` : s.name;
    const old = pushedNames.inputs[id];
    if (old && old !== name && inputs.has(old) && !inputs.has(name)) {
      await call('SetInputName', { inputName: old, newInputName: name });
      inputs.set(name, inputs.get(old)!);
      inputs.delete(old);
    }
    const settings = obsSettings(doc, s);
    const sig = JSON.stringify(settings);
    if (!inputs.has(name)) {
      await call('CreateInput', { sceneName: POOL, inputName: name, inputKind: obsKind(s.kind), inputSettings: settings, sceneItemEnabled: false });
      inputs.set(name, obsKind(s.kind));
    } else if (lastSettings[id] !== sig) {
      await call('SetInputSettings', { inputName: name, inputSettings: settings, overlay: true });
    }
    lastSettings[id] = sig;
    pushedNames.inputs[id] = name;
    if (s.audio) {
      try {
        await call('SetInputVolume', { inputName: name, inputVolumeMul: s.audio.volume });
        await call('SetInputMute', { inputName: name, inputMuted: s.audio.muted });
        await call('SetInputAudioMonitorType', { inputName: name, monitorType: MONITOR[s.audio.monitor] });
        await call('SetInputAudioTracks', { inputName: name, inputAudioTracks: Object.fromEntries(s.audio.tracks.map((v, i) => [String(i + 1), v])) });
      } catch (e: any) {
        audioWarnings.push(e.message); // e.g. a browser source with audio control off; keep syncing video
      }
    }
  }
  // Inputs removed from the pool
  for (const [id, name] of Object.entries(pushedNames.inputs)) {
    if (!doc.sources[id]) {
      if (inputs.has(name)) await call('RemoveInput', { inputName: name });
      delete pushedNames.inputs[id];
      delete lastSettings[id];
    }
  }

  // 2. Scenes
  const wantedPrivate = new Set<string>();
  const wantedInstances = new Set<string>();
  const usePlugin = kindList.includes('lobs_instance');
  set({ pluginMode: usePlugin });
  for (const sceneId of doc.sceneOrder) {
    const scene = doc.scenes[sceneId];
    const old = pushedNames.scenes[sceneId];
    if (old && old !== scene.name && sceneNames.has(old) && !sceneNames.has(scene.name)) {
      await call('SetSceneName', { sceneName: old, newSceneName: scene.name });
      sceneNames.delete(old);
      sceneNames.add(scene.name);
    }
    await ensureScene(scene.name, sceneNames);
    pushedNames.scenes[sceneId] = scene.name;

    const leaves = flatten(doc, sceneId).filter(({ inst }) => KIND_INFO[doc.sources[inst.sourceId!]?.kind]?.video);
    const outerItems: any[] = (await call('GetSceneItemList', { sceneName: scene.name })).sceneItems;
    const order: number[] = [];

    for (const { inst, visible, opacity } of leaves) {
      const src = doc.sources[inst.sourceId!];
      const inputName = obsInputName(src);
      const crop = getProp(doc, inst, 'crop');
      let wrapper: string; // the per-layer OBS source that owns this layer's filters
      let outerCrop = { r: 0, b: 0 };

      if (usePlugin) {
        // LOBS plugin: one "lobs_instance" input per layer renders the shared input, cropped.
        wrapper = instanceInputName(inst.id);
        wantedInstances.add(wrapper);
        const settings = { source: inputName, crop_left: crop.l, crop_top: crop.t, crop_right: crop.r, crop_bottom: crop.b };
        if (!inputs.has(wrapper)) {
          await call('CreateInput', { sceneName: POOL, inputName: wrapper, inputKind: 'lobs_instance', inputSettings: settings, sceneItemEnabled: false });
          inputs.set(wrapper, 'lobs_instance');
        } else await call('SetInputSettings', { inputName: wrapper, inputSettings: settings, overlay: true });
      } else {
        // Fallback without the plugin: a private scene holding the shared input.
        wrapper = privateSceneName(inst.id);
        wantedPrivate.add(wrapper);
        await ensureScene(wrapper, sceneNames);
        const inner: any[] = (await call('GetSceneItemList', { sceneName: wrapper })).sceneItems;
        for (const it of inner) if (it.sourceName !== inputName) await call('RemoveSceneItem', { sceneName: wrapper, sceneItemId: it.sceneItemId });
        let innerId = inner.find((it) => it.sourceName === inputName)?.sceneItemId;
        if (innerId == null) innerId = (await call('CreateSceneItem', { sceneName: wrapper, sourceName: inputName })).sceneItemId;
        const it = (await call('GetSceneItemTransform', { sceneName: wrapper, sceneItemId: innerId })).sceneItemTransform;
        const sw = it.sourceWidth || W, sh = it.sourceHeight || H;
        bs.sizes[src.name] = [sw, sh];
        const cw = Math.max(1, sw - crop.l - crop.r), ch = Math.max(1, sh - crop.t - crop.b);
        const k = Math.min(1, W / cw, H / ch);
        await call('SetSceneItemTransform', {
          sceneName: wrapper,
          sceneItemId: innerId,
          sceneItemTransform: { positionX: 0, positionY: 0, rotation: 0, alignment: 5, boundsType: 'OBS_BOUNDS_NONE', scaleX: k, scaleY: k, cropLeft: crop.l, cropTop: crop.t, cropRight: crop.r, cropBottom: crop.b },
        });
        outerCrop = { r: Math.max(0, Math.floor(W - cw * k)), b: Math.max(0, Math.floor(H - ch * k)) };
      }

      await syncFilters(wrapper, getProp(doc, inst, 'filters'), opacity);

      // Outer item: the wrapper placed on the user scene.
      let outer = outerItems.find((o) => o.sourceName === wrapper);
      if (!outer) {
        const r = await call('CreateSceneItem', { sceneName: scene.name, sourceName: wrapper });
        outer = { sceneItemId: r.sceneItemId, sourceName: wrapper };
        outerItems.push(outer);
      }
      const t = getProp(doc, inst, 'transform');
      await call('SetSceneItemTransform', {
        sceneName: scene.name,
        sceneItemId: outer.sceneItemId,
        sceneItemTransform: {
          positionX: t.x,
          positionY: t.y,
          rotation: t.rot,
          alignment: 5,
          boundsType: 'OBS_BOUNDS_STRETCH',
          boundsAlignment: 5,
          boundsWidth: Math.max(1, t.w),
          boundsHeight: Math.max(1, t.h),
          cropLeft: 0,
          cropTop: 0,
          cropRight: outerCrop.r,
          cropBottom: outerCrop.b,
        },
      });
      if (usePlugin) {
        const ot = (await call('GetSceneItemTransform', { sceneName: scene.name, sceneItemId: outer.sceneItemId })).sceneItemTransform;
        if (ot.sourceWidth) bs.sizes[src.name] = [ot.sourceWidth + crop.l + crop.r, ot.sourceHeight + crop.t + crop.b];
      }
      await call('SetSceneItemEnabled', { sceneName: scene.name, sceneItemId: outer.sceneItemId, sceneItemEnabled: visible });
      await call('SetSceneItemLocked', { sceneName: scene.name, sceneItemId: outer.sceneItemId, sceneItemLocked: true });
      order.push(outer.sceneItemId);
    }

    // LOBS owns this scene: drop anything that isn't in the document.
    for (const o of outerItems) if (!order.includes(o.sceneItemId)) await call('RemoveSceneItem', { sceneName: scene.name, sceneItemId: o.sceneItemId });
    for (let i = 0; i < order.length; i++) await call('SetSceneItemIndex', { sceneName: scene.name, sceneItemId: order[i], sceneItemIndex: i });
  }

  // 3. Clean up deleted scenes and orphaned instance scenes.
  const program = (await call('GetCurrentProgramScene')).currentProgramSceneName as string;
  for (const [id, name] of Object.entries(pushedNames.scenes)) {
    if (!doc.scenes[id]) {
      // Never delete the on-air scene out from under OBS: move program to another LOBS scene first.
      const fallback = doc.sceneOrder.map((s) => doc.scenes[s].name).find((n) => sceneNames.has(n));
      if (name === program && fallback) await call('SetCurrentProgramScene', { sceneName: fallback });
      if (sceneNames.has(name)) await call('RemoveScene', { sceneName: name });
      delete pushedNames.scenes[id];
    }
  }
  for (const name of sceneNames) {
    if (!name.startsWith(PREFIX + 'ins_') || wantedPrivate.has(name)) continue;
    const fallback = doc.sceneOrder.map((s) => doc.scenes[s].name).find((n) => sceneNames.has(n));
    if (name === program && fallback) await call('SetCurrentProgramScene', { sceneName: fallback });
    await call('RemoveScene', { sceneName: name });
  }
  for (const name of inputs.keys()) if (name.startsWith(INSTANCE_PREFIX) && !wantedInstances.has(name)) await call('RemoveInput', { inputName: name });
  saveNames();
  if (audioWarnings.length) throw new Error(audioWarnings.join(' · '));
}

async function syncFilters(sourceName: string, filters: Filter[], opacity: number) {
  const existing: any[] = (await call('GetSourceFilterList', { sourceName })).filters;
  const wanted: { name: string; kind: string; enabled: boolean; settings: Record<string, unknown> }[] = [];
  filters.forEach((f, i) => {
    const m = obsFilter(f);
    if (m) wanted.push({ name: `LOBS ${i + 1} ${f.type}`, kind: m.kind, enabled: f.enabled, settings: m.settings });
  });
  if (opacity < 1) wanted.push({ name: 'LOBS opacity', kind: 'color_filter_v2', enabled: true, settings: { opacity } });

  for (const e of existing) if (!wanted.some((w) => w.name === e.filterName && w.kind === e.filterKind)) await call('RemoveSourceFilter', { sourceName, filterName: e.filterName });
  for (let i = 0; i < wanted.length; i++) {
    const w = wanted[i];
    const had = existing.find((e) => e.filterName === w.name && e.filterKind === w.kind);
    if (!had) await call('CreateSourceFilter', { sourceName, filterName: w.name, filterKind: w.kind, filterSettings: w.settings });
    else {
      await call('SetSourceFilterSettings', { sourceName, filterName: w.name, filterSettings: w.settings, overlay: false });
    }
    await call('SetSourceFilterEnabled', { sourceName, filterName: w.name, filterEnabled: w.enabled });
    await call('SetSourceFilterIndex', { sourceName, filterName: w.name, filterIndex: i });
  }
}

// ---------- thumbnails ----------

let thumbTimer: number | undefined;
function startThumbs() {
  stopThumbs();
  const tick = async () => {
    if (!connected()) return;
    const { doc, ui } = getState();
    const names = new Map<string, string>(); // LOBS name -> OBS name
    const addSrc = (s?: Source) => s && KIND_INFO[s.kind].video && names.set(s.name, obsInputName(s));
    if (ui.sceneId) for (const { inst } of flatten(doc, ui.sceneId)) addSrc(doc.sources[inst.sourceId!]);
    if (ui.poolSelection) addSrc(doc.sources[ui.poolSelection]);
    if (ui.sceneView === 'grid') for (const id of doc.sceneOrder) names.set(doc.scenes[id].name, doc.scenes[id].name);
    const thumbs = { ...bs.thumbs };
    for (const [key, sourceName] of names) {
      try {
        const r = await call('GetSourceScreenshot', { sourceName, imageFormat: 'jpg', imageWidth: 480, imageCompressionQuality: 60 });
        thumbs[key] = r.imageData;
      } catch {}
    }
    set({ thumbs });
    thumbTimer = window.setTimeout(tick, 1500);
  };
  void tick();
}
function stopThumbs() {
  clearTimeout(thumbTimer);
}
