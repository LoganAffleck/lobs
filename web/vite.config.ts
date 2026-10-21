import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { resolve } from 'node:path'

// Two client entries: the site (index.html; every route is prerendered from it by
// scripts/prerender.mjs) and the screenshot harness (shot.html), which renders one
// LOBS UI state full-window for scripts/shots.mjs.
export default defineConfig({
  plugins: [react()],
  server: { port: 5320, strictPort: true },
  preview: { port: 5321, strictPort: true },
  build: {
    rollupOptions: {
      input: { index: resolve(__dirname, 'index.html'), shot: resolve(__dirname, 'shot.html'), gem: resolve(__dirname, 'gem.html') },
    },
  },
})
