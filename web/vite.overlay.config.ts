import { defineConfig } from 'vite'
import { resolve } from 'node:path'

// The gem browser source (overlays/gem.html): one classic script, since a page opened from a
// file can't load module scripts. `npx vite build --config vite.overlay.config.ts`
export default defineConfig({
  publicDir: false, // the site's public files stay out of overlays
  build: {
    outDir: resolve(__dirname, '../overlays'),
    emptyOutDir: false,
    lib: { entry: resolve(__dirname, 'src/overlay/gem-overlay.ts'), name: 'LobsGem', formats: ['iife'], fileName: () => 'gem.js' },
  },
})
