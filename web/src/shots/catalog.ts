import { patchLayer, sampleState, type UiState, type View } from '../ui'

// Every docs and site screenshot, by name. Each is the sample show plus what's open or
// selected. `npm run shots` renders them all to public/shots/<name>.png; the site and docs
// reference those files. Add a shot here, run the script, and it exists.
export interface Shot {
  title: string
  state: UiState
  view?: View
  /** CSS px; rendered at 2x. */
  size?: [number, number]
  /** Crop the screenshot to [x, y, w, h] (CSS px), for readable close-ups of a region. */
  clip?: [number, number, number, number]
}

const base = sampleState
const with_ = (patch: Partial<UiState>, s = base()): UiState => ({ ...s, ...patch })

const live = { streaming: true, recording: false, replay: true, vcam: false, streamTime: '01:02:11', kbps: 5980, dropped: 0, recTime: '00:00:00', recMb: 0 }

function layersState(): UiState {
  const s = base()
  s.scenes = s.scenes.map((sc) => sc.id !== 'sc_soon' ? sc : {
    ...sc,
    layers: [
      { id: 'ly_grp', name: 'Lower third', kind: 'group', x: 120, y: 820, w: 760, h: 170, expanded: true, children: [
        { id: 'ly_lt_text', name: 'Name', kind: 'text', x: 150, y: 850, w: 520, h: 70, text: 'mossy_kettle', fill: '#ffffff', font: { size: 56, bold: true }, color: '#22c55e' },
        { id: 'ly_lt_box', name: 'Plate', kind: 'rect', x: 120, y: 820, w: 760, h: 140, fill: '#7c3aed', radius: 10, color: '#ec4899' },
      ] },
      ...sc.layers,
    ],
  })
  return with_({ editSceneId: 'sc_soon', programSceneId: 'sc_chat', selection: ['ly_lt_text'], menu: { id: 'layer', target: 'ly_lt_text', hover: 'Bring forward' } }, s)
}

function toolsState(): UiState {
  const s = base()
  s.scenes = s.scenes.map((sc) => sc.id !== 'sc_soon' ? sc : {
    ...sc,
    layers: [
      { id: 'ly_draw', name: 'Drawing', kind: 'pencil', x: 700, y: 760, w: 600, h: 140, stroke: '#ffffff', strokeWidth: 8, color: '#f59e0b', points: [[700, 860], [780, 790], [860, 800], [940, 880], [1020, 890], [1100, 800], [1180, 770], [1260, 830]] },
      { id: 'ly_arrow', name: 'Arrow', kind: 'arrow', x: 1080, y: 300, w: 300, h: 300, stroke: '#ffffff', strokeWidth: 8, color: '#f97316', points: [[1100, 600], [1360, 330]] },
      { id: 'ly_ell', name: 'Ellipse', kind: 'ellipse', x: 200, y: 700, w: 240, h: 200, fill: '#ef4444', stroke: '#ffffff', strokeWidth: 6, color: '#ef4444' },
      { id: 'ly_rect', name: 'Rectangle', kind: 'rect', x: 1400, y: 160, w: 400, h: 200, fill: '#ef4444', stroke: '#ffffff', strokeWidth: 6, color: '#ec4899' },
      ...sc.layers,
    ],
  })
  return with_({ editSceneId: 'sc_soon', programSceneId: 'sc_chat', selection: ['ly_rect'], hover: { tool: 'ellipse' } }, s)
}

export const SHOTS: Record<string, Shot> = {
  editor: { title: 'The editor', state: base() },
  'editor-live': {
    title: 'Live, with the layers locked',
    state: with_({ editSceneId: 'sc_game', programSceneId: 'sc_game', selection: [], outputs: live, stats: { cpu: 6.2, fps: 60, targetFps: 60, renderMs: 1.1 } }),
  },
  'scenes-grid': { title: 'Scenes as a grid', state: with_({ sceneView: 'grid', selection: [] }) },
  'scene-menu': { title: 'Scene menu', state: with_({ menu: { id: 'scene', target: 'sc_chat', hover: 'Duplicate with linked layers' } }) },
  'sources-hover': { title: 'Adding a source to the scene', state: with_({ editSceneId: 'sc_soon', programSceneId: 'sc_chat', selection: [], hover: { source: 'src_cam' } }) },
  'source-menu': { title: 'Where a source is used', state: with_({ menu: { id: 'pool', target: 'src_cam', hover: 'Used in Just Chatting, Gameplay' } }) },
  'source-inspector': { title: 'Editing a source', state: with_({ selection: [], selectedSourceId: 'src_cam' }) },
  'add-source': { title: 'Add a source', state: with_({ dialog: 'addSource' }) },
  'linked-filters': {
    title: 'Filters linked across scenes',
    state: (() => {
      const s = with_({ menu: { id: 'linked', target: 'ly_chat_cam:filters' }, sourceSectionOpen: false })
      return s
    })(),
  },
  'link-menu': {
    title: 'Linking a section',
    state: with_({ menu: { id: 'link', target: 'ly_chat_cam:transform', sub: 'Link with Gameplay', hover: "Keep this scene's values" } }),
  },
  crop: {
    title: 'Crop to a circle',
    state: with_({ editSceneId: 'sc_game', programSceneId: 'sc_soon', selection: ['ly_game_cam'], cropMode: true }),
  },
  layers: { title: 'Layers and groups', state: layersState() },
  tools: { title: 'Graphic tools', state: toolsState() },
  laser: {
    title: 'The laser pointer',
    state: with_({
      editSceneId: 'sc_game', programSceneId: 'sc_game', selection: [], tool: 'laser', outputs: live,
      laser: [[700, 640], [760, 600], [830, 580], [900, 590], [960, 630], [990, 690], [980, 750], [930, 790]],
    }),
  },
  transitions: { title: 'Transitions', state: with_({ dialog: 'transitions' }) },
  'transition-menu': { title: 'Transition menu', state: with_({ menu: { id: 'transition', hover: 'Edit transitions…' } }) },
  'window-menu': { title: 'Window menu', state: with_({ menu: { id: 'window', hover: 'Reset layout' } }) },
  import: { title: 'Import from OBS', state: with_({ menu: { id: 'import', hover: 'OBS: Untitled  ·  6 scenes' } }) },
  'copy-menu': { title: 'Copy to another scene', state: with_({ menu: { id: 'copy', sub: 'Gameplay', hover: 'Linked copy' } }) },
  mixer: { title: 'Audio mixer', state: with_({ leftTab: 'Audio Mixer' }) },
  history: { title: 'History', state: with_({ rightTab: 'History', historyIndex: 4 }) },
  'help-card': { title: 'Panel help', state: with_({ help: 'Layers' }) },
  'canvas-help': { title: 'Canvas help', state: with_({ help: 'Canvas' }) },
  setup: { title: 'Setup assistant', state: with_({ dialog: 'setup', setupStep: 0 }) },
  'setup-server': { title: 'Closest server', state: with_({ dialog: 'setup', setupStep: 2 }) },
  'setup-test': { title: 'Connection test', state: with_({ dialog: 'setup', setupStep: 3 }) },
  ...Object.fromEntries(['stream', 'recording', 'video', 'audio', 'hotkeys', 'engine', 'about'].map((n, i) => [`settings-${n}`, { title: `Settings: ${n}`, state: with_({ dialog: 'settings', settingsSection: i }) }])),
  // Site feature crops: the same states, cut to the part each section talks about.
  'feat-sources': { title: 'Where a source is used', clip: [0, 262, 760, 520], state: with_({ menu: { id: 'pool', target: 'src_cam', hover: 'Used in Just Chatting, Gameplay' } }) },
  'feat-linked': { title: 'Linked filters', clip: [760, 250, 760, 600], state: with_({ menu: { id: 'linked', target: 'ly_chat_cam:filters' }, layout: { ...base().layout, layersH: 150 } }) },
  'feat-crop': { title: 'Crop to a circle', clip: [760, 44, 760, 640], state: with_({ editSceneId: 'sc_game', programSceneId: 'sc_soon', selection: ['ly_game_cam'], cropMode: true, layout: { ...base().layout, layersH: 150 } }) },
  'feat-tools': { title: 'Graphic tools', view: 'canvas', size: [1000, 640], state: toolsState() },
  'feat-layers': { title: 'Layers and groups', clip: [760, 44, 760, 480], state: layersState() },
  'feat-live': { title: 'Live lock', clip: [330, 0, 1190, 620], state: with_({ editSceneId: 'sc_game', programSceneId: 'sc_game', selection: [], outputs: live, stats: { cpu: 6.2, fps: 60, targetFps: 60, renderMs: 1.1 } }) },
  'feat-setup': { title: 'Connection test', clip: [390, 200, 740, 500], state: with_({ dialog: 'setup', setupStep: 3 }) },
  'new-project': { title: 'New project', state: with_({ dialog: 'newProject' }) },
  stats: { title: 'Stats', state: with_({ help: 'Stats', outputs: { ...live, recording: true, recTime: '00:41:07', recMb: 3702 }, stats: { cpu: 6.2, fps: 60, targetFps: 60, renderMs: 1.1 } }) },
  'projector-menu': { title: 'Projectors', state: with_({ selection: [], menu: { id: 'canvas', at: [520, 260], sub: 'Open projector', hover: 'Display 2 (1920×1080)' } }) },
  assets: { title: 'Assets', view: 'assets', size: [340, 300], state: base() },
  // Close-ups
  'panel-inspector': { title: 'Inspector', view: 'inspector', size: [340, 860], state: with_({ sourceSectionOpen: true }) },
  'panel-inspector-shape': { title: 'Shape inspector', view: 'inspector', size: [340, 640], state: { ...toolsState(), hover: null } },
  'panel-layers': { title: 'Layers', view: 'layers', size: [340, 220], state: base() },
  'panel-sources': { title: 'Sources', view: 'sources', size: [340, 360], state: with_({ hover: { source: 'src_display' } }) },
  'panel-scenes': { title: 'Scenes', view: 'scenes', size: [320, 220], state: base() },
  'panel-scenes-grid': { title: 'Scene grid', view: 'scenes', size: [320, 420], state: with_({ sceneView: 'grid', layout: { ...base().layout, left: 320 } }) },
  'panel-canvas': { title: 'Canvas', view: 'canvas', size: [1100, 700], state: base() },
  'panel-mixer': { title: 'Mixer', view: 'sources', size: [340, 300], state: with_({ leftTab: 'Audio Mixer' }) },
  'panel-crop': { title: 'Crop section', view: 'inspector', size: [340, 760], state: with_({ editSceneId: 'sc_game', selection: ['ly_game_cam'], cropMode: true }) },
}

/** Small edits on top of a shot, for one-off variants (`?shot=crop&patch=…`). */
export { patchLayer }
