// The whole LOBS editor as one plain, serializable state. Screenshots and videos inject it
// (a menu open, a layer selected, live for 1:02:11); the React UI renders it and, when clicked,
// changes it with the same reducer the docs and videos can drive.
import type { IconName } from './icons.gen'

export type SourceKind = 'camera' | 'display' | 'window' | 'game' | 'browser' | 'mic' | 'desktop_audio' | 'image' | 'media'
export type LayerKind = 'source' | 'text' | 'color' | 'rect' | 'ellipse' | 'arrow' | 'pencil' | 'image' | 'group'
/** Stand-in pictures for video sources (the web has no capture devices). */
export type Art = 'facecam' | 'game' | 'desktop' | 'webcam-utility' | 'browser-alerts' | 'none'

export interface Source {
  id: string
  name: string
  kind: SourceKind
  /** Shown under the name: the device and size, e.g. "Logitech Brio · 1920×1080". */
  detail?: string
  /** A problem badge instead of the detail ("Not found", "In use"). */
  problem?: string
  muted?: boolean
  /** Peak dB per channel for audio meters, -60..0. */
  level?: [number, number]
  art?: Art
  /** Native size, used by Fit/Fill and the crop math. */
  size?: [number, number]
}

export interface Filter {
  kind: 'color' | 'chroma_key' | 'sharpness' | 'lut'
  label: string
  enabled: boolean
  params: { key: string; value: number | string }[]
}

export interface Crop { l: number; t: number; r: number; b: number }

export interface Layer {
  id: string
  name: string
  kind: LayerKind
  sourceId?: string
  x: number
  y: number
  w: number
  h: number
  rot?: number
  flipH?: boolean
  flipV?: boolean
  crop?: Crop
  /** Corner radius, % of the short side (50 = round). */
  radius?: number
  opacity?: number
  visible?: boolean
  locked?: boolean
  /** The layer-list dot. */
  color?: string
  /** Inspector sections shared with other scenes: group -> peer scene names. */
  links?: Partial<Record<LinkGroup, string[]>>
  filters?: Filter[]
  // Content layers
  fill?: string
  stroke?: string
  strokeWidth?: number
  text?: string
  font?: { family?: string; size?: number; bold?: boolean; italic?: boolean }
  /** Pencil points / arrow ends, in canvas px. */
  points?: [number, number][]
  children?: Layer[]
  expanded?: boolean
  /** The scene's bottom color layer: stays last, can't be dragged. */
  background?: boolean
}

export type LinkGroup = 'transform' | 'crop' | 'filters' | 'visibility'

export interface Scene { id: string; name: string; layers: Layer[] }

export interface Transition { id: string; name: string; type: 'fade' | 'cut' | 'swipe' | 'slide' | 'fade_to_color' | 'luma_wipe' | 'stinger'; durationMs: number }

export interface Asset { name: string; kind: 'folder' | 'image' | 'video' | 'audio'; detail?: string }

export interface HistoryStep { label: string; time: string }

export type Tool = 'select' | 'text' | 'rect' | 'ellipse' | 'arrow' | 'pencil' | 'laser'

/** Which popup is open. `at` places a context menu (app px); otherwise it hangs under its button. */
export interface MenuState {
  id: 'window' | 'import' | 'transition' | 'scene' | 'layer' | 'pool' | 'copy' | 'canvas' | 'addFilter' | 'linked' | 'link' | 'sourceDevice'
  /** The row it belongs to (scene, layer or source id) or the inspector group. */
  target?: string
  /** An open submenu, by its label. */
  sub?: string
  /** Highlighted row label (hover), for screenshots. */
  hover?: string
  at?: [number, number]
}

export type Dialog = null | 'addSource' | 'transitions' | 'rename' | 'settings' | 'setup' | 'newProject'

export interface Outputs {
  streaming: boolean
  recording: boolean
  replay: boolean
  vcam: boolean
  /** "Click again to end stream". */
  confirmingEnd?: boolean
  streamTime?: string
  kbps?: number
  dropped?: number
  recTime?: string
  recMb?: number
}

export interface UiState {
  title: string
  scenes: Scene[]
  sources: Source[]
  assets: Asset[]
  transitions: Transition[]
  history: HistoryStep[]
  historyIndex: number

  editSceneId: string
  programSceneId: string
  /** Selected layer ids in the edited scene. */
  selection: string[]
  /** A selected Sources row (the Inspector then shows the source). */
  selectedSourceId: string | null
  currentTransition: string
  tool: Tool
  cropMode: boolean
  liveLocked: boolean

  sceneView: 'list' | 'grid'
  poolFilter: 'all' | 'video' | 'audio'
  poolSearch: string | null
  /** Current tab of each tabbed panel group. */
  leftTab: 'Sources' | 'Audio Mixer'
  rightTab: 'Inspector' | 'History'
  sourceSectionOpen: boolean
  lockAspect: boolean
  previewPercent: number

  menu: MenuState | null
  dialog: Dialog
  /** A panel's (?) help card, by panel title ("Scenes", "Canvas" …). */
  help: string | null
  /** Forced hover, for screenshots: a layer row, a source row, a tool or a header button. */
  hover: { layer?: string; source?: string; tool?: Tool; button?: string } | null
  toast: string
  status: string
  stats: { cpu: number; fps: number; targetFps: number; renderMs: number }
  outputs: Outputs
  /** The laser trail, canvas px, newest last. */
  laser: [number, number][] | null
  /** Settings page section (0 Stream … 6 About) and setup step, when those are open. */
  settingsSection: number
  setupStep: number
  addKind: SourceKind
  /** Visible app chrome: 'window' adds the Windows title bar the real screenshots have. */
  frame: 'window' | 'none'
  /** Size of the layout's side columns, px. */
  layout: { left: number; right: number; scenesH: number; sourcesH: number; layersH: number }
}

export type DeepPartial<T> = { [K in keyof T]?: T[K] extends (infer U)[] ? U[] : T[K] extends object | null ? DeepPartial<NonNullable<T[K]>> | T[K] : T[K] }

export const CANVAS_W = 1920
export const CANVAS_H = 1080

export function iconForSource(kind: SourceKind): IconName {
  return ({ camera: 'video', display: 'monitor', window: 'app-window', game: 'gamepad-2', browser: 'globe', mic: 'mic', desktop_audio: 'volume-2', image: 'image', media: 'film' } as const)[kind]
}

export function iconForLayer(l: Layer, sources: Source[]): IconName {
  if (l.kind === 'source') {
    const s = sources.find((s) => s.id === l.sourceId)
    return s ? iconForSource(s.kind) : 'file'
  }
  return ({ text: 'type', color: 'palette', rect: 'square', ellipse: 'circle', arrow: 'arrow-up-right', pencil: 'pencil', image: 'image', group: 'layers', source: 'file' } as const)[l.kind]
}

export const isVideo = (s: Source) => s.kind !== 'mic' && s.kind !== 'desktop_audio'

/** Flattens a scene's layer tree to list rows, top first. */
export function flatLayers(layers: Layer[], depth = 0): { layer: Layer; depth: number }[] {
  return layers.flatMap((l) => [{ layer: l, depth }, ...(l.kind === 'group' && l.expanded !== false && l.children ? flatLayers(l.children, depth + 1) : [])])
}

export function findLayer(layers: Layer[], id: string): Layer | undefined {
  for (const l of layers) {
    if (l.id === id) return l
    const c = l.children && findLayer(l.children, id)
    if (c) return c
  }
}

export function mapLayers(layers: Layer[], f: (l: Layer) => Layer): Layer[] {
  return layers.map((l) => f(l.children ? { ...l, children: mapLayers(l.children, f) } : l))
}

/** Scenes (other than `sceneId`) that place this source. */
export function scenesUsing(state: Pick<UiState, 'scenes'>, sourceId: string): Scene[] {
  const has = (ls: Layer[]): boolean => ls.some((l) => l.sourceId === sourceId || (!!l.children && has(l.children)))
  return state.scenes.filter((s) => has(s.layers))
}

export function editScene(state: Pick<UiState, 'scenes' | 'editSceneId'>): Scene {
  return state.scenes.find((s) => s.id === state.editSceneId) ?? state.scenes[0]
}

function isPlainObject(v: unknown): v is Record<string, unknown> {
  return typeof v === 'object' && v !== null && !Array.isArray(v)
}

/** Deep merge for injected state: objects merge, arrays and scalars replace. */
export function merge<T>(base: T, patch: unknown): T {
  if (!isPlainObject(patch) || !isPlainObject(base)) return (patch === undefined ? base : patch) as T
  const out: Record<string, unknown> = { ...base }
  for (const [k, v] of Object.entries(patch)) out[k] = isPlainObject(v) && isPlainObject(out[k]) ? merge(out[k], v) : v
  return out as T
}

/** Patches one layer anywhere in any scene, by id (handy in shot definitions). */
export function patchLayer(state: UiState, id: string, patch: Partial<Layer>): UiState {
  return { ...state, scenes: state.scenes.map((s) => ({ ...s, layers: mapLayers(s.layers, (l) => (l.id === id ? { ...l, ...patch } : l)) })) }
}
