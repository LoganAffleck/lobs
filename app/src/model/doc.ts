// Pure operations on a LobsDoc. All mutators expect a draft (already cloned) doc.
import {
  AudioSettings,
  Filter,
  FilterType,
  Instance,
  LobsDoc,
  PROP_GROUPS,
  PropGroup,
  PropMap,
  Scene,
  Source,
  SourceKind,
} from './types';

export const uid = (p = '') => p + Math.random().toString(36).slice(2, 10);
const clone = <T>(v: T): T => structuredClone(v);

export const KIND_INFO: Record<SourceKind, { label: string; icon: string; video: boolean; audio: boolean; size: [number, number] | 'canvas' }> = {
  camera: { label: 'Camera', icon: '🎥', video: true, audio: false, size: [1280, 720] },
  display: { label: 'Display Capture', icon: '🖥', video: true, audio: false, size: 'canvas' },
  window: { label: 'Window Capture', icon: '🪟', video: true, audio: false, size: 'canvas' },
  game: { label: 'Game Capture', icon: '🎮', video: true, audio: false, size: 'canvas' },
  image: { label: 'Image', icon: '🖼', video: true, audio: false, size: [800, 450] },
  media: { label: 'Media File', icon: '🎞', video: true, audio: true, size: [1280, 720] },
  browser: { label: 'Browser', icon: '🌐', video: true, audio: true, size: [800, 600] },
  color: { label: 'Color', icon: '🟪', video: true, audio: false, size: 'canvas' },
  text: { label: 'Text', icon: '🔤', video: true, audio: false, size: [600, 120] },
  mic: { label: 'Microphone', icon: '🎙', video: false, audio: true, size: [0, 0] },
  desktop_audio: { label: 'Desktop Audio', icon: '🔊', video: false, audio: true, size: [0, 0] },
};

export const FILTER_INFO: Record<FilterType, { label: string; defaults: Filter['settings'] }> = {
  color: { label: 'Color Correction', defaults: { brightness: 0, contrast: 0, saturation: 0, gamma: 0, hue: 0 } },
  chroma_key: { label: 'Chroma Key', defaults: { key_color: '#00ff00', similarity: 400, smoothness: 80 } },
  sharpness: { label: 'Sharpen', defaults: { amount: 0.08 } },
  blur: { label: 'Blur', defaults: { size: 8 } },
  lut: { label: 'LUT', defaults: { path: '', amount: 1 } },
};

const TAGS = ['#e5484d', '#f76b15', '#ffc53d', '#46a758', '#12a594', '#0090ff', '#8e4ec6', '#d6409f'];

export function emptyDoc(): LobsDoc {
  return { version: 1, canvas: { w: 1920, h: 1080 }, sources: {}, sourceOrder: [], scenes: {}, sceneOrder: [], instances: {}, links: {}, bins: [] };
}

export function defaultAudio(kind: SourceKind): AudioSettings {
  return { volume: 1, muted: false, monitor: 'off', tracks: [true, false, false, false, false, false], filters: [] };
}

export function addSource(doc: LobsDoc, kind: SourceKind, name: string, settings: Record<string, unknown> = {}): Source {
  const s: Source = {
    id: uid('src_'),
    name: uniqueName(name, Object.values(doc.sources).map((x) => x.name)),
    kind,
    settings,
    bin: '',
    color: TAGS[doc.sourceOrder.length % TAGS.length],
    audio: KIND_INFO[kind].audio ? defaultAudio(kind) : undefined,
  };
  doc.sources[s.id] = s;
  doc.sourceOrder.push(s.id);
  return s;
}

export function uniqueName(base: string, taken: string[]) {
  if (!taken.includes(base)) return base;
  let i = 2;
  while (taken.includes(`${base} ${i}`)) i++;
  return `${base} ${i}`;
}

export function addScene(doc: LobsDoc, name: string): Scene {
  const s: Scene = { id: uid('scn_'), name: uniqueName(name, Object.values(doc.scenes).map((x) => x.name)), folder: '', favorite: false, layers: [] };
  doc.scenes[s.id] = s;
  doc.sceneOrder.push(s.id);
  return s;
}

export function defaultProps(doc: LobsDoc, src?: Source): PropMap {
  const size = src ? KIND_INFO[src.kind].size : 'canvas';
  const [w, h] = size === 'canvas' ? [doc.canvas.w, doc.canvas.h] : size;
  return {
    transform: { x: Math.round((doc.canvas.w - w) / 2), y: Math.round((doc.canvas.h - h) / 2), w, h, rot: 0 },
    crop: { l: 0, t: 0, r: 0, b: 0 },
    filters: [],
    visibility: { visible: true, opacity: 1 },
  };
}

/** Place a pool source on a scene. Changes to the instance are local to that scene. */
export function placeSource(doc: LobsDoc, sceneId: string, sourceId: string, at?: { x: number; y: number }): Instance {
  const src = doc.sources[sourceId];
  const props = defaultProps(doc, src);
  if (at) {
    props.transform.x = Math.round(at.x - props.transform.w / 2);
    props.transform.y = Math.round(at.y - props.transform.h / 2);
  }
  const inst: Instance = { id: uid('ins_'), sceneId, kind: 'source', sourceId, name: src.name, locked: false, props, links: {} };
  doc.instances[inst.id] = inst;
  doc.scenes[sceneId].layers.unshift(inst.id);
  return inst;
}

// ---------- property access with linking ----------

export function getProp<G extends PropGroup>(doc: LobsDoc, inst: Instance, g: G): PropMap[G] {
  const linkId = inst.links[g];
  if (linkId && doc.links[linkId]) return doc.links[linkId].value as PropMap[G];
  return inst.props[g];
}

export function setProp<G extends PropGroup>(doc: LobsDoc, instId: string, g: G, value: PropMap[G]) {
  const inst = doc.instances[instId];
  const linkId = inst.links[g];
  if (linkId && doc.links[linkId]) doc.links[linkId].value = value;
  else inst.props[g] = value;
}

export function linkedPeers(doc: LobsDoc, inst: Instance, g: PropGroup): Instance[] {
  const linkId = inst.links[g];
  if (!linkId) return [];
  return Object.values(doc.instances).filter((i) => i.id !== inst.id && i.links[g] === linkId);
}

export function isAnyLinked(doc: LobsDoc, inst: Instance) {
  return PROP_GROUPS.some((g) => linkedPeers(doc, inst, g).length > 0);
}

/** Detach one instance from a link; it keeps the current value as its own. */
export function unlink(doc: LobsDoc, instId: string, g: PropGroup) {
  const inst = doc.instances[instId];
  const linkId = inst.links[g];
  if (!linkId) return;
  (inst.props as any)[g] = clone(getProp(doc, inst, g));
  delete inst.links[g];
  gcLinks(doc);
}

/** Link `instId` into `targetId`'s group. `keep` decides which side's value wins. */
export function linkTo(doc: LobsDoc, instId: string, targetId: string, g: PropGroup, keep: 'mine' | 'theirs') {
  const inst = doc.instances[instId];
  const target = doc.instances[targetId];
  const winner = keep === 'mine' ? getProp(doc, inst, g) : getProp(doc, target, g);
  let linkId = target.links[g];
  if (!linkId) {
    linkId = uid('lnk_');
    doc.links[linkId] = { id: linkId, group: g, value: clone(target.props[g]) };
    target.links[g] = linkId;
  }
  // If inst was in another link, move everything from that link over too.
  const old = inst.links[g];
  if (old && old !== linkId) for (const i of Object.values(doc.instances)) if (i.links[g] === old) i.links[g] = linkId;
  inst.links[g] = linkId;
  doc.links[linkId].value = clone(winner);
  gcLinks(doc);
}

function gcLinks(doc: LobsDoc) {
  const count: Record<string, string[]> = {};
  for (const i of Object.values(doc.instances)) for (const [g, id] of Object.entries(i.links)) (count[id!] ??= []).push(i.id + ':' + g);
  for (const id of Object.keys(doc.links)) {
    const users = count[id] ?? [];
    if (users.length <= 1) {
      // A link with one member is just a local value.
      for (const u of users) {
        const [iid, g] = u.split(':') as [string, PropGroup];
        (doc.instances[iid].props as any)[g] = clone(doc.links[id].value);
        delete doc.instances[iid].links[g];
      }
      delete doc.links[id];
    }
  }
}

// ---------- tree helpers ----------

export function parentOf(doc: LobsDoc, instId: string): Instance | undefined {
  return Object.values(doc.instances).find((i) => i.children?.includes(instId));
}

export function siblingsArray(doc: LobsDoc, instId: string): string[] {
  const p = parentOf(doc, instId);
  return p ? p.children! : doc.scenes[doc.instances[instId].sceneId].layers;
}

export function descendants(doc: LobsDoc, instId: string): Instance[] {
  const inst = doc.instances[instId];
  if (inst.kind !== 'group') return [inst];
  return [inst, ...inst.children!.flatMap((c) => descendants(doc, c))];
}

/** Leaf source instances of a scene, bottom-first (render order), with group visibility folded in. */
export function flatten(doc: LobsDoc, sceneId: string): { inst: Instance; visible: boolean; opacity: number }[] {
  const out: { inst: Instance; visible: boolean; opacity: number }[] = [];
  const walk = (ids: string[], vis: boolean, op: number) => {
    for (const id of [...ids].reverse()) {
      const inst = doc.instances[id];
      if (!inst) continue;
      const v = getProp(doc, inst, 'visibility');
      if (inst.kind === 'group') walk(inst.children!, vis && v.visible, op * v.opacity);
      else out.push({ inst, visible: vis && v.visible, opacity: op * v.opacity });
    }
  };
  walk(doc.scenes[sceneId]?.layers ?? [], true, 1);
  return out;
}

export function groupBounds(doc: LobsDoc, groupId: string) {
  const leaves = descendants(doc, groupId).filter((i) => i.kind === 'source');
  if (!leaves.length) return { x: 0, y: 0, w: 0, h: 0, rot: 0 };
  const ts = leaves.map((i) => getProp(doc, i, 'transform'));
  const x = Math.min(...ts.map((t) => t.x));
  const y = Math.min(...ts.map((t) => t.y));
  return { x, y, w: Math.max(...ts.map((t) => t.x + t.w)) - x, h: Math.max(...ts.map((t) => t.y + t.h)) - y, rot: 0 };
}

export function moveBy(doc: LobsDoc, instId: string, dx: number, dy: number, seen = new Set<string>()) {
  for (const i of descendants(doc, instId)) {
    if (i.kind !== 'source') continue;
    const key = i.links.transform ?? i.id; // don't double-move linked transforms
    if (seen.has(key)) continue;
    seen.add(key);
    const t = getProp(doc, i, 'transform');
    setProp(doc, i.id, 'transform', { ...t, x: Math.round(t.x + dx), y: Math.round(t.y + dy) });
  }
}

export function groupSelection(doc: LobsDoc, ids: string[]): Instance | undefined {
  if (!ids.length) return;
  const arr = siblingsArray(doc, ids[0]);
  const members = arr.filter((id) => ids.includes(id)); // same level only, keep order
  if (!members.length) return;
  const sceneId = doc.instances[members[0]].sceneId;
  const g: Instance = { id: uid('ins_'), sceneId, kind: 'group', name: uniqueName('Group', Object.values(doc.instances).map((i) => i.name)), children: members, locked: false, props: defaultProps(doc), links: {} };
  doc.instances[g.id] = g;
  const at = arr.indexOf(members[0]);
  for (const m of members) arr.splice(arr.indexOf(m), 1);
  arr.splice(at, 0, g.id);
  return g;
}

export function ungroup(doc: LobsDoc, groupId: string) {
  const g = doc.instances[groupId];
  if (g.kind !== 'group') return;
  const arr = siblingsArray(doc, groupId);
  arr.splice(arr.indexOf(groupId), 1, ...g.children!);
  removeInstanceRecord(doc, groupId);
}

function removeInstanceRecord(doc: LobsDoc, id: string) {
  delete doc.instances[id];
  gcLinks(doc);
}

export function deleteInstance(doc: LobsDoc, id: string) {
  if (!doc.instances[id]) return;
  const arr = siblingsArray(doc, id);
  arr.splice(arr.indexOf(id), 1);
  for (const d of descendants(doc, id)) delete doc.instances[d.id];
  gcLinks(doc);
}

export function deleteSource(doc: LobsDoc, sourceId: string) {
  for (const i of Object.values(doc.instances)) if (i.sourceId === sourceId && doc.instances[i.id]) deleteInstance(doc, i.id);
  delete doc.sources[sourceId];
  doc.sourceOrder = doc.sourceOrder.filter((s) => s !== sourceId);
}

export function deleteScene(doc: LobsDoc, sceneId: string) {
  for (const id of [...doc.scenes[sceneId].layers]) deleteInstance(doc, id);
  delete doc.scenes[sceneId];
  doc.sceneOrder = doc.sceneOrder.filter((s) => s !== sceneId);
}

/** Move an instance before/after/into another, within one scene. */
export function reorder(doc: LobsDoc, dragId: string, targetId: string, where: 'before' | 'after' | 'into') {
  if (dragId === targetId) return;
  if (descendants(doc, dragId).some((d) => d.id === targetId)) return; // can't drop into itself
  const from = siblingsArray(doc, dragId);
  from.splice(from.indexOf(dragId), 1);
  const target = doc.instances[targetId];
  if (where === 'into' && target.kind === 'group') {
    target.children!.unshift(dragId);
    return;
  }
  const to = siblingsArray(doc, targetId);
  to.splice(to.indexOf(targetId) + (where === 'after' ? 1 : 0), 0, dragId);
}

// ---------- copy between scenes ----------

export type CopyMode = 'linked' | 'independent';

/** Copy instances (with group subtrees) into another scene. Linked copies share every property group. */
export function copyToScene(doc: LobsDoc, ids: string[], sceneId: string, mode: CopyMode): string[] {
  const made: string[] = [];
  const copy = (id: string): string => {
    const src = doc.instances[id];
    const n: Instance = clone(src);
    n.id = uid('ins_');
    n.sceneId = sceneId;
    if (src.kind === 'group') n.children = src.children!.map(copy);
    if (mode === 'independent') {
      for (const g of PROP_GROUPS) (n.props as any)[g] = clone(getProp(doc, src, g));
      n.links = {};
    } else if (src.kind === 'source') {
      doc.instances[n.id] = n;
      for (const g of PROP_GROUPS) linkTo(doc, n.id, src.id, g, 'theirs');
      return n.id;
    }
    doc.instances[n.id] = n;
    return n.id;
  };
  for (const id of [...ids].reverse()) {
    const nid = copy(id);
    doc.scenes[sceneId].layers.unshift(nid);
    made.push(nid);
  }
  return made;
}

export function duplicateScene(doc: LobsDoc, sceneId: string, mode: CopyMode): Scene {
  const s = doc.scenes[sceneId];
  const n = addScene(doc, s.name + ' copy');
  n.folder = s.folder;
  copyToScene(doc, s.layers, n.id, mode);
  return n;
}

export function newFilter(type: FilterType): Filter {
  return { id: uid('flt_'), type, enabled: true, settings: { ...FILTER_INFO[type].defaults } };
}

export function instancesOfSource(doc: LobsDoc, sourceId: string) {
  return Object.values(doc.instances).filter((i) => i.sourceId === sourceId);
}

export function sampleDoc(): LobsDoc {
  const d = emptyDoc();
  const cam = addSource(d, 'camera', 'Facecam');
  const game = addSource(d, 'game', 'Game Capture');
  const disp = addSource(d, 'display', 'Display 1');
  const bg = addSource(d, 'color', 'Background', { color: '#1d1b2e' });
  const title = addSource(d, 'text', 'Title', { text: 'Starting soon' });
  addSource(d, 'mic', 'Mic');
  addSource(d, 'desktop_audio', 'Desktop Audio');
  d.bins = ['Overlays'];
  title.bin = 'Overlays';
  bg.bin = 'Overlays';

  const start = addScene(d, 'Starting Soon');
  placeSource(d, start.id, bg.id);
  const t = placeSource(d, start.id, title.id);
  t.props.transform = { x: 660, y: 480, w: 600, h: 120, rot: 0 };

  const chat = addScene(d, 'Just Chatting');
  placeSource(d, chat.id, disp.id);
  const c1 = placeSource(d, chat.id, cam.id);
  c1.props.transform = { x: 1200, y: 560, w: 640, h: 360, rot: 0 };
  c1.props.filters = [newFilter('color')];

  const play = addScene(d, 'Gameplay');
  placeSource(d, play.id, game.id);
  const c2 = placeSource(d, play.id, cam.id);
  c2.props.transform = { x: 1500, y: 40, w: 384, h: 216, rot: 0 };
  c2.props.crop = { l: 160, t: 0, r: 160, b: 0 };
  linkTo(d, c2.id, c1.id, 'filters', 'theirs'); // same grade everywhere, different size

  addScene(d, 'BRB');
  return d;
}
