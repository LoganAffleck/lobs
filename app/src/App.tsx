import { useEffect, useRef } from 'react';
import { copyToScene, moveBy, ungroup } from './model/doc';
import { commit, endGesture, gesture, getState, liveLocked, newDoc, redo, replaceDoc, setUi, toast, undo, useStore } from './model/store';
import { setTransition, toggleRecord, toggleStream, useBridge, disconnect } from './obs/bridge';
import { AskHost, askConfirm, MenuHost } from './ui/common';
import { AudioFlow } from './ui/AudioFlow';
import { Canvas } from './ui/Canvas';
import { AddSourceDialog, ConnectDialog, CopyDialog } from './ui/Dialogs';
import { HistoryPanel } from './ui/HistoryPanel';
import { Inspector } from './ui/Inspector';
import { deleteIds, groupIds, Layers } from './ui/Layers';
import { MediaPool } from './ui/MediaPool';
import { ScenePanel } from './ui/ScenePanel';

let clipboard: string[] = [];

function TopBar() {
  const past = useStore((s) => s.past.length);
  const future = useStore((s) => s.future.length);
  const nextUndo = useStore((s) => s.current.label);
  const b = useBridge((x) => x);
  const file = useRef<HTMLInputElement>(null);

  const save = () => {
    const blob = new Blob([JSON.stringify(getState().doc, null, 2)], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'show.lobs.json';
    a.click();
    URL.revokeObjectURL(a.href);
  };
  const open = async (f: File) => {
    try {
      const doc = JSON.parse(await f.text());
      if (doc.version !== 1 || !doc.scenes) throw new Error('Not a LOBS document');
      replaceDoc(doc, `Open ${f.name}`);
    } catch (e: any) {
      toast(e.message);
    }
  };

  const dot = b.status === 'connected' ? (b.syncError ? 'warn' : 'ok') : b.status === 'connecting' ? 'warn' : '';
  return (
    <div className="topbar">
      <div className="logo">L<span>OBS</span></div>
      <button className="ghost" onClick={async () => (await askConfirm('New document?', 'This replaces the current document. You can undo it.', 'New')) && newDoc()}>New</button>
      <button className="ghost" onClick={() => file.current?.click()}>Open</button>
      <button className="ghost" onClick={save}>Save</button>
      <input ref={file} type="file" accept=".json" hidden onChange={(e) => e.target.files?.[0] && void open(e.target.files[0])} />
      <div className="sep" />
      <button className="ghost" disabled={!past} onClick={undo} title={`Undo ${nextUndo} (Ctrl+Z)`}>↶</button>
      <button className="ghost" disabled={!future} onClick={redo} title="Redo (Ctrl+Shift+Z)">↷</button>
      <div className="grow" />
      <button className="pill" onClick={() => setUi({ connectDialog: true })} title={b.syncError ?? (b.lastSync ? 'Synced ' + new Date(b.lastSync).toLocaleTimeString() : '')}>
        <span className={'dot ' + dot} />
        {b.status === 'connected' ? (b.syncing ? 'Syncing…' : b.syncError ? 'Sync error' : `OBS ${b.obsVersion}`) : b.status === 'connecting' ? 'Connecting…' : 'Connect OBS'}
      </button>
      {b.status === 'connected' && (
        <>
          <select value={b.transition} onChange={(e) => void setTransition(e.target.value)} title="Transition">
            {b.transitions.map((t) => <option key={t}>{t}</option>)}
          </select>
          <button className={b.streaming ? 'btn-live' : ''} onClick={() => void toggleStream()}>{b.streaming ? '● Stop stream' : 'Go live'}</button>
          <button className={b.recording ? 'btn-live' : ''} onClick={() => void toggleRecord()}>{b.recording ? '■ Stop rec' : 'Record'}</button>
          <button className="ghost" onClick={() => void disconnect()} title="Disconnect">⏏</button>
        </>
      )}
    </div>
  );
}

function RightPanel() {
  const tab = useStore((s) => s.ui.rightTab);
  return (
    <div className="panel" style={{ flex: 1.3 }}>
      <div className="panel-h">
        <div className="tabs">
          {(['inspector', 'audio', 'history'] as const).map((t) => (
            <button key={t} className={tab === t ? 'active' : ''} onClick={() => setUi({ rightTab: t })}>
              {t === 'inspector' ? 'Inspector' : t === 'audio' ? 'Audio flow' : 'History'}
            </button>
          ))}
        </div>
      </div>
      <div className="panel-b" style={{ padding: 0 }}>
        {tab === 'inspector' ? <Inspector /> : tab === 'audio' ? <AudioFlow /> : <HistoryPanel />}
      </div>
    </div>
  );
}

function useShortcuts() {
  useEffect(() => {
    let nudge: { dx: number; dy: number; sel: string[] } | null = null;
    const down = (e: KeyboardEvent) => {
      const el = e.target as HTMLElement;
      if (el.closest('input, textarea, select') || document.querySelector('.modal-bg')) return;
      const { ui, doc } = getState();
      const mod = e.ctrlKey || e.metaKey;
      const k = e.key.toLowerCase();
      if (mod && k === 'z') return e.preventDefault(), e.shiftKey ? redo() : undo();
      if (mod && k === 'y') return e.preventDefault(), redo();
      if (mod && k === 'g') return e.preventDefault(), e.shiftKey ? ui.selection.forEach((id) => doc.instances[id]?.kind === 'group' && commit('Ungroup', (d) => ungroup(d, id))) : groupIds(ui.selection);
      if (mod && k === 'c' && ui.selection.length) return (clipboard = ui.selection), toast(`Copied ${ui.selection.length} layer(s)`);
      if (mod && k === 'v' && clipboard.length && ui.sceneId) {
        e.preventDefault();
        const ids = clipboard.filter((id) => doc.instances[id]);
        if (!ids.length) return;
        const fromScene = doc.instances[ids[0]].sceneId;
        if (fromScene === ui.sceneId || ui.copyDefault) {
          const mode = fromScene === ui.sceneId ? 'independent' : ui.copyDefault!;
          let made: string[] = [];
          commit(`Paste (${mode})`, (d) => (made = copyToScene(d, ids, ui.sceneId!, mode)));
          setUi({ selection: made });
        } else setUi({ copyDialog: { ids, targets: [ui.sceneId] } });
        return;
      }
      if (mod && k === 'd' && ui.selection.length && ui.sceneId) {
        e.preventDefault();
        let made: string[] = [];
        commit('Duplicate layer', (d) => {
          made = copyToScene(d, ui.selection, ui.sceneId!, 'independent');
          made.forEach((id) => moveBy(d, id, 20, 20));
        });
        return setUi({ selection: made });
      }
      if ((e.key === 'Delete' || e.key === 'Backspace') && ui.selection.length) return e.preventDefault(), deleteIds(ui.selection);
      if (e.key === 'Escape') return setUi({ selection: [] });
      if (mod && k === 'a' && ui.sceneId) return e.preventDefault(), setUi({ selection: [...doc.scenes[ui.sceneId].layers] });
      const arrows: Record<string, [number, number]> = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] };
      if (arrows[e.key] && ui.selection.length) {
        e.preventDefault();
        if (liveLocked(e.altKey)) return toast('Live lock is on — hold Alt to nudge while live');
        const [dx, dy] = arrows[e.key].map((v) => v * (e.shiftKey ? 10 : 1));
        // Held arrow keys accumulate into one undo step, committed on key release.
        nudge ??= { dx: 0, dy: 0, sel: ui.selection };
        nudge.dx += dx;
        nudge.dy += dy;
        const n = nudge;
        gesture((d) => {
          const seen = new Set<string>();
          for (const id of n.sel) moveBy(d, id, n.dx, n.dy, seen);
        });
      }
    };
    const up = (e: KeyboardEvent) => {
      if (nudge && e.key.startsWith('Arrow')) {
        nudge = null;
        endGesture('Nudge');
      }
    };
    window.addEventListener('keydown', down);
    window.addEventListener('keyup', up);
    return () => {
      window.removeEventListener('keydown', down);
      window.removeEventListener('keyup', up);
    };
  }, []);
}

export function App() {
  const toastMsg = useStore((s) => s.ui.toast);
  const copyKey = useStore((s) => (s.ui.copyDialog ? s.ui.copyDialog.ids.join() : ''));
  useShortcuts();
  return (
    <div className="app">
      <TopBar />
      <div className="main">
        <div className="col">
          <ScenePanel />
          <MediaPool />
        </div>
        <Canvas />
        <div className="col right">
          <Layers />
          <RightPanel />
        </div>
      </div>
      <CopyDialog key={copyKey} />
      <AddSourceDialog />
      <ConnectDialog />
      <AskHost />
      <MenuHost />
      {toastMsg && <div className="toast">{toastMsg}</div>}
    </div>
  );
}
