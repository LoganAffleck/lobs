import { useLayoutEffect, useRef, useState, type CSSProperties, type ReactNode } from 'react'
import { createPortal } from 'react-dom'
import { ICONS, type IconName } from './icons.gen'
import { useStore } from './store'
import type { MenuState } from './model'

export function Icon({ name, size = 14, color, stroke = 2, style }: { name: IconName; size?: number; color?: string; stroke?: number; style?: CSSProperties }) {
  return (
    <svg
      className="ico" width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor"
      strokeWidth={stroke} strokeLinecap="round" strokeLinejoin="round" style={{ color, ...style }}
      dangerouslySetInnerHTML={{ __html: ICONS[name] }}
    />
  )
}

type Variant = 'primary' | 'secondary' | 'tertiary' | 'ghost' | 'danger'

export function Btn({ icon, text, variant = 'secondary', sel, hov, fill, disabled, iconColor, onClick, children, className = '', style, id }: {
  icon?: IconName; text?: ReactNode; variant?: Variant; sel?: boolean; hov?: boolean; fill?: boolean; disabled?: boolean
  iconColor?: string; onClick?: () => void; children?: ReactNode; className?: string; style?: CSSProperties; id?: string
}) {
  const cls = ['btn', variant, sel && 'sel', hov && 'hov', fill && 'fill', icon && !text && 'icon-only', className].filter(Boolean).join(' ')
  return (
    <button id={id} className={cls} disabled={disabled} onClick={onClick} style={style} data-ui={typeof text === 'string' ? text : icon}>
      {icon && <Icon name={icon} color={iconColor} />}
      {text}
      {children}
    </button>
  )
}

export function Badge({ text, tone }: { text: ReactNode; tone?: 'live' | 'link' | 'problem' }) {
  return <span className={`badge ${tone ?? ''}`}>{text}</span>
}

export function Tabs({ items, current, onPick }: { items: string[]; current: number; onPick?: (i: number) => void }) {
  return (
    <div className="tabs">
      {items.map((t, i) => <button key={t} className={i === current ? 'on' : ''} onClick={() => onPick?.(i)}>{t}</button>)}
    </div>
  )
}

const fmt = (v: number) => String(Math.round(v * 100) / 100)

export function NumField({ label, value, suffix, disabled, focus, typed }: { label: string; value: number; suffix?: string; disabled?: boolean; focus?: boolean; typed?: string }) {
  return (
    <div className={`input ${disabled ? 'disabled' : ''} ${focus ? 'focus' : ''}`}>
      <span className="label">{label}</span>
      <span className="val">{typed ?? fmt(value)}{focus && <span className="caret" />}</span>
      {suffix && <span className="unit">{suffix}</span>}
    </div>
  )
}

export function Field({ children, placeholder, focus, right }: { children?: ReactNode; placeholder?: string; focus?: boolean; right?: ReactNode }) {
  return (
    <div className={`input ${focus ? 'focus' : ''}`}>
      {children ? <span className="val ellipsis">{children}</span> : <span className="placeholder">{placeholder}</span>}
      {focus && <span className="caret" />}
      {right}
    </div>
  )
}

export function FillSlider({ label, frac, value }: { label: string; frac: number; value: string }) {
  const f = Math.max(0, Math.min(1, frac))
  return (
    <div className="slider">
      <div className="fillbar" style={{ width: `${f * 100}%` }} />
      <div className="grip" style={{ left: `max(4px, calc(${f * 100}% - 6px))` }} />
      <span className="grow">{label}</span>
      <span className="muted">{value}</span>
    </div>
  )
}

export function Switch({ on, text, onClick }: { on: boolean; text?: string; onClick?: () => void }) {
  return (
    <button className={`switch ${on ? 'on' : ''}`} onClick={onClick}>
      <span className="track" />
      {text && <span className="lbl">{text}</span>}
    </button>
  )
}

export function Check({ on, text }: { on: boolean; text: string }) {
  return (
    <span className={`check ${on ? 'on' : ''}`}>
      <span className="box">{on && <Icon name="check" size={11} stroke={3} />}</span>
      {text}
    </span>
  )
}

export function Select({ value, placeholder = 'Choose…', onClick }: { value?: string; placeholder?: string; onClick?: () => void }) {
  return (
    <button className="btn tertiary fill" style={{ justifyContent: 'space-between', paddingRight: 8 }} onClick={onClick}>
      <span className={`ellipsis ${value ? '' : 'muted'}`} style={{ fontWeight: 400 }}>{value ?? placeholder}</span>
      <span className="muted" style={{ fontSize: 10 }}>▾</span>
    </button>
  )
}

export interface MenuEntry {
  text?: string
  sel?: boolean
  disabled?: boolean
  kbd?: string
  sep?: boolean
  sub?: MenuEntry[]
  onClick?: () => void
}

/**
 * A dropdown or context menu, shown when `state.menu.id === id` (and the target matches).
 * Place it inside the element it hangs from (any positioned parent). Like the app's popups it
 * draws above every panel: it's portaled to the editor root and placed from its parent's box,
 * corrected for any scale on the way (the site's hero, a video capture).
 */
export function Menu({ id, target, entries, width = 220, align = 'left', offset, matchWidth }: {
  id: MenuState['id']; target?: string; entries: MenuEntry[]; width?: number
  /** Which edge lines up with the parent's. */
  align?: 'left' | 'right'
  /** From the parent's bottom-left (or top-left with `state.menu.at`), unscaled px. */
  offset?: [number, number]
  matchWidth?: boolean
}) {
  const { s, d } = useStore()
  const probe = useRef<HTMLSpanElement>(null)
  const [place, setPlace] = useState<{ root: HTMLElement; x: number; y: number; w: number; subLeft: boolean } | null>(null)
  const m = s.menu
  const open = !!m && m.id === id && (target === undefined || m.target === target)
  useLayoutEffect(() => {
    const el = probe.current?.parentElement
    const root = probe.current?.closest('.lobs') as HTMLElement | null
    if (!open || !el || !root) return setPlace(null)
    const rr = root.getBoundingClientRect(), k = rr.width / root.offsetWidth || 1
    const a = el.getBoundingClientRect()
    const ax = (a.left - rr.left) / k, ay = (a.top - rr.top) / k, aw = a.width / k, ah = a.height / k
    const w = matchWidth ? Math.max(width, aw) : width
    let x: number, y: number
    if (m!.at) { x = ax + m!.at[0]; y = ay + m!.at[1] }
    else { x = align === 'right' ? ax + aw - w : ax; y = ay + ah + 4 }
    if (offset) { x += offset[0]; y += offset[1] }
    const W = root.offsetWidth, H = root.offsetHeight
    const h = entries.reduce((n, e) => n + (e.sep ? 9 : 28), 10)
    x = Math.max(4, Math.min(x, W - w - 4))
    if (y + h > H - 4) y = Math.max(4, (m!.at ? y : ay - 4) - h)
    setPlace({ root, x, y, w, subLeft: x + w + 222 > W })
  }, [open, m?.at?.[0], m?.at?.[1], width, align, offset?.[0], offset?.[1], matchWidth, entries.length])
  return (
    <>
      <span ref={probe} style={{ display: 'none' }} />
      {open && place && createPortal(
        <MenuList entries={entries} width={place.w} style={{ left: place.x, top: place.y }} subLeft={place.subLeft} open={m!}
          onClose={() => d({ type: 'menu', menu: null })} onSub={(sub) => d({ type: 'menu', menu: { ...m!, sub } })} />,
        place.root,
      )}
    </>
  )
}

function MenuList({ entries, width, style, open, onClose, onSub, className = '', subLeft }: { entries: MenuEntry[]; width: number; style?: CSSProperties; open: MenuState; onClose: () => void; onSub: (s: string) => void; className?: string; subLeft?: boolean }) {
  return (
    <div className={`menu ${className}`} style={{ width, ...style }} onClick={(e) => e.stopPropagation()}>
      {entries.map((e, i) => {
        if (e.sep) return <div key={i} className="sep" />
        const subOpen = !!e.sub && open.sub === e.text
        const cls = ['mi', e.disabled && 'dis', e.sel && 'sel', (open.hover === e.text || subOpen) && 'hov'].filter(Boolean).join(' ')
        return (
          <div key={i} className={cls} data-ui={e.text}
            onMouseEnter={() => e.sub && onSub(e.text!)}
            onClick={() => { if (e.disabled || e.sub) return; e.onClick?.(); onClose() }}>
            <span className="ellipsis grow">{e.text}</span>
            {e.kbd && <span className="kbd">{e.kbd}</span>}
            {e.sel && <Icon name="check" />}
            {e.sub && <Icon name="chevron-right" color="var(--muted)" />}
            {subOpen && <MenuList className="submenu" style={subLeft ? { left: 'auto', right: 'calc(100% + 2px)' } : undefined} entries={e.sub!} width={220} open={{ ...open, sub: undefined }} onClose={onClose} onSub={() => {}} />}
          </div>
        )
      })}
    </div>
  )
}

/** Wraps a trigger so its menu hangs under it. */
export function Anchor({ children, style }: { children: ReactNode; style?: CSSProperties }) {
  return <span style={{ position: 'relative', display: 'inline-flex', ...style }}>{children}</span>
}

/** The (?) button and its card: a title and "Key: text" points, like FHelp.qml. */
export function Help({ title, points, align = 'left', small }: { title: string; points: string[]; align?: 'left' | 'right'; small?: boolean }) {
  const { s, d } = useStore()
  const open = s.help === title
  return (
    <Anchor>
      <Btn icon="circle-help" variant="ghost" className={small ? 'help' : ''} onClick={() => d({ type: 'set', patch: { help: open ? null : title } })} />
      {open && (
        <div className="popover" style={{ top: 'calc(100% + 4px)', [align]: 0 }} onClick={(e) => e.stopPropagation()}>
          <h4>{title}</h4>
          {points.map((p) => {
            const c = p.indexOf(': ')
            return <div key={p} className="pt"><span>{c > 0 ? <><b>{p.slice(0, c)}</b> {p.slice(c + 2)}</> : p}</span></div>
          })}
        </div>
      )}
    </Anchor>
  )
}
