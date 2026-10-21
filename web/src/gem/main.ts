import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js'
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js'
import { TexturePass } from 'three/examples/jsm/postprocessing/TexturePass.js'
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js'
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js'
import { FullScreenQuad } from 'three/examples/jsm/postprocessing/Pass.js'
import GUI, { type Controller } from 'lil-gui'
import { applyLook, buildCut, DEFAULTS, finishShader, makeMaterial, type GemParams } from './gem'
import { ShaderPass } from 'three/examples/jsm/postprocessing/ShaderPass.js'
import { LOGO } from './logo'

// Gem lab: tweak the LOBS logo stone live, check it at icon sizes, export it.
// Each frame adds a few random wavelengths and frost directions per pixel; while nothing
// changes, frames average into a smooth render. Settings persist in the URL hash and browser.

const PRESETS: Record<string, Partial<GemParams>> = {
  Amethyst: {},
  Tunic: {
    shape: 'crystal', sides: 6, length: 1.3, tip: 0.95, ior: 1.75, dispersion: 0.5, color: '#8f6cff', density: 0.9, glow: '#b98cff', diffusion: 0.1,
    frost: 0.12, bounces: 8, surface: 0.7, edge: 1.1, edgeWidth: 0.018, edgeColor: '#fff5fb', edgeRainbow: 0.6, lights: 7, lightSize: 0.22, lightPower: 2.2, contrast: 0,
    rainbow: 1, hue: 0.7, key: 6, keySize: 0.38, keyAzimuth: 30, keyElevation: 25, keyColor: '#fff0f8', sky: '#4a3a9a', floor: '#1c1248', exposure: 0.85,
    bloom: 0.6, bloomRadius: 0.7, bloomThreshold: 1.1, background: '#2b2160', view: 'side', tilt: -28, roll: 24, angle: 15,
  },
  'LOBS logo': LOGO,
  D20: {
    shape: 'd20', ior: 1.75, dispersion: 0.3, color: '#a06cff', density: 0.8, glow: '#c08cff', diffusion: 0.12, frost: 0.14, bounces: 8, surface: 0.8,
    edge: 0.7, edgeWidth: 0.012, edgeColor: '#ffe8fb', edgeRainbow: 0.3, lights: 7, lightSize: 0.25, lightPower: 2.4, contrast: 0, rainbow: 1, hue: 0.72,
    key: 4, keySize: 0.45, keyAzimuth: -40, keyElevation: 45, keyColor: '#e8fff4', sky: '#4a3a9a', floor: '#1c1248', exposure: 0.9,
    bloom: 0.45, bloomRadius: 0.6, bloomThreshold: 1.2, background: '#1b0f5a', view: 'top', camAz: 0, camEl: 90, angle: 0, tilt: 0, roll: 0, fov: 8,
  },
  'Current logo (5-sided)': { sides: 5, table: 0.5, crown: 0.22, pavilion: 0.7, crownFacets: false, pavilionFacets: false, ior: 1.7, dispersion: 0.03, color: '#9c5cf0', density: 1.6, diffusion: 0.25, rainbow: 0, view: 'top' },
  Diamond: { color: '#ffffff', density: 0.05, glow: '#ffffff', diffusion: 0, ior: 2.42, dispersion: 0.11, rainbow: 0, lightPower: 9, sky: '#1a1a1f', bloom: 0.45 },
  Aurora: { color: '#c9b8ff', density: 0.5, ior: 2.2, dispersion: 0.25, rainbow: 1, hue: 0.45, lights: 9, glow: '#8ff0ff', diffusion: 0.12, frost: 0.06, edge: 0.6 },
  'Opal glow': { color: '#e8d8ff', density: 0.4, diffusion: 0.9, glow: '#ffb8f0', dispersion: 0.2, rainbow: 0.8, hue: 0.8, frost: 0.3, bloom: 0.9, bloomRadius: 0.8, key: 5 },
  'Deep violet': { color: '#6a2be0', density: 2.6, glow: '#7b3cff', diffusion: 0.35, lightPower: 10, rainbow: 0.2, bloom: 0.7 },
}

const KEY = 'lobs-gem'
function load(): GemParams {
  try {
    if (location.hash.length > 1) return { ...DEFAULTS, ...JSON.parse(decodeURIComponent(location.hash.slice(1))) }
  } catch {}
  try { const s = localStorage.getItem(KEY); if (s) return { ...DEFAULTS, ...JSON.parse(s) } } catch {}
  return { ...DEFAULTS }
}
const p = load()

const canvas = document.querySelector<HTMLCanvasElement>('#stage')!
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, preserveDrawingBuffer: true, alpha: true })
renderer.setPixelRatio(Math.min(devicePixelRatio, 2))
// Neutral keeps saturated purples purple (ACES pushes them toward blue).
renderer.toneMapping = THREE.NeutralToneMapping

const scene = new THREE.Scene()
const camera = new THREE.PerspectiveCamera(30, 1, 0.1, 400)
const controls = new OrbitControls(camera, canvas)
controls.enableDamping = true
controls.enablePan = false
controls.minDistance = 2
controls.maxDistance = 200

const material = makeMaterial()
const gem = new THREE.Mesh(new THREE.BufferGeometry(), material)
gem.rotation.order = 'ZXY' // spin about the stone's own axis, then tilt, then roll
scene.add(gem)

// ---------------- accumulation ----------------
const rtOpts = { type: THREE.HalfFloatType, depthBuffer: true }
const sampleRT = new THREE.WebGLRenderTarget(1, 1, rtOpts)
const accRT = [new THREE.WebGLRenderTarget(1, 1, rtOpts), new THREE.WebGLRenderTarget(1, 1, rtOpts)]
let accRead = 0
let samples = 0
const MAX_SAMPLES = 400
const blend = new FullScreenQuad(new THREE.ShaderMaterial({
  uniforms: { tNew: { value: null }, tPrev: { value: null }, uW: { value: 1 } },
  vertexShader: 'varying vec2 vUv; void main() { vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }',
  fragmentShader: 'uniform sampler2D tNew, tPrev; uniform float uW; varying vec2 vUv; void main() { gl_FragColor = mix(texture2D(tPrev, vUv), texture2D(tNew, vUv), uW); }',
}))
const blendU = (blend.material as THREE.ShaderMaterial).uniforms

let seed = 0
function addSample() {
  material.uniforms.uSeed.value = ++seed
  renderer.setRenderTarget(sampleRT)
  renderer.render(scene, camera)
  const write = 1 - accRead
  blendU.tNew.value = sampleRT.texture
  blendU.tPrev.value = accRT[accRead].texture
  blendU.uW.value = 1 / (samples + 1)
  renderer.setRenderTarget(accRT[write])
  blend.render(renderer)
  renderer.setRenderTarget(null)
  accRead = write
  samples++
}
const reset = () => { samples = 0 }

const composer = new EffectComposer(renderer)
const texPass = new TexturePass(accRT[0].texture)
composer.addPass(texPass)
const bloom = new UnrealBloomPass(new THREE.Vector2(1, 1), p.bloom, p.bloomRadius, p.bloomThreshold)
composer.addPass(bloom)
composer.addPass(new OutputPass())
const finish = new ShaderPass(finishShader)
composer.addPass(finish)

function setSizes(w: number, h: number) {
  renderer.setSize(w, h, false)
  const pr = renderer.getPixelRatio()
  for (const rt of [sampleRT, ...accRT]) rt.setSize(Math.floor(w * pr), Math.floor(h * pr))
  composer.setSize(w, h)
  bloom.resolution.set(w, h)
  camera.aspect = w / h
  camera.updateProjectionMatrix()
  reset()
}

// ---------------- the stone ----------------
let cutKey = ''
let radius = 1.2
function rebuildCut() {
  const key = JSON.stringify([p.shape, p.sides, p.table, p.crown, p.pavilion, p.girdle, p.crownFacets, p.pavilionFacets, p.culet, p.length, p.tip])
  if (key === cutKey) return
  const fresh = cutKey === ''
  cutKey = key
  const cut = buildCut(p)
  gem.geometry.dispose()
  gem.geometry = cut.geometry
  cut.planes.forEach((q, i) => material.uniforms.uPlanes.value[i].copy(q))
  material.uniforms.uPlaneCount.value = cut.planes.length
  facetInfo.textContent = `${cut.planes.length} facets`
  const changedSize = Math.abs(cut.radius - radius) > 0.05
  radius = cut.radius
  if (changedSize && !fresh) placeCamera()
}

const VIEWS = { top: [0, 90], tilted: [0, 38.5], side: [0, 5.7] } as const
function setView(v: GemParams['view']) {
  ;[p.camAz, p.camEl] = VIEWS[v]
  placeCamera()
  spinAngle = 0
}

/** Puts the camera on its orbit from camAz / camEl, at a distance that fits the stone. */
function placeCamera() {
  // Distance that keeps the stone the same size whatever the lens.
  camera.fov = p.fov
  camera.updateProjectionMatrix()
  const d = (radius * 1.35) / Math.tan(D(p.fov) / 2), az = D(p.camAz), el = D(Math.min(89.99, Math.max(-89.99, p.camEl)))
  camera.position.set(Math.sin(az) * Math.cos(el) * d, Math.sin(el) * d, Math.cos(az) * Math.cos(el) * d)
  controls.target.set(0, 0, 0)
  camera.lookAt(0, 0, 0)
  controls.update()
  reset()
}

// Dragging the view writes the orbit back into the settings (so links keep the exact angle).
let placing = false
function readCamera() {
  if (placing) return
  const v = camera.position.clone().normalize()
  p.camAz = Math.round(THREE.MathUtils.radToDeg(Math.atan2(v.x, v.z)) * 10) / 10
  p.camEl = Math.round(THREE.MathUtils.radToDeg(Math.asin(v.y)) * 10) / 10
  camControllers.forEach((c) => c.updateDisplay())
}

/** Rounds the orbit and the stone's pose to the nearest 15°, so the silhouette sits square. */
function straighten() {
  const r = (x: number) => Math.round(x / 15) * 15
  p.camAz = r(p.camAz); p.camEl = r(p.camEl)
  p.angle = ((r(p.angle) % 360) + 360) % 360; p.tilt = r(p.tilt); p.roll = r(p.roll)
  p.spin = 0; spinAngle = 0
  gui.controllersRecursive().forEach((c) => c.updateDisplay())
  placing = true; placeCamera(); placing = false
  save()
}

function save() {
  const json = JSON.stringify(p)
  try { localStorage.setItem(KEY, json) } catch {}
  history.replaceState(null, '', '#' + encodeURIComponent(json))
}

// ---------------- controls ----------------
const gui = new GUI({ title: 'LOBS gem' })
const presetPick = { preset: 'Amethyst' }
gui.add(presetPick, 'preset', Object.keys(PRESETS)).name('Start from').onChange((name: string) => {
  Object.assign(p, DEFAULTS, PRESETS[name])
  gui.controllersRecursive().forEach((c) => c.updateDisplay())
  rebuildCut()
  // A preset with its own camera angle keeps it; others get their named view.
  if (PRESETS[name].camEl !== undefined) { placing = true; placeCamera(); placing = false } else setView(p.view)
  changed()
})

const cutF = gui.addFolder('Cut')
cutF.add(p, 'shape', ['brilliant', 'crystal', 'bipyramid', 'd20']).name('Shape')
const sidesC = cutF.add(p, 'sides', 3, 12, 1).name('Sides')
const brilliantOnly: Controller[] = [
  cutF.add(p, 'table', 0.15, 0.9, 0.01).name('Table width'),
  cutF.add(p, 'girdle', 0, 0.3, 0.005).name('Girdle'),
  cutF.add(p, 'culet', 0, 0.4, 0.01).name('Culet'),
  cutF.add(p, 'crownFacets').name('Crown facets'),
  cutF.add(p, 'pavilionFacets').name('Pavilion facets'),
]
const notCrystal: Controller[] = [
  cutF.add(p, 'crown', 0.05, 0.8, 0.01).name('Crown height'),
  cutF.add(p, 'pavilion', 0.2, 1.6, 0.01).name('Pavilion depth'),
]
const crystalOnly: Controller[] = [cutF.add(p, 'length', 0.2, 3, 0.01).name('Length')]
const tipC = cutF.add(p, 'tip', 0, 2, 0.01).name('Tip height')
function showCutControls() {
  const d20 = p.shape === 'd20'
  sidesC.show(!d20)
  brilliantOnly.forEach((c) => c.show(p.shape === 'brilliant'))
  notCrystal.forEach((c) => c.show(p.shape !== 'crystal' && !d20))
  crystalOnly.forEach((c) => c.show(p.shape === 'crystal'))
  tipC.show(p.shape !== 'brilliant' && !d20)
}

const stone = gui.addFolder('Stone')
stone.addColor(p, 'color').name('Body color')
stone.add(p, 'density', 0, 4, 0.01).name('Color depth')
stone.add(p, 'ior', 1.3, 2.8, 0.01).name('Refraction (IOR)')
stone.add(p, 'dispersion', 0, 0.8, 0.001).name('Dispersion (fire)')
stone.add(p, 'frost', 0, 1, 0.005).name('Frost (diffusion)')
stone.add(p, 'diffusion', 0, 1.5, 0.01).name('Inner glow')
stone.addColor(p, 'glow').name('Glow color')
stone.add(p, 'surface', 0, 2, 0.01).name('Surface shine')
stone.add(p, 'bounces', 1, 12, 1).name('Bounces')
stone.add(p, 'edge', 0, 4, 0.01).name('Edge glow')
stone.add(p, 'edgeWidth', 0.002, 0.1, 0.001).name('Edge width')
stone.addColor(p, 'edgeColor').name('Edge color')
stone.add(p, 'edgeRainbow', 0, 1, 0.01).name('Edge rainbow')

const light = gui.addFolder('Light')
light.add(p, 'key', 0, 30, 0.1).name('Key light')
light.add(p, 'keySize', 0.1, 1.5, 0.01).name('Key size')
light.add(p, 'keyAzimuth', -180, 180, 1).name('Key direction (°)')
light.add(p, 'keyElevation', -60, 90, 1).name('Key height (°)')
light.addColor(p, 'keyColor').name('Key color')
light.add(p, 'lights', 0, 12, 1).name('Small lights')
light.add(p, 'lightSize', 0.03, 0.8, 0.01).name('Small light size')
light.add(p, 'lightPower', 0, 30, 0.1).name('Small light brightness')
light.add(p, 'rainbow', 0, 1, 0.01).name('Rainbow lights')
light.add(p, 'hue', 0, 1, 0.01).name('Hue')
light.add(p, 'lightTurn', 0, 360, 1).name('Lights turn (°)')
light.add(p, 'lightHeight', -60, 60, 1).name('Lights height (°)')
const lightTurnC = light.controllers.at(-2)!
light.add(p, 'lightSpin', 0, 2, 0.01).name('Light spin').onChange((v: number) => {
  // Stopping the spin keeps the lights where it left them.
  if (v === 0 && lightPhase !== 0) { p.lightTurn = Math.round((((p.lightTurn + THREE.MathUtils.radToDeg(lightPhase)) % 360) + 360) % 360); lightPhase = 0; lightTurnC.updateDisplay() }
})
light.add(p, 'contrast', 0, 1.2, 0.01).name('Contrast (viewer shadow)')
light.addColor(p, 'sky').name('Sky')
light.addColor(p, 'floor').name('Floor')
light.add(p, 'exposure', 0.2, 3, 0.01).name('Exposure')

const look = gui.addFolder('Glow & pose')
look.add(p, 'bayer').name('Bayer sampling (no crawl)')
look.add(p, 'ditherLevels', 0, 16, 1).name('Bayer posterize (0 = off)')
look.add(p, 'bloom', 0, 3, 0.01).name('Bloom')
look.add(p, 'bloomRadius', 0, 1.5, 0.01).name('Bloom radius')
look.add(p, 'bloomThreshold', 0, 3, 0.01).name('Bloom threshold')
look.addColor(p, 'background').name('Background')
look.add(p, 'view', ['top', 'tilted', 'side']).name('View').onChange(setView)
look.add(p, 'angle', 0, 360, 1).name('Angle (°)')
look.add(p, 'tilt', -90, 90, 1).name('Tilt (°)')
look.add(p, 'roll', -90, 90, 1).name('Roll (°)')
look.add(p, 'spin', 0, 1, 0.01).name('Turntable (Space)')
const camControllers = [
  look.add(p, 'camAz', -180, 180, 0.5).name('Camera turn (°)').onChange(() => { placing = true; placeCamera(); placing = false }),
  look.add(p, 'camEl', -89, 90, 0.5).name('Camera height (°)').onChange(() => { placing = true; placeCamera(); placing = false }),
]
look.add(p, 'fov', 3, 60, 1).name('Lens (° of view)').onChange(() => { placing = true; placeCamera(); placing = false })
look.add({ straighten }, 'straighten').name('Straighten (snap to 15°)')
look.add({ reset: () => setView(p.view) }, 'reset').name('Reset camera')

const actions = {
  png: () => exportPng(1024),
  pngClear: () => exportPng(1024, true),
  json: () => navigator.clipboard.writeText(JSON.stringify(p, null, 2)).then(() => toast('Settings copied')),
  link: () => navigator.clipboard.writeText(location.href).then(() => toast('Link copied')),
  reset: () => { Object.assign(p, DEFAULTS); gui.controllersRecursive().forEach((c) => c.updateDisplay()); rebuildCut(); setView(p.view); changed() },
}
const out = gui.addFolder('Export')
out.add(actions, 'png').name('PNG 1024 (with background)')
out.add(actions, 'pngClear').name('PNG 1024 (transparent, no bloom)')
out.add(actions, 'json').name('Copy settings JSON')
out.add(actions, 'link').name('Copy share link')
out.add(actions, 'reset').name('Reset')

gui.onChange(changed)
function changed() {
  rebuildCut()
  showCutControls()
  bloom.strength = p.bloom
  finish.uniforms.uLevels.value = p.ditherLevels
  bloom.radius = p.bloomRadius
  bloom.threshold = p.bloomThreshold
  document.body.style.background = p.background
  reset()
  save()
}
controls.addEventListener('change', () => { reset(); readCamera() })
// A finished drag is a change worth keeping in the link.
controls.addEventListener('end', save)

// ---------------- icon-size previews ----------------
const facetInfo = document.querySelector<HTMLElement>('#facets')!
const sampleInfo = document.querySelector<HTMLElement>('#samples')!
const sizes = [16, 24, 32, 64, 128]
const previews = sizes.map((s) => {
  const c = document.createElement('canvas')
  c.width = c.height = s * 2
  c.style.width = c.style.height = `${s}px`
  c.title = `${s}px`
  document.querySelector('#previews')!.append(c)
  return c
})
function drawPreviews() {
  const w = canvas.width, h = canvas.height, side = Math.min(w, h) * CROP
  for (const c of previews) {
    const g = c.getContext('2d')!
    g.imageSmoothingQuality = 'high'
    g.clearRect(0, 0, c.width, c.height)
    g.drawImage(canvas, (w - side) / 2, (h - side) / 2, side, side, 0, 0, c.width, c.height)
  }
}

function toast(t: string) {
  const el = document.querySelector<HTMLElement>('#toast')!
  el.textContent = t
  el.style.opacity = '1'
  setTimeout(() => (el.style.opacity = '0'), 1400)
}

// The stone fills about three quarters of the view; this crop (a share of the short side)
// frames it with a little room, for previews and exports alike.
const CROP = 0.82

/** A converged square render at `size`, cropped to the stone, as a PNG data URL. */
function renderPng(size: number, transparent = false, passes = 160): string {
  const pr = renderer.getPixelRatio()
  renderer.setPixelRatio(1)
  setSizes(size / CROP, size / CROP)
  pose(0)
  if (transparent) { scene.background = null; renderer.setClearColor(0x000000, 0); bloom.enabled = false }
  for (let i = 0; i < passes; i++) addSample()
  texPass.map = accRT[accRead].texture
  composer.render()
  const crop = document.createElement('canvas')
  crop.width = crop.height = size
  const off = (canvas.width - size) / 2
  crop.getContext('2d')!.drawImage(canvas, off, (canvas.height - size) / 2, size, size, 0, 0, size, size)
  const url = crop.toDataURL('image/png')
  bloom.enabled = true
  renderer.setPixelRatio(pr)
  resize()
  return url
}

function exportPng(size: number, transparent = false) {
  const a = document.createElement('a')
  a.download = `lobs-gem-${size}${transparent ? '-transparent' : ''}.png`
  a.href = renderPng(size, transparent)
  a.click()
}

// For scripts/logo.mjs: render the current look without the UI.
declare global { interface Window { __gemRender?: typeof renderPng } }
window.__gemRender = renderPng

// ---------------- loop ----------------
function resize() { setSizes(canvas.clientWidth, canvas.clientHeight) }
addEventListener('resize', resize)

const clock = new THREE.Clock()
const bg = new THREE.Color()
let spinAngle = 0
let lightPhase = 0
// Space starts or stops the turntable; stopping folds the turn into Angle so the pose stays.
let lastSpin = 0.15
addEventListener('keydown', (e) => {
  if (e.code !== 'Space' || (e.target as HTMLElement).closest('.lil-gui')) return
  e.preventDefault()
  if (p.spin > 0) { lastSpin = p.spin; p.spin = 0; p.angle = Math.round((p.angle + THREE.MathUtils.radToDeg(spinAngle)) % 360 + 360) % 360; spinAngle = 0 }
  else p.spin = lastSpin
  gui.controllersRecursive().forEach((c) => c.updateDisplay())
  save()
})

const inv = new THREE.Matrix4()
const D = THREE.MathUtils.degToRad
function pose(dt: number) {
  spinAngle += dt * p.spin
  gem.rotation.set(D(p.tilt), D(p.angle) + spinAngle, D(p.roll))
  gem.updateMatrixWorld()
  inv.copy(gem.matrixWorld).invert()
  material.uniforms.uCamObj.value.copy(camera.position).applyMatrix4(inv)
  material.uniforms.uObjToWorld.value.setFromMatrix4(gem.matrixWorld)
  material.uniforms.uViewer.value.copy(camera.position).normalize()
  lightPhase += dt * p.lightSpin
  applyLook(material, p, lightPhase)
  scene.background = bg.set(p.background)
}
function frame() {
  controls.update()
  const dt = clock.getDelta()
  pose(dt)
  // Moving (turntable, spinning lights): keep a short running average instead of converging.
  if (p.spin > 0 || p.lightSpin > 0) samples = Math.min(samples, 2)
  if (samples < MAX_SAMPLES) addSample()
  texPass.map = accRT[accRead].texture
  composer.render()
  drawPreviews()
  sampleInfo.textContent = samples >= MAX_SAMPLES ? 'converged' : `${samples} samples`
  requestAnimationFrame(frame)
}

rebuildCut()
showCutControls()
placing = true; placeCamera(); placing = false
changed()
resize()
frame()
