import { addScene, deleteScene, duplicateScene, flatten, getProp, KIND_INFO } from '../model/doc';
import { commit, setUi, useStore } from '../model/store';
import { LobsDoc, Scene } from '../model/types';
import { take, useBridge } from '../obs/bridge';
import { askConfirm, askText, EditableName, openMenu } from './common';

export function ScenePanel() {
  const doc = useStore((s) => s.doc);
  const ui = useStore((s) => s.ui);
  const q = ui.sceneSearch.toLowerCase();
  const scenes = doc.sceneOrder.map((id) => doc.scenes[id]).filter((s) => s.name.toLowerCase().includes(q));
  const favorites = scenes.filter((s) => s.favorite);
  const folders = [...new Set(scenes.filter((s) => !s.favorite).map((s) => s.folder))].sort((a, b) => (a === '' ? -1 : b === '' ? 1 : a.localeCompare(b)));

  const newScene = () => {
    let id = '';
    commit('Add scene', (d) => (id = addScene(d, 'Scene').id));
    setUi({ sceneId: id, selection: [] });
  };

  const body = (list: Scene[]) =>
    ui.sceneView === 'grid' ? (
      <div className="scene-grid">
        {list.map((s) => (
          <SceneTile key={s.id} scene={s} />
        ))}
      </div>
    ) : (
      list.map((s) => <SceneRow key={s.id} scene={s} />)
    );

  return (
    <div className="panel" style={{ flex: 1.1 }}>
      <div className="panel-h">
        <h3>Scenes</h3>
        <button className={'ghost' + (ui.sceneView === 'list' ? ' on' : '')} title="List view" onClick={() => setUi({ sceneView: 'list' })}>☰</button>
        <button className={'ghost' + (ui.sceneView === 'grid' ? ' on' : '')} title="Grid view" onClick={() => setUi({ sceneView: 'grid' })}>▦</button>
        <button className="ghost" title="Add scene" onClick={newScene}>＋</button>
      </div>
      <div style={{ padding: '6px 8px 2px' }}>
        <input placeholder="Search scenes" value={ui.sceneSearch} onChange={(e) => setUi({ sceneSearch: e.target.value })} style={{ width: '100%' }} />
      </div>
      <div className="panel-b">
        {!doc.sceneOrder.length && <div className="empty">No scenes yet.<br /><button onClick={newScene}>Add a scene</button></div>}
        {favorites.length > 0 && (
          <>
            <div className="folder">★ Favorites</div>
            {body(favorites)}
          </>
        )}
        {folders.map((f) => (
          <div key={f || '_root'}>
            {(f || favorites.length > 0 || folders.length > 1) && <div className="folder">{f ? '📁 ' + f : 'Scenes'}</div>}
            {body(scenes.filter((s) => !s.favorite && s.folder === f))}
          </div>
        ))}
      </div>
    </div>
  );
}

function sceneMenu(scene: Scene) {
  return [
    { label: 'Take to program', onClick: () => void take(scene.id) },
    { sep: true, label: '' },
    { label: 'Rename…', onClick: async () => { const n = await askText('Rename scene', scene.name); if (n) commit('Rename scene', (d) => void (d.scenes[scene.id].name = n)); } },
    { label: scene.favorite ? 'Remove from favorites' : 'Add to favorites', onClick: () => commit('Toggle favorite', (d) => void (d.scenes[scene.id].favorite = !scene.favorite)) },
    { label: 'Move to folder…', onClick: async () => { const f = await askText('Folder name (empty for none)', scene.folder); if (f !== null) commit('Move scene to folder', (d) => void (d.scenes[scene.id].folder = f.trim())); } },
    { sep: true, label: '' },
    { label: 'Duplicate (linked layers)', onClick: () => { let id = ''; commit('Duplicate scene (linked)', (d) => (id = duplicateScene(d, scene.id, 'linked').id)); setUi({ sceneId: id }); } },
    { label: 'Duplicate (independent layers)', onClick: () => { let id = ''; commit('Duplicate scene', (d) => (id = duplicateScene(d, scene.id, 'independent').id)); setUi({ sceneId: id }); } },
    { sep: true, label: '' },
    { label: 'Delete scene', danger: true, onClick: async () => { if (await askConfirm('Delete scene?', `“${scene.name}” and its layers will be removed. Sources stay in the Media Pool.`)) commit('Delete scene', (d) => deleteScene(d, scene.id)); } },
  ];
}

const select = (id: string) => setUi({ sceneId: id, selection: [] });

function SceneRow({ scene }: { scene: Scene }) {
  const ui = useStore((s) => s.ui);
  const count = scene.layers.length;
  return (
    <div
      className={'row' + (ui.sceneId === scene.id ? ' sel' : '')}
      onClick={() => select(scene.id)}
      onContextMenu={(e) => openMenu(e, sceneMenu(scene))}
      title="Click to edit · right-click for more"
    >
      <EditableName value={scene.name} onRename={(n) => commit('Rename scene', (d) => void (d.scenes[scene.id].name = n))} />
      {ui.programSceneId === scene.id && <span className="badge live">PGM</span>}
      <span className="muted small">{count}</span>
      <button className="ghost hover-only" title={scene.favorite ? 'Unfavorite' : 'Favorite'} onClick={(e) => (e.stopPropagation(), commit('Toggle favorite', (d) => void (d.scenes[scene.id].favorite = !scene.favorite)))}>{scene.favorite ? '★' : '☆'}</button>
      <button className="ghost hover-only" title="More" onClick={(e) => openMenu(e, sceneMenu(scene))}>⋯</button>
    </div>
  );
}

function SceneTile({ scene }: { scene: Scene }) {
  const ui = useStore((s) => s.ui);
  const doc = useStore((s) => s.doc);
  const thumb = useBridge((b) => b.thumbs[scene.name]);
  return (
    <div
      className={'tile' + (ui.sceneId === scene.id ? ' sel' : '') + (ui.programSceneId === scene.id ? ' program' : '')}
      onClick={() => select(scene.id)}
      onDoubleClick={() => void take(scene.id)}
      onContextMenu={(e) => openMenu(e, sceneMenu(scene))}
      title="Click to edit · double-click to take live"
    >
      <div className="thumb">{thumb ? <img src={thumb} style={{ width: '100%', height: '100%', objectFit: 'cover' }} /> : <MiniScene doc={doc} sceneId={scene.id} />}</div>
      <div className="label">
        <span>{scene.name}</span>
        {ui.programSceneId === scene.id && <span className="badge live">PGM</span>}
        {scene.favorite && <span>★</span>}
      </div>
    </div>
  );
}

/** Vector thumbnail of a scene from the document (used when OBS isn't connected). */
export function MiniScene({ doc, sceneId }: { doc: LobsDoc; sceneId: string }) {
  const { w, h } = doc.canvas;
  return (
    <svg viewBox={`0 0 ${w} ${h}`} style={{ width: '100%', height: '100%', display: 'block' }}>
      {flatten(doc, sceneId).map(({ inst, visible, opacity }) => {
        if (!visible) return null;
        const src = doc.sources[inst.sourceId!];
        if (!src || !KIND_INFO[src.kind].video) return null;
        const t = getProp(doc, inst, 'transform');
        const fill = src.kind === 'color' ? String(src.settings.color ?? '#333') : src.color;
        return (
          <g key={inst.id} opacity={opacity} transform={`translate(${t.x} ${t.y}) rotate(${t.rot})`}>
            <rect width={t.w} height={t.h} fill={fill} fillOpacity={src.kind === 'color' ? 1 : 0.45} stroke={src.color} strokeWidth={6} />
          </g>
        );
      })}
    </svg>
  );
}
