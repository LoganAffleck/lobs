import { useEffect, useState } from 'react';
import { FILTER_INFO, getProp, groupBounds, instancesOfSource, KIND_INFO, linkedPeers, linkTo, moveBy, newFilter, setProp, ungroup, unlink } from '../model/doc';
import { commit, endGesture, gesture, setUi, useStore } from '../model/store';
import { Filter, FilterType, Instance, LobsDoc, PropGroup, PropMap, Source } from '../model/types';
import { DEVICE_PROP, listItems, useBridge } from '../obs/bridge';
import { NumField } from './common';

const GROUP_LABEL: Record<PropGroup, string> = { transform: 'Transform', crop: 'Crop', filters: 'Filters', visibility: 'Visibility' };

export function Inspector() {
  const doc = useStore((s) => s.doc);
  const ui = useStore((s) => s.ui);
  const sel = ui.selection.filter((id) => doc.instances[id]);
  if (sel.length === 1) return <InstanceInspector key={sel[0]} inst={doc.instances[sel[0]]} doc={doc} />;
  if (sel.length > 1) return <MultiInspector ids={sel} doc={doc} />;
  if (ui.poolSelection && doc.sources[ui.poolSelection]) return <SourceInspector source={doc.sources[ui.poolSelection]} doc={doc} />;
  return (
    <div className="empty">
      Select a layer to edit it for this scene,
      <br />or a Media Pool source to change its input.
    </div>
  );
}

// ---------- sliders that preview live and land as one undo step ----------

function Slider({ label, value, min, max, step = 0.01, fmt, onChange, undoLabel }: { label: string; value: number; min: number; max: number; step?: number; fmt?: (v: number) => string; onChange: (d: LobsDoc, v: number) => void; undoLabel: string }) {
  return (
    <div className="slider-row">
      <span>{label}</span>
      <input
        type="range"
        min={min}
        max={max}
        step={step}
        value={value}
        onChange={(e) => gesture((d) => onChange(d, Number(e.target.value)))}
        onPointerUp={() => endGesture(undoLabel)}
        onKeyUp={() => endGesture(undoLabel)}
        onBlur={() => endGesture(undoLabel)}
      />
      <span>{fmt ? fmt(value) : Math.round(value * 100) / 100}</span>
    </div>
  );
}

// ---------- link state for one property group ----------

function LinkBar({ doc, inst, group }: { doc: LobsDoc; inst: Instance; group: PropGroup }) {
  const peers = linkedPeers(doc, inst, group);
  const [pick, setPick] = useState('');
  const candidates = inst.kind === 'source' ? instancesOfSource(doc, inst.sourceId!).filter((i) => i.id !== inst.id && !peers.includes(i)) : [];
  const where = (i: Instance) => `${doc.scenes[i.sceneId]?.name ?? '?'}${i.name !== inst.name ? ' · ' + i.name : ''}`;

  if (peers.length)
    return (
      <div className="link-bar">
        <span className="badge link">🔗 Linked</span>
        <span title={peers.map(where).join(', ')}>
          with {peers.length === 1 ? where(peers[0]) : `${peers.length} others`}
        </span>
        <span className="grow" />
        <button className="ghost small" onClick={() => commit(`Unlink ${GROUP_LABEL[group]}`, (d) => unlink(d, inst.id, group))}>Unlink</button>
      </div>
    );
  if (!candidates.length) return <div className="link-bar"><span>Local to this scene</span></div>;
  return (
    <div className="link-bar">
      <span>Local</span>
      <span className="grow" />
      {!pick ? (
        <select value="" onChange={(e) => setPick(e.target.value)} style={{ fontSize: 11 }}>
          <option value="">Link to…</option>
          {candidates.map((c) => <option key={c.id} value={c.id}>{where(c)}</option>)}
        </select>
      ) : (
        <>
          <span>Which wins?</span>
          <button className="small" onClick={() => (commit(`Link ${GROUP_LABEL[group]}`, (d) => linkTo(d, inst.id, pick, group, 'mine')), setPick(''))}>This one</button>
          <button className="small" onClick={() => (commit(`Link ${GROUP_LABEL[group]}`, (d) => linkTo(d, inst.id, pick, group, 'theirs')), setPick(''))}>Theirs</button>
          <button className="ghost small" onClick={() => setPick('')}>✕</button>
        </>
      )}
    </div>
  );
}

function Section({ doc, inst, group, children, extra }: { doc: LobsDoc; inst: Instance; group: PropGroup; children: React.ReactNode; extra?: React.ReactNode }) {
  return (
    <div className="insp-sec">
      <h4>{GROUP_LABEL[group]}<span className="grow" />{extra}</h4>
      <LinkBar doc={doc} inst={inst} group={group} />
      {children}
    </div>
  );
}

// ---------- instance ----------

function InstanceInspector({ inst, doc }: { inst: Instance; doc: LobsDoc }) {
  const sizes = useBridge((b) => b.sizes);
  if (inst.kind === 'group') return <GroupInspector inst={inst} doc={doc} />;
  const src = doc.sources[inst.sourceId!];
  const t = getProp(doc, inst, 'transform');
  const c = getProp(doc, inst, 'crop');
  const v = getProp(doc, inst, 'visibility');
  const filters = getProp(doc, inst, 'filters');
  const uses = instancesOfSource(doc, src.id);
  const set = <G extends PropGroup>(label: string, g: G, value: PropMap[G]) => commit(label, (d) => setProp(d, inst.id, g, value));
  const { w: W, h: H } = doc.canvas;
  const [sw, sh] = sizes[src.name] ?? [t.w, t.h];
  const aspect = Math.max(1, sw - c.l - c.r) / Math.max(1, sh - c.t - c.b);

  const fit = (mode: 'fit' | 'fill') => {
    const k = mode === 'fit' ? Math.min(W / aspect, H) : Math.max(W / aspect, H);
    const w = Math.round(k * aspect), h = Math.round(k);
    set(mode === 'fit' ? 'Fit to canvas' : 'Fill canvas', 'transform', { ...t, x: Math.round((W - w) / 2), y: Math.round((H - h) / 2), w, h, rot: 0 });
  };

  return (
    <div>
      <div className="insp-sec">
        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
          <span className="tag" style={{ background: src.color, height: 28 }} />
          <div style={{ flex: 1, minWidth: 0 }}>
            <div style={{ fontWeight: 600 }}>{inst.name}</div>
            <div className="muted small">
              {KIND_INFO[src.kind].icon} {src.name} · used {uses.length}× in {new Set(uses.map((u) => u.sceneId)).size} scene(s)
            </div>
          </div>
          <button className="small" onClick={() => setUi({ poolSelection: src.id, selection: [] })} title="Edit the input (applies everywhere)">Source…</button>
        </div>
        <div className="muted small" style={{ marginTop: 8 }}>Changes below apply only to this scene unless a section is 🔗 linked.</div>
      </div>

      <Section doc={doc} inst={inst} group="transform">
        <div className="fields">
          <NumField label="X" value={t.x} onCommit={(x) => set('Set X', 'transform', { ...t, x })} />
          <NumField label="Y" value={t.y} onCommit={(y) => set('Set Y', 'transform', { ...t, y })} />
          <NumField label="W" value={t.w} onCommit={(w) => set('Set width', 'transform', { ...t, w: Math.max(1, w) })} />
          <NumField label="H" value={t.h} onCommit={(h) => set('Set height', 'transform', { ...t, h: Math.max(1, h) })} />
        </div>
        <div className="fields" style={{ marginTop: 6 }}>
          <NumField label="↻" value={t.rot} onCommit={(rot) => set('Rotate', 'transform', { ...t, rot })} />
        </div>
        <div className="btn-row" style={{ marginTop: 8 }}>
          <button className="small" onClick={() => fit('fit')}>Fit</button>
          <button className="small" onClick={() => fit('fill')}>Fill</button>
          <button className="small" onClick={() => set('Center', 'transform', { ...t, x: Math.round((W - t.w) / 2), y: Math.round((H - t.h) / 2) })}>Center</button>
          <button className="small" onClick={() => set('Match source aspect', 'transform', { ...t, h: Math.round(t.w / aspect) })}>Fix aspect</button>
        </div>
      </Section>

      <Section doc={doc} inst={inst} group="crop" extra={<button className="ghost small" onClick={() => set('Reset crop', 'crop', { l: 0, t: 0, r: 0, b: 0 })}>Reset</button>}>
        <div className="fields">
          <NumField label="L" value={c.l} onCommit={(l) => set('Crop', 'crop', { ...c, l: Math.max(0, l) })} />
          <NumField label="T" value={c.t} onCommit={(tt) => set('Crop', 'crop', { ...c, t: Math.max(0, tt) })} />
          <NumField label="R" value={c.r} onCommit={(r) => set('Crop', 'crop', { ...c, r: Math.max(0, r) })} />
          <NumField label="B" value={c.b} onCommit={(b) => set('Crop', 'crop', { ...c, b: Math.max(0, b) })} />
        </div>
        <div className="muted small" style={{ marginTop: 4 }}>In source pixels{sizes[src.name] ? ` (source is ${sw}×${sh})` : ''}.</div>
      </Section>

      <Section doc={doc} inst={inst} group="filters">
        <FilterStack inst={inst} filters={filters} />
      </Section>

      <Section doc={doc} inst={inst} group="visibility">
        <label style={{ display: 'flex', gap: 6, alignItems: 'center', marginBottom: 4 }}>
          <input type="checkbox" checked={v.visible} onChange={() => set(v.visible ? 'Hide layer' : 'Show layer', 'visibility', { ...v, visible: !v.visible })} /> Visible
        </label>
        <Slider label="Opacity" value={v.opacity} min={0} max={1} fmt={(x) => Math.round(x * 100) + '%'} undoLabel="Opacity" onChange={(d, opacity) => setProp(d, inst.id, 'visibility', { ...v, opacity })} />
      </Section>
    </div>
  );
}

function FilterStack({ inst, filters }: { inst: Instance; filters: Filter[] }) {
  const put = (label: string, next: Filter[]) => commit(label, (d) => setProp(d, inst.id, 'filters', next));
  const live = (d: LobsDoc, i: number, key: string, val: number | string) =>
    setProp(d, inst.id, 'filters', filters.map((f, j) => (j === i ? { ...f, settings: { ...f.settings, [key]: val } } : f)));
  return (
    <div>
      {!filters.length && <div className="muted small" style={{ marginBottom: 6 }}>No filters on this instance.</div>}
      {filters.map((f, i) => (
        <div className="filter-card" key={f.id}>
          <div className="fh">
            <input type="checkbox" checked={f.enabled} onChange={() => put('Toggle filter', filters.map((x, j) => (j === i ? { ...x, enabled: !x.enabled } : x)))} />
            <b style={{ flex: 1 }}>{FILTER_INFO[f.type].label}</b>
            <button className="ghost small" disabled={i === 0} onClick={() => { const n = [...filters]; [n[i - 1], n[i]] = [n[i], n[i - 1]]; put('Reorder filter', n); }}>↑</button>
            <button className="ghost small" disabled={i === filters.length - 1} onClick={() => { const n = [...filters]; [n[i + 1], n[i]] = [n[i], n[i + 1]]; put('Reorder filter', n); }}>↓</button>
            <button className="ghost small" onClick={() => put('Remove filter', filters.filter((_, j) => j !== i))}>✕</button>
          </div>
          <FilterControls f={f} onLive={(k, v) => (d: LobsDoc) => live(d, i, k, v)} onCommit={(k, v) => put('Edit filter', filters.map((x, j) => (j === i ? { ...x, settings: { ...x.settings, [k]: v } } : x)))} />
        </div>
      ))}
      <select value="" onChange={(e) => e.target.value && put('Add filter', [...filters, newFilter(e.target.value as FilterType)])}>
        <option value="">＋ Add filter…</option>
        {(Object.keys(FILTER_INFO) as FilterType[]).map((k) => <option key={k} value={k}>{FILTER_INFO[k].label}</option>)}
      </select>
    </div>
  );
}

function FilterControls({ f, onLive, onCommit }: { f: Filter; onLive: (k: string, v: number) => (d: LobsDoc) => void; onCommit: (k: string, v: string | number) => void }) {
  const s = f.settings as Record<string, any>;
  const sl = (key: string, label: string, min: number, max: number, step = 0.01) => (
    <Slider key={key} label={label} value={Number(s[key])} min={min} max={max} step={step} undoLabel={`Filter ${label}`} onChange={(d, v) => onLive(key, v)(d)} />
  );
  switch (f.type) {
    case 'color':
      return <>{sl('brightness', 'Brightness', -1, 1)}{sl('contrast', 'Contrast', -1, 1)}{sl('saturation', 'Saturation', -1, 1)}{sl('gamma', 'Gamma', -1, 1)}{sl('hue', 'Hue', -180, 180, 1)}</>;
    case 'chroma_key':
      return (
        <>
          <div className="slider-row"><span>Key color</span><input type="color" value={String(s.key_color)} onChange={(e) => onCommit('key_color', e.target.value)} /><span /></div>
          {sl('similarity', 'Similarity', 1, 1000, 1)}
          {sl('smoothness', 'Smoothness', 1, 1000, 1)}
        </>
      );
    case 'sharpness':
      return sl('amount', 'Amount', 0, 1);
    case 'blur':
      return <>{sl('size', 'Size', 0, 50, 1)}<div className="muted small">Needs a blur plugin in OBS; preview only for now.</div></>;
    case 'lut':
      return (
        <>
          <div className="slider-row"><span>LUT file</span><input defaultValue={String(s.path)} placeholder="C:\\path\\look.cube" onBlur={(e) => onCommit('path', e.target.value)} /><span /></div>
          {sl('amount', 'Amount', 0, 1)}
        </>
      );
  }
}

// ---------- group ----------

function GroupInspector({ inst, doc }: { inst: Instance; doc: LobsDoc }) {
  const v = getProp(doc, inst, 'visibility');
  const b = groupBounds(doc, inst.id);
  return (
    <div>
      <div className="insp-sec">
        <div style={{ fontWeight: 600 }}>📂 {inst.name}</div>
        <div className="muted small">Group · {inst.children!.length} layer(s)</div>
        <div className="btn-row" style={{ marginTop: 8 }}>
          <button className="small" onClick={() => commit('Ungroup', (d) => ungroup(d, inst.id))}>Ungroup</button>
          <button className="small" onClick={() => setUi({ copyDialog: { ids: [inst.id] } })}>Copy to scenes…</button>
        </div>
      </div>
      <div className="insp-sec">
        <h4>Position</h4>
        <div className="fields two">
          <NumField label="X" value={b.x} onCommit={(x) => commit('Move group', (d) => moveBy(d, inst.id, x - b.x, 0))} />
          <NumField label="Y" value={b.y} onCommit={(y) => commit('Move group', (d) => moveBy(d, inst.id, 0, y - b.y))} />
        </div>
        <div className="muted small" style={{ marginTop: 4 }}>{Math.round(b.w)}×{Math.round(b.h)} bounds</div>
      </div>
      <Section doc={doc} inst={inst} group="visibility">
        <label style={{ display: 'flex', gap: 6, alignItems: 'center', marginBottom: 4 }}>
          <input type="checkbox" checked={v.visible} onChange={() => commit('Toggle group', (d) => setProp(d, inst.id, 'visibility', { ...v, visible: !v.visible }))} /> Visible
        </label>
        <Slider label="Opacity" value={v.opacity} min={0} max={1} fmt={(x) => Math.round(x * 100) + '%'} undoLabel="Group opacity" onChange={(d, opacity) => setProp(d, inst.id, 'visibility', { ...v, opacity })} />
      </Section>
    </div>
  );
}

// ---------- multi ----------

function MultiInspector({ ids, doc }: { ids: string[]; doc: LobsDoc }) {
  const leaves = ids.map((id) => doc.instances[id]);
  const bounds = leaves.map((i) => (i.kind === 'group' ? groupBounds(doc, i.id) : getProp(doc, i, 'transform')));
  const align = (mode: 'l' | 'c' | 'r' | 't' | 'm' | 'b') => {
    const x0 = Math.min(...bounds.map((b) => b.x)), x1 = Math.max(...bounds.map((b) => b.x + b.w));
    const y0 = Math.min(...bounds.map((b) => b.y)), y1 = Math.max(...bounds.map((b) => b.y + b.h));
    commit('Align layers', (d) => {
      leaves.forEach((i, k) => {
        const b = bounds[k];
        const dx = mode === 'l' ? x0 - b.x : mode === 'r' ? x1 - (b.x + b.w) : mode === 'c' ? (x0 + x1) / 2 - (b.x + b.w / 2) : 0;
        const dy = mode === 't' ? y0 - b.y : mode === 'b' ? y1 - (b.y + b.h) : mode === 'm' ? (y0 + y1) / 2 - (b.y + b.h / 2) : 0;
        moveBy(d, i.id, dx, dy);
      });
    });
  };
  const distribute = (axis: 'x' | 'y') => {
    const order = leaves.map((i, k) => ({ i, b: bounds[k] })).sort((a, b) => a.b[axis] - b.b[axis]);
    if (order.length < 3) return;
    const size = axis === 'x' ? 'w' : 'h';
    const total = order.reduce((s, o) => s + o.b[size], 0);
    const span = order[order.length - 1].b[axis] + order[order.length - 1].b[size] - order[0].b[axis];
    const gap = (span - total) / (order.length - 1);
    commit('Distribute layers', (d) => {
      let at = order[0].b[axis];
      for (const o of order) {
        moveBy(d, o.i.id, axis === 'x' ? at - o.b.x : 0, axis === 'y' ? at - o.b.y : 0);
        at += o.b[size] + gap;
      }
    });
  };
  const opacity = getProp(doc, leaves[0], 'visibility').opacity;
  return (
    <div>
      <div className="insp-sec"><b>{ids.length} layers selected</b></div>
      <div className="insp-sec">
        <h4>Align</h4>
        <div className="btn-row">
          <button className="small" onClick={() => align('l')}>⇤ Left</button>
          <button className="small" onClick={() => align('c')}>↔ Center</button>
          <button className="small" onClick={() => align('r')}>⇥ Right</button>
          <button className="small" onClick={() => align('t')}>⤒ Top</button>
          <button className="small" onClick={() => align('m')}>↕ Middle</button>
          <button className="small" onClick={() => align('b')}>⤓ Bottom</button>
        </div>
        <h4 style={{ marginTop: 10 }}>Distribute</h4>
        <div className="btn-row">
          <button className="small" disabled={ids.length < 3} onClick={() => distribute('x')}>Horizontally</button>
          <button className="small" disabled={ids.length < 3} onClick={() => distribute('y')}>Vertically</button>
        </div>
      </div>
      <div className="insp-sec">
        <h4>Visibility (all)</h4>
        <div className="btn-row" style={{ marginBottom: 6 }}>
          <button className="small" onClick={() => commit('Show layers', (d) => leaves.forEach((i) => setProp(d, i.id, 'visibility', { ...getProp(d, d.instances[i.id], 'visibility'), visible: true })))}>Show all</button>
          <button className="small" onClick={() => commit('Hide layers', (d) => leaves.forEach((i) => setProp(d, i.id, 'visibility', { ...getProp(d, d.instances[i.id], 'visibility'), visible: false })))}>Hide all</button>
        </div>
        <Slider label="Opacity" value={opacity} min={0} max={1} fmt={(x) => Math.round(x * 100) + '%'} undoLabel={`Opacity (${ids.length} layers)`} onChange={(d, o) => leaves.forEach((i) => setProp(d, i.id, 'visibility', { ...getProp(d, d.instances[i.id], 'visibility'), opacity: o }))} />
      </div>
      <div className="insp-sec">
        <button className="small" onClick={() => setUi({ copyDialog: { ids } })}>Copy to scenes…</button>
      </div>
    </div>
  );
}

// ---------- pool source ----------

function SourceInspector({ source, doc }: { source: Source; doc: LobsDoc }) {
  const status = useBridge((b) => b.status);
  const thumb = useBridge((b) => b.thumbs[source.name]);
  const [devices, setDevices] = useState<{ name: string; value: string }[] | null>(null);
  const st = source.settings as Record<string, any>;
  const uses = instancesOfSource(doc, source.id);
  const info = KIND_INFO[source.kind];
  const setS = (key: string, val: unknown) => commit(`Edit ${source.name}`, (d) => void (d.sources[source.id].settings = { ...d.sources[source.id].settings, [key]: val }));
  useEffect(() => setDevices(null), [source.id, status]);

  const text = (key: string, label: string, ph = '') => (
    <><label>{label}</label><input key={source.id + key} defaultValue={String(st[key] ?? '')} placeholder={ph} onBlur={(e) => e.target.value !== String(st[key] ?? '') && setS(key, e.target.value)} onKeyDown={(e) => e.key === 'Enter' && (e.target as HTMLInputElement).blur()} /></>
  );

  return (
    <div>
      <div className="insp-sec">
        <div style={{ display: 'flex', gap: 8, alignItems: 'center' }}>
          <span style={{ fontSize: 20 }}>{info.icon}</span>
          <div style={{ flex: 1 }}>
            <div style={{ fontWeight: 600 }}>{source.name}</div>
            <div className="muted small">{info.label} · Media Pool source</div>
          </div>
        </div>
        {thumb && info.video && <img src={thumb} style={{ width: '100%', marginTop: 8, borderRadius: 4 }} />}
        <div className="muted small" style={{ marginTop: 8 }}>Input settings apply to every scene that uses this source. Size, crop and filters are set per scene on each layer.</div>
      </div>
      <div className="insp-sec">
        <h4>Input</h4>
        <div className="kv">
          <label>Name</label>
          <input key={source.id + 'name'} defaultValue={source.name} onBlur={(e) => e.target.value.trim() && e.target.value !== source.name && commit('Rename source', (d) => void (d.sources[source.id].name = e.target.value.trim()))} />
          <label>Bin</label>
          <select value={source.bin} onChange={(e) => commit('Move source to bin', (d) => void (d.sources[source.id].bin = e.target.value))}>
            <option value="">(root)</option>
            {doc.bins.map((b) => <option key={b}>{b}</option>)}
          </select>
          <label>Tag</label>
          <input type="color" value={source.color} onChange={(e) => commit('Tag color', (d) => void (d.sources[source.id].color = e.target.value))} />
          {source.kind === 'text' && <>{text('text', 'Text')}{text('size', 'Font size')}<label>Color</label><input type="color" value={String(st.textColor ?? '#ffffff')} onChange={(e) => setS('textColor', e.target.value)} /></>}
          {source.kind === 'color' && <><label>Color</label><input type="color" value={String(st.color ?? '#000000')} onChange={(e) => setS('color', e.target.value)} /></>}
          {source.kind === 'browser' && <>{text('url', 'URL', 'https://')}{text('width', 'Width', '800')}{text('height', 'Height', '600')}</>}
          {source.kind === 'image' && text('file', 'File', 'C:\\path\\image.png')}
          {source.kind === 'media' && <>{text('file', 'File', 'C:\\path\\video.mp4')}<label>Loop</label><input type="checkbox" checked={!!st.loop} onChange={() => setS('loop', !st.loop)} /></>}
          {DEVICE_PROP[source.kind] && (
            <>
              <label>Device</label>
              {status !== 'connected' ? (
                <span className="muted small">Connect to OBS to pick a device</span>
              ) : devices ? (
                <select value={String(st.device ?? '')} onChange={(e) => setS('device', e.target.value)}>
                  <option value="">(default)</option>
                  {devices.map((d) => <option key={d.value} value={d.value}>{d.name}</option>)}
                </select>
              ) : (
                <button className="small" onClick={() => void listItems(source).then(setDevices)}>Load devices</button>
              )}
            </>
          )}
        </div>
      </div>
      <div className="insp-sec">
        <h4>Used in</h4>
        {!info.video ? (
          <div className="muted small">Audio source — mix it in the <a href="#" onClick={(e) => (e.preventDefault(), setUi({ rightTab: 'audio' }))}>Audio</a> tab.</div>
        ) : uses.length ? (
          uses.map((u) => (
            <div key={u.id} className="row" onClick={() => setUi({ sceneId: u.sceneId, selection: [u.id], poolSelection: null })}>
              <span className="name">{doc.scenes[u.sceneId]?.name}</span>
              {Object.keys(u.links).length > 0 && <span className="badge link">🔗 {Object.keys(u.links).length}</span>}
            </div>
          ))
        ) : (
          <div className="muted small">Not on any scene yet. Drag it onto the canvas.</div>
        )}
      </div>
    </div>
  );
}
