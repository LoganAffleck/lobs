import '@fontsource-variable/inter'
import './lobs.css'
import { useMemo, type CSSProperties } from 'react'
import { merge, type UiState } from './model'
import { sampleState } from './sample'
import { LobsProvider, useStore } from './store'
import { Header, StatusBar, WindowFrame } from './Shell'
import { AssetsPanel, ScenesPanel, SourcesPanel } from './panels/Left'
import { InspectorPanel, LayersPanel } from './panels/Right'
import { CanvasPanel } from './panels/Canvas'
import { Overlays } from './overlays/Overlays'

/** One panel on its own, for close-ups in docs and videos. */
export type View = 'app' | 'scenes' | 'sources' | 'assets' | 'layers' | 'inspector' | 'canvas'

function Editor({ view }: { view: View }) {
  const { s, d } = useStore()
  const close = () => (s.menu || s.help) && d({ type: 'set', patch: { menu: null, help: null } })
  if (view !== 'app') {
    const P = { scenes: ScenesPanel, sources: SourcesPanel, assets: AssetsPanel, layers: LayersPanel, inspector: InspectorPanel, canvas: CanvasPanel }[view]
    return <div className="dock" style={{ flexDirection: 'column' }} onClick={close}><P style={{ flex: 1 }} /></div>
  }
  return (
    <>
      {s.frame === 'window' && <WindowFrame title={s.title} />}
      <Header />
      <div className="dock" onClick={close}>
        <div className="col" style={{ width: s.layout.left }}>
          <ScenesPanel style={{ height: s.layout.scenesH, flex: 'none' }} />
          <SourcesPanel style={{ height: s.layout.sourcesH, flex: 'none' }} />
          <AssetsPanel />
        </div>
        <CanvasPanel />
        <div className="col" style={{ width: s.layout.right }}>
          <LayersPanel style={{ height: s.layout.layersH, flex: 'none' }} />
          <InspectorPanel />
        </div>
      </div>
      <StatusBar />
      <Overlays />
    </>
  )
}

export interface LobsAppProps {
  /** A full state, or a partial one merged over the sample show. */
  state?: Partial<UiState> | Record<string, unknown>
  /** Merged over the state on every render without resetting clicks: timers, meters, stats. */
  live?: Record<string, unknown>
  /** Clicks change the UI (default). Off for static screenshots. */
  interactive?: boolean
  view?: View
  className?: string
  style?: CSSProperties
}

export function LobsApp({ state, live, interactive = true, view = 'app', className = '', style }: LobsAppProps) {
  const full = useMemo(() => merge(sampleState(), state ?? {}), [state])
  return (
    <LobsProvider state={full} live={live} interactive={interactive}>
      <div className={`lobs ${className}`} style={style} onContextMenu={(e) => e.preventDefault()}>
        <Editor view={view} />
      </div>
    </LobsProvider>
  )
}

export default LobsApp
