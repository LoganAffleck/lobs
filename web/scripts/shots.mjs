// Renders every shot in src/shots/catalog.ts to public/shots/<name>.png at 2x.
//   npm run shots              all of them
//   npm run shots -- crop tools   just these
// Starts its own Vite dev server, so nothing else needs to be running. Uses Playwright's
// Chromium if it's installed, else the system's Edge or Chrome.
import { createServer } from 'vite'
import { chromium } from 'playwright'
import { mkdirSync } from 'node:fs'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const out = join(root, 'public/shots')
mkdirSync(out, { recursive: true })

const server = await createServer({ root, server: { port: 5329, strictPort: false }, logLevel: 'error' })
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
  const page0 = await browser.newPage()
  await page0.goto(`${base}/shot.html`)
  const shots = await page0.evaluate(() => Object.fromEntries(Object.entries(window.__SHOTS__).map(([k, v]) => [k, { size: v.size ?? null, clip: v.clip ?? null }])))
  await page0.close()

  const wanted = process.argv.slice(2)
  const names = wanted.length ? wanted : Object.keys(shots)
  for (const name of names) {
    if (!(name in shots)) { console.error(`no shot "${name}"`); process.exitCode = 1; continue }
    const [width, height] = shots[name].size ?? [1520, 900]
    const c = shots[name].clip
    const ctx = await browser.newContext({ viewport: { width, height }, deviceScaleFactor: 2, colorScheme: 'dark' })
    const page = await ctx.newPage()
    const errors = []
    page.on('pageerror', (e) => errors.push(e.message))
    await page.goto(`${base}/shot.html?shot=${encodeURIComponent(name)}`)
    await page.waitForFunction(() => window.__shotReady !== undefined, null, { timeout: 15000 })
    await page.evaluate(() => window.__shotReady)
    // Hold animations at their end so menus and dialogs aren't caught mid-fade.
    await page.addStyleTag({ content: '*, *::before, *::after { animation-duration: 0s !important; animation-delay: 0s !important; transition: none !important; }' })
    await page.screenshot({ path: join(out, `${name}.png`), clip: c ? { x: c[0], y: c[1], width: c[2], height: c[3] } : undefined })
    await ctx.close()
    if (errors.length) { console.error(`${name}: ${errors.join('; ')}`); process.exitCode = 1 } else console.log(`${name}.png`)
  }
} finally {
  await browser.close()
  await server.close()
}
