import { createRoot } from 'react-dom/client'
import { LobsApp, merge } from '../ui'
import { SHOTS } from './catalog'

// Renders one LOBS UI state, full window, for scripts/shots.mjs (and for checking a shot by
// hand: /shot.html?shot=crop, or ?shot=editor&state=<url-encoded JSON patch>).
// Without ?shot it lists every shot.
const q = new URLSearchParams(location.search)
const name = q.get('shot')
const patch = q.get('state')
const root = createRoot(document.getElementById('root')!)

declare global { interface Window { __shotReady?: Promise<void>; __SHOTS__?: typeof SHOTS } }
window.__SHOTS__ = SHOTS

if (q.has('video')) {
  // The Canvas component, scrubbed by hand: ?video&t=2.5&props=<url-encoded JSON>
  const props = JSON.parse(q.get('props') ?? '{}')
  const t = Number(q.get('t') ?? 0)
  import('../video/LobsVideo').then(({ default: LobsVideo }) => root.render(<LobsVideo {...props} time={t} />))
} else if (!name) {
  document.body.style.overflow = 'auto'
  root.render(
    <div style={{ font: '14px Inter, sans-serif', color: '#eee', padding: 24, display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(360px, 1fr))', gap: 16 }}>
      {Object.entries(SHOTS).map(([k, v]) => (
        <a key={k} href={`?shot=${k}`} style={{ color: '#eee', textDecoration: 'none' }}>
          <div style={{ aspectRatio: `${(v.size ?? [1520, 900])[0]} / ${(v.size ?? [1520, 900])[1]}`, maxHeight: 260, border: '1px solid #333', borderRadius: 8, overflow: 'hidden', background: `url(/shots/${k}.png) center/contain no-repeat #111` }} />
          <div style={{ marginTop: 6 }}><b>{k}</b> · {v.title}</div>
        </a>
      ))}
    </div>,
  )
} else {
  const shot = SHOTS[name]
  if (!shot) throw new Error(`No shot "${name}"`)
  const state = patch ? merge(shot.state, JSON.parse(patch)) : shot.state
  root.render(<LobsApp state={state} view={shot.view} interactive={q.has('live')} />)
  // Ready once fonts are in and layout has settled (ResizeObserver sizes the canvas).
  window.__shotReady = document.fonts.ready.then(() => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(() => r()))))
}
