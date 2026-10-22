import { useLayoutEffect, useRef, useState } from 'react'
import { LobsApp } from '../../ui'
import { DOWNLOAD, DownloadIcon, VERSION } from '../Layout'
import { HeroGem } from '../HeroGem'

const W = 1520, H = 900

/** The editor model, live and clickable, scaled to the column. */
function HeroApp() {
  const ref = useRef<HTMLDivElement>(null)
  const [k, setK] = useState(0.7)
  useLayoutEffect(() => {
    const el = ref.current!
    const fit = () => setK(el.clientWidth / W)
    fit()
    const ro = new ResizeObserver(fit)
    ro.observe(el)
    return () => ro.disconnect()
  }, [])
  return (
    <div className="appframe" ref={ref} style={{ aspectRatio: `${W} / ${H}` }}>
      <div className="scaler" style={{ width: W, height: H, transform: `scale(${k})` }}>
        <LobsApp />
      </div>
    </div>
  )
}

export function Home() {
  return (
    <section className="hero">
      <div className="wrap">
        <h1>Logan's OBS</h1>
        <p className="lede">OBS with a creative UI for building awesome scenes.</p>
        <div className="ctas">
          <a className="sbtn primary" href={DOWNLOAD} download><DownloadIcon />Download for Windows</a>
        </div>
        <div className="facts"><span>Free</span><span>Open source</span><span>Windows 64-bit</span><span>v{VERSION} preview</span></div>
      </div>
      <div className="stagewrap">
        <HeroGem />
        <HeroApp />
      </div>
    </section>
  )
}
