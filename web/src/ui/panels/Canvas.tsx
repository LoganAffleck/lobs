import { useLayoutEffect, useRef, useState } from 'react'
import { Anchor, Btn, Help, Icon, Menu } from '../primitives'
import { useStore } from '../store'
import { Group, HELP } from '../Shell'
import { ScenePicture } from '../ScenePicture'
import { CANVAS_H, CANVAS_W, editScene, findLayer, type Layer, type Tool } from '../model'
import type { IconName } from '../icons.gen'

const TOOLS: { id: Tool; icon: IconName; key: string }[] = [
  { id: 'select', icon: 'mouse-pointer-2', key: 'V' },
  { id: 'text', icon: 'type', key: 'T' },
  { id: 'rect', icon: 'square', key: 'R' },
  { id: 'ellipse', icon: 'circle', key: 'O' },
  { id: 'arrow', icon: 'arrow-up-right', key: 'A' },
  { id: 'pencil', icon: 'pencil', key: 'P' },
  { id: 'laser', icon: 'siren', key: 'L' },
]

function useSize<T extends HTMLElement>(fallback: [number, number]) {
  const ref = useRef<T>(null)
  const [size, setSize] = useState(fallback)
  useLayoutEffect(() => {
    const el = ref.current
    if (!el) return
    const measure = () => setSize([el.clientWidth, el.clientHeight])
    measure()
    const ro = new ResizeObserver(measure)
    ro.observe(el)
    return () => ro.disconnect()
  }, [])
  return [ref, size] as const
}

function Selection({ l, k, crop }: { l: Layer; k: number; crop: boolean }) {
  const box = { left: l.x * k - 0.5, top: l.y * k - 0.5, width: l.w * k + 1, height: l.h * k + 1 }
  const at = (x: number, y: number, v = false) => <i className={v ? 'v' : ''} style={crop
    ? { left: `calc(${x * 100}% - ${v ? 1.5 : x === 0 ? 0 : x === 1 ? 12 : 6}px)`, top: `calc(${y * 100}% - ${v ? (y === 0 ? 0 : y === 1 ? 12 : 6) : 1.5}px)` }
    : { left: `calc(${x * 100}% - 4px)`, top: `calc(${y * 100}% - 4px)` }} />
  return (
    <div className={`selbox ${crop ? 'crop' : ''}`} style={box}>
      {crop
        ? <>{at(0, 0)}{at(0, 0, true)}{at(1, 0)}{at(1, 0, true)}{at(0, 1)}{at(0, 1, true)}{at(1, 1)}{at(1, 1, true)}{at(0.5, 0)}{at(0.5, 1)}{at(0, 0.5, true)}{at(1, 0.5, true)}</>
        : <>{at(0, 0)}{at(1, 0)}{at(0, 1)}{at(1, 1)}</>}
    </div>
  )
}

export function CanvasPanel() {
  const { s, d, interactive } = useStore()
  const scene = editScene(s)
  const [ref, [sw, sh]] = useSize<HTMLDivElement>([1000, 640])
  const availW = Math.max(40, sw - 16), availH = Math.max(40, sh - 56 - 16)
  const width = Math.floor(Math.min(availW, (availH * CANVAS_W) / CANVAS_H))
  const k = width / CANVAS_W
  const tr = s.transitions.find((t) => t.id === s.currentTransition)
  const trText = tr ? tr.name + (tr.type === 'cut' || tr.type === 'stinger' ? '' : ` · ${tr.durationMs >= 1000 ? `${tr.durationMs / 1000} s` : `${tr.durationMs} ms`}`) : 'Transition'
  const live = s.outputs.streaming || s.outputs.recording
  const sel = s.selection.map((id) => findLayer(scene.layers, id)).filter((x): x is Layer => !!x)

  // Drag to move (the select tool). Hit-tests top-down like the canvas does; locked and
  // background layers stay put, and while live only Alt moves (live lock).
  const drag = useRef<{ id: string; x0: number; y0: number; lx: number; ly: number } | null>(null)
  const onDown = (e: React.PointerEvent) => {
    if (s.tool !== 'select' || !interactive) return
    const r = (e.currentTarget as HTMLElement).getBoundingClientRect()
    const cx = (e.clientX - r.left) / k, cy = (e.clientY - r.top) / k
    const hit = scene.layers.find((l) => l.visible !== false && !l.background && cx >= l.x && cx <= l.x + l.w && cy >= l.y && cy <= l.y + l.h)
    if (!hit) return d({ type: 'clearSelection' })
    d({ type: 'selectLayer', id: hit.id })
    if (hit.locked || (live && s.liveLocked && !e.altKey)) return
    drag.current = { id: hit.id, x0: e.clientX, y0: e.clientY, lx: hit.x, ly: hit.y }
    ;(e.currentTarget as HTMLElement).setPointerCapture(e.pointerId)
  }
  const onMove = (e: React.PointerEvent) => {
    const g = drag.current
    if (!g) return
    d({ type: 'setLayer', id: g.id, patch: { x: Math.round(g.lx + (e.clientX - g.x0) / k), y: Math.round(g.ly + (e.clientY - g.y0) / k) } })
  }
  const onUp = () => { drag.current = null }

  return (
    // The video is a dock panel like the others (its own tab), so rows of panels can sit under or over it.
    <Group tabs={['Video']} id="Canvas" style={{ flex: 1, minWidth: 0 }}>
      {live && (
        <div className="livebar">
          <span className="d" />
          <span className="grow">{s.outputs.streaming ? 'Live' : 'Recording'}{s.liveLocked ? ' · layers are locked. Hold Alt to move one.' : ' · live lock is off.'}</span>
          <Btn text={s.liveLocked ? 'Unlock' : 'Lock again'} variant="ghost" onClick={() => d({ type: 'set', patch: { liveLocked: !s.liveLocked } })} />
        </div>
      )}
      <div className="chead">
        <span className="title">{scene.name}</span>
        {scene.id === s.programSceneId && <span className={`badge ${live ? 'live' : ''}`}>{live ? 'On air' : 'Showing'}</span>}
        <span className="grow" />
        <Help title="Canvas" points={HELP.Canvas} align="right" />
        <Btn text={`${s.previewPercent}%`} variant="ghost" sel={[25, 33, 50, 100, 200].includes(s.previewPercent)} />
        <Anchor>
          <Btn text={trText} variant="ghost" sel={s.menu?.id === 'transition'} onClick={() => d({ type: 'menu', menu: s.menu?.id === 'transition' ? null : { id: 'transition' } })} />
          <Menu id="transition" align="right" entries={[
            ...s.transitions.map((t) => ({ text: t.name, sel: t.id === s.currentTransition, onClick: () => d({ type: 'set', patch: { currentTransition: t.id } }) })),
            { sep: true },
            { text: 'Edit transitions…', onClick: () => d({ type: 'set', patch: { dialog: 'transitions' } }) },
          ]} />
        </Anchor>
        {scene.id !== s.programSceneId && <Btn text="Show" variant="primary" onClick={() => d({ type: 'take' })} />}
      </div>
      <div className="stage" ref={ref}>
        <div className="preview" style={{ width, height: Math.round(CANVAS_H * k) }}
          onPointerDown={onDown} onPointerMove={onMove} onPointerUp={onUp}
          onContextMenu={(e) => { e.preventDefault(); const r = e.currentTarget.getBoundingClientRect(); d({ type: 'menu', menu: { id: 'canvas', at: [e.clientX - r.left, e.clientY - r.top] } }) }}>
          <ScenePicture scene={scene} sources={s.sources} width={width} uid="preview">
            {s.tool === 'select' && sel.map((l) => <Selection key={l.id} l={l} k={k} crop={s.cropMode && sel.length === 1} />)}
            {s.laser && s.laser.length > 1 && (
              <svg style={{ position: 'absolute', inset: 0 }} width={width} height={CANVAS_H * k} viewBox={`0 0 ${CANVAS_W} ${CANVAS_H}`}>
                {s.laser.slice(1).map(([x, y], i) => {
                  const [px, py] = s.laser![i]
                  const a = (i + 1) / s.laser!.length
                  return <line key={i} x1={px} y1={py} x2={x} y2={y} stroke="#ef4444" strokeOpacity={a} strokeWidth={4 + 10 * a} strokeLinecap="round" />
                })}
                <circle cx={s.laser.at(-1)![0]} cy={s.laser.at(-1)![1]} r="12" fill="#fff" stroke="#ef4444" strokeWidth="6" />
              </svg>
            )}
          </ScenePicture>
          <Menu id="canvas" entries={[
            { text: 'Open projector', sub: [{ text: 'Display 1 (2560×1440)' }, { text: 'Display 2 (1920×1080)' }, { sep: true }, { text: 'New window' }] },
            { text: 'Open scene projector', sub: [{ text: 'Display 1 (2560×1440)' }, { text: 'Display 2 (1920×1080)' }, { sep: true }, { text: 'New window' }] },
          ]} />
        </div>
        <div className="toolsbar">
          {TOOLS.map((t) => (
            <button key={t.id} className={`tool ${s.tool === t.id ? 'on' : ''} ${s.hover?.tool === t.id ? 'hov' : ''}`} data-ui={`tool:${t.id}`} onClick={() => d({ type: 'setTool', tool: t.id })}>
              <Icon name={t.icon} size={15} />
            </button>
          ))}
        </div>
      </div>
      {s.toast && <span className="toast">{s.toast}</span>}
    </Group>
  )
}
