import { ReactNode, useEffect, useRef, useState, useSyncExternalStore } from 'react';

// ---------- context menus ----------

export interface MenuItem {
  label: string;
  onClick?: () => void;
  disabled?: boolean;
  danger?: boolean;
  sep?: boolean;
}
let menu: { x: number; y: number; items: MenuItem[] } | null = null;
const menuLs = new Set<() => void>();
const setMenu = (m: typeof menu) => {
  menu = m;
  menuLs.forEach((l) => l());
};
export function openMenu(e: { clientX: number; clientY: number; preventDefault?: () => void; stopPropagation?: () => void }, items: MenuItem[]) {
  e.preventDefault?.();
  e.stopPropagation?.();
  setMenu({ x: e.clientX, y: e.clientY, items });
}
export function MenuHost() {
  const m = useSyncExternalStore((l) => (menuLs.add(l), () => menuLs.delete(l)), () => menu);
  useEffect(() => {
    if (!m) return;
    const close = () => setMenu(null);
    window.addEventListener('mousedown', close);
    window.addEventListener('blur', close);
    return () => {
      window.removeEventListener('mousedown', close);
      window.removeEventListener('blur', close);
    };
  }, [m]);
  if (!m) return null;
  const x = Math.min(m.x, window.innerWidth - 200);
  const y = Math.min(m.y, window.innerHeight - m.items.length * 30 - 10);
  return (
    <div className="menu" style={{ left: x, top: y }} onMouseDown={(e) => e.stopPropagation()}>
      {m.items.map((it, i) =>
        it.sep ? (
          <hr key={i} />
        ) : (
          <button key={i} disabled={it.disabled} className={it.danger ? 'danger' : ''} onClick={() => (setMenu(null), it.onClick?.())}>
            {it.label}
          </button>
        ),
      )}
    </div>
  );
}

// ---------- prompt / confirm dialogs ----------

type Ask = { title: string; initial?: string; confirm?: string; body?: string; resolve: (v: string | null) => void };
let ask: Ask | null = null;
const askLs = new Set<() => void>();
const setAsk = (a: Ask | null) => {
  ask = a;
  askLs.forEach((l) => l());
};
export const askText = (title: string, initial = '') => new Promise<string | null>((resolve) => setAsk({ title, initial, resolve }));
export const askConfirm = (title: string, body: string, confirm = 'Delete') =>
  new Promise<boolean>((resolve) => setAsk({ title, body, confirm, resolve: (v) => resolve(v !== null) }));

export function AskHost() {
  const a = useSyncExternalStore((l) => (askLs.add(l), () => askLs.delete(l)), () => ask);
  const [val, setVal] = useState('');
  useEffect(() => setVal(a?.initial ?? ''), [a]);
  if (!a) return null;
  const done = (v: string | null) => (setAsk(null), a.resolve(v));
  const isText = a.confirm === undefined;
  return (
    <Modal title={a.title} onClose={() => done(null)}>
      <div className="mb">
        {a.body && <div className="muted">{a.body}</div>}
        {isText && (
          <input autoFocus value={val} onChange={(e) => setVal(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && done(val)} />
        )}
      </div>
      <div className="mf">
        <button onClick={() => done(null)}>Cancel</button>
        <button className={isText ? 'primary' : 'primary danger'} autoFocus={!isText} onClick={() => done(isText ? val : '')}>
          {isText ? 'OK' : a.confirm}
        </button>
      </div>
    </Modal>
  );
}

export function Modal({ title, children, onClose, wide }: { title: string; children: ReactNode; onClose: () => void; wide?: boolean }) {
  useEffect(() => {
    const k = (e: KeyboardEvent) => e.key === 'Escape' && onClose();
    window.addEventListener('keydown', k);
    return () => window.removeEventListener('keydown', k);
  }, [onClose]);
  return (
    <div className="modal-bg" onMouseDown={onClose}>
      <div className={'modal' + (wide ? ' wide' : '')} onMouseDown={(e) => e.stopPropagation()}>
        <h2>{title}</h2>
        {children}
      </div>
    </div>
  );
}

// ---------- number field with math ----------

export function evalMath(s: string): number | null {
  if (!/^[\d\s+\-*/().]+$/.test(s)) return null;
  try {
    const v = Function(`"use strict"; return (${s})`)();
    return typeof v === 'number' && isFinite(v) ? v : null;
  } catch {
    return null;
  }
}

/** Accepts math like `1920/2`; commits on Enter or blur. */
export function NumField({ label, value, onCommit, step = 1, disabled }: { label?: string; value: number; onCommit: (v: number) => void; step?: number; disabled?: boolean }) {
  const [text, setText] = useState(String(value));
  const focused = useRef(false);
  useEffect(() => {
    if (!focused.current) setText(String(Math.round(value * 1000) / 1000));
  }, [value]);
  const commit = () => {
    const v = evalMath(text);
    if (v !== null && v !== value) onCommit(v);
    else setText(String(Math.round(value * 1000) / 1000));
  };
  return (
    <div className="field">
      {label && <label>{label}</label>}
      <input
        disabled={disabled}
        value={text}
        onFocus={(e) => ((focused.current = true), e.target.select())}
        onBlur={() => ((focused.current = false), commit())}
        onChange={(e) => setText(e.target.value)}
        onKeyDown={(e) => {
          if (e.key === 'Enter') (e.target as HTMLInputElement).blur();
          if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
            e.preventDefault();
            const d = (e.key === 'ArrowUp' ? 1 : -1) * step * (e.shiftKey ? 10 : 1);
            onCommit(value + d);
          }
        }}
      />
    </div>
  );
}

/** Inline-editable label: double-click to rename. */
export function EditableName({ value, onRename, className }: { value: string; onRename: (v: string) => void; className?: string }) {
  const [editing, setEditing] = useState(false);
  const [text, setText] = useState(value);
  if (editing)
    return (
      <input
        autoFocus
        className={className}
        value={text}
        onChange={(e) => setText(e.target.value)}
        onFocus={(e) => e.target.select()}
        onClick={(e) => e.stopPropagation()}
        onBlur={() => {
          setEditing(false);
          if (text.trim() && text !== value) onRename(text.trim());
        }}
        onKeyDown={(e) => {
          if (e.key === 'Enter') (e.target as HTMLInputElement).blur();
          if (e.key === 'Escape') (setText(value), setEditing(false));
          e.stopPropagation();
        }}
        style={{ flex: 1 }}
      />
    );
  return (
    <span className={'name ' + (className ?? '')} onDoubleClick={(e) => (e.stopPropagation(), setText(value), setEditing(true))} title="Double-click to rename">
      {value}
    </span>
  );
}
