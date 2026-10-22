import { Anchor, Btn, Icon, Menu, Tabs, Field } from '../primitives'
import { useStore } from '../store'
import { Group } from '../Shell'
import { ScenePicture } from '../ScenePicture'
import { Art } from '../Art'
import { editScene, flatLayers, iconForSource, isVideo, scenesUsing, type Source } from '../model'

export function ScenesPanel({ style }: { style?: React.CSSProperties }) {
  const { s, d } = useStore()
  const live = s.outputs.streaming || s.outputs.recording
  const showBadge = <span className={`badge ${live ? 'live' : ''}`}>{live ? 'On air' : 'Showing'}</span>
  return (
    <Group tabs={['Scenes']} style={style}>
      <div className="phead">
        <div className="lead">
          <Btn icon="list" variant="ghost" sel={s.sceneView === 'list'} onClick={() => d({ type: 'set', patch: { sceneView: 'list' } })} />
          <Btn icon="layout-grid" variant="ghost" sel={s.sceneView === 'grid'} onClick={() => d({ type: 'set', patch: { sceneView: 'grid' } })} />
        </div>
        <span className="grow" />
        <Btn icon="plus" text="Scene" variant="secondary" />
      </div>
      {s.sceneView === 'list' ? (
        <div className="list">
          {s.scenes.map((sc) => (
            <div key={sc.id} className={`lrow ${sc.id === s.editSceneId ? 'sel' : ''}`} data-scene={sc.name}
              onClick={() => d({ type: 'editScene', id: sc.id })}
              onDoubleClick={() => { d({ type: 'editScene', id: sc.id }); d({ type: 'take' }) }}
              onContextMenu={(e) => { e.preventDefault(); d({ type: 'editScene', id: sc.id }); d({ type: 'menu', menu: { id: 'scene', target: sc.id } }) }}>
              <span className="name grow ellipsis">{sc.name}</span>
              {sc.id === s.programSceneId && showBadge}
              <span className="count">{flatLayers(sc.layers).length}</span>
              <Menu id="scene" target={sc.id} offset={[40, -6]} entries={[
                { text: 'Show this scene', onClick: () => d({ type: 'take' }) },
                { text: 'Rename…' },
                { text: 'Duplicate with linked layers' },
                { text: 'Duplicate' },
                { sep: true },
                { text: 'Delete scene' },
              ]} />
            </div>
          ))}
        </div>
      ) : (
        <div className="list"><div className="grid">
          {s.scenes.map((sc) => (
            <div key={sc.id} className={`tile ${sc.id === s.programSceneId ? 'prog' : sc.id === s.editSceneId ? 'edit' : ''}`} data-scene={sc.name}
              onClick={() => d({ type: 'editScene', id: sc.id })} onDoubleClick={() => d({ type: 'take' })}>
              <div className="thumb"><ScenePicture scene={sc} sources={s.sources} width={(s.layout.left - 46) / 2} uid={`grid-${sc.id}`} /></div>
              <div className="lbl"><span className="grow ellipsis" style={{ fontWeight: sc.id === s.editSceneId ? 550 : 400 }}>{sc.name}</span>{sc.id === s.programSceneId && showBadge}</div>
            </div>
          ))}
        </div></div>
      )}
    </Group>
  )
}

function SourceRow({ src }: { src: Source }) {
  const { s, d } = useStore()
  const scene = editScene(s)
  const inScene = flatLayers(scene.layers).some(({ layer }) => layer.sourceId === src.id)
  const video = isVideo(src)
  const hov = s.hover?.source === src.id
  const used = scenesUsing(s, src.id).map((x) => x.name)
  return (
    <div className={`srow ${s.selectedSourceId === src.id ? 'sel' : ''} ${hov ? 'hov' : ''}`} data-source={src.name}
      onClick={() => d({ type: 'selectSource', id: src.id })}
      onDoubleClick={() => video && !inScene && d({ type: 'toggleInScene', id: src.id })}
      onContextMenu={(e) => { e.preventDefault(); d({ type: 'selectSource', id: src.id }); d({ type: 'menu', menu: { id: 'pool', target: src.id } }) }}>
      <div className={`thumb ${src.problem ? 'problem' : ''}`}>
        {video && src.art && !src.problem ? <div style={{ position: 'absolute', inset: 1, borderRadius: 3, overflow: 'hidden' }}><Art kind={src.art} uid={`pool-${src.id}`} /></div>
          : <Icon name={src.problem ? 'triangle-alert' : iconForSource(src.kind)} color={src.problem ? '#fbbf24' : 'var(--muted)'} style={{ marginTop: video ? 0 : -8 }} />}
        {!video && (
          <div className="meter2">
            {(src.level ?? [-60, -60]).map((db, i) => (
              <div key={i}><div style={{ width: `${Math.max(0, (db + 60) / 60) * 100}%`, background: src.muted ? 'var(--muted)' : db > -6 ? '#ef4444' : db > -18 ? '#f59e0b' : '#22c55e' }} /></div>
            ))}
          </div>
        )}
      </div>
      <div className="grow" style={{ display: 'flex', flexDirection: 'column', gap: 1 }}>
        <span className="name ellipsis" style={{ color: src.muted ? 'var(--muted)' : undefined }}>{src.name}</span>
        {src.problem ? <span><span className="badge problem">{src.problem}</span></span> : <span className="caption ellipsis">{src.detail}</span>}
      </div>
      {!video && <Btn icon={src.muted ? 'volume-x' : 'volume-2'} variant="ghost" iconColor={src.muted ? 'var(--danger)' : undefined} />}
      {video && (
        <button className={`intoggle ${inScene ? '' : 'add'}`} onClick={(e) => { e.stopPropagation(); d({ type: 'toggleInScene', id: src.id }) }}>
          {inScene ? <span className="gdot" /> : <Icon name="plus" size={12} color="var(--fg)" />}
          {inScene ? 'In scene' : hov ? 'Add to scene' : ''}
        </button>
      )}
      <Anchor>
        <Btn icon="ellipsis" variant="ghost" className="more" onClick={() => d({ type: 'menu', menu: { id: 'pool', target: src.id } })} />
        <Menu id="pool" target={src.id} align="right" entries={[
          ...(video ? [{ text: inScene ? `Remove from ${scene.name}` : `Add to ${scene.name}`, onClick: () => d({ type: 'toggleInScene', id: src.id }) }] : []),
          { text: 'Settings…', onClick: () => d({ type: 'selectSource', id: src.id }) },
          { text: 'Rename…' },
          { sep: true },
          { text: !video ? 'Plays in every scene' : used.length ? `Used in ${used.join(', ')}` : 'Not used in any scene yet', disabled: true },
          { sep: true },
          { text: 'Delete source' },
        ]} />
      </Anchor>
    </div>
  )
}

function SourcesBody() {
  const { s, d } = useStore()
  const q = (s.poolSearch ?? '').toLowerCase()
  const rows = s.sources.filter((x) => !q || x.name.toLowerCase().includes(q) || (x.detail ?? '').toLowerCase().includes(q))
  const video = rows.filter(isVideo), audio = rows.filter((x) => !isVideo(x))
  const shown = s.poolFilter === 'video' ? video : s.poolFilter === 'audio' ? audio : null
  const filters = ['all', 'video', 'audio'] as const
  return (
    <>
      <div className="phead">
        <div className="lead" style={{ marginLeft: 0 }}>
          <Tabs items={['All', 'Video', 'Audio']} current={filters.indexOf(s.poolFilter)} onPick={(i) => d({ type: 'set', patch: { poolFilter: filters[i] } })} />
        </div>
        <span className="grow" />
        <Btn icon="search" variant="ghost" sel={s.poolSearch !== null} onClick={() => d({ type: 'set', patch: { poolSearch: s.poolSearch === null ? '' : null } })} />
        <Btn icon="plus" text="Source" variant="secondary" onClick={() => d({ type: 'set', patch: { dialog: 'addSource' } })} />
      </div>
      {s.poolSearch !== null && (
        <div style={{ padding: '0 10px 4px' }}>
          <Field focus placeholder="Search by name or device">{s.poolSearch || undefined}</Field>
        </div>
      )}
      <div className="list" style={{ paddingTop: 4 }}>
        {shown ? shown.map((x) => <SourceRow key={x.id} src={x} />) : (
          <>
            {video.map((x) => <SourceRow key={x.id} src={x} />)}
            {video.length > 0 && audio.length > 0 && <div className="divider" />}
            {audio.map((x) => <SourceRow key={x.id} src={x} />)}
          </>
        )}
        {rows.length === 0 && <div className="emptycap">{q ? `No sources match “${s.poolSearch}”.` : 'Add each camera, capture or mic once,\nthen drag it onto any scene.'}</div>}
      </div>
    </>
  )
}

function MixerBody() {
  const { s } = useStore()
  const scene = editScene(s)
  const audio = s.sources.filter((x) => !isVideo(x))
  return (
    <>
      <div className="phead">
        <span className="meta" style={{ marginLeft: 0 }}>{audio.length === 1 ? '1 input' : `${audio.length} inputs`}</span>
        <span className="grow" />
        <Btn icon="search" variant="ghost" />
        {/* Shows the layout in use (rows here) and flips to the other. */}
        <Btn icon="rows-3" variant="ghost" />
      </div>
      <div className="mixer">
        {audio.map((a) => {
          const [l, r] = a.level ?? [-60, -60]
          return (
            <div key={a.id} className="mixrow">
              <div className="row" style={{ gap: 6 }}>
                <Icon name={iconForSource(a.kind)} color="var(--muted)" />
                <span className="grow ellipsis" style={{ fontWeight: 550, color: a.muted ? 'var(--muted)' : undefined, cursor: 'grab' }}>{a.name}</span>
                {a.pinned && <Icon name="pin" size={12} color="var(--muted)" />}
                <span className="caption" style={{ width: 46, textAlign: 'right' }}>0.0 dB</span>
                <Btn icon={a.muted ? 'volume-x' : 'volume-2'} variant="ghost" iconColor={a.muted ? 'var(--danger)' : undefined} />
                <Btn icon="headphone-off" variant="ghost" />
                <Btn icon="ellipsis" variant="ghost" />
              </div>
              {[l, r].map((db, i) => <div key={i} className="mixmeter"><div style={{ width: `${Math.max(0, (db + 60) / 60) * 100}%`, backgroundSize: `${100 / Math.max(0.01, (db + 60) / 60)}% 100%`, opacity: a.muted ? 0.3 : 1 }} /></div>)}
            </div>
          )
        })}
        <span className="caption">Global audio plays in every scene. {scene.name} adds no scene audio.</span>
      </div>
    </>
  )
}

export function SourcesPanel({ style }: { style?: React.CSSProperties }) {
  const { s, d } = useStore()
  return (
    <Group tabs={['Sources', 'Audio Mixer']} current={s.leftTab} onTab={(t) => d({ type: 'set', patch: { leftTab: t as 'Sources' | 'Audio Mixer' } })} style={style}>
      {s.leftTab === 'Sources' ? <SourcesBody /> : <MixerBody />}
    </Group>
  )
}

export function AssetsPanel({ style }: { style?: React.CSSProperties }) {
  const { s } = useStore()
  return (
    <Group tabs={['Assets']} flex style={style}>
      <div className="phead">
        <span className="grow" />
        <Btn text="Name ↑" variant="ghost" />
        <Btn icon="layout-grid" variant="ghost" sel />
        <Btn icon="list" variant="ghost" />
        <span style={{ width: 8 }} />
        <Btn icon="search" variant="ghost" />
        <Btn icon="ellipsis" variant="ghost" />
        <Btn icon="plus" text="Add" variant="secondary" />
      </div>
      <div className="assetgrid">
        {s.assets.map((a) => a.kind === 'folder'
          ? <div key={a.name} className="asset"><Icon name="folder" color="#e9b949" /><span className="ellipsis">{a.name}</span></div>
          : <div key={a.name} className="asset"><Icon name={a.kind === 'image' ? 'image' : a.kind === 'video' ? 'film' : 'music'} color="var(--muted)" /><span className="ellipsis">{a.name}</span></div>)}
      </div>
      <div className="dropzone">Drop files here, or click to add them</div>
    </Group>
  )
}
