import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { descendants, flatten, getProp, groupBounds, KIND_INFO, moveBy, parentOf, placeSource, setProp } from '../model/doc';
import { commit, endGesture, gesture, getState, liveLocked, setUi, toast, useStore } from '../model/store';
import { Filter, Instance, LobsDoc, Transform } from '../model/types';
import { take, useBridge } from '../obs/bridge';
import { DRAG_SOURCE } from './MediaPool';

type Guide = { axis: 'x' | 'y'; at: number };

const pathTo = (doc: LobsDoc, id: string) => {
  const path = [id];
  for (let p = parentOf(doc, id); p; p = parentOf(doc, p.id)) path.unshift(p.id);
  return path;
};

const boundsOf = (doc: LobsDoc, id: string): Transform => {
  const i = doc.instances[id];
  return i.kind === 'group' ? groupBounds(doc, id) : getProp(doc, i, 'transform');
};

export function Canvas() {
  const doc = useStore((s) => s.doc);
  const ui = useStore((s) => s.ui);
  const thumbs = useBridge((b) => b.thumbs);
  const sizes = useBridge((b) => b.sizes);
  const wrap = useRef<HTMLDivElement>(null);
  const [box, setBox] = useState({ w: 800, h: 450 });
  const [guides, setGuides] = useState<Guide[]>([]);
  const [dropping, setDropping] = useState(false);
  const [safe, setSafe] = useState(false);
  const { w: W, h: H } = doc.canvas;
  const scene = ui.sceneId ? doc.scenes[ui.sceneId] : null;

  useLayoutEffect(() => {
    const el = wrap.current!;
    const ro = new ResizeObserver(() => setBox({ w: el.clientWidth, h: el.clientHeight }));
    ro.observe(el);
    return () => ro.disconnect();
  }, []);
  const scale = Math.min((box.w - 40) / W, (box.h - 40) / H);
  const left = (box.w - W * scale) / 2;
  const top = (box.h - H * scale) / 2;

  const toDoc = (e: { clientX: number; clientY: number }) => {
    const r = wrap.current!.getBoundingClientRect();
    return { x: (e.clientX - r.left - left) / scale, y: (e.clientY - r.top - top) / scale };
  };

  // ---------- move ----------
  const startMove = (e: React.MouseEvent, leafId: string) => {
    e.stopPropagation();
    if (e.button !== 0) return;
    const st = getState();
    const path = pathTo(st.doc, leafId);
    let target = path.find((id) => st.ui.selection.includes(id)) ?? path[0];
    let sel = st.ui.selection;
    if (e.shiftKey) sel = sel.includes(target) ? sel.filter((s) => s !== target) : [...sel, target];
    else if (!sel.includes(target)) sel = [target];
    setUi({ selection: sel, poolSelection: null });
    if (e.shiftKey) return;

    const movable = sel.filter((id) => !descendants(st.doc, id).some((d) => d.locked) && !pathTo(st.doc, id).some((p) => st.doc.instances[p].locked));
    if (!movable.length) return toast('Layer is locked');
    if (liveLocked(e.altKey)) return toast('Live lock is on — hold Alt to move layers while live');

    const base = st.doc;
    const start = toDoc(e);
    const moving = new Set(movable.flatMap((id) => descendants(base, id).map((d) => d.id)));
    const bs = movable.map((id) => boundsOf(base, id));
    const ub = { x: Math.min(...bs.map((b) => b.x)), y: Math.min(...bs.map((b) => b.y)), r: Math.max(...bs.map((b) => b.x + b.w)), b: Math.max(...bs.map((b) => b.y + b.h)) };
    const others = flatten(base, ui.sceneId!).filter(({ inst }) => !moving.has(inst.id)).map(({ inst }) => getProp(base, inst, 'transform'));
    const xs = [0, W / 2, W, ...others.flatMap((t) => [t.x, t.x + t.w / 2, t.x + t.w])];
    const ys = [0, H / 2, H, ...others.flatMap((t) => [t.y, t.y + t.h / 2, t.y + t.h])];
    let moved = false;

    const onMove = (ev: MouseEvent) => {
      const p = toDoc(ev);
      let dx = p.x - start.x, dy = p.y - start.y;
      if (!moved && Math.hypot(dx * scale, dy * scale) < 3) return;
      moved = true;
      const g: Guide[] = [];
      if (!ev.ctrlKey) {
        const th = 8 / scale;
        const snap = (edges: number[], lines: number[], axis: 'x' | 'y') => {
          let best: { d: number; at: number } | null = null;
          for (const e of edges) for (const l of lines) if (Math.abs(l - e) < th && (!best || Math.abs(l - e) < Math.abs(best.d))) best = { d: l - e, at: l };
          if (best) g.push({ axis, at: best.at });
          return best?.d ?? 0;
        };
        dx += snap([ub.x + dx, (ub.x + ub.r) / 2 + dx, ub.r + dx], xs, 'x');
        dy += snap([ub.y + dy, (ub.y + ub.b) / 2 + dy, ub.b + dy], ys, 'y');
      }
      setGuides(g);
      gesture((d) => {
        const seen = new Set<string>();
        for (const id of movable) moveBy(d, id, dx, dy, seen);
      });
    };
    const onUp = () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
      setGuides([]);
      endGesture(movable.length > 1 ? `Move ${movable.length} layers` : `Move ${base.instances[movable[0]].name}`);
    };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
  };

  // ---------- resize ----------
  const startResize = (e: React.MouseEvent, id: string, corner: 'tl' | 'tr' | 'bl' | 'br') => {
    e.stopPropagation();
    const base = getState().doc;
    if (base.instances[id].locked) return toast('Layer is locked');
    if (liveLocked(e.altKey)) return toast('Live lock is on — hold Alt to resize while live');
    const t0 = getProp(base, base.instances[id], 'transform');
    const aspect = t0.w / t0.h;
    const onMove = (ev: MouseEvent) => {
      const p = toDoc(ev);
      const ax = corner.includes('l') ? t0.x + t0.w : t0.x; // anchor = opposite corner
      const ay = corner.includes('t') ? t0.y + t0.h : t0.y;
      let w = Math.max(8, Math.abs(p.x - ax));
      let h = Math.max(8, Math.abs(p.y - ay));
      if (!ev.shiftKey) {
        if (w / h > aspect) h = w / aspect;
        else w = h * aspect;
      }
      const x = corner.includes('l') ? ax - w : ax;
      const y = corner.includes('t') ? ay - h : ay;
      gesture((d) => setProp(d, id, 'transform', { ...t0, x: Math.round(x), y: Math.round(y), w: Math.round(w), h: Math.round(h) }));
    };
    const onUp = () => {
      window.removeEventListener('mousemove', onMove);
      window.removeEventListener('mouseup', onUp);
      endGesture(`Resize ${base.instances[id].name}`);
    };
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);
  };

  // ---------- drop from pool ----------
  const onDrop = (e: React.DragEvent) => {
    setDropping(false);
    const sid = e.dataTransfer.getData(DRAG_SOURCE);
    if (!sid || !scene) return;
    const src = doc.sources[sid];
    if (!KIND_INFO[src.kind].video) return toast('Audio sources live in the Audio tab, not on the canvas');
    const p = toDoc(e);
    let id = '';
    commit(`Place ${src.name}`, (d) => (id = placeSource(d, scene.id, sid, p).id));
    setUi({ selection: [id], poolSelection: null, rightTab: 'inspector' });
  };

  useEffect(() => setGuides([]), [ui.sceneId]);

  const leaves = scene ? flatten(doc, scene.id) : [];
  const isProgram = ui.programSceneId === scene?.id;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', minHeight: 0, minWidth: 0 }}>
      {ui.live && (
        <div className="lock-banner">
          <span className="dot live" /> LIVE · {ui.liveLockOverride ? 'Live lock is off — edits move layers on air' : <>layers are locked. Hold <kbd>Alt</kbd> to move.</>}
          <span className="grow" />
          <button className="ghost" onClick={() => setUi({ liveLockOverride: !ui.liveLockOverride })}>{ui.liveLockOverride ? 'Re-lock' : 'Unlock'}</button>
        </div>
      )}
      <div className="canvas-head">
        <span className="title">{scene?.name ?? 'No scene'}</span>
        {scene && (isProgram ? <span className="badge live">ON PROGRAM</span> : <span className="badge">PREVIEW · editing</span>)}
        <span className="grow" />
        <button className={'ghost' + (safe ? ' on' : '')} onClick={() => setSafe(!safe)} title="Title/action safe areas">Safe areas</button>
        <span className="muted small">{W}×{H} · {Math.round(scale * 100)}%</span>
        {scene && !isProgram && <button className="primary" onClick={() => void take(scene.id)} title="Send this scene to program">Take ▶</button>}
      </div>
      <div
        className="stage-wrap"
        ref={wrap}
        onMouseDown={() => setUi({ selection: [] })}
        onDragOver={(e) => {
          if (e.dataTransfer.types.includes(DRAG_SOURCE)) (e.preventDefault(), setDropping(true));
        }}
        onDragLeave={() => setDropping(false)}
        onDrop={onDrop}
      >
        {!scene ? (
          <div className="empty">Create a scene to start.</div>
        ) : (
          <div className="stage" style={{ left, top, width: W, height: H, transform: `scale(${scale})` }}>
            {leaves.map(({ inst, visible, opacity }) => {
              const src = doc.sources[inst.sourceId!];
              if (!src || !KIND_INFO[src.kind].video) return null;
              const t = getProp(doc, inst, 'transform');
              return (
                <div
                  key={inst.id}
                  className={'layer' + (thumbs[src.name] ? '' : ' placeholder')}
                  style={{ left: t.x, top: t.y, width: t.w, height: t.h, transform: `rotate(${t.rot}deg)`, opacity: visible ? opacity : 0.12, outline: visible ? undefined : '4px dashed #555', cursor: 'move', filter: cssFilters(getProp(doc, inst, 'filters')) }}
                  onMouseDown={(e) => startMove(e, inst.id)}
                  onDoubleClick={(e) => (e.stopPropagation(), setUi({ selection: [inst.id] }))}
                >
                  <LayerContent doc={doc} inst={inst} thumb={thumbs[src.name]} size={sizes[src.name]} />
                </div>
              );
            })}
            {safe && (
              <>
                <div style={{ position: 'absolute', left: W * 0.05, top: H * 0.05, width: W * 0.9, height: H * 0.9, border: '3px dashed #ffffff55', pointerEvents: 'none' }} />
                <div style={{ position: 'absolute', left: W * 0.1, top: H * 0.1, width: W * 0.8, height: H * 0.8, border: '3px dashed #ffc53d66', pointerEvents: 'none' }} />
              </>
            )}
            {ui.selection.filter((id) => doc.instances[id]?.sceneId === scene.id).map((id) => {
              const inst = doc.instances[id];
              const b = boundsOf(doc, id);
              const linked = !!inst.links.transform;
              const hs = 14 / scale;
              return (
                <div key={id} className={'sel-box' + (inst.kind === 'group' ? ' group' : '') + (linked ? ' linked' : '')} style={{ left: b.x, top: b.y, width: b.w, height: b.h, transform: `rotate(${b.rot}deg)`, borderWidth: 2 / scale }}>
                  {inst.kind === 'source' && ui.selection.length === 1 &&
                    (['tl', 'tr', 'bl', 'br'] as const).map((c) => (
                      <div
                        key={c}
                        className="handle"
                        style={{ width: hs, height: hs, borderWidth: 2 / scale, left: c.includes('l') ? -hs / 2 : undefined, right: c.includes('r') ? -hs / 2 : undefined, top: c.includes('t') ? -hs / 2 : undefined, bottom: c.includes('b') ? -hs / 2 : undefined, cursor: c === 'tl' || c === 'br' ? 'nwse-resize' : 'nesw-resize' }}
                        onMouseDown={(e) => startResize(e, id, c)}
                      />
                    ))}
                </div>
              );
            })}
            {guides.map((g, i) => (
              <div key={i} className="guide" style={g.axis === 'x' ? { left: g.at - 1 / scale, top: 0, width: 2 / scale, height: H } : { top: g.at - 1 / scale, left: 0, height: 2 / scale, width: W }} />
            ))}
          </div>
        )}
        {dropping && <div className="drop-hint" />}
      </div>
      <div className="canvas-bar">
        <span>Drag sources from the Media Pool onto the canvas.</span>
        <span className="grow" />
        <span><kbd>Shift</kbd> free resize · <kbd>Ctrl</kbd> no snap · <kbd>Alt</kbd> move while live</span>
      </div>
    </div>
  );
}

/** Approximate the color filter in the editor preview. */
function cssFilters(filters: Filter[]) {
  const parts: string[] = [];
  for (const f of filters) {
    if (!f.enabled) continue;
    const s = f.settings as any;
    if (f.type === 'color') parts.push(`brightness(${1 + s.brightness}) contrast(${1 + s.contrast}) saturate(${1 + s.saturation}) hue-rotate(${s.hue}deg)`);
    if (f.type === 'blur') parts.push(`blur(${s.size}px)`);
  }
  return parts.join(' ') || undefined;
}

function LayerContent({ doc, inst, thumb, size }: { doc: LobsDoc; inst: Instance; thumb?: string; size?: [number, number] }) {
  const src = doc.sources[inst.sourceId!];
  const t = getProp(doc, inst, 'transform');
  const c = getProp(doc, inst, 'crop');
  if (thumb) {
    const [sw, sh] = size ?? [t.w, t.h];
    const kx = t.w / Math.max(1, sw - c.l - c.r);
    const ky = t.h / Math.max(1, sh - c.t - c.b);
    return <img src={thumb} draggable={false} style={{ position: 'absolute', left: -c.l * kx, top: -c.t * ky, width: sw * kx, height: sh * ky, pointerEvents: 'none' }} />;
  }
  if (src.kind === 'color') return <div style={{ position: 'absolute', inset: 0, background: String(src.settings.color ?? '#000') }} />;
  if (src.kind === 'text')
    return (
      <div style={{ position: 'absolute', inset: 0, display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: Number(src.settings.size ?? 96), fontWeight: 700, color: String(src.settings.textColor ?? '#fff'), whiteSpace: 'nowrap', background: '#0006' }}>
        {String(src.settings.text ?? '')}
      </div>
    );
  return (
    <>
      <div className="ph-icon">{KIND_INFO[src.kind].icon}</div>
      <div className="ph-name">{src.name}</div>
      <div style={{ position: 'absolute', inset: 0, border: `6px solid ${src.color}` }} />
    </>
  );
}
