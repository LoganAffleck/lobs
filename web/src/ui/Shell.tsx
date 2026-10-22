import type { ReactNode } from 'react'
import { Anchor, Btn, Help, Icon, Menu } from './primitives'
import { useStore } from './store'
import type { IconName } from './icons.gen'

// Panel help, word for word from PanelDragController's entries in Main.qml.
export const HELP: Record<string, string[]> = {
  Scenes: ['Click: edit a scene without showing it.', 'Show or double-click: switch to the scene.', 'Grid: shows live thumbnails of every scene.', 'Right-click: rename, duplicate with linked or independent layers, or delete.'],
  Sources: ['Inputs: added once, shared by every scene.', 'Place it: drag onto the canvas or double-click.', 'Per scene: size, crop and filters.', 'Files are in Assets. Add text and color from Layers.'],
  Assets: ["Project files: this project's assets folder.", 'Add files: drag in from File Explorer or use + Asset.', 'Place it: drag onto the canvas or double-click.', 'Right-click: show in File Explorer or move to Recycle Bin.'],
  Layers: ['Order: layers higher in the list draw on top.', 'Linked: settings shared with another scene.', 'Shortcuts: Ctrl+C / Ctrl+V copy and paste (Ctrl+Shift+V pastes linked), Ctrl+D duplicate, Ctrl+G group, Delete remove.', 'Copy to: copy layers to another scene.'],
  Inspector: ['This scene: layer changes apply here only.', 'Linked: shared with another scene. Unlink to separate.', 'Numbers: math like 1920/2 works. Up/Down nudge, Shift for 10.', 'Source: select one in Sources to edit its input.'],
  'Audio Mixer': ['Global: Mic and Desktop Audio play in every scene.', 'Scene audio: plays only in scenes that contain it.', 'Tracks: track 1 goes to the stream; recordings can use tracks 1-6.', 'Monitor: hear it in headphones. Monitor only keeps it out of output.', 'Fader: set the level. Meter shows peak dB.'],
  History: ['Undo: Ctrl+Z; redo: Ctrl+Shift+Z or Ctrl+Y.', 'Jump: click a step to go back to it.'],
  Stats: ["Frames missed: the PC couldn't render in time.", 'Frames skipped: the encoder fell behind.', "Dropped frames: the network couldn't keep up.", 'Orange: more than 1%, worth a look.'],
  Captions: ['Speech to text from one audio source, on this PC.', 'Send to stream: viewers turn on CC to see them.', 'Language: pick it, or let LOBS detect it.', 'Text shows up a few seconds after speech.'],
  Canvas: ['Move: drag a layer. Snaps to edges and layers.', 'Resize: drag a corner. Shift: free resize.', 'Ctrl: drag without snapping.', 'Groups: click selects the group. Each double-click goes one level deeper.', 'Nudge: arrow keys move 1 px, Shift+arrows 10 px.', 'Live lock: hold Alt to move layers while live.', 'Show: switch to this scene.'],
}

const PANELS = ['Scenes', 'Sources', 'Assets', 'Layers', 'Inspector', 'Audio Mixer', 'History', 'Stats', 'Captions', 'Chat', 'Stream info']
// Closed in the default layout (the Window menu opens them).
const CLOSED = ['Stats', 'Captions', 'Chat', 'Stream info']

function Seg({ icon, text, on, dot = '#22c55e', onClick }: { icon: IconName; text: string; on: boolean; dot?: string; onClick: () => void }) {
  return (
    <button className={`seg ${on ? 'on' : ''}`} onClick={onClick} data-ui={text}>
      <span className="icowrap">
        <Icon name={icon} color={on ? 'var(--fg)' : 'var(--muted)'} />
        {on && <span className="dot" style={{ background: dot }} />}
      </span>
      {text}
    </button>
  )
}

export function Header() {
  const { s, d } = useStore()
  const o = s.outputs
  const menu = (id: 'window' | 'import') => d({ type: 'menu', menu: s.menu?.id === id ? null : { id } })
  const hov = (b: string) => s.hover?.button === b
  return (
    <div className="header">
      {/* The top bar is the title bar: the app's mark, name and Settings are one button. */}
      <button className={`brand ${hov('settings') ? 'hov' : ''}`} data-ui="settings" onClick={() => d({ type: 'set', patch: { dialog: 'settings' } })}>
        <img src="/brand/lobs-gem-64.png" alt="" width={20} height={20} draggable={false} />
        LOBS
        <Icon name="settings" size={14} color={hov('settings') ? 'var(--fg)' : 'var(--muted)'} />
      </button>
      <Btn text="New" variant="ghost" hov={hov('New')} onClick={() => d({ type: 'set', patch: { dialog: 'newProject' } })} />
      <Btn text="Open" variant="ghost" hov={hov('Open')} />
      <Anchor>
        <Btn text="Import" variant="ghost" sel={s.menu?.id === 'import'} onClick={() => menu('import')} />
        <Menu id="import" width={280} entries={[
          { text: 'OBS: Untitled  ·  6 scenes' },
          { text: 'OBS: Speedrun  ·  3 scenes' },
          { sep: true },
          { text: 'Choose a file...' },
        ]} />
      </Anchor>
      <Btn text="Save" variant="ghost" hov={hov('Save')} />
      <span className="vsep" />
      <Anchor>
        <Btn text="Window" variant="ghost" sel={s.menu?.id === 'window'} onClick={() => menu('window')} />
        <Menu id="window" width={240} entries={[
          ...PANELS.map((p) => ({ text: p, sel: !CLOSED.includes(p) })),
          { sep: true },
          { text: 'Show all panels' },
          { text: 'Reset layout' },
        ]} />
      </Anchor>
      <span className="vsep" />
      <Btn icon="undo-2" variant="ghost" disabled={s.historyIndex <= 0} onClick={() => d({ type: 'jumpHistory', index: Math.max(0, s.historyIndex - 1) })} />
      <Btn icon="redo-2" variant="ghost" disabled={s.historyIndex >= s.history.length - 1} onClick={() => d({ type: 'jumpHistory', index: Math.min(s.history.length - 1, s.historyIndex + 1) })} />
      <span className="grow" />
      <div className="outs">
        <div className="outgroup">
          <Seg icon="history" text="Replay" on={o.replay} onClick={() => d({ type: 'toggleReplay' })} />
          {o.replay && <Btn icon="save" text="Save" variant="secondary" style={{ height: 22, marginRight: 4 }} />}
          <span className="div" />
          <Seg icon="video" text="Virtual cam" on={o.vcam} onClick={() => d({ type: 'toggleVcam' })} />
          <span className="div" />
          <Seg icon={o.recording ? 'square' : 'circle'} text={o.recording ? o.recTime ?? '00:00:00' : 'Record'} on={o.recording} dot="var(--live)" onClick={() => d({ type: 'toggleRecording' })} />
        </div>
        <button className={`golive ${o.streaming ? 'on' : ''} ${o.confirmingEnd ? 'confirm' : ''}`} data-ui="Go live" onClick={() => d({ type: 'toggleStreaming' })}>
          {o.streaming ? <span className="livedot" /> : <Icon name="radio-tower" color="var(--s1)" />}
          {!o.streaming ? 'Go live' : o.confirmingEnd ? 'Click again to end stream' : `LIVE  ${o.streamTime ?? '00:00:00'}`}
          {o.streaming && !o.confirmingEnd && <span className="kbps">{Math.round(o.kbps ?? 0)} kbps</span>}
        </button>
      </div>
      <div className="winbtns">
        <span className="winbtn"><Icon name="minus" size={15} color="var(--muted)" /></span>
        <span className="winbtn"><Icon name="square" size={15} color="var(--muted)" /></span>
        <span className="winbtn close"><Icon name="x" size={15} color="var(--muted)" /></span>
      </div>
    </div>
  )
}

export function StatusBar() {
  const { s } = useStore()
  const { d } = useStore()
  const o = s.outputs, st = s.stats
  const kv = (k: string, v: string) => <><span className="muted">{k}</span><span style={{ textAlign: 'right' }}>{v}</span></>
  return (
    <div className="statusbar" style={{ position: 'relative' }} onClick={() => d({ type: 'set', patch: { help: s.help === 'Stats' ? null : 'Stats' } })}>
      {s.help === 'Stats' && (
        <div className="popover" style={{ right: 8, bottom: 32, width: 340, display: 'grid', gridTemplateColumns: '1fr auto', columnGap: 16, rowGap: 6, fontSize: 12 }} onClick={(e) => e.stopPropagation()}>
          <h4 style={{ gridColumn: 'span 2' }}>Stats</h4>
          {kv('Frame rate', `${st.fps.toFixed(2)} / ${st.targetFps} fps`)}
          {kv('Render time', `${st.renderMs.toFixed(2)} ms`)}
          {kv('Frames missed (render lag)', '0 / 223804 (0.0%)')}
          {kv('Frames skipped (encoder lag)', '0 / 223790 (0.0%)')}
          {kv('CPU', `${st.cpu.toFixed(1)}%`)}
          {kv('Memory', '412 MB')}
          {kv('Free space for recordings', '318.4 GB')}
          <div style={{ gridColumn: 'span 2', height: 1, background: 'var(--border)', margin: '4px 0' }} />
          {kv('Stream', o.streaming ? `${o.streamTime} · ${Math.round(o.kbps ?? 0)} kbps` : 'Off')}
          {o.streaming && kv('Dropped frames (network)', `${o.dropped ?? 0} / 223790 (0.0%)`)}
          {kv('Recording', o.recording ? `${o.recTime} · ${(o.recMb ?? 0).toFixed(0)} MB` : 'Off')}
          {kv('Replay buffer', o.replay ? 'On · last 30 s' : 'Off')}
          {kv('Virtual camera', o.vcam ? 'On' : 'Off')}
        </div>
      )}
      <span className="grow ellipsis">{s.status}</span>
      {o.streaming && <span className="s fg"><span className="d" style={{ background: 'var(--live)' }} />LIVE {o.streamTime} · {Math.round(o.kbps ?? 0)} kbps · {o.dropped ?? 0} dropped (0.0%)</span>}
      {o.recording && <span className="s fg"><span className="d" style={{ background: 'var(--live)' }} />REC {o.recTime} · {(o.recMb ?? 0).toFixed(0)} MB</span>}
      {o.replay && <span className="s"><span className="d" style={{ background: '#22c55e' }} />Replay on</span>}
      {o.vcam && <span className="s"><span className="d" style={{ background: '#22c55e' }} />Virtual camera</span>}
      <span className="s" style={{ color: st.cpu > 80 ? '#fbbf24' : undefined }}>CPU {st.cpu.toFixed(1)}%</span>
      <span className="s">{st.fps.toFixed(0)} / {st.targetFps} fps · {st.renderMs.toFixed(1)} ms</span>
    </div>
  )
}

/** A dock panel group: rounded surface-2 card, tab strip with grips, help on the current tab. */
export function Group({ tabs, current = tabs[0], onTab, children, flex, style, id }: { tabs: string[]; current?: string; onTab?: (t: string) => void; children: ReactNode; flex?: boolean; style?: React.CSSProperties; id?: string }) {
  return (
    <div className={`group ${flex ? 'flex' : ''}`} style={style} data-panel={id ?? current}>
      <div className="tabstrip">
        {tabs.map((t) => (
          <div key={t} className={`ptab ${t === current ? 'on' : ''}`} onClick={() => onTab?.(t)} data-ui={`tab:${t}`}>
            <span className="grip"><Icon name="grip-vertical" size={12} color="var(--muted)" /></span>
            {t}
            {t === current && HELP[t] && <Help title={t} points={HELP[t]} small />}
          </div>
        ))}
      </div>
      <div className="body">{children}</div>
    </div>
  )
}
