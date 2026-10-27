import { mountHeroGem } from '../gem/hero'
import { LOGO } from '../gem/logo'

// The site's logo gem as a browser source for LOBS or OBS: full page, transparent, turning.
// Options in the URL: ?turn=12 (degrees a second), ?size=1 (zoom; under 1 leaves room around it).
const q = new URLSearchParams(location.search)
const canvas = document.createElement('canvas')
const size = Math.max(0.2, +(q.get('size') ?? 1))
Object.assign(canvas.style, { position: 'fixed', inset: '0', width: '100vw', height: '100vh', display: 'block', transform: `scale(${size})` })
document.body.appendChild(canvas)
mountHeroGem(canvas, {
  transparent: true,
  turn: +(q.get('turn') ?? 12),
  // The logo's own lights (the hero lowers them under the page's editor preview).
  look: { keyElevation: LOGO.keyElevation, keyAzimuth: LOGO.keyAzimuth, lightHeight: LOGO.lightHeight },
})
