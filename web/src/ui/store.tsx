import { createContext, useContext, useMemo, useReducer, useState, type ReactNode } from 'react'
import { editScene, findLayer, mapLayers, merge, type Layer, type LinkGroup, type MenuState, type Tool, type UiState } from './model'

// Names follow the app's controller (app.editScene, app.take, app.toggleLock …) so a cursor
// script reads like what someone does in LOBS.
export type Action =
  | { type: 'set'; patch: Partial<UiState> }
  | { type: 'replace'; state: UiState }
  | { type: 'editScene'; id: string }
  | { type: 'take' }
  | { type: 'selectLayer'; id: string; add?: boolean }
  | { type: 'selectSource'; id: string }
  | { type: 'clearSelection' }
  | { type: 'toggleVisible'; id: string }
  | { type: 'toggleLock'; id: string }
  | { type: 'toggleGroup'; id: string }
  | { type: 'moveLayer'; id: string; by: number }
  | { type: 'setLayer'; id: string; patch: Partial<Layer> }
  | { type: 'toggleFilter'; index: number }
  | { type: 'unlink'; group: LinkGroup }
  | { type: 'setTool'; tool: Tool }
  | { type: 'menu'; menu: MenuState | null }
  | { type: 'toggleStreaming' }
  | { type: 'toggleRecording' }
  | { type: 'toggleReplay' }
  | { type: 'toggleVcam' }
  | { type: 'toggleInScene'; id: string }
  | { type: 'jumpHistory'; index: number }

function updateEditScene(s: UiState, f: (layers: Layer[]) => Layer[]): UiState {
  return { ...s, scenes: s.scenes.map((sc) => (sc.id === s.editSceneId ? { ...sc, layers: f(sc.layers) } : sc)) }
}
const patch = (s: UiState, id: string, f: (l: Layer) => Partial<Layer>) => updateEditScene(s, (ls) => mapLayers(ls, (l) => (l.id === id ? { ...l, ...f(l) } : l)))

export function reduce(s: UiState, a: Action): UiState {
  switch (a.type) {
    case 'set': return { ...s, ...a.patch }
    case 'replace': return a.state
    case 'editScene': return a.id === s.editSceneId ? { ...s, menu: null } : { ...s, editSceneId: a.id, selection: [], selectedSourceId: null, menu: null, cropMode: false }
    case 'take': return { ...s, programSceneId: s.editSceneId }
    case 'selectLayer': {
      const sel = a.add ? (s.selection.includes(a.id) ? s.selection.filter((x) => x !== a.id) : [...s.selection, a.id]) : [a.id]
      return { ...s, selection: sel, selectedSourceId: null, menu: s.menu?.target === a.id ? s.menu : null }
    }
    case 'selectSource': return { ...s, selectedSourceId: a.id, selection: [], menu: s.menu?.target === a.id ? s.menu : null }
    case 'clearSelection': return { ...s, selection: [], selectedSourceId: null, menu: null, cropMode: false }
    case 'toggleVisible': return patch(s, a.id, (l) => ({ visible: l.visible === false }))
    case 'toggleLock': return patch(s, a.id, (l) => ({ locked: !l.locked }))
    case 'toggleGroup': return patch(s, a.id, (l) => ({ expanded: l.expanded === false }))
    case 'setLayer': return patch(s, a.id, () => a.patch)
    case 'moveLayer': return updateEditScene(s, (ls) => {
      const i = ls.findIndex((l) => l.id === a.id)
      const j = i + a.by
      if (i < 0 || j < 0 || j >= ls.length || ls[j].background || ls[i].background) return ls
      const out = ls.slice()
      ;[out[i], out[j]] = [out[j], out[i]]
      return out
    })
    case 'toggleFilter': {
      const id = s.selection[0]
      return id ? patch(s, id, (l) => ({ filters: (l.filters ?? []).map((f, i) => (i === a.index ? { ...f, enabled: !f.enabled } : f)) })) : s
    }
    case 'unlink': {
      const id = s.selection[0]
      return id ? { ...patch(s, id, (l) => ({ links: { ...l.links, [a.group]: undefined } })), menu: null } : s
    }
    case 'setTool': return { ...s, tool: a.tool, selection: a.tool === 'select' ? s.selection : [], menu: null }
    case 'menu': return { ...s, menu: a.menu }
    case 'toggleStreaming': {
      const o = s.outputs
      if (!o.streaming) return { ...s, outputs: { ...o, streaming: true, confirmingEnd: false } }
      if (!o.confirmingEnd) return { ...s, outputs: { ...o, confirmingEnd: true } }
      return { ...s, outputs: { ...o, streaming: false, confirmingEnd: false } }
    }
    case 'toggleRecording': return { ...s, outputs: { ...s.outputs, recording: !s.outputs.recording } }
    case 'toggleReplay': return { ...s, outputs: { ...s.outputs, replay: !s.outputs.replay } }
    case 'toggleVcam': return { ...s, outputs: { ...s.outputs, vcam: !s.outputs.vcam } }
    case 'toggleInScene': {
      const sc = editScene(s)
      const placed = sc.layers.find((l) => l.sourceId === a.id)
      if (placed) return updateEditScene(s, (ls) => ls.filter((l) => l !== placed))
      const src = s.sources.find((x) => x.id === a.id)
      if (!src) return s
      const layer: Layer = { id: `ly_${a.id}_${sc.id}`, name: src.name, kind: 'source', sourceId: src.id, x: 640, y: 300, w: 640, h: 360, color: '#3b82f6' }
      return { ...updateEditScene(s, (ls) => [layer, ...ls]), selection: [layer.id] }
    }
    case 'jumpHistory': return { ...s, historyIndex: a.index }
  }
}

interface Ctx { s: UiState; d: (a: Action) => void; interactive: boolean }
const StoreCtx = createContext<Ctx | null>(null)

export function useStore(): Ctx {
  const c = useContext(StoreCtx)
  if (!c) throw new Error('LOBS UI parts must be inside <LobsApp> or <LobsProvider>')
  return c
}

export function useSelectedLayer(): Layer | undefined {
  const { s } = useStore()
  return s.selection.length === 1 ? findLayer(editScene(s).layers, s.selection[0]) : undefined
}

/**
 * Holds the UI state. `state` is the injected starting point: whenever it changes (a new shot,
 * a video keyframe), the UI resets to it; clicks in between change the local copy.
 */
export function LobsProvider({ state, live, interactive = true, children }: { state: UiState; live?: Record<string, unknown>; interactive?: boolean; children: ReactNode }) {
  const [base, d] = useReducer(reduce, state)
  // `live` values (a running timer, moving meters) sit on top without resetting clicks.
  const s = useMemo(() => (live ? merge(base, live) : base), [base, live])
  // Reset during render, not in an effect, so a video frame never shows the previous state.
  const [prev, setPrev] = useState(state)
  if (prev !== state) {
    setPrev(state)
    d({ type: 'replace', state })
  }
  const value = useMemo(() => ({ s, d: interactive ? d : () => {}, interactive }), [s, interactive])
  return <StoreCtx.Provider value={value}>{children}</StoreCtx.Provider>
}

export { merge }
