import type { ReactNode } from 'react'
import { Btn, Check, Field, FillSlider, Icon, Select, Switch, Badge } from '../primitives'
import { useStore } from '../store'
import { editScene, type SourceKind } from '../model'
import type { IconName } from '../icons.gen'

function Scrim({ children }: { children: ReactNode }) {
  return <div className="scrim" onClick={(e) => e.stopPropagation()}>{children}</div>
}

const KINDS: [SourceKind, IconName, string][] = [
  ['camera', 'video', 'Camera'], ['display', 'monitor', 'Display'], ['window', 'app-window', 'Window'], ['game', 'gamepad-2', 'Game'],
  ['browser', 'globe', 'Browser'], ['mic', 'mic', 'Mic'], ['desktop_audio', 'volume-2', 'Desktop'],
]

function AddSource() {
  const { s, d } = useStore()
  const close = () => d({ type: 'set', patch: { dialog: null } })
  return (
    <Scrim>
      <div className="dialog">
        <h3>Add a source</h3>
        <span className="caption">Each input is added once and shared by every scene.</span>
        <div className="kinds">
          {KINDS.map(([k, icon, name]) => (
            <button key={k} className={`kind ${s.addKind === k ? 'on' : ''}`} onClick={() => d({ type: 'set', patch: { addKind: k } })}>
              <Icon name={icon} size={18} color={s.addKind === k ? 'var(--fg)' : 'var(--muted)'} />{name}
            </button>
          ))}
        </div>
        <Field placeholder="Name (optional)" focus />
        {s.addKind === 'browser' && <Field placeholder="URL, e.g. your alert box link" />}
        {s.addKind !== 'mic' && s.addKind !== 'desktop_audio' && <Check on text={`Also place on ${editScene(s).name}`} />}
        <div className="row" style={{ gap: 4, marginTop: 4 }}>
          <span className="grow" />
          <Btn text="Cancel" variant="ghost" onClick={close} />
          <Btn text="Add source" variant="primary" onClick={close} />
        </div>
      </div>
    </Scrim>
  )
}

function Transitions() {
  const { s, d } = useStore()
  const cur = s.transitions.find((t) => t.id === s.currentTransition) ?? s.transitions[0]
  const icon: Record<string, IconName> = { fade: 'blend', cut: 'scissors', swipe: 'move-horizontal', slide: 'arrow-left-right', fade_to_color: 'palette', luma_wipe: 'wand-sparkles', stinger: 'zap' }
  return (
    <Scrim>
      <div className="dialog" style={{ width: 740, height: 584 }}>
        <h3>Transitions</h3>
        <div className="row grow" style={{ alignItems: 'stretch', gap: 14, minHeight: 0 }}>
          <div style={{ width: 204, display: 'flex', flexDirection: 'column', gap: 2, borderRight: '1px solid var(--border)', paddingRight: 14 }}>
            {s.transitions.map((t) => (
              <div key={t.id} className={`lrow ${t.id === cur.id ? 'sel' : ''}`} style={{ padding: '0 8px', height: 34 }}>
                <Icon name={icon[t.type]} color="var(--muted)" />
                <span className="name grow">{t.name}</span>
                {t.id === s.currentTransition && <Badge text="In use" />}
              </div>
            ))}
            <span className="grow" />
            <Btn icon="plus" text="Add transition" />
          </div>
          <div className="grow" style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
            <div style={{ aspectRatio: '16/9', background: '#4f6fbf', borderRadius: 4, position: 'relative', display: 'grid', placeItems: 'center', color: '#fff', fontSize: 150, fontWeight: 800 }}>
              A
              <Btn icon="play" text="Preview" style={{ position: 'absolute', right: 8, bottom: 8 }} />
            </div>
            <div className="row" style={{ gap: 6 }}>
              <div className="grow"><Field>{cur.name}</Field></div>
              <span className="muted" style={{ padding: '0 8px' }}>In use</span>
              <Btn icon="trash-2" variant="ghost" />
            </div>
            {cur.type !== 'cut' && cur.type !== 'stinger' && <FillSlider label="Duration" frac={(cur.durationMs - 100) / 2900} value={`${cur.durationMs} ms`} />}
            {cur.type === 'cut' && <span className="caption">Switches instantly, with no animation.</span>}
          </div>
        </div>
        <div className="row" style={{ gap: 12 }}>
          <span className="grow" />
          <span className="caption">Double-click a transition to use it for Show.</span>
          <Btn text="Done" variant="primary" onClick={() => d({ type: 'set', patch: { dialog: null } })} />
        </div>
      </div>
    </Scrim>
  )
}

const SECTIONS: [string, IconName, string][] = [
  ['Stream', 'radio-tower', 'Where LOBS sends your live video.'],
  ['Recording', 'circle', "Where recordings go and how they're encoded."],
  ['Video', 'monitor', 'Output size, frame rate and encoder.'],
  ['Audio', 'audio-lines', 'Devices for your global mic and desktop audio.'],
  ['Hotkeys', 'keyboard', 'Work even while a game has focus.'],
  ['OBS engine', 'refresh-cw', "Updates from OBS's official releases."],
  ['About', 'info', 'Versions, licenses and where LOBS keeps its files.'],
]

function Row({ t, sub, children }: { t: string; sub?: string; children?: ReactNode }) {
  return (
    <div className="crow">
      <div className="grow" style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
        <span className="t">{t}</span>
        {sub && <span className="caption">{sub}</span>}
      </div>
      {children && <div style={{ width: 232, display: 'flex', justifyContent: 'flex-end', gap: 6 }}>{children}</div>}
    </div>
  )
}
const Label = ({ children }: { children: ReactNode }) => <span className="caption" style={{ fontWeight: 550, margin: '8px 4px -4px' }}>{children}</span>

function SettingsBody({ i }: { i: number }) {
  switch (i) {
    case 0: return <>
      <div className="card"><Row t="Setup assistant" sub="Picks a server, bitrate and video settings."><Btn icon="wand-sparkles" text="Run setup" /></Row></div>
      <Label>Destination</Label>
      <div className="card"><Row t="Service" sub="Pick Custom for any RTMP or SRT server."><Select value="Twitch" /></Row><Row t="Server" sub="Auto picks the closest ingest."><Select value="Auto (Recommended)" /></Row></div>
      <Label>Stream key</Label>
      <div className="card"><Row t="Key saved" sub="Stored encrypted."><div className="grow"><Field placeholder="Replace key…" /></div><Btn icon="trash-2" variant="ghost" /></Row></div>
      <Label>Quality</Label>
      <div className="card"><Row t="Video bitrate" sub="6000 kbps suits 1080p60 on Twitch. Services may cap it."><div className="grow"><Field right={<span className="caption">kbps</span>}>6000</Field></div></Row><Row t="Audio bitrate"><Select value="160 kbps" /></Row></div>
    </>
    case 1: return <>
      <div className="card"><Row t="Folder" sub="New recordings are named by date and time."><div className="grow"><Field>C:\Users\you\Videos\LOBS</Field></div></Row>
        <Row t="Format" sub="MKV survives a crash; MP4 plays everywhere."><Select value="MP4" /></Row>
        <Row t="Recording quality"><Select value="High, medium files" /></Row></div>
      <Label>Replay buffer</Label>
      <div className="card"><Row t="Keep the last" sub="Turn it on from the top bar. Save replay writes to your recordings folder."><Select value="30 s" /></Row></div>
    </>
    case 2: return <div className="card">
      <Row t="Resolution" sub="Your canvas stays 1920 × 1080 and is scaled to this."><Select value="1920×1080" /></Row>
      <Row t="Frame rate"><Select value="60 fps" /></Row>
      <Row t="Video encoder" sub="Hardware encoders use the GPU."><Select value="Automatic · NVIDIA NVENC H.264" /></Row>
      <Row t="Preset" sub="Quality looks best; Speed uses the least GPU."><Select value="Quality" /></Row>
    </div>
    case 3: return <>
      <div className="card"><Row t="Mic" sub="Always mixed in, in every scene."><Select value="Shure MV7" /></Row><Row t="Desktop Audio" sub="Always mixed in, in every scene."><Select value="Default device" /></Row></div>
      <span className="caption">Levels, monitoring and tracks are in the Audio tab.</span>
    </>
    case 4: return <>
      <Label>Output</Label>
      <div className="card">{['Start or stop streaming', 'Start or stop recording', 'Turn the replay buffer on or off', 'Save replay', 'Start or stop the virtual camera'].map((t, j) => <Row key={t} t={t}><div className="grow"><Field>{j === 3 ? 'Ctrl + F9' : <span className="muted">Not set</span>}</Field></div></Row>)}</div>
      <Label>Scenes</Label>
      <div className="card">{['Show Starting Soon', 'Show Just Chatting', 'Show Gameplay'].map((t, j) => <Row key={t} t={t}><div className="grow"><Field>{`Ctrl + ${j + 1}`}</Field></div></Row>)}</div>
    </>
    case 5: return <>
      <div className="card"><Row t="OBS 32.2.2" sub="Up to date · checked today at 8:01 PM"><Btn text="Check now" /></Row><Row t="Include OBS betas"><Switch on={false} /></Row></div>
      <span className="caption" style={{ lineHeight: 1.5 }}>LOBS checks for new OBS releases once a day. Downloads are checked against their SHA-256, tested with LOBS before switching, and LOBS goes back to the previous engine if a new one fails to start twice.</span>
    </>
    default: return <div className="card">
      <Row t="Version" sub="0.3.1 · Qt 6.11, C++20" />
      <Row t="Engine" sub="OBS 32.2.2 (official release build)" />
      <Row t="Shows and settings" sub="C:\Users\you\AppData\Roaming\LOBS\LOBS" />
      <Row t="Open source" sub="OBS Studio (libobs) GPL-2.0 · LOBS GPL-2.0-or-later · Inter (SIL OFL 1.1) · Lucide icons (ISC)" />
    </div>
  }
}

function Settings() {
  const { s, d } = useStore()
  const i = s.settingsSection
  return (
    <div className="settings">
      <nav>
        <div className="row" style={{ gap: 8, height: 40, padding: '0 4px', marginBottom: 6 }}>
          <Btn icon="x" variant="ghost" onClick={() => d({ type: 'set', patch: { dialog: null } })} />
          <span style={{ fontSize: 14, fontWeight: 550 }}>Settings</span>
        </div>
        {SECTIONS.map(([t, icon], j) => (
          <div key={t} className={`lrow ${j === i ? 'sel' : ''}`} onClick={() => d({ type: 'set', patch: { settingsSection: j } })}>
            <Icon name={icon} color={j === i ? 'var(--fg)' : 'var(--muted)'} /><span className="name">{t}</span>
          </div>
        ))}
      </nav>
      <div className="grow" style={{ display: 'flex', justifyContent: 'center', overflow: 'hidden' }}>
        <main className="grow">
          <div><h2>{SECTIONS[i][0]}</h2><p className="muted" style={{ margin: '6px 0 0' }}>{SECTIONS[i][2]}</p></div>
          <SettingsBody i={i} />
        </main>
      </div>
    </div>
  )
}

const STEPS = ['Welcome to LOBS', 'Your channel', 'Closest server', 'Test your connection', 'Video and bitrate', 'Recording', 'Twitch panels', "You're set"]

function Choice({ icon, t, sub, on }: { icon: IconName; t: string; sub: string; on?: boolean }) {
  return (
    <div style={{ padding: '14px 12px', display: 'flex', gap: 12, alignItems: 'center', borderRadius: 10, background: 'var(--s2)', border: on ? '2px solid var(--fg)' : '1px solid var(--border)' }}>
      <Icon name={icon} size={18} />
      <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}><span style={{ fontSize: 13, fontWeight: 550 }}>{t}</span><span className="caption">{sub}</span></div>
    </div>
  )
}

function Setup() {
  const { s, d } = useStore()
  const i = s.setupStep
  const body = [
    <>
      <span className="muted">A few questions and a quick test to set up streaming and recording. You can change it all later in Settings.</span>
      <Choice icon="radio-tower" t="Stream and record" sub="Twitch, YouTube or another service." on />
      <Choice icon="film" t="Just record" sub="No streaming." />
      <span className="caption" style={{ marginTop: 6 }}>Where do you stream?</span>
      <div className="row" style={{ gap: 6 }}><Btn text="Twitch" variant="tertiary" sel /><Btn text="YouTube" variant="tertiary" /><Btn text="Other (RTMP)" variant="tertiary" /></div>
    </>,
    <>
      <span className="caption">Twitch username</span><Field right={undefined}><span className="muted">twitch.tv/</span>mossy_kettle</Field>
      <span className="caption">Stream key</span><div className="row" style={{ gap: 6 }}><div className="grow"><Field>••••••••••••••••••••••</Field></div><Btn icon="external-link" text="Get my key" /></div>
    </>,
    <>
      <span className="muted">Sorted by response time.</span>
      {[['US West: Seattle, WA', '18 ms', true], ['US West: San Jose, CA', '31 ms'], ['US West: Los Angeles, CA', '39 ms'], ['US Central: Denver, CO', '44 ms']].map(([n, ms, best]) => (
        <div key={n as string} className={`lrow ${best ? 'sel' : ''}`} style={{ padding: '0 10px' }}><span className="name grow">{n}</span>{best && <Badge text="Fastest" tone="link" />}<span className="count">{ms}</span></div>
      ))}
      <div className="row"><Btn icon="refresh-cw" text="Check again" variant="ghost" /></div>
    </>,
    <>
      <span className="muted">A 10 second test stream. Nothing goes live.</span>
      <div className="card" style={{ padding: 16, display: 'flex', gap: 24 }}>
        <div><div style={{ fontSize: 22, fontWeight: 700 }}>14.2 Mbps</div><span className="caption">upload</span></div>
        <div><div style={{ fontSize: 22, fontWeight: 700 }}>0.0%</div><span className="caption">frames dropped</span></div>
      </div>
      <div className="row"><Btn icon="refresh-cw" text="Test again" /></div>
    </>,
    <>
      <div className="row" style={{ gap: 6 }}><span className="caption" style={{ width: 90 }}>Resolution</span><Select value="1920×1080" /></div>
      <div className="row" style={{ gap: 6 }}><span className="caption" style={{ width: 90 }}>Frame rate</span><Select value="60 fps (games, motion)" /></div>
      <FillSlider label="Stream bitrate" frac={1} value="6.0 Mbps" />
    </>,
    <>
      <div className="row" style={{ gap: 6 }}><span className="caption" style={{ width: 90 }}>Save to</span><div className="grow"><Field>C:\Users\you\Videos\LOBS</Field></div><Btn text="Browse…" /></div>
      <div className="row" style={{ gap: 6 }}><span className="caption" style={{ width: 90 }}>Quality</span><Select value="High (moderate files)" /></div>
      <div className="row" style={{ gap: 6 }}><span className="caption" style={{ width: 90 }}>Format</span><Select value="MKV (survives crashes)" /></div>
    </>,
    <>
      <div className="row" style={{ gap: 8 }}>
        <div className="grow"><Choice icon="message-square" t="Chat" sub="Read and reply to chat." on /></div>
        <div className="grow"><Choice icon="info" t="Stream info" sub="Title, category and tags." on /></div>
      </div>
    </>,
    <>
      <div className="card">{[['Stream', 'Twitch · Seattle · 6000 kbps'], ['Video', '1920×1080 at 60 fps'], ['Recording', 'MKV, high quality'], ['Panels', 'Chat and Stream info']].map(([k, v]) => <Row key={k} t={k} sub={v} />)}</div>
      <span className="caption">Finish to save.</span>
    </>,
  ][i]
  return (
    <Scrim>
      <div className="dialog" style={{ width: 700, padding: 20, gap: 14 }}>
        <div className="row" style={{ gap: 6 }}>{STEPS.map((_, j) => <span key={j} className="grow" style={{ height: 3, borderRadius: 2, background: j <= i ? 'var(--fg)' : 'var(--s5)' }} />)}</div>
        <h3 style={{ fontSize: 22, fontWeight: 700 }}>{STEPS[i]}</h3>
        {body}
        <div className="row" style={{ marginTop: 6 }}>
          {i === 0 ? <Btn text="Skip setup" variant="ghost" onClick={() => d({ type: 'set', patch: { dialog: null } })} /> : <Btn text="Back" variant="ghost" onClick={() => d({ type: 'set', patch: { setupStep: i - 1 } })} />}
          <span className="grow" />
          <Btn text={i === STEPS.length - 1 ? 'Finish' : 'Continue'} variant="primary" onClick={() => d({ type: 'set', patch: i === STEPS.length - 1 ? { dialog: null } : { setupStep: i + 1 } })} />
        </div>
      </div>
    </Scrim>
  )
}

function NewProject() {
  const { d } = useStore()
  const close = () => d({ type: 'set', patch: { dialog: null } })
  return (
    <Scrim>
      <div className="dialog" style={{ width: 480 }}>
        <h3>New project</h3>
        <span className="caption">Name</span>
        <Field focus>Friday speedruns</Field>
        <span className="caption">Location</span>
        <div className="row" style={{ gap: 6 }}><div className="grow"><Field>C:\Users\you\Documents\LOBS Projects</Field></div><Btn text="Browse…" /></div>
        <div className="row" style={{ gap: 8, padding: '8px 10px', borderRadius: 8, background: '#2a2008', border: '1px solid #6b5320', color: '#fde68a' }}>
          <Icon name="triangle-alert" color="#fbbf24" />
          <span className="grow">Your current show isn't saved as a project. Save it first, or it will be replaced.</span>
          <Btn text="Save as…" variant="ghost" />
        </div>
        <div className="row" style={{ gap: 4, marginTop: 4 }}>
          <span className="grow" />
          <Btn text="Cancel" variant="ghost" onClick={close} />
          <Btn text="Create project" variant="primary" onClick={close} />
        </div>
      </div>
    </Scrim>
  )
}

/** The next launch after a crash offers to report it (crashDialog in Main.qml). */
function Crash() {
  const { d } = useStore()
  const close = () => d({ type: 'set', patch: { dialog: null } })
  return (
    <Scrim>
      <div className="dialog" style={{ width: 420 }}>
        <h3>LOBS closed unexpectedly</h3>
        <span className="muted" style={{ lineHeight: 1.5 }}>Sorry about that. LOBS saved a crash report on this PC. Reporting it on GitHub helps get it fixed. The issue asks for the log, which is safe to post. Keep the .dmp file private: it can hold what LOBS had in memory, like your stream key.</span>
        <div className="row" style={{ gap: 4, marginTop: 4 }}>
          <Btn text="Show files" variant="ghost" />
          <span className="grow" />
          <Btn text="Not now" variant="secondary" onClick={close} />
          <Btn text="Report on GitHub" variant="primary" onClick={close} />
        </div>
      </div>
    </Scrim>
  )
}

export function Overlays() {
  const { s } = useStore()
  switch (s.dialog) {
    case 'addSource': return <AddSource />
    case 'transitions': return <Transitions />
    case 'settings': return <Settings />
    case 'setup': return <Setup />
    case 'newProject': return <NewProject />
    case 'crash': return <Crash />
    default: return null
  }
}
