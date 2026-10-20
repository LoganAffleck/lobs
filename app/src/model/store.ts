// App store: document + undo history + UI state. Every document edit is a named, undoable command.
import { useSyncExternalStore } from 'react';
import { emptyDoc, sampleDoc } from './doc';
import { LobsDoc } from './types';
import type { CopyMode } from './doc';

export interface HistoryEntry {
  label: string;
  doc: LobsDoc;
  at: number;
}

export interface UiState {
  sceneId: string | null; // scene being edited (preview)
  programSceneId: string | null; // scene on program (from OBS, or local)
  selection: string[]; // instance ids
  poolSelection: string | null;
  sceneView: 'list' | 'grid';
  sceneSearch: string;
  rightTab: 'inspector' | 'history' | 'audio';
  live: boolean; // streaming or recording
  liveLockOverride: boolean;
  toast: string | null;
  copyDefault: CopyMode | null; // remembered copy mode
  copyDialog: { ids: string[]; targets?: string[] } | null;
  addSourceDialog: boolean;
  connectDialog: boolean;
}

export interface State {
  doc: LobsDoc;
  past: HistoryEntry[];
  future: HistoryEntry[];
  current: HistoryEntry; // the entry describing how we got to `doc`
  ui: UiState;
}

const KEY = 'lobs.doc.v1';

function loadDoc(): LobsDoc {
  try {
    const raw = localStorage.getItem(KEY);
    if (raw) return JSON.parse(raw);
  } catch {}
  return sampleDoc();
}

const initialDoc = loadDoc();
let state: State = {
  doc: initialDoc,
  past: [],
  future: [],
  current: { label: 'Open document', doc: initialDoc, at: Date.now() },
  ui: {
    sceneId: initialDoc.sceneOrder[0] ?? null,
    programSceneId: null,
    selection: [],
    poolSelection: null,
    sceneView: 'list',
    sceneSearch: '',
    rightTab: 'inspector',
    live: false,
    liveLockOverride: false,
    toast: null,
    copyDefault: null,
    copyDialog: null,
    addSourceDialog: false,
    connectDialog: false,
  },
};

const listeners = new Set<() => void>();
const docListeners = new Set<(d: LobsDoc) => void>();
let saveTimer: number | undefined;

function emit(docChanged: boolean) {
  for (const l of listeners) l();
  if (docChanged) {
    for (const l of docListeners) l(state.doc);
    clearTimeout(saveTimer);
    saveTimer = window.setTimeout(() => {
      try {
        localStorage.setItem(KEY, JSON.stringify(state.doc));
      } catch {}
    }, 300);
  }
}

export const getState = () => state;
export const subscribe = (l: () => void) => (listeners.add(l), () => listeners.delete(l));
export const onDocChange = (l: (d: LobsDoc) => void) => (docListeners.add(l), () => docListeners.delete(l));

export function useStore<T>(sel: (s: State) => T): T {
  return useSyncExternalStore(subscribe, () => sel(state));
}

const MAX_HISTORY = 200;

/** Run an undoable edit. */
export function commit(label: string, fn: (draft: LobsDoc) => void) {
  const draft = structuredClone(state.doc);
  fn(draft);
  const entry = { label, doc: draft, at: Date.now() };
  state = { ...state, doc: draft, past: [...state.past, state.current].slice(-MAX_HISTORY), current: entry, future: [] };
  pruneSelection();
  emit(true);
}

// Gestures (drags) preview changes live and land in history as one entry.
let gestureBase: LobsDoc | null = null;
export function gesture(fn: (draft: LobsDoc) => void) {
  gestureBase ??= state.doc;
  const draft = structuredClone(gestureBase);
  fn(draft);
  state = { ...state, doc: draft };
  emit(true);
}
export function endGesture(label: string) {
  if (!gestureBase) return;
  const changed = gestureBase !== state.doc;
  const base = gestureBase;
  gestureBase = null;
  if (!changed) return;
  const entry = { label, doc: state.doc, at: Date.now() };
  state = { ...state, past: [...state.past, state.current].slice(-MAX_HISTORY), current: entry, future: [] };
  void base;
  emit(false);
}

export function undo() {
  const prev = state.past[state.past.length - 1];
  if (!prev) return;
  state = { ...state, doc: prev.doc, past: state.past.slice(0, -1), future: [state.current, ...state.future], current: prev };
  pruneSelection();
  toast(`Undo`);
  emit(true);
}

export function redo() {
  const next = state.future[0];
  if (!next) return;
  state = { ...state, doc: next.doc, past: [...state.past, state.current], future: state.future.slice(1), current: next };
  pruneSelection();
  toast(`Redo: ${next.label}`);
  emit(true);
}

/** Jump to any point in history (past index, or -1 for current). */
export function jumpTo(index: number, inFuture: boolean) {
  const all = [...state.past, state.current, ...state.future];
  const target = inFuture ? state.past.length + 1 + index : index;
  state = { ...state, past: all.slice(0, target), current: all[target], future: all.slice(target + 1), doc: all[target].doc };
  pruneSelection();
  emit(true);
}

export function setUi(patch: Partial<UiState>) {
  state = { ...state, ui: { ...state.ui, ...patch } };
  emit(false);
}

let toastTimer: number | undefined;
export function toast(msg: string) {
  clearTimeout(toastTimer);
  state = { ...state, ui: { ...state.ui, toast: msg } };
  toastTimer = window.setTimeout(() => setUi({ toast: null }), 2200);
  emit(false);
}

function pruneSelection() {
  const { doc, ui } = state;
  const sel = ui.selection.filter((id) => doc.instances[id]);
  const sceneId = ui.sceneId && doc.scenes[ui.sceneId] ? ui.sceneId : doc.sceneOrder[0] ?? null;
  if (sel.length !== ui.selection.length || sceneId !== ui.sceneId) state = { ...state, ui: { ...ui, selection: sel, sceneId } };
}

export function replaceDoc(doc: LobsDoc, label: string) {
  commit(label, (d) => {
    for (const k of Object.keys(d)) delete (d as any)[k];
    Object.assign(d, structuredClone(doc));
  });
  setUi({ sceneId: doc.sceneOrder[0] ?? null, selection: [] });
}

export function newDoc() {
  replaceDoc(emptyDoc(), 'New document');
}

/** Live lock: while streaming/recording, moving layers needs Alt held (or an explicit unlock). */
export function liveLocked(altKey: boolean) {
  return state.ui.live && !state.ui.liveLockOverride && !altKey;
}
