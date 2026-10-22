import type { Filter, Layer, UiState } from './model'

const colorCorrection = (brightness = 0.04, contrast = 0.1, saturation = 0.15): Filter => ({
  kind: 'color',
  label: 'Color Correction',
  enabled: true,
  params: [
    { key: 'brightness', value: brightness },
    { key: 'contrast', value: contrast },
    { key: 'saturation', value: saturation },
  ],
})

const bg = (id: string, fill: string): Layer => ({ id, name: 'Background', kind: 'color', x: 0, y: 0, w: 1920, h: 1080, fill, color: '#fbbf24', background: true, locked: true })

/** The show new docs, shots and videos start from (the app's own sample show, a little fuller). */
export function sampleState(): UiState {
  return {
    title: 'LOBS',
    sources: [
      { id: 'src_cam', name: 'Facecam', kind: 'camera', detail: 'Logitech Brio · 1920×1080', art: 'facecam', size: [1920, 1080] },
      { id: 'src_game', name: 'Game Capture', kind: 'game', detail: 'Learn GDScript From Zero · 1920×1080', art: 'game', size: [1920, 1080] },
      { id: 'src_steam', name: 'Steam', kind: 'window', detail: '[steamwebhelper.exe]: Steam · 2000×1451', art: 'steam', size: [2000, 1451] },
      { id: 'src_alerts', name: 'Alerts', kind: 'browser', detail: 'streamelements.com/overlay', art: 'browser-alerts', size: [1920, 1080] },
      { id: 'src_mic', name: 'Mic', kind: 'mic', detail: 'Shure MV7', level: [-14, -15] },
      { id: 'src_desk', name: 'Desktop Audio', kind: 'desktop_audio', detail: 'Windows default device', level: [-24, -23] },
    ],
    scenes: [
      {
        id: 'sc_soon',
        name: 'Starting Soon',
        layers: [
          { id: 'ly_soon_title', name: 'Title', kind: 'text', x: 560, y: 430, w: 800, h: 150, text: 'Starting soon', fill: '#ffffff', font: { family: 'Inter', size: 120, bold: true }, color: '#22c55e' },
          { id: 'ly_soon_sub', name: 'Schedule', kind: 'text', x: 660, y: 600, w: 600, h: 60, text: 'Tonight: learning GDScript from zero', fill: '#c4b5fd', font: { family: 'Inter', size: 40 }, color: '#22c55e' },
          bg('ly_soon_bg', '#1d1b2e'),
        ],
      },
      {
        id: 'sc_chat',
        name: 'Just Chatting',
        layers: [
          { id: 'ly_chat_alerts', name: 'Alerts', kind: 'source', sourceId: 'src_alerts', x: 0, y: 0, w: 1920, h: 1080, color: '#8b5cf6' },
          {
            id: 'ly_chat_cam', name: 'Facecam', kind: 'source', sourceId: 'src_cam', x: 1200, y: 560, w: 640, h: 360, radius: 6, color: '#ef4444',
            filters: [colorCorrection()], links: { filters: ['Gameplay'] },
          },
          { id: 'ly_chat_steam', name: 'Steam', kind: 'source', sourceId: 'src_steam', x: 80, y: 100, w: 1048, h: 760, radius: 2, color: '#3b82f6' },
          { ...bg('ly_chat_bg', '#1d1b2e'), src: '/art/background.jpg' },
        ],
      },
      {
        id: 'sc_game',
        name: 'Gameplay',
        layers: [
          {
            id: 'ly_game_cam', name: 'Facecam', kind: 'source', sourceId: 'src_cam', x: 1520, y: 40, w: 360, h: 360, radius: 50, color: '#ef4444',
            crop: { l: 420, t: 0, r: 420, b: 0 }, filters: [colorCorrection()], links: { filters: ['Just Chatting'] },
          },
          { id: 'ly_game_cap', name: 'Game Capture', kind: 'source', sourceId: 'src_game', x: 0, y: 0, w: 1920, h: 1080, color: '#22c55e' },
          bg('ly_game_bg', '#000000'),
        ],
      },
      {
        id: 'sc_brb',
        name: 'BRB',
        layers: [
          { id: 'ly_brb_title', name: 'Title', kind: 'text', x: 610, y: 470, w: 700, h: 140, text: 'Back in 5', fill: '#ffffff', font: { family: 'Inter', size: 120, bold: true }, color: '#22c55e' },
          bg('ly_brb_bg', '#2a1a3e'),
        ],
      },
    ],
    assets: [
      { name: 'Images', kind: 'folder' },
      { name: 'Videos', kind: 'folder' },
      { name: 'Stingers', kind: 'folder' },
      { name: 'logo.png', kind: 'image', detail: '512×512' },
      { name: 'intro.mp4', kind: 'video', detail: '0:12' },
    ],
    transitions: [
      { id: 'tr_fade', name: 'Fade', type: 'fade', durationMs: 300 },
      { id: 'tr_cut', name: 'Cut', type: 'cut', durationMs: 0 },
      { id: 'tr_swipe', name: 'Swipe', type: 'swipe', durationMs: 500 },
      { id: 'tr_stinger', name: 'Stinger', type: 'stinger', durationMs: 0 },
    ],
    history: [
      { label: 'Open show', time: '8:02 PM' },
      { label: 'Add Facecam', time: '8:03 PM' },
      { label: 'Place Facecam', time: '8:03 PM' },
      { label: 'Crop Facecam', time: '8:05 PM' },
      { label: 'Round corners', time: '8:05 PM' },
      { label: 'Link filters', time: '8:06 PM' },
      { label: 'Move Facecam', time: '8:07 PM' },
    ],
    historyIndex: 6,

    editSceneId: 'sc_chat',
    programSceneId: 'sc_soon',
    selection: ['ly_chat_cam'],
    selectedSourceId: null,
    currentTransition: 'tr_fade',
    tool: 'select',
    cropMode: false,
    liveLocked: true,

    sceneView: 'list',
    poolFilter: 'all',
    poolSearch: null,
    leftTab: 'Sources',
    rightTab: 'Inspector',
    sourceSectionOpen: false,
    lockAspect: true,
    previewPercent: 50,

    menu: null,
    dialog: null,
    help: null,
    hover: null,
    toast: '',
    status: 'Engine running',
    stats: { cpu: 0.8, fps: 60, targetFps: 60, renderMs: 0.5 },
    outputs: { streaming: false, recording: false, replay: false, vcam: false, streamTime: '00:00:00', kbps: 6000, dropped: 0, recTime: '00:00:00', recMb: 0 },
    laser: null,
    settingsSection: 0,
    setupStep: 0,
    addKind: 'camera',
    layout: { left: 320, right: 340, scenesH: 220, sourcesH: 300, layersH: 300 },
  }
}
