import * as THREE from 'three'
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js'
import { TexturePass } from 'three/examples/jsm/postprocessing/TexturePass.js'
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js'
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js'
import { ShaderPass } from 'three/examples/jsm/postprocessing/ShaderPass.js'
import { FullScreenQuad } from 'three/examples/jsm/postprocessing/Pass.js'
import { applyLook, buildCut, DEFAULTS, finishShader, makeMaterial, type GemParams } from './gem'
import { LOGO } from './logo'

// The logo gem for the site hero: the gem lab's ray tracer with the locked look, turning slowly
// through a loop of angles that catch the light. Kept light: capped resolution, fewer bounces,
// one sample a frame blended into a short running average (smooth frost without full
// accumulation), and no work at all when it's off screen or the tab is hidden.

// The editor preview covers the stone's middle, so only its rim shows. The lights sit low, so
// their reflections and bloom land on the part below the preview.
const HERO_LIGHT: Partial<GemParams> = { keyElevation: -30, keyAzimuth: 0, lightHeight: -60 }

export interface HeroGemOptions {
  onReady?: () => void
  /** Element that takes pointer input (hover to lean, drag to spin). Defaults to the canvas. */
  target?: HTMLElement
  /** Look settings over the hero's own (the overlay drops the hero's low lights). */
  look?: Partial<GemParams>
  /** Turn speed in degrees a second (the hero: 2.4). */
  turn?: number
  /** Draw on transparent instead of black: the stone keeps its brightness as alpha (overlay). */
  transparent?: boolean
}

export function mountHeroGem(canvas: HTMLCanvasElement, opts: HeroGemOptions): () => void {
  // Drawn on black and composited with mix-blend-mode: screen (see site.css), so the stone and
  // its bloom add light to whatever is behind the canvas, with no visible box.
  // Style, switchable from the page URL to compare: ?gem=smooth (default: random sampling
  // averaged over many frames, which the slow turn allows), ?gem=bayer (an ordered, screen-locked
  // pattern), ?gem=retro (that plus Bayer posterizing), ?gem=noise (the old short average).
  const style = new URLSearchParams(location.search).get('gem') ?? 'smooth'
  const p: GemParams = {
    ...DEFAULTS, ...LOGO, bounces: 5, background: '#000000', exposure: (LOGO.exposure ?? 1) * 0.92,
    bayer: style === 'bayer' || style === 'retro', ditherLevels: style === 'retro' ? 6 : 0,
    ...HERO_LIGHT, ...opts.look,
  }
  // For tuning: ?gemlook=<JSON> overrides any look settings on the page.
  try { Object.assign(p, JSON.parse(new URLSearchParams(location.search).get('gemlook') ?? '{}')) } catch {}
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: !!opts.transparent, powerPreference: 'high-performance' })
  renderer.setPixelRatio(Math.min(devicePixelRatio, 1)) // it fills the screen: 1x is plenty
  renderer.toneMapping = THREE.NeutralToneMapping

  const scene = new THREE.Scene()
  scene.background = new THREE.Color(p.background)
  const camera = new THREE.PerspectiveCamera(p.fov, 1, 0.1, 100)
  const material = makeMaterial()
  const cut = buildCut(p)
  cut.planes.forEach((q, i) => material.uniforms.uPlanes.value[i].copy(q))
  material.uniforms.uPlaneCount.value = cut.planes.length
  const gem = new THREE.Mesh(cut.geometry, material)
  gem.rotation.order = 'ZXY'
  scene.add(gem)
  // Framed a little looser than the lab, so the bloom has room around the stone.
  const d = (cut.radius * 1.3) / Math.tan(THREE.MathUtils.degToRad(p.fov) / 2)
  camera.position.set(0, 0, d)
  camera.lookAt(0, 0, 0)

  const rtOpts = { type: THREE.HalfFloatType }
  const sampleRT = new THREE.WebGLRenderTarget(1, 1, { ...rtOpts, depthBuffer: true })
  const acc = [new THREE.WebGLRenderTarget(1, 1, rtOpts), new THREE.WebGLRenderTarget(1, 1, rtOpts)]
  let read = 0
  const blend = new FullScreenQuad(new THREE.ShaderMaterial({
    uniforms: { tNew: { value: null }, tPrev: { value: null }, uW: { value: 1 } },
    vertexShader: 'varying vec2 vUv; void main() { vUv = uv; gl_Position = vec4(position.xy, 0.0, 1.0); }',
    fragmentShader: 'uniform sampler2D tNew, tPrev; uniform float uW; varying vec2 vUv; void main() { gl_FragColor = mix(texture2D(tPrev, vUv), texture2D(tNew, vUv), uW); }',
  }))
  const bu = (blend.material as THREE.ShaderMaterial).uniforms

  const composer = new EffectComposer(renderer)
  const tex = new TexturePass(acc[0].texture)
  composer.addPass(tex)
  // A soft rim, not a halo: only the brightest edges and sparkles bloom.
  const bloom = new UnrealBloomPass(new THREE.Vector2(1, 1), 0.4, 0.35, 1.15)
  composer.addPass(bloom)
  composer.addPass(new OutputPass())
  // Last: the soft edge falloff plus dither or Bayer posterize (see finishShader).
  const finish = new ShaderPass(finishShader)
  finish.uniforms.uFalloff.value = 1
  finish.uniforms.uLevels.value = p.ditherLevels
  composer.addPass(finish)
  // On black, the image is light added to whatever is behind it: as premultiplied color, that's
  // the same picture with its brightest channel as alpha.
  if (opts.transparent) {
    renderer.setClearColor(0x000000, 0)
    composer.addPass(new ShaderPass({
      uniforms: { tDiffuse: { value: null } },
      vertexShader: 'varying vec2 vUv; void main() { vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
      fragmentShader: 'uniform sampler2D tDiffuse; varying vec2 vUv; void main() { vec3 c = texture2D(tDiffuse, vUv).rgb; gl_FragColor = vec4(c, max(c.r, max(c.g, c.b))); }',
    }))
  }

  let seed = 0
  function sample(weight: number) {
    material.uniforms.uSeed.value = ++seed
    renderer.setRenderTarget(sampleRT)
    renderer.render(scene, camera)
    bu.tNew.value = sampleRT.texture
    bu.tPrev.value = acc[read].texture
    bu.uW.value = weight
    renderer.setRenderTarget(acc[1 - read])
    blend.render(renderer)
    renderer.setRenderTarget(null)
    read = 1 - read
  }

  function resize() {
    const w = canvas.clientWidth, h = canvas.clientHeight
    if (!w || !h) return
    renderer.setSize(w, h, false)
    const pr = renderer.getPixelRatio()
    for (const rt of [sampleRT, ...acc]) rt.setSize(Math.floor(w * pr), Math.floor(h * pr))
    composer.setSize(w, h)
    bloom.resolution.set(w / 2, h / 2)
    camera.aspect = w / h
    camera.updateProjectionMatrix()
  }

  const D = THREE.MathUtils.degToRad
  const inv = new THREE.Matrix4()
  // A slow tour: a full turn every ~2.5 min, with the tilt and roll drifting on slower, unrelated
  // cycles, so it keeps finding new faces to light without ever repeating exactly.
  // On top of the tour: the pointer leans it (lean, eased), and drags spin it (spinAngle), with
  // inertia that settles back into the slow turn.
  const lean = { x: 0, y: 0, tx: 0, ty: 0 }
  // It arrives spinning (the intro) and the same inertia that settles a flick settles that.
  let spinAngle = -60, spinVel = matchMedia('(prefers-reduced-motion: reduce)').matches ? 0 : 110, dragging = false
  function pose(t: number) {
    gem.rotation.set(
      D(p.tilt + 14 * Math.sin(t * 0.06) + lean.y * 18),
      D(t * (opts.turn ?? 2.4) + spinAngle + lean.x * 22),
      D(9 * Math.sin(t * 0.04 + 1.3) - lean.x * 8),
    )
    gem.updateMatrixWorld()
    inv.copy(gem.matrixWorld).invert()
    material.uniforms.uCamObj.value.copy(camera.position).applyMatrix4(inv)
    material.uniforms.uObjToWorld.value.setFromMatrix4(gem.matrixWorld)
    material.uniforms.uViewer.value.copy(camera.position).normalize()
    applyLook(material, p, t * 0.05)
  }

  function draw() {
    finish.uniforms.uSeed.value = (seed * 7.31) % 97
    tex.map = acc[read].texture
    composer.render()
  }

  const still = matchMedia('(prefers-reduced-motion: reduce)').matches
  let visible = true, running = true, raf = 0, frames = 0
  const start = performance.now()
  // If frames come slower than ~45 fps (a weaker GPU), drop the render resolution a step,
  // down to 0.75x, rather than stutter. Measured over a second of frames at a time.
  let last = 0, slow = 0, count = 0, prevT = 0
  function adapt(now: number) {
    if (last) { count++; if (now - last > 22) slow++ }
    last = now
    if (count < 60) return
    const pr = renderer.getPixelRatio()
    if (slow > 30 && pr > 0.75) { renderer.setPixelRatio(Math.max(0.75, pr - 0.25)); resize(); frames = 0 }
    slow = 0; count = 0
  }

  function frame(now: number) {
    raf = 0
    if (!running || !visible || document.hidden) { last = 0; return }
    adapt(now)
    const dt = prevT ? Math.min(0.05, (now - prevT) / 1000) : 0
    prevT = now
    const t = (performance.now() - start) / 1000 + 8 // start on a good angle, not face-on
    const ease = 1 - Math.exp(-dt * 4)
    const dl = Math.abs(lean.tx - lean.x) + Math.abs(lean.ty - lean.y)
    lean.x += (lean.tx - lean.x) * ease
    lean.y += (lean.ty - lean.y) * ease
    if (!dragging) { spinAngle += spinVel * dt; spinVel *= Math.exp(-dt * 1.8) }
    pose(t)
    // Blend in each new sample; early frames weigh more so it settles fast. Fast moves (a flick,
    // a big lean) weigh the new frame more, so they don't smear.
    const motion = Math.min(0.55, Math.abs(spinVel) / 300 + dl * 1.5 + (dragging ? 0.3 : 0))
    sample(frames < 6 ? 1 / (frames + 1) : (style === 'smooth' ? 0.06 : 0.22) + motion)
    frames++
    draw()
    raf = requestAnimationFrame(frame)
  }
  const kick = () => { if (!raf && running && visible && !document.hidden && !still) raf = requestAnimationFrame(frame) }

  resize()
  if (still) {
    // Reduced motion: one converged still frame.
    pose(8)
    for (let i = 0; i < 48; i++) sample(1 / (i + 1))
    draw()
  } else {
    pose(8)
    for (let i = 0; i < 6; i++) sample(1 / (i + 1))
    draw()
    kick()
  }
  opts.onReady?.()

  const io = new IntersectionObserver(([e]) => { visible = e.isIntersecting; kick() })
  io.observe(canvas)
  const ro = new ResizeObserver(() => { resize(); frames = 0; if (still) { pose(8); for (let i = 0; i < 48; i++) sample(1 / (i + 1)); draw() } })
  ro.observe(canvas)
  const onVis = () => { prevT = 0; kick() }
  document.addEventListener('visibilitychange', onVis)

  // Pointer input. Clicks on the page's own controls, text and the editor model are left alone.
  const target = opts.target ?? canvas
  const skip = (e: PointerEvent) => !!(e.target as Element).closest?.('a, button, input, select, textarea, h1, p, .appframe, .ctas')
  let lastX = 0, lastMove = 0
  const onMove = (e: PointerEvent) => {
    const r = target.getBoundingClientRect()
    lean.tx = Math.max(-1, Math.min(1, ((e.clientX - r.left) / r.width - 0.5) * 2))
    lean.ty = Math.max(-1, Math.min(1, ((e.clientY - r.top) / Math.min(r.height, 900) - 0.4) * 2))
    if (dragging) {
      const dx = e.clientX - lastX, now = performance.now(), dt = Math.max(1, now - lastMove) / 1000
      spinAngle += dx * 0.4
      spinVel = spinVel * 0.6 + ((dx * 0.4) / dt) * 0.4
      lastX = e.clientX; lastMove = now
    }
  }
  const onDown = (e: PointerEvent) => {
    if (still || e.button !== 0 || skip(e)) return
    e.preventDefault() // a drag that crosses the headline shouldn't select it
    dragging = true; spinVel = 0
    lastX = e.clientX; lastMove = performance.now()
    target.setPointerCapture(e.pointerId)
    target.style.cursor = 'grabbing'
  }
  const onUp = () => { dragging = false; target.style.cursor = '' }
  const onLeave = () => { lean.tx = 0; lean.ty = 0 }
  if (!still) {
    target.addEventListener('pointermove', onMove)
    target.addEventListener('pointerdown', onDown)
    target.addEventListener('pointerup', onUp)
    target.addEventListener('pointercancel', onUp)
    target.addEventListener('pointerleave', onLeave)
    target.style.touchAction = 'pan-y' // horizontal swipes spin it; vertical ones still scroll
  }

  return () => {
    running = false
    if (raf) cancelAnimationFrame(raf)
    io.disconnect(); ro.disconnect()
    document.removeEventListener('visibilitychange', onVis)
    target.removeEventListener('pointermove', onMove)
    target.removeEventListener('pointerdown', onDown)
    target.removeEventListener('pointerup', onUp)
    target.removeEventListener('pointercancel', onUp)
    target.removeEventListener('pointerleave', onLeave)
    composer.dispose(); sampleRT.dispose(); acc.forEach((r) => r.dispose())
    cut.geometry.dispose(); material.dispose(); renderer.dispose()
  }
}
