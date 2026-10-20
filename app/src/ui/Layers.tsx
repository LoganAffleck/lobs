import { useState } from 'react';
import { deleteInstance, getProp, groupSelection, isAnyLinked, KIND_INFO, placeSource, reorder, setProp, ungroup } from '../model/doc';
import { commit, getState, setUi, toast, useStore } from '../model/store';
import { Instance } from '../model/types';
import { EditableName, openMenu } from './common';
import { DRAG_SOURCE } from './MediaPool';

const DRAG_LAYER = 'application/x-lobs-layer';

export function layerActions(ids: string[]) {
  const doc = getState().doc;
  const one = ids.length === 1 ? doc.instances[ids[0]] : null;
  return [
    { label: 'Copy to scenes…', onClick: () => setUi({ copyDialog: { ids } }), disabled: !ids.length },
    { label: 'Group selection', onClick: () => groupIds(ids), disabled: !ids.length },
    { label: 'Ungroup', onClick: () => commit('Ungroup', (d) => ungroup(d, one!.id)), disabled: one?.kind !== 'group' },
    { sep: true, label: '' },
    { label: 'Delete', danger: true, onClick: () => deleteIds(ids), disabled: !ids.length },
  ];
}

export function groupIds(ids: string[]) {
  if (!ids.length) return;
  let gid = '';
  commit('Group layers', (d) => (gid = groupSelection(d, ids)?.id ?? ''));
  if (gid) setUi({ selection: [gid] });
}

export function deleteIds(ids: string[]) {
  if (!ids.length) return;
  commit(ids.length > 1 ? `Delete ${ids.length} layers` : 'Delete layer', (d) => ids.forEach((id) => deleteInstance(d, id)));
}

export function Layers() {
  const doc = useStore((s) => s.doc);
  const ui = useStore((s) => s.ui);
  const scene = ui.sceneId ? doc.scenes[ui.sceneId] : null;
  return (
    <div className="panel" style={{ flex: 0.9 }}>
      <div className="panel-h">
        <h3>Layers</h3>
        <button className="ghost" title="Group (Ctrl+G)" disabled={!ui.selection.length} onClick={() => groupIds(ui.selection)}>⧉ Group</button>
        <button className="ghost" title="Copy to other scenes" disabled={!ui.selection.length} onClick={() => setUi({ copyDialog: { ids: ui.selection } })}>⇢ Copy to…</button>
        <button className="ghost" title="Delete" disabled={!ui.selection.length} onClick={() => deleteIds(ui.selection)}>🗑</button>
      </div>
      <div
        className="panel-b"
        onDragOver={(e) => e.dataTransfer.types.includes(DRAG_SOURCE) && e.preventDefault()}
        onDrop={(e) => {
          const sid = e.dataTransfer.getData(DRAG_SOURCE);
          if (!sid || !scene) return;
          if (!KIND_INFO[doc.sources[sid].kind].video) return toast('Audio sources live in the Audio tab');
          let id = '';
          commit(`Place ${doc.sources[sid].name}`, (d) => (id = placeSource(d, scene.id, sid).id));
          setUi({ selection: [id] });
        }}
      >
        {scene && !scene.layers.length && <div className="empty">Empty scene. Drag a source here or onto the canvas.</div>}
        {scene?.layers.map((id) => <LayerRow key={id} id={id} depth={0} />)}
      </div>
    </div>
  );
}

function LayerRow({ id, depth }: { id: string; depth: number }) {
  const doc = useStore((s) => s.doc);
  const sel = useStore((s) => s.ui.selection);
  const [open, setOpen] = useState(true);
  const [drop, setDrop] = useState<'before' | 'after' | 'into' | null>(null);
  const inst: Instance | undefined = doc.instances[id];
  if (!inst) return null;
  const src = inst.sourceId ? doc.sources[inst.sourceId] : null;
  const vis = getProp(doc, inst, 'visibility');
  const filters = inst.kind === 'source' ? getProp(doc, inst, 'filters') : [];
  const linked = isAnyLinked(doc, inst);
  const selected = sel.includes(id);

  const click = (e: React.MouseEvent) => {
    const s = getState().ui.selection;
    setUi({ selection: e.shiftKey || e.ctrlKey ? (s.includes(id) ? s.filter((x) => x !== id) : [...s, id]) : [id], poolSelection: null, rightTab: 'inspector' });
  };

  return (
    <>
      <div
        className={'row' + (selected ? ' sel' : '') + (vis.visible ? '' : ' dim') + (drop ? ' drop-' + drop : '')}
        style={{ paddingLeft: 6 + depth * 16 }}
        onClick={click}
        onContextMenu={(e) => {
          if (!selected) setUi({ selection: [id] });
          openMenu(e, layerActions(selected ? getState().ui.selection : [id]));
        }}
        draggable
        onDragStart={(e) => (e.stopPropagation(), e.dataTransfer.setData(DRAG_LAYER, id))}
        onDragOver={(e) => {
          if (!e.dataTransfer.types.includes(DRAG_LAYER)) return;
          e.preventDefault();
          e.stopPropagation();
          const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
          const f = (e.clientY - r.top) / r.height;
          setDrop(inst.kind === 'group' && f > 0.3 && f < 0.7 ? 'into' : f < 0.5 ? 'before' : 'after');
        }}
        onDragLeave={() => setDrop(null)}
        onDrop={(e) => {
          const dragId = e.dataTransfer.getData(DRAG_LAYER);
          const where = drop;
          setDrop(null);
          if (!dragId || !where) return;
          e.stopPropagation();
          commit(where === 'into' ? 'Move into group' : 'Reorder layer', (d) => reorder(d, dragId, id, where));
        }}
        title={src ? `${KIND_INFO[src.kind].label} · source “${src.name}”` : 'Group'}
      >
        {inst.kind === 'group' ? (
          <button className="ghost" style={{ padding: '0 2px' }} onClick={(e) => (e.stopPropagation(), setOpen(!open))}>{open ? '▾' : '▸'}</button>
        ) : (
          <span className="tag" style={{ background: src?.color }} />
        )}
        <span>{inst.kind === 'group' ? '📂' : src ? KIND_INFO[src.kind].icon : '?'}</span>
        <EditableName value={inst.name} onRename={(n) => commit('Rename layer', (d) => void (d.instances[id].name = n))} />
        {linked && <span className="badge link" title="Some properties are linked to other scenes">🔗</span>}
        {filters.length > 0 && <span className="badge" title={`${filters.length} filter(s)`}>fx{filters.length}</span>}
        <button
          className={'ghost' + (inst.locked ? '' : ' hover-only')}
          title={inst.locked ? 'Unlock' : 'Lock'}
          onClick={(e) => (e.stopPropagation(), commit(inst.locked ? 'Unlock layer' : 'Lock layer', (d) => void (d.instances[id].locked = !inst.locked)))}
        >
          {inst.locked ? '🔒' : '🔓'}
        </button>
        <button
          className="ghost"
          title={vis.visible ? 'Hide' : 'Show'}
          onClick={(e) => (e.stopPropagation(), commit(vis.visible ? 'Hide layer' : 'Show layer', (d) => setProp(d, id, 'visibility', { ...vis, visible: !vis.visible })))}
          style={{ opacity: vis.visible ? 1 : 0.4 }}
        >
          {vis.visible ? '👁' : '◌'}
        </button>
      </div>
      {inst.kind === 'group' && open && inst.children!.map((c) => <LayerRow key={c} id={c} depth={depth + 1} />)}
    </>
  );
}
