import { useState } from 'react';
import { deleteSource, instancesOfSource, KIND_INFO, placeSource } from '../model/doc';
import { commit, getState, setUi, toast, useStore } from '../model/store';
import { Source } from '../model/types';
import { askConfirm, askText, EditableName, openMenu } from './common';

export const DRAG_SOURCE = 'application/x-lobs-source';

export function MediaPool() {
  const doc = useStore((s) => s.doc);
  const poolSel = useStore((s) => s.ui.poolSelection);
  const [q, setQ] = useState('');
  const [closed, setClosed] = useState<Record<string, boolean>>({});
  const [dropBin, setDropBin] = useState<string | null>(null);
  const sources = doc.sourceOrder.map((id) => doc.sources[id]).filter((s) => s.name.toLowerCase().includes(q.toLowerCase()));
  const bins = ['', ...doc.bins];

  const addBin = async () => {
    const n = await askText('New bin');
    if (n?.trim()) commit('Add bin', (d) => void (d.bins.includes(n.trim()) || d.bins.push(n.trim())));
  };

  return (
    <div className="panel" style={{ flex: 1 }}>
      <div className="panel-h">
        <h3>Media Pool</h3>
        <button className="ghost" title="New bin" onClick={addBin}>📁＋</button>
        <button className="ghost" title="Add source" onClick={() => setUi({ addSourceDialog: true })}>＋</button>
      </div>
      <div style={{ padding: '6px 8px 2px' }}>
        <input placeholder="Search sources" value={q} onChange={(e) => setQ(e.target.value)} style={{ width: '100%' }} />
      </div>
      <div className="panel-b">
        {!doc.sourceOrder.length && (
          <div className="empty">
            Add each camera, screen, or file once.
            <br />Then drag it onto any scene.
            <br />
            <br />
            <button className="primary" onClick={() => setUi({ addSourceDialog: true })}>Add source</button>
          </div>
        )}
        {bins.map((bin) => {
          const items = sources.filter((s) => s.bin === bin);
          if (bin && !items.length && q) return null;
          return (
            <div
              key={bin || '_root'}
              onDragOver={(e) => {
                if (e.dataTransfer.types.includes(DRAG_SOURCE)) (e.preventDefault(), setDropBin(bin));
              }}
              onDragLeave={() => setDropBin(null)}
              onDrop={(e) => {
                const id = e.dataTransfer.getData(DRAG_SOURCE);
                setDropBin(null);
                if (id && doc.sources[id]?.bin !== bin) commit('Move source to bin', (d) => void (d.sources[id].bin = bin));
              }}
            >
              {bin && (
                <div
                  className={'folder' + (dropBin === bin ? ' drop' : '')}
                  onClick={() => setClosed({ ...closed, [bin]: !closed[bin] })}
                  onContextMenu={(e) =>
                    openMenu(e, [
                      { label: 'Rename bin…', onClick: async () => { const n = await askText('Rename bin', bin); if (n?.trim()) commit('Rename bin', (d) => { d.bins = d.bins.map((b) => (b === bin ? n.trim() : b)); for (const s of Object.values(d.sources)) if (s.bin === bin) s.bin = n.trim(); }); } },
                      { label: 'Delete bin (keep sources)', onClick: () => commit('Delete bin', (d) => { d.bins = d.bins.filter((b) => b !== bin); for (const s of Object.values(d.sources)) if (s.bin === bin) s.bin = ''; }) },
                    ])
                  }
                  style={{ cursor: 'pointer' }}
                >
                  {closed[bin] ? '▸' : '▾'} 📁 {bin} <span className="muted">{items.length}</span>
                </div>
              )}
              {!closed[bin] && items.map((s) => <PoolItem key={s.id} source={s} selected={poolSel === s.id} />)}
            </div>
          );
        })}
      </div>
    </div>
  );
}

function PoolItem({ source, selected }: { source: Source; selected: boolean }) {
  const doc = useStore((s) => s.doc);
  const uses = instancesOfSource(doc, source.id);
  const sceneCount = new Set(uses.map((u) => u.sceneId)).size;
  const info = KIND_INFO[source.kind];

  const place = () => {
    const sceneId = getState().ui.sceneId;
    if (!sceneId) return toast('Open a scene first');
    if (!info.video) return toast('Audio sources live in the Audio tab, not on the canvas');
    let id = '';
    commit(`Place ${source.name}`, (d) => (id = placeSource(d, sceneId, source.id).id));
    setUi({ selection: [id], poolSelection: null, rightTab: 'inspector' });
  };

  return (
    <div
      className={'row' + (selected ? ' sel' : '') + (uses.length || !info.video ? '' : ' dim')}
      draggable
      onDragStart={(e) => {
        e.dataTransfer.setData(DRAG_SOURCE, source.id);
        e.dataTransfer.effectAllowed = 'copyMove';
      }}
      onClick={() => setUi({ poolSelection: source.id, selection: [], rightTab: source.audio && !info.video ? 'audio' : 'inspector' })}
      onDoubleClick={place}
      onContextMenu={(e) =>
        openMenu(e, [
          { label: 'Place on current scene', onClick: place, disabled: !info.video },
          { label: `Select all instances (${uses.length})`, disabled: !uses.length, onClick: () => {
            const here = uses.filter((u) => u.sceneId === getState().ui.sceneId);
            const sceneId = here.length ? getState().ui.sceneId : uses[0].sceneId;
            setUi({ sceneId, selection: uses.filter((u) => u.sceneId === sceneId).map((u) => u.id) });
            if (sceneCount > 1) toast(`Used in ${sceneCount} scenes — showing this one`);
          } },
          { label: 'Rename…', onClick: async () => { const n = await askText('Rename source', source.name); if (n?.trim()) commit('Rename source', (d) => void (d.sources[source.id].name = n.trim())); } },
          { sep: true, label: '' },
          { label: 'Delete source', danger: true, onClick: async () => {
            if (!uses.length || (await askConfirm('Delete source?', `“${source.name}” is used ${uses.length} time(s) in ${sceneCount} scene(s). Those layers will be removed too.`)))
              commit('Delete source', (d) => deleteSource(d, source.id));
          } },
        ])
      }
      title={`${info.label} · double-click to place, or drag onto the canvas`}
    >
      <span className="tag" style={{ background: source.color }} />
      <span>{info.icon}</span>
      <EditableName value={source.name} onRename={(n) => commit('Rename source', (d) => void (d.sources[source.id].name = n))} />
      {info.video ? (
        uses.length ? <span className="badge" title={`${uses.length} instance(s) in ${sceneCount} scene(s)`}>×{uses.length}</span> : <span className="muted small">unused</span>
      ) : (
        <span className="badge">audio</span>
      )}
    </div>
  );
}
