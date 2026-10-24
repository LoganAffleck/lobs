// Renders og.html to public/og.png, the 1200x630 card link previews show (Discord, X, Slack).
//   npm run og
// Starts its own Vite dev server, like shots.mjs; uses Playwright's Chromium, else Edge or Chrome.
import { createServer } from 'vite'
import { chromium } from 'playwright'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const server = await createServer({ root, server: { port: 5331, strictPort: false }, logLevel: 'error' })
await server.listen()
const base = server.resolvedUrls.local[0].replace(/\/$/, '')

async function launch() {
  for (const opts of [{}, { channel: 'msedge' }, { channel: 'chrome' }]) {
    try { return await chromium.launch(opts) } catch {}
  }
  throw new Error('No Chromium found. Run `npx playwright install chromium`.')
}
const browser = await launch()
try {
  const page = await browser.newPage({ viewport: { width: 1200, height: 630 }, deviceScaleFactor: 1 })
  await page.goto(`${base}/og.html`)
  await page.waitForFunction(() => window.__ogReady === true, null, { timeout: 15000 })
  await page.evaluate(() => Promise.all([...document.images].map((i) => i.decode().catch(() => {}))))
  await page.screenshot({ path: join(root, 'public/og.png') })
  console.log('public/og.png')
} finally {
  await browser.close()
  await server.close()
}
