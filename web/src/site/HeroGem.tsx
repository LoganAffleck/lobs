import { useEffect, useRef, useState } from 'react'

// The hero's logo gem, large, behind the headline; hover leans it, dragging spins it. The prerendered page (and anyone without WebGL) gets the rendered PNG;
// in the browser the live ray-traced gem loads separately (three.js is its own chunk, so it
// never holds up the page) and fades in over it once its first frame is drawn.
export function HeroGem() {
  const canvas = useRef<HTMLCanvasElement>(null)
  const [live, setLive] = useState(false)
  useEffect(() => {
    let dispose: (() => void) | undefined
    let cancelled = false
    const el = canvas.current
    if (!el || !('WebGL2RenderingContext' in window)) return
    import('../gem/hero')
      .then(({ mountHeroGem }) => {
        if (cancelled) return
        dispose = mountHeroGem(el, { onReady: () => setLive(true), target: el.closest<HTMLElement>('.hero') ?? undefined })
      })
      .catch(() => {}) // keep the still image
    return () => { cancelled = true; dispose?.() }
  }, [])
  return (
    <div className="herogem" aria-hidden="true">
      <canvas ref={canvas} className={live ? 'on' : ''} />
    </div>
  )
}
