// Renders the LOBS logo gem (the "LOBS logo" look, ray traced like the site hero) as frame
// sequences for the app's animations, into native/resources/gem/:
//   flip-<i>.jpg  a quarter turn about the horizontal axis, from the logo pose (Settings opening
//                 plays it forward, the gem rolling down; closing plays it back, rolling up)
//   spin-<i>.jpg  a third of a turn about the stone's own axis, which loops (the Setup welcome)
// On black: the app's shader (native/shaders/gem.frag) makes brightness the coverage. Each is
// rendered at twice its size and halved, for clean edges.
//   npm run gem-frames
import { createServer } from 'vite'
import { chromium } from 'playwright'
import { mkdirSync, writeFileSync } from 'node:fs'
import { join, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const out = join(root, '../native/resources/gem')
mkdirSync(out, { recursive: true })

const FLIP = { frames: 45, size: 64 }  // 2° apart; shown at 20 px in the top bar (64 for HiDPI)
const FLIP_DEGREES = 90
const SPIN = { frames: 120, size: 336 } // 1° apart; shown at 168 px on the welcome screen (2x)
// The d20 looks the same every 120 degrees about its own axis: a third of a turn loops.
const SPIN_DEGREES = 120

const server = await createServer({ root, server: { port: 5331, strictPort: false }, logLevel: 'error' })
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
  await page.goto(`${base}/gem.html`)
  await page.evaluate(() => localStorage.removeItem('lobs-gem'))
  await page.goto(`${base}/gem.html`)
  await page.waitForFunction(() => !!window.__gemRender && !!window.__gemSet)
  await page.evaluate(() => {
    const sel = document.querySelector('.lil-gui select')
    sel.value = [...sel.options].find((o) => o.textContent === 'LOBS logo').value
    sel.dispatchEvent(new Event('change'))
  })
  const logoTilt = 42 // LOGO.tilt (src/gem/logo.ts)

  const buf = (url) => Buffer.from(url.split(',')[1], 'base64')
  async function sequence(name, { frames, size }, poseAt) {
    for (let i = 0; i < frames; i++) {
      // Rendered on black with its bloom at twice the size, halved (clean edges), and faded to
      // black before the frame's edge so no square shows.
      const url = await page.evaluate(async ([o, size]) => {
        window.__gemSet(o)
        const img = new Image()
        await new Promise((r) => { img.onload = r; img.src = window.__gemRender(size * 2, false, 120) })
        const c = document.createElement('canvas')
        c.width = c.height = size
        const g = c.getContext('2d')
        g.imageSmoothingQuality = 'high'
        g.drawImage(img, 0, 0, size, size)
        const d = g.getImageData(0, 0, size, size), px = d.data
        for (let k = 0; k < px.length; k += 4) {
          const x = (k / 4) % size - size / 2 + 0.5, y = Math.floor(k / 4 / size) - size / 2 + 0.5, r = Math.hypot(x, y) / (size / 2)
          const fade = r < 0.8 ? 1 : r > 1 ? 0 : (1 - r) / 0.2
          px[k] *= fade; px[k + 1] *= fade; px[k + 2] *= fade
        }
        g.putImageData(d, 0, 0)
        return c.toDataURL('image/jpeg', 0.92)
      }, [poseAt(i, frames), size])
      writeFileSync(join(out, `${name}-${i}.jpg`), buf(url))
    }
    console.log(`wrote ${frames} ${name} frames at ${size} px`)
  }
  // Rolling down: the top of the stone comes toward you and over.
  // The roll ends exactly a quarter turn down; the spin's last frame stops one short of the first.
  await sequence('flip', FLIP, (i, n) => ({ tilt: logoTilt - FLIP_DEGREES * i / (n - 1), angle: 0, spin: 0 }))
  await sequence('spin', SPIN, (i, n) => ({ tilt: logoTilt, angle: SPIN_DEGREES * i / n, spin: 0 }))
} finally {
  await browser.close()
  await server.close()
}
