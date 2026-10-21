import { useLayoutEffect, useRef, useState, type ReactNode } from 'react'
import { LobsApp } from '../../ui'
import { Icon } from '../../ui/primitives'
import type { IconName } from '../../ui/icons.gen'
import { DOWNLOAD, DownloadIcon, GitHubIcon, REPO, VERSION } from '../Layout'
import { SHOTS } from '../../shots/catalog'
import { HeroGem } from '../HeroGem'

const W = 1520, H = 900
const dims = (name: string) => { const s = SHOTS[name]; return s?.clip ? s.clip.slice(2) : s?.size ?? [W, H] }

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

function Feature({ kicker, title, children, shot, alt, flip, more }: { kicker: string; title: string; children: ReactNode; shot: string; alt: string; flip?: boolean; more?: [string, string] }) {
  return (
    <div className={`feature ${flip ? 'flip' : ''}`}>
      <div className="copy">
        <p className="kicker">{kicker}</p>
        <h3>{title}</h3>
        <p>{children}</p>
        {more && <a className="more" href={more[1]}>{more[0]} →</a>}
      </div>
      <div className="shot"><img src={`/shots/${shot}.png`} alt={alt} loading="lazy" width={dims(shot)[0]} height={dims(shot)[1]} /></div>
    </div>
  )
}

const ALSO: [IconName, string, string][] = [
  ['message-square', 'Twitch chat + stream info', 'Change your title without alt-tabbing.'],
  ['folder', 'Asset library', 'Overlays and stingers, drag to place.'],
  ['history', 'Replay buffer', 'Up to 2 minutes. Hit Save.'],
  ['video', 'Virtual camera', 'Your scene, in Discord or Zoom.'],
  ['expand', 'Projectors', 'Fullscreen on your second monitor.'],
  ['undo-2', 'Undo history', '200 steps of "wait, go back."'],
  ['keyboard', 'Global hotkeys', 'Work with a game focused.'],
  ['audio-lines', 'Audio effects', 'Noise suppression for that keyboard.'],
  ['refresh-cw', 'OBS engine updates', 'Tested first, rolled back if broken.'],
]

const FAQ: [string, ReactNode][] = [
  ['Do I need OBS installed?', <p>Nope. LOBS ships its own OBS 32.2.2 engine and leaves your OBS alone.</p>],
  ['Can I import my OBS scenes?', <p>Yep. Scenes, sources, crops, main filters and audio come over. Groups get flattened and nested scenes get skipped. <a href="/docs/importing-from-obs/">Details</a></p>],
  ['Do OBS plugins work?', <p>Not yet. That includes obs-websocket.</p>],
  ['Why is Windows yelling at me?', <p>The installer isn't code-signed yet. Click <b>More info</b>, then <b>Run anyway</b>.</p>],
  ['Is it free?', <p>Yes. GPL, no account, no watermark. <a href={REPO}>Source on GitHub</a>.</p>],
  ['Mac? Linux?', <p>Windows only for now.</p>],
]

export function Home() {
  return (
    <>
      <section className="hero">
        <HeroGem />
        <div className="wrap">
          <h1>Built on OBS. Made for creating.</h1>
          <p className="lede">Same capture and streaming you trust, plus layers, linking, crop handles and drawing tools.</p>
          <div className="ctas">
            <a className="sbtn primary" href={DOWNLOAD}><DownloadIcon />Download for Windows</a>
            <a className="sbtn ghost" href="/docs/getting-started/">Docs</a>
          </div>
          <div className="facts"><span>Free</span><span>Open source</span><span>Windows 64-bit</span><span>v{VERSION} preview</span></div>
          <div className="madewith">Made with Claude Opus 5.5</div>
        </div>
        <div className="stagewrap">
          <HeroApp />
          <p className="stagenote">A working model of the editor. Click around.</p>
        </div>
      </section>

      <div className="wrap" id="features">
        <Feature kicker="Sources" title="Add the cam once. That's the feature." shot="feat-sources" alt="Facecam used in Just Chatting and Gameplay" more={['Scenes and sources', '/docs/scenes-and-sources/']}>
          Every scene gets its own copy to resize, crop and filter. The webcam only gets captured once.
        </Feature>
        <Feature flip kicker="Linking" title="Color grade once, not once per scene." shot="feat-linked" alt="Filters linked with Gameplay" more={['Inspector', '/docs/inspector/']}>
          Link a cam's filters, size or crop across scenes. Or don't. Your call.
        </Feature>
        <Feature kicker="Crop" title="Circle cam in two clicks." shot="feat-crop" alt="A facecam cropped to a circle" more={['Crop & corners', '/docs/inspector/#crop-corners']}>
          Crop right on the preview, pick 1:1, slide the corners to 50%. Done.
        </Feature>
        <Feature flip kicker="Graphic tools" title="Draw on stream like a sports commentator." shot="feat-tools" alt="Shapes and arrows drawn on a scene" more={['Graphic tools', '/docs/graphic-tools/']}>
          Boxes, arrows, text, a pencil, and a laser pointer that fades out on its own.
        </Feature>
        <Feature kicker="Layers" title="Layers that work like Figma." shot="feat-layers" alt="Layers panel with a group and its menu" more={['Layers', '/docs/layers/']}>
          Drag to reorder, Ctrl+G to group, Ctrl+] to bring forward.
        </Feature>
        <Feature flip kicker="Going live" title="Hard to mess up mid-stream." shot="feat-live" alt="The editor while live" more={['Going live', '/docs/going-live/']}>
          Layers lock while you're live (hold Alt to move one). Ending the stream takes two clicks.
        </Feature>
        <Feature kicker="Setup" title="Setup that tests your internet first." shot="feat-setup" alt="The connection test" more={['Getting started', '/docs/getting-started/']}>
          A 10 second test stream that doesn't go live, then it picks your bitrate.
        </Feature>

        <div className="band">
          <div className="grow">
            <h2>Try it.</h2>
            <p>No admin needed. Your OBS stays untouched.</p>
          </div>
          <a className="sbtn primary" href={DOWNLOAD}><DownloadIcon />Download LOBS {VERSION}</a>
        </div>

        <section className="section">
          <h2 className="h2">Also in the box</h2>
          <div className="grid3">
            {ALSO.map(([icon, t, p]) => <div key={t}><h4><Icon name={icon} size={18} />{t}</h4><p>{p}</p></div>)}
          </div>
        </section>

        <section className="section">
          <h2 className="h2">Same engine as OBS. Different editor.</h2>
          <table className="compare">
            <thead><tr><th /><th>OBS Studio</th><th>LOBS</th></tr></thead>
            <tbody>
              <tr><td>One cam, two looks</td><td>Two sources</td><td>One source</td></tr>
              <tr><td>Filters</td><td>Per source</td><td>Per scene, linkable</td></tr>
              <tr><td>Video filters</td><td>Lots, plus plugins</td><td>Four, for now</td></tr>
              <tr><td>Canvas</td><td>Any size</td><td>1920×1080</td></tr>
              <tr><td>Plugins</td><td>Yes</td><td>Not yet</td></tr>
              <tr><td>Platforms</td><td>Windows, Mac, Linux</td><td>Windows</td></tr>
            </tbody>
          </table>
        </section>

        <section className="section">
          <h2 className="h2">Questions</h2>
          <div className="faq">
            {FAQ.map(([q, a]) => <details key={q}><summary>{q}</summary><div className="a">{a}</div></details>)}
          </div>
        </section>

        <section className="final">
          <h2 className="h2">Set up once. Go talk to chat.</h2>
          <div className="ctas">
            <a className="sbtn primary" href={DOWNLOAD}><DownloadIcon />Download for Windows</a>
            <a className="sbtn ghost" href={REPO}><GitHubIcon />Source</a>
          </div>
        </section>
      </div>
    </>
  )
}
