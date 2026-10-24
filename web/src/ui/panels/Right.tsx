import { useState } from 'react'
import { Anchor, Btn, FillSlider, Icon, Menu, NumField, Select, Switch, Badge, Field } from '../primitives'
import { useSelectedLayer, useStore } from '../store'
import { Group } from '../Shell'
import { editScene, flatLayers, iconForLayer, iconForSource, scenesUsing, type Layer, type LinkGroup, type Source } from '../model'

export function LayersPanel({ style }: { style?: React.CSSProperties }) {
  const { s, d } = useStore()
  const scene = editScene(s)
  const rows = flatLayers(scene.layers)
  return (
    <Group tabs={['Layers']} style={style}>
      <div className="phead">
        <span className="meta" style={{ marginLeft: 0 }}>{scene.name}</span>
        <span className="grow" />
        <Btn icon="group" variant="ghost" />
        <Anchor>
          <Btn icon="copy" variant="ghost" sel={s.menu?.id === 'copy'} onClick={() => d({ type: 'menu', menu: s.menu?.id === 'copy' ? null : { id: 'copy' } })} />
          <Menu id="copy" align="right" entries={s.scenes.map((sc) => ({ text: sc.name, sub: [{ text: 'Linked copy' }, { text: 'Independent copy' }] }))} />
        </Anchor>
        <Btn icon="trash-2" variant="ghost" />
      </div>
      <div className="list">
        {rows.map(({ layer: l, depth }) => <LayerRow key={l.id} l={l} depth={depth} />)}
        {rows.length === 0 && <div className="emptycap">{'Empty scene.\nDrag a source from Sources onto the canvas.'}</div>}
      </div>
    </Group>
  )
}

function LayerRow({ l, depth }: { l: Layer; depth: number }) {
  const { s, d } = useStore()
  const sel = s.selection.includes(l.id)
  const hov = s.hover?.layer === l.id
  const vis = l.visible !== false
  const linked = !!l.links && Object.values(l.links).some((x) => x && x.length)
  const fx = l.filters?.length ?? 0
  return (
    <div className={`lrow ${sel ? 'sel' : ''} ${hov ? 'hov' : ''}`} style={{ paddingLeft: 12 + depth * 16, paddingRight: 8 }} data-layer={l.name}
      onClick={(e) => d({ type: 'selectLayer', id: l.id, add: e.shiftKey || e.ctrlKey })}
      onContextMenu={(e) => { e.preventDefault(); d({ type: 'selectLayer', id: l.id }); d({ type: 'menu', menu: { id: 'layer', target: l.id } }) }}>
      {l.kind === 'group'
        ? <span className="chev" onClick={(e) => { e.stopPropagation(); d({ type: 'toggleGroup', id: l.id }) }}><Icon name={l.expanded === false ? 'chevron-right' : 'chevron-down'} size={12} color="var(--muted)" /></span>
        : <span className="dot" style={{ background: l.color ?? '#a3a3a3' }} />}
      <Icon name={iconForLayer(l, s.sources)} color={vis ? 'var(--fg)' : 'var(--muted)'} />
      <span className="name grow ellipsis" style={{ color: vis ? undefined : 'var(--muted)' }}>{l.name}</span>
      {linked && <Icon name="link-2" color="var(--link)" />}
      {fx > 0 && <Badge text={`${fx} fx`} />}
      <span className="tools">
        {!l.background && <Btn icon="chevron-up" variant="ghost" onClick={() => d({ type: 'moveLayer', id: l.id, by: -1 })} />}
        {!l.background && <Btn icon="chevron-down" variant="ghost" onClick={() => d({ type: 'moveLayer', id: l.id, by: 1 })} />}
        <Btn icon={l.locked ? 'lock' : 'lock-open'} variant="ghost" onClick={() => d({ type: 'toggleLock', id: l.id })} />
      </span>
      {l.locked && !sel && !hov && <Btn icon="lock" variant="ghost" style={{ marginLeft: -84 }} />}
      <Btn icon={vis ? 'eye' : 'eye-off'} variant="ghost" onClick={() => d({ type: 'toggleVisible', id: l.id })} />
      <Menu id="layer" target={l.id} offset={[60, -6]} entries={[
        { text: 'Rename…' },
        { text: 'Copy', kbd: 'Ctrl C' },
        ...(l.background ? [] : [{ text: 'Cut', kbd: 'Ctrl X' }]),
        { text: 'Paste', kbd: 'Ctrl V' },
        { text: 'Paste linked', kbd: 'Ctrl Shift V' },
        { sep: true },
        { text: 'Duplicate' },
        { text: 'Group selection' },
        { text: 'Ungroup', disabled: l.kind !== 'group' },
        { sep: true },
        { text: 'Bring to front', kbd: 'Ctrl Shift ]' },
        { text: 'Bring forward', kbd: 'Ctrl ]', onClick: () => d({ type: 'moveLayer', id: l.id, by: -1 }) },
        { text: 'Send backward', kbd: 'Ctrl [', onClick: () => d({ type: 'moveLayer', id: l.id, by: 1 }) },
        { text: 'Send to back', kbd: 'Ctrl Shift [' },
        ...(l.background ? [] : [{ sep: true }, { text: 'Delete' }]),
      ]} />
    </div>
  )
}

/** Section header: reset when changed, a "Linked · Scene" chip when shared, a link button when linkable. */
function Section({ title, group, l, changed, children }: { title: string; group: LinkGroup; l: Layer; changed?: boolean; children: React.ReactNode }) {
  const { s, d } = useStore()
  const peers = l.links?.[group] ?? []
  const others = l.sourceId ? scenesUsing(s, l.sourceId).filter((x) => x.id !== s.editSceneId).map((x) => x.name) : []
  const key = `${l.id}:${group}`
  return (
    <div className="sec">
      <div className="sh">
        <span className="t">{title}</span>
        {changed && <Btn icon="rotate-ccw" variant="ghost" />}
        {peers.length > 0 && (
          <Anchor>
            <Btn icon="link-2" variant="ghost" className="linkchip" text={`Linked · ${peers.length === 1 ? peers[0] : `${peers.length} scenes`}`}
              sel={s.menu?.id === 'linked' && s.menu.target === key} onClick={() => d({ type: 'menu', menu: { id: 'linked', target: key } })} />
            <Menu id="linked" target={key} align="right" entries={[
              ...peers.map((p) => ({ text: `Shared with ${p}`, disabled: true })),
              { sep: true },
              { text: 'Unlink in this scene', onClick: () => d({ type: 'unlink', group }) },
            ]} />
          </Anchor>
        )}
        {peers.length === 0 && others.length > 0 && (
          <Anchor>
            <Btn icon="link-2" variant="ghost" sel={s.menu?.id === 'link' && s.menu.target === key} onClick={() => d({ type: 'menu', menu: { id: 'link', target: key } })} />
            <Menu id="link" target={key} width={260} align="right" entries={others.map((o) => ({
              text: `Link with ${o}`, sub: [{ text: "Keep this scene's values" }, { text: `Use ${o}'s values` }],
            }))} />
          </Anchor>
        )}
      </div>
      {children}
    </div>
  )
}

function SourceSection({ l }: { l: Layer }) {
  const { s, d } = useStore()
  const src = s.sources.find((x) => x.id === l.sourceId)
  if (!src) return null
  const count = scenesUsing(s, src.id).length
  const open = s.sourceSectionOpen
  return (
    <div>
      <div className="srchead" onClick={() => d({ type: 'set', patch: { sourceSectionOpen: !open } })}>
        <div className="row" style={{ gap: 6 }}>
          <Icon name={open ? 'chevron-down' : 'chevron-right'} size={12} color="var(--muted)" />
          <Icon name={iconForSource(src.kind)} />
          <span className="t grow ellipsis">Source · {src.name}</span>
          <span className="badge link"><Icon name="link-2" size={10} />{count > 1 ? `Used in ${count} scenes` : 'Shared input'}</span>
        </div>
        {!open && <span className="caption" style={{ paddingLeft: 38 }}>{src.problem ? <span className="badge problem">{src.problem}</span> : src.detail}</span>}
      </div>
      {open && (
        <div style={{ padding: '0 12px 14px', display: 'flex', flexDirection: 'column', gap: 8 }}>
          <Select value={src.detail?.split(' · ')[0]} />
          {src.kind === 'media' && <MediaControls src={src} />}
          <div className="row" style={{ gap: 8 }}>
            <span className="caption grow">{count > 1 ? `Changes here apply in every scene that uses ${src.name}.` : `Changes here apply wherever ${src.name} is used.`}</span>
            <Btn text="Open in Sources" variant="ghost" />
          </div>
        </div>
      )}
    </div>
  )
}

const label = (k: string) => k.charAt(0).toUpperCase() + k.slice(1).replace('_', ' ')
const range = (k: string): [number, number, number] => (k === 'hue' ? [-180, 180, 0] : k === 'similarity' || k === 'smoothness' ? [1, 1000, 0] : k === 'amount' ? [0, 1, 2] : [-1, 1, 2])

function LayerInspector({ l }: { l: Layer }) {
  const { s, d } = useStore()
  const src = s.sources.find((x) => x.id === l.sourceId)
  const croppable = l.kind === 'source' || l.kind === 'image'
  const c = l.crop ?? { l: 0, t: 0, r: 0, b: 0 }
  const vw = src?.size ? src.size[0] - c.l - c.r : l.w, vh = src?.size ? src.size[1] - c.t - c.b : l.h
  const va = vw / vh
  const near = (r: number) => Math.abs(va - r) < 0.01
  const cropped = !!(c.l || c.t || c.r || c.b)
  const radius = l.radius ?? 0
  const [cropOpen, setCropOpen] = useState(false)
  const shape = l.kind === 'rect' || l.kind === 'ellipse' || l.kind === 'arrow' || l.kind === 'pencil'
  return (
    <>
      {src ? <SourceSection l={l} /> : (
        <div style={{ padding: '9px 12px', display: 'flex', flexDirection: 'column', gap: 8 }}>
          <div className="row" style={{ gap: 8, height: 28 }}>
            <Icon name={iconForLayer(l, s.sources)} />
            <span style={{ fontSize: 13, fontWeight: 550 }}>{shape ? 'Shape' : l.kind === 'text' ? 'Text' : l.kind === 'color' ? 'Color' : l.name}</span>
          </div>
          {l.kind === 'text' && <Field>{l.text}</Field>}
          {(l.kind === 'rect' || l.kind === 'ellipse' || l.kind === 'color' || l.kind === 'text') && (
            <div className="kv"><span className="k">{l.kind === 'text' || l.kind === 'color' ? 'Color' : 'Fill'}</span>
              <Field right={<span className="swatch" style={{ background: l.fill || 'transparent', order: -1 }} />}>{(l.fill || 'None').toUpperCase()}</Field>
              {shape && <Btn text="None" variant="ghost" sel={!l.fill} />}</div>
          )}
          {shape && (
            <div className="kv"><span className="k">{l.kind === 'arrow' || l.kind === 'pencil' ? 'Color' : 'Stroke'}</span>
              <Field right={<span className="swatch" style={{ background: l.stroke, order: -1 }} />}>{(l.stroke ?? '').toUpperCase()}</Field>
              {l.kind !== 'arrow' && l.kind !== 'pencil' && <Btn text="None" variant="ghost" sel={!l.stroke} />}</div>
          )}
          {shape && <FillSlider label={l.kind === 'arrow' || l.kind === 'pencil' ? 'Thickness' : 'Stroke width'} frac={(l.strokeWidth ?? 6) / 48} value={`${l.strokeWidth ?? 6} px`} />}
        </div>
      )}

      {/* As the app: one labeled row per property, each action beside the value it changes. */}
      <Section title="Transform" group="transform" l={l} changed={!!l.rot}>
        <div className="kv"><span className="k rl">Position</span>
          <NumField label="X" value={l.x} /><NumField label="Y" value={l.y} />
          <Btn icon="crosshair" variant="ghost" /></div>
        <div className="kv"><span className="k rl">Size</span>
          <NumField label="W" value={l.w} /><NumField label="H" value={l.h} />
          <Btn icon={s.lockAspect ? 'link-2' : 'unlink-2'} variant="ghost" sel={s.lockAspect} onClick={() => d({ type: 'set', patch: { lockAspect: !s.lockAspect } })} /></div>
        <div className="kv" style={{ gap: 4 }}><span className="k rl" />
          <Btn text="Fit" variant="ghost" /><Btn text="Fill" variant="ghost" /><Btn text="Original" variant="ghost" /></div>
        <div className="kv"><span className="k rl">Rotate</span>
          <NumField label="" suffix="°" value={l.rot ?? 0} />
          <Btn icon="flip-horizontal-2" variant="ghost" sel={l.flipH} onClick={() => d({ type: 'setLayer', id: l.id, patch: { flipH: !l.flipH } })} />
          <Btn icon="flip-vertical-2" variant="ghost" sel={l.flipV} onClick={() => d({ type: 'setLayer', id: l.id, patch: { flipV: !l.flipV } })} /></div>
      </Section>

      <Section title={croppable ? 'Crop & corners' : 'Corners'} group="crop" l={l} changed={cropped || radius > 0}>
        {croppable && (
          <>
            <div className="kv"><span className="k rl">Crop</span>
              <Btn icon="crop" text={s.cropMode ? 'Done' : 'Crop'} variant={s.cropMode ? 'primary' : 'secondary'} fill onClick={() => d({ type: 'set', patch: { cropMode: !s.cropMode } })} />
              <Btn icon={cropOpen ? 'chevron-up' : 'chevron-down'} text={!cropped ? 'None' : near(16 / 9) ? '16:9' : near(4 / 3) ? '4:3' : near(1) ? '1:1' : near(9 / 16) ? '9:16' : 'Custom'} variant="ghost" onClick={() => setCropOpen(!cropOpen)} /></div>
            {cropOpen && (
              <>
                <div className="kv" style={{ gap: 4 }}><span className="k rl" />
                  <Btn text="None" variant="ghost" sel={!cropped} />
                  <Btn text="16:9" variant="ghost" sel={cropped && near(16 / 9)} />
                  <Btn text="4:3" variant="ghost" sel={near(4 / 3)} />
                  <Btn text="1:1" variant="ghost" sel={near(1)} />
                  <Btn text="9:16" variant="ghost" sel={near(9 / 16)} /></div>
                <div className="kv"><span className="k rl" />
                  <NumField label="L" value={c.l} /><NumField label="T" value={c.t} /><NumField label="R" value={c.r} /><NumField label="B" value={c.b} /></div>
              </>
            )}
          </>
        )}
        <div className="kv"><span className="k rl">Corners</span>
          <div className="grow"><FillSlider label="" frac={radius / 50} value={radius <= 0.5 ? 'Square' : radius >= 49.5 ? 'Round' : `${Math.round(radius)}%`} /></div></div>
      </Section>

      <Section title="Filters" group="filters" l={l} changed={(l.filters?.length ?? 0) > 0}>
        {(l.filters ?? []).map((f, i) => (
          <div key={i} className="fcard">
            <div className="row" style={{ gap: 4 }}>
              <Switch on={f.enabled} onClick={() => d({ type: 'toggleFilter', index: i })} />
              <span className="grow" style={{ fontWeight: 550, marginLeft: 4 }}>{f.label}</span>
              <Btn icon="trash-2" variant="ghost" />
            </div>
            {f.params.map((p) => {
              if (typeof p.value === 'string') return <Field key={p.key}>{p.value}</Field>
              const [lo, hi, dec] = range(p.key)
              return <FillSlider key={p.key} label={label(p.key)} frac={(p.value - lo) / (hi - lo)} value={p.value.toFixed(dec)} />
            })}
          </div>
        ))}
        <Anchor style={{ display: 'flex' }}>
          <Select placeholder="Add filter…" onClick={() => d({ type: 'menu', menu: { id: 'addFilter' } })} />
          <Menu id="addFilter" matchWidth entries={[{ text: 'Color Correction' }, { text: 'Chroma Key' }, { text: 'Sharpen' }, { text: 'LUT' }]} />
        </Anchor>
      </Section>

      <Section title="Visibility" group="visibility" l={l}>
        <Switch on={l.visible !== false} text="Visible" onClick={() => d({ type: 'toggleVisible', id: l.id })} />
        <FillSlider label="Opacity" frac={l.opacity ?? 1} value={`${Math.round((l.opacity ?? 1) * 100)}%`} />
      </Section>
    </>
  )
}

const TOOL_NAMES = { text: 'Text', rect: 'Rectangle', ellipse: 'Ellipse', arrow: 'Arrow', pencil: 'Pencil', laser: 'Laser pointer' } as const

function ToolDefaults() {
  const { s } = useStore()
  const t = s.tool as Exclude<typeof s.tool, 'select'>
  const usesFill = t === 'rect' || t === 'ellipse' || t === 'laser'
  const lineTool = t === 'arrow' || t === 'pencil'
  return (
    <div style={{ padding: 14, display: 'flex', flexDirection: 'column', gap: 10 }}>
      <span style={{ fontWeight: 550 }}>{TOOL_NAMES[t]}</span>
      <span className="caption" style={{ lineHeight: 1.4 }}>
        {t === 'laser' ? 'A fading trail. Nothing is added to the scene.' : t === 'text' ? 'Drag up or down on the canvas to set the size, then type. Esc or a click elsewhere finishes.' : 'Used for the next one you draw. Select a shape to change its own.'}
      </span>
      {t === 'text' && <div className="row" style={{ gap: 4 }}><Select value="Inter" /><Btn text="B" variant="ghost" sel style={{ fontWeight: 700 }} /><Btn text="I" variant="ghost" style={{ fontStyle: 'italic' }} /></div>}
      {usesFill && <div className="kv"><span className="k">{t === 'laser' ? 'Color' : 'Fill'}</span><Field right={<span className="swatch" style={{ background: '#EF4444', order: -1 }} />}>#EF4444</Field>{t !== 'laser' && <Btn text="None" variant="ghost" />}</div>}
      {t !== 'laser' && <div className="kv"><span className="k">{lineTool || t === 'text' ? 'Color' : 'Stroke'}</span><Field right={<span className="swatch" style={{ background: '#FFFFFF', order: -1 }} />}>#FFFFFF</Field></div>}
      {t === 'laser' && <FillSlider label="Trail length" frac={0.3} value="0.60 s" />}
      {t !== 'laser' && t !== 'text' && <FillSlider label={lineTool ? 'Thickness' : 'Stroke width'} frac={6 / 48} value="6 px" />}
    </div>
  )
}

/** Play, restart, stop and seek for a video or audio file, under the file (MediaControls.qml). */
function MediaControls({ src }: { src: Source }) {
  const m = src.media ?? { playing: false, timeMs: 0, durationMs: 0 }
  const fmt = (ms: number) => {
    const t = Math.max(0, Math.floor(ms / 1000)), h = Math.floor(t / 3600), mm = Math.floor(t / 60) % 60, ss = String(t % 60).padStart(2, '0')
    return h ? `${h}:${String(mm).padStart(2, '0')}:${ss}` : `${mm}:${ss}`
  }
  return (
    <div className="mediabar">
      <Btn icon={m.playing ? 'pause' : 'play'} variant="ghost" />
      <Btn icon="rotate-ccw" variant="ghost" />
      <Btn icon="square" variant="ghost" />
      <div className="grow" style={{ marginLeft: 6, minWidth: 120 }}><FillSlider label="" frac={m.timeMs / Math.max(1, m.durationMs)} value={`${fmt(m.timeMs)} / ${fmt(m.durationMs)}`} /></div>
    </div>
  )
}

function SourceInspector({ id }: { id: string }) {
  const { s, d } = useStore()
  const src = s.sources.find((x) => x.id === id)!
  const used = scenesUsing(s, id)
  const kindLabel = ({ camera: 'Camera', display: 'Display capture', window: 'Window capture', game: 'Game capture', browser: 'Browser', mic: 'Microphone', desktop_audio: 'Desktop audio', image: 'Image', media: 'Media' } as const)[src.kind]
  const video = src.kind !== 'mic' && src.kind !== 'desktop_audio'
  return (
    <div style={{ padding: '4px 12px 12px', display: 'flex', flexDirection: 'column', gap: 8 }}>
      <div className="row" style={{ gap: 8, height: 32 }}><Icon name={iconForSource(src.kind)} size={16} /><span style={{ fontSize: 14, fontWeight: 550 }}>{src.name}</span></div>
      <span className="caption" style={{ lineHeight: 1.4 }}>{kindLabel} · input settings apply to every scene that uses it. Size, crop and filters are set per layer.</span>
      <Select value={src.detail?.split(' · ')[0]} />
      {src.kind === 'camera' && <div className="row" style={{ gap: 6 }}><Select value="1920×1080" /><Select value="30 fps" /></div>}
      {src.kind === 'media' && <MediaControls src={src} />}
      <span style={{ fontSize: 13, fontWeight: 550, marginTop: 8 }}>Used in</span>
      {used.map((u) => <Btn key={u.id} text={u.name} variant="ghost" style={{ justifyContent: 'flex-start' }} onClick={() => d({ type: 'editScene', id: u.id })} />)}
      {used.length === 0 && <span className="caption">{video ? 'Not on any scene yet — drag it onto the canvas.' : 'Audio source — always mixed in.'}</span>}
    </div>
  )
}

export function InspectorPanel({ style }: { style?: React.CSSProperties }) {
  const { s, d } = useStore()
  const l = useSelectedLayer()
  return (
    <Group tabs={['Inspector', 'History']} current={s.rightTab} onTab={(t) => d({ type: 'set', patch: { rightTab: t as 'Inspector' | 'History' } })} flex style={style}>
      {s.rightTab === 'History' ? (
        <div className="list">
          {s.history.map((h, i) => (
            <div key={i} className={`lrow ${i === s.historyIndex ? 'sel' : ''}`} onClick={() => d({ type: 'jumpHistory', index: i })}>
              <span className="name grow ellipsis" style={{ color: i > s.historyIndex ? 'var(--muted)' : undefined }}>{h.label}</span>
              <span className="count">{h.time}</span>
            </div>
          ))}
        </div>
      ) : (
        <div className="insp">
          {l ? <LayerInspector l={l} />
            : s.selectedSourceId ? <SourceInspector id={s.selectedSourceId} />
            : s.selection.length > 1 ? <div style={{ padding: 12, fontSize: 14, fontWeight: 550 }}>{s.selection.length} layers</div>
            : s.tool !== 'select' ? <ToolDefaults />
            : <span className="caption" style={{ padding: 24, textAlign: 'center' }}>Select a layer or source to edit it.</span>}
        </div>
      )}
    </Group>
  )
}
