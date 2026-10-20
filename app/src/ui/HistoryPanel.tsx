import { jumpTo, redo, undo, useStore } from '../model/store';

const time = (t: number) => new Date(t).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

export function HistoryPanel() {
  const past = useStore((s) => s.past);
  const current = useStore((s) => s.current);
  const future = useStore((s) => s.future);
  const all = [...past, current, ...future];
  return (
    <div>
      <div className="insp-sec" style={{ display: 'flex', gap: 6 }}>
        <button className="small" disabled={!past.length} onClick={undo}>↶ Undo</button>
        <button className="small" disabled={!future.length} onClick={redo}>↷ Redo</button>
        <span className="grow" />
        <span className="muted small">{past.length} step(s) back</span>
      </div>
      <div style={{ padding: 4 }}>
        {[...all].reverse().map((h, ri) => {
          const i = all.length - 1 - ri;
          const cls = i === past.length ? ' cur' : i > past.length ? ' future' : '';
          return (
            <div key={i + h.label + h.at} className={'hist-item' + cls} onClick={() => jumpTo(i, false)} title="Jump to this point">
              <span>{h.label}</span>
              <span className="t">{time(h.at)}</span>
            </div>
          );
        })}
      </div>
    </div>
  );
}
