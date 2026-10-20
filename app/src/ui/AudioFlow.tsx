// Audio signal-flow view: every audio path from source to output, visible in one place.
import { addSource, instancesOfSource, KIND_INFO } from '../model/doc';
import { commit, endGesture, gesture, useStore } from '../model/store';
import { AudioSettings, LobsDoc, MonitorType, Source } from '../model/types';

const MONITOR_LABEL: Record<MonitorType, string> = { off: 'Off', monitor_only: 'Monitor only', monitor_and_output: 'Monitor + output' };

const db = (v: number) => (v <= 0 ? '-∞' : (20 * Math.log10(v)).toFixed(1)) + ' dB';

function warnings(doc: LobsDoc, s: Source, a: AudioSettings): string[] {
  const w: string[] = [];
  if (s.kind === 'desktop_audio' && a.monitor === 'monitor_and_output') w.push('Echo risk: monitoring desktop audio plays it back into itself. Set Monitor to Off.');
  if (s.kind === 'mic' && a.monitor !== 'off') w.push('You will hear your own mic. Use headphones to avoid feedback.');
  if (a.monitor === 'monitor_only') w.push('Monitor only: you hear it, but the stream and recording don’t.');
  if (!a.tracks.some(Boolean)) w.push('Not sent to any track — this source reaches no output.');
  else if (!a.tracks[0]) w.push('Track 1 is off — by default the stream only uses track 1.');
  if (a.muted) w.push('Muted.');
  if (!KIND_INFO[s.kind].video || instancesOfSource(doc, s.id).length) return w;
  w.push('Has audio, but isn’t placed on any scene, so it plays only while OBS has it active.');
  return w;
}

export function AudioFlow() {
  const doc = useStore((s) => s.doc);
  const sources = doc.sourceOrder.map((id) => doc.sources[id]).filter((s) => s.audio);
  const set = (s: Source, label: string, patch: Partial<AudioSettings>) => commit(label, (d) => void (d.sources[s.id].audio = { ...d.sources[s.id].audio!, ...patch }));

  return (
    <div style={{ padding: 8 }}>
      <div className="muted small" style={{ marginBottom: 8 }}>
        Source → fader → monitor → tracks → outputs. Mic and desktop audio are global (always on); media and browser audio
        follow the scene they’re placed on. Windows’ own volume mixer can still lower OBS — check it if levels look wrong.
      </div>
      {!sources.length && (
        <div className="empty">
          No audio sources.
          <br />
          <button onClick={() => commit('Add mic', (d) => void addSource(d, 'mic', 'Mic'))}>Add mic</button>{' '}
          <button onClick={() => commit('Add desktop audio', (d) => void addSource(d, 'desktop_audio', 'Desktop Audio'))}>Add desktop audio</button>
        </div>
      )}
      {sources.map((s) => {
        const a = s.audio!;
        const w = warnings(doc, s, a);
        return (
          <div className="flow" key={s.id}>
            <div className="flow-h">
              <span className="tag" style={{ background: s.color, height: 16 }} />
              {KIND_INFO[s.kind].icon} {s.name}
              <span className="grow" />
              <button className={'small' + (a.muted ? ' btn-live' : '')} onClick={() => set(s, a.muted ? 'Unmute' : 'Mute', { muted: !a.muted })}>{a.muted ? 'Muted' : 'Mute'}</button>
            </div>
            <div className="flow-chain">
              <div className="node full">
                <span className="nt" style={{ width: 92 }}>Fader {db(a.volume)}</span>
                <input
                  type="range"
                  min={0}
                  max={1}
                  step={0.01}
                  value={a.volume}
                  onChange={(e) => gesture((d) => void (d.sources[s.id].audio = { ...a, volume: Number(e.target.value) }))}
                  onPointerUp={() => endGesture(`Volume ${s.name}`)}
                  onKeyUp={() => endGesture(`Volume ${s.name}`)}
                />
              </div>
              <div className="node" style={{ flex: 1 }}>
                <span className="nt">Monitor</span>
                <select value={a.monitor} onChange={(e) => set(s, 'Monitor mode', { monitor: e.target.value as MonitorType })} style={{ fontSize: 11 }}>
                  {(Object.keys(MONITOR_LABEL) as MonitorType[]).map((m) => <option key={m} value={m}>{MONITOR_LABEL[m]}</option>)}
                </select>
              </div>
              <div className="arrow">→</div>
              <div className="node">
                <span className="nt">Tracks</span>
                <div className="tracks">
                  {a.tracks.map((on, i) => (
                    <button key={i} className={on ? 'on' : ''} onClick={() => set(s, `Track ${i + 1}`, { tracks: a.tracks.map((t, j) => (j === i ? !t : t)) })}>{i + 1}</button>
                  ))}
                </div>
              </div>
              <div className="arrow">→</div>
              <div className="node">
                <span className="nt">Out</span>
                <span style={{ color: a.tracks[0] && !a.muted && a.monitor !== 'monitor_only' ? 'var(--ok)' : 'var(--faint)' }}>● Stream</span>
                <span style={{ color: a.tracks.some(Boolean) && !a.muted && a.monitor !== 'monitor_only' ? 'var(--ok)' : 'var(--faint)' }}>● Record</span>
              </div>
            </div>
            {w.map((x) => <div className="warn" key={x}>⚠ {x}</div>)}
          </div>
        );
      })}
    </div>
  );
}
