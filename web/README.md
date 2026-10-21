# LOBS website and UI model

The site (homepage and docs), and a React model of the LOBS editor that the site, the docs
screenshots and videos all render from.

```bash
npm install
npm run dev        # http://localhost:5320, docs at /docs/
npm run shots      # render every screenshot to public/shots (or: npm run shots -- crop tools)
npm run build      # static site in dist/, one prerendered page per route, links checked
```

## Layout

```
src/ui/        the editor model: state (model.ts), reducer (store.tsx), panels, dialogs, styles
src/shots/     catalog.ts names every screenshot as a UI state; shot.html renders one
src/video/     LobsVideo.tsx, the editor as a Canvas component
src/site/      homepage, docs pages, prerender entry
docs/          the docs, one Markdown file per page
scripts/       icons (from native/icons), shots, prerender, link check
```

## The UI model

`<LobsApp state={…} />` renders the editor from one plain object, `UiState` in
`src/ui/model.ts`: the show (scenes, sources, layers), what's selected, which tab, menu, dialog or
help card is open, the tool, the outputs and stats. Pass a partial state and it's merged over the
sample show. Clicks change it through the same reducer the app's controller verbs are named after
(`editScene`, `take`, `toggleLock` …).

Styles are the app's own tokens (`Pal` in `native/src/main.cpp`), and icons are generated from
`native/icons`, so the model draws what the app draws. When the QML changes, change the matching
file in `src/ui`.

## Screenshots

Add an entry to `SHOTS` in `src/shots/catalog.ts`, then `npm run shots -- <name>`. Options:
`view` renders one panel, `size` sets the window, `clip` crops to a region. Open
`/shot.html` in the dev server to see them all, or `/shot.html?shot=<name>` for one.

In docs, `![Caption](shot:name)` embeds a shot at its real size.

## Videos (Canvas)

From the agent-native-video repo:

```bash
node bin/canvas.mjs project import-component <project> ../LOBS/web/src/video/LobsVideo.tsx
```

Then add a `domCapture` layer with that asset, `pixelSize: [1520, 900]`, and props:

```json
{
  "shot": "editor-live",
  "live": { "from": 3725 },
  "keyframes": [
    { "at": 1, "actions": [{ "type": "editScene", "id": "sc_game" }, { "type": "selectLayer", "id": "ly_game_cam" }] },
    { "at": 2, "state": { "menu": { "id": "layer", "target": "ly_game_cam" } } }
  ]
}
```

`keyframes` apply in order once the layer's time passes `at`. `live.from` runs the LIVE and REC
timers and moves the meters without resetting clicks. The cursor can click the real UI: buttons
have `data-ui="<label>"`, rows `data-scene`, `data-source` or `data-layer="<name>"`. Preview a
state by hand with `/shot.html?video&t=2.5&props=<url-encoded JSON>`.

## Gem lab (logo)

`/gem.html` in the dev server: the LOBS gem, ray traced in three.js (`src/gem`). The stone is
convex, so the shader intersects each ray with the facet planes exactly and bounces it inside
with real Fresnel and total internal reflection, tracing R, G and B at slightly different
indices of refraction (the rainbow fire). Tweak the cut, stone, light and glow; check it at
16 to 128 px in the corner; export a 1024 px PNG. Settings live in the URL, so a link is a look.
