import { useLayoutEffect, useMemo, useState } from 'react'
import { LobsApp, merge, reduce, sampleState, type Action, type UiState, type View } from '../ui'
import { SHOTS } from '../shots/catalog'

// The LOBS editor as a Canvas component (agent-native-video):
//
//   canvas project import-component <dir> ../LOBS/web/src/video/LobsVideo.tsx
//
// Canvas renders it as `<LobsVideo {...props} time={t} />` in a 100vw × 100vh page and drives
// the real mouse into it, so clicks work as in the site's hero. Everything else is props:
//
//   shot       start from a named state in src/shots/catalog.ts ("crop", "editor-live" …)
//   state      a patch merged over that (or over the sample show)
//   keyframes  [{ at: 1.2, state: {...} }, { at: 2, actions: [{ type: "take" }] }]: applied in
//              order once `time` passes `at`. Each crossing resets the UI to the computed state
//              (clicks since the last keyframe are dropped), so frames are the same every render.
//   live       { from: 3725 } runs the LIVE and REC timers from that many seconds, and moves the
//              meters and CPU a little, without resetting anything.
//   view       one panel only: "inspector", "layers", "canvas" …
//   design     the layout size, [1520, 900] by default, scaled to fill the page. Use a larger
//              size for a roomier layout, or the page's own size for 1:1 pixels.
//
// Cursor targets: buttons carry data-ui="<label>" (data-ui="Go live", data-ui="tool:rect",
// data-ui="tab:History"), rows data-scene / data-source / data-layer="<name>", panels
// data-panel="<title>". Example: "click": "lyr_app:[data-layer=\"Facecam\"]".

export interface Keyframe { at: number; state?: Record<string, unknown>; actions?: Action[] }

export interface LobsVideoProps {
  shot?: string
  state?: Record<string, unknown>
  keyframes?: Keyframe[]
  live?: { from?: number; meters?: boolean }
  view?: View
  design?: [number, number]
  interactive?: boolean
  time?: number
}

const hms = (sec: number) => [sec / 3600, (sec / 60) % 60, sec % 60].map((v) => String(Math.floor(v)).padStart(2, '0')).join(':')

function useViewport(): [number, number] {
  const [v, setV] = useState<[number, number]>(() => (typeof window === 'undefined' ? [1520, 900] : [innerWidth, innerHeight]))
  useLayoutEffect(() => {
    const f = () => setV([innerWidth, innerHeight])
    addEventListener('resize', f)
    return () => removeEventListener('resize', f)
  }, [])
  return v
}

export default function LobsVideo({ shot, state, keyframes = [], live, view, design = [1520, 900], interactive = true, time = 0 }: LobsVideoProps) {
  const sorted = useMemo(() => [...keyframes].sort((a, b) => a.at - b.at), [keyframes])
  let passed = 0
  while (passed < sorted.length && sorted[passed].at <= time) passed++

  // Recomputed only when another keyframe is crossed, so the UI keeps clicks in between.
  const computed = useMemo(() => {
    let s: UiState = shot ? (SHOTS[shot]?.state ?? sampleState()) : sampleState()
    if (state) s = merge(s, state)
    for (const k of sorted.slice(0, passed)) {
      if (k.state) s = merge(s, k.state)
      for (const a of k.actions ?? []) s = reduce(s, a)
    }
    return s
  }, [shot, state, sorted, passed])

  const overlay = useMemo(() => {
    if (!live) return undefined
    const t = (live.from ?? 0) + time
    const wobble = (f: number, a: number) => Math.sin(time * f) * a + Math.sin(time * f * 2.3 + 1) * a * 0.5
    const o: Record<string, unknown> = {
      outputs: { streamTime: hms(t), recTime: hms(Math.max(0, t - 120)), kbps: Math.round(5980 + wobble(1.3, 25)), recMb: Math.round(Math.max(0, t - 120) * 1.5) },
      stats: { cpu: Math.max(0.3, 6 + wobble(0.9, 1.2)), fps: 60, targetFps: 60, renderMs: 1.1 + wobble(1.7, 0.15) },
    }
    if (live.meters !== false) {
      o.sources = computed.sources.map((src) => (src.level ? { ...src, level: [-14 + wobble(7, 6), -15 + wobble(6.3, 6)] as [number, number] } : src))
    }
    return o
  }, [live, time, computed.sources])

  const [vw, vh] = useViewport()
  const [dw, dh] = design
  const k = Math.min(vw / dw, vh / dh)
  return (
    <div style={{ width: '100vw', height: '100vh', overflow: 'hidden', background: '#171717' }}>
      <div style={{ width: dw, height: dh, transform: `translate(${(vw - dw * k) / 2}px, ${(vh - dh * k) / 2}px) scale(${k})`, transformOrigin: '0 0' }}>
        <LobsApp state={computed} live={overlay} view={view ?? SHOTS[shot ?? '']?.view} interactive={interactive} />
      </div>
    </div>
  )
}
