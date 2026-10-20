// LOBS document model. The document is the source of truth; OBS is a render target.

export type SourceKind =
  | 'camera'
  | 'display'
  | 'window'
  | 'game'
  | 'image'
  | 'media'
  | 'browser'
  | 'color'
  | 'text'
  | 'mic'
  | 'desktop_audio';

export type MonitorType = 'off' | 'monitor_only' | 'monitor_and_output';

export interface AudioSettings {
  volume: number; // linear 0..1
  muted: boolean;
  monitor: MonitorType;
  tracks: boolean[]; // 6 tracks
  filters: Filter[];
}

/** A Source is added once to the Media Pool. It owns only input settings and audio. */
export interface Source {
  id: string;
  name: string;
  kind: SourceKind;
  settings: Record<string, unknown>;
  bin: string; // Media Pool folder, '' = root
  color: string; // tag color
  audio?: AudioSettings;
}

export type FilterType = 'color' | 'chroma_key' | 'sharpness' | 'blur' | 'lut';

export interface Filter {
  id: string;
  type: FilterType;
  enabled: boolean;
  settings: Record<string, number | string | boolean>;
}

export interface Transform {
  x: number;
  y: number;
  w: number;
  h: number;
  rot: number;
}

export interface Crop {
  l: number;
  t: number;
  r: number;
  b: number;
}

export interface Visibility {
  visible: boolean;
  opacity: number; // 0..1
}

/** Property groups an instance owns. Each one can be local or linked. */
export interface PropMap {
  transform: Transform;
  crop: Crop;
  filters: Filter[];
  visibility: Visibility;
}
export type PropGroup = keyof PropMap;
export const PROP_GROUPS: PropGroup[] = ['transform', 'crop', 'filters', 'visibility'];

/** An Instance is a placement of a source (or a group) on one scene. */
export interface Instance {
  id: string;
  sceneId: string;
  kind: 'source' | 'group';
  sourceId?: string; // kind === 'source'
  name: string; // groups have names; source instances default to the source name
  children?: string[]; // kind === 'group', top-first
  locked: boolean;
  props: PropMap; // local values; ignored for groups linked in `links`
  links: Partial<Record<PropGroup, string>>; // group -> link id
}

/** A link shares one property group's value between several instances. */
export interface Link {
  id: string;
  group: PropGroup;
  value: PropMap[PropGroup];
}

export interface Scene {
  id: string;
  name: string;
  folder: string;
  favorite: boolean;
  layers: string[]; // top-level instance ids, top-first
}

export interface LobsDoc {
  version: 1;
  canvas: { w: number; h: number };
  sources: Record<string, Source>;
  sourceOrder: string[];
  scenes: Record<string, Scene>;
  sceneOrder: string[];
  instances: Record<string, Instance>;
  links: Record<string, Link>;
  bins: string[];
}
