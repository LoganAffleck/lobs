import type { CSSProperties } from 'react'
import { Art } from './Art'
import { CANVAS_H, CANVAS_W, type Filter, type Layer, type Scene, type Source } from './model'

function cssFilter(filters: Filter[] | undefined): string | undefined {
  const cc = filters?.find((f) => f.enabled && f.kind === 'color')
  if (!cc) return undefined
  const p = (k: string) => Number(cc.params.find((x) => x.key === k)?.value ?? 0)
  return `brightness(${1 + p('brightness')}) contrast(${1 + p('contrast')}) saturate(${1 + p('saturation')})`
}

function LayerView({ l, sources, uid }: { l: Layer; sources: Source[]; uid: string }) {
  if (l.visible === false) return null
  const radius = l.radius ? (l.radius / 100) * Math.min(l.w, l.h) : 0
  const box: CSSProperties = {
    position: 'absolute', left: l.x, top: l.y, width: l.w, height: l.h,
    opacity: l.opacity ?? 1,
    transform: `${l.rot ? `rotate(${l.rot}deg)` : ''} ${l.flipH ? 'scaleX(-1)' : ''} ${l.flipV ? 'scaleY(-1)' : ''}`.trim() || undefined,
    borderRadius: radius, overflow: 'hidden', filter: cssFilter(l.filters),
  }
  switch (l.kind) {
    case 'group':
      return <>{[...(l.children ?? [])].reverse().map((c) => <LayerView key={c.id} l={c} sources={sources} uid={uid} />)}</>
    case 'color':
      return <div style={{ ...box, background: l.fill }}>{l.src && <img src={l.src} alt="" draggable={false} style={{ width: '100%', height: '100%', objectFit: 'cover', display: 'block' }} />}</div>
    case 'text':
      return (
        <div style={{ ...box, overflow: 'visible', color: l.fill ?? '#fff', display: 'flex', alignItems: 'center', justifyContent: 'center', whiteSpace: 'nowrap',
          font: `${l.font?.italic ? 'italic ' : ''}${l.font?.bold ? 700 : 400} ${l.font?.size ?? 72}px/1 ${l.font?.family ?? 'Inter'}, 'Inter Variable', sans-serif`, letterSpacing: '-0.02em' }}>
          {l.text}
        </div>
      )
    case 'rect':
    case 'ellipse':
      return <div style={{ ...box, background: l.fill || 'transparent', border: l.stroke ? `${l.strokeWidth ?? 6}px solid ${l.stroke}` : undefined, borderRadius: l.kind === 'ellipse' ? '50%' : radius }} />
    case 'arrow':
    case 'pencil': {
      const pts = l.points ?? []
      const sw = l.strokeWidth ?? 6
      const d = pts.map(([x, y], i) => `${i ? 'L' : 'M'}${x - l.x} ${y - l.y}`).join(' ')
      let head = null
      if (l.kind === 'arrow' && pts.length >= 2) {
        const [[x0, y0], [x1, y1]] = pts.slice(-2)
        const a = Math.atan2(y1 - y0, x1 - x0), k = sw * 4
        const p = (da: number) => `${x1 - l.x - k * Math.cos(a + da)} ${y1 - l.y - k * Math.sin(a + da)}`
        head = <path d={`M${p(0.5)} L${x1 - l.x} ${y1 - l.y} L${p(-0.5)}`} />
      }
      return (
        <svg style={{ ...box, overflow: 'visible', borderRadius: 0 }} viewBox={`0 0 ${l.w} ${l.h}`} fill="none" stroke={l.stroke ?? '#fff'} strokeWidth={sw} strokeLinecap="round" strokeLinejoin="round">
          <path d={d} />{head}
        </svg>
      )
    }
    case 'image':
      return l.src
        ? <div style={box}><img src={l.src} alt="" draggable={false} style={{ width: '100%', height: '100%', objectFit: 'cover', display: 'block' }} /></div>
        : <div style={{ ...box, background: 'linear-gradient(135deg,#a465f4,#4f179f)' }} />
    case 'source': {
      const src = sources.find((s) => s.id === l.sourceId)
      if (!src) return null
      const [sw, sh] = src.size ?? [CANVAS_W, CANVAS_H]
      const c = l.crop ?? { l: 0, t: 0, r: 0, b: 0 }
      const kx = l.w / Math.max(1, sw - c.l - c.r), ky = l.h / Math.max(1, sh - c.t - c.b)
      return (
        <div style={box}>
          <div style={{ position: 'absolute', left: -c.l * kx, top: -c.t * ky, width: sw * kx, height: sh * ky }}>
            <Art kind={src.art ?? 'none'} uid={`${uid}-${l.id}`} />
          </div>
        </div>
      )
    }
  }
}

/** A scene as the engine would render it, at `width` px wide (16:9). */
export function ScenePicture({ scene, sources, width, uid, children }: { scene: Scene; sources: Source[]; width: number; uid: string; children?: React.ReactNode }) {
  const k = width / CANVAS_W
  return (
    <div style={{ position: 'relative', width, height: CANVAS_H * k, overflow: 'hidden', background: '#000' }}>
      <div style={{ position: 'absolute', left: 0, top: 0, width: CANVAS_W, height: CANVAS_H, transform: `scale(${k})`, transformOrigin: '0 0' }}>
        {[...scene.layers].reverse().map((l) => <LayerView key={l.id} l={l} sources={sources} uid={uid} />)}
      </div>
      {children}
    </div>
  )
}
