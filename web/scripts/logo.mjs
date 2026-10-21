// Renders the LOBS logo from a gem lab look into public/brand/:
//   lobs-gem-1024.png (on its background), lobs-gem-<size>.png (transparent, 1024 → 16),
//   lobs.ico (16–256, for the Windows app). (The site's favicons are the flat mark, lobs-logo.svg.)
//   npm run logo                      the "LOBS logo" preset
//   npm run logo -- "<gem lab link>"  any look, by its share link
//   add --app to also install it as the LOBS app icon (native/resources: lobs.ico, icon/lobs-*.png)
import { createServer } from 'vite'
import { chromium } from 'playwright'
import { mkdirSync, writeFileSync, copyFileSync } from 'node:fs'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const out = join(root, 'public/brand')
mkdirSync(out, { recursive: true })

const args = process.argv.slice(2)
const app = args.includes('--app')
const link = args.find((a) => a.startsWith('http'))
const hash = link ? new URL(link).hash : ''

const server = await createServer({ root, server: { port: 5330, strictPort: false }, logLevel: 'error' })
await server.listen()
const base = server.resolvedUrls.local[0].replace(/\/$/, '')

let browser
for (const opts of [{ args: ['--use-angle=d3d11', '--enable-gpu'] }, { channel: 'msedge' }, { channel: 'chrome' }]) {
  try { browser = await chromium.launch(opts); break } catch {}
}
if (!browser) throw new Error('No Chromium found. Run `npx playwright install chromium`.')

try {
  const page = await browser.newPage({ viewport: { width: 900, height: 900 } })
  page.on('pageerror', (e) => { console.error(e.message); process.exitCode = 1 })
  // Start clean so the browser's saved look doesn't win over the link or preset.
  await page.goto(`${base}/gem.html`)
  await page.evaluate(() => localStorage.removeItem('lobs-gem'))
  await page.goto(`${base}/gem.html${hash}`)
  await page.waitForFunction(() => !!window.__gemRender)
  if (!hash) {
    await page.evaluate(() => {
      const sel = document.querySelector('.lil-gui select')
      sel.value = [...sel.options].find((o) => o.textContent === 'LOBS logo').value
      sel.dispatchEvent(new Event('change'))
    })
  }

  const shots = await page.evaluate(async () => {
    const r = window.__gemRender
    const opaque = r(1024, false, 400)
    const clear = r(1024, true, 400)
    // Step down by halves from the 1024 master: sharper small sizes than one big resize.
    const img = await new Promise((res) => { const i = new Image(); i.onload = () => res(i); i.src = clear })
    const sizes = [512, 256, 128, 64, 48, 40, 32, 24, 20, 16]
    const outs = {}
    let src = img, srcSize = 1024
    for (const s of sizes) {
      while (srcSize / 2 >= s) {
        const c = document.createElement('canvas')
        c.width = c.height = srcSize / 2
        const g = c.getContext('2d')
        g.imageSmoothingQuality = 'high'
        g.drawImage(src, 0, 0, c.width, c.height)
        src = c; srcSize = c.width
      }
      const c = document.createElement('canvas')
      c.width = c.height = s
      const g = c.getContext('2d')
      g.imageSmoothingQuality = 'high'
      g.drawImage(src, 0, 0, s, s)
      outs[s] = c.toDataURL('image/png')
    }
    return { opaque, clear, outs }
  })

  const buf = (url) => Buffer.from(url.split(',')[1], 'base64')
  writeFileSync(join(out, 'lobs-gem-1024.png'), buf(shots.opaque))
  writeFileSync(join(out, 'lobs-gem-1024-transparent.png'), buf(shots.clear))
  for (const [s, url] of Object.entries(shots.outs)) writeFileSync(join(out, `lobs-gem-${s}.png`), buf(url))

  // ICO with PNG-compressed entries (Windows Vista and later read these directly).
  const icoSizes = [16, 24, 32, 48, 64, 128, 256]
  const pngs = icoSizes.map((s) => buf(shots.outs[s]))
  const header = Buffer.alloc(6 + 16 * pngs.length)
  header.writeUInt16LE(0, 0); header.writeUInt16LE(1, 2); header.writeUInt16LE(pngs.length, 4)
  let offset = header.length
  pngs.forEach((png, i) => {
    const s = icoSizes[i], e = 6 + i * 16
    header.writeUInt8(s >= 256 ? 0 : s, e); header.writeUInt8(s >= 256 ? 0 : s, e + 1)
    header.writeUInt16LE(1, e + 4); header.writeUInt16LE(32, e + 6)
    header.writeUInt32LE(png.length, e + 8); header.writeUInt32LE(offset, e + 12)
    offset += png.length
  })
  writeFileSync(join(out, 'lobs.ico'), Buffer.concat([header, ...pngs]))
  if (app) {
    const res = join(root, '../native/resources')
    copyFileSync(join(out, 'lobs.ico'), join(res, 'lobs.ico'))
    for (const s of [16, 20, 24, 32, 40, 48, 64, 128, 256]) copyFileSync(join(out, `lobs-gem-${s}.png`), join(res, `icon/lobs-${s}.png`))
    console.log('installed native/resources/lobs.ico and icon/lobs-*.png')
  }
  console.log(`wrote public/brand: 1024 (opaque + transparent), ${Object.keys(shots.outs).join(', ')}, lobs.ico`)
} finally {
  await browser.close()
  await server.close()
}
