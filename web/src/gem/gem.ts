import * as THREE from 'three'
import { ConvexGeometry } from 'three/examples/jsm/geometries/ConvexGeometry.js'

// A cut gem, ray traced. The stone is convex, so it's exactly the intersection of its facet
// planes: the shader finds where a ray leaves by intersecting those planes (no meshes, no
// screen-space tricks), then reflects or refracts it with real Fresnel, up to N bounces inside.
//
// Each frame traces a few random wavelengths per pixel, each with its own index of refraction
// (the fire), through randomly roughened facets (frost). Frames average while nothing changes,
// so a still view converges to a smooth spectral render.

export type Shape = 'brilliant' | 'crystal' | 'bipyramid' | 'd20'

export interface GemParams {
  // Cut
  shape: Shape
  sides: number
  table: number
  crown: number
  pavilion: number
  girdle: number
  crownFacets: boolean
  pavilionFacets: boolean
  culet: number
  length: number
  tip: number
  // Stone
  ior: number
  dispersion: number
  color: string
  density: number
  glow: string
  diffusion: number
  frost: number
  bounces: number
  surface: number
  edge: number
  edgeWidth: number
  edgeColor: string
  edgeRainbow: number
  // Light
  lights: number
  lightSize: number
  lightPower: number
  contrast: number
  lightSpin: number
  /** Where the ring of small lights sits: turn around the stone and a height offset, degrees. */
  lightTurn: number
  lightHeight: number
  rainbow: number
  hue: number
  key: number
  keySize: number
  keyAzimuth: number
  keyElevation: number
  keyColor: string
  sky: string
  floor: string
  exposure: number
  // Look
  bloom: number
  bloomRadius: number
  bloomThreshold: number
  background: string
  view: 'top' | 'tilted' | 'side'
  spin: number
  angle: number
  tilt: number
  roll: number
  /** Camera orbit around the stone, degrees: turn around it, height above it. */
  camAz: number
  camEl: number
  /** Camera lens, degrees of view. Narrow is flatter (no perspective skew), good for logos. */
  fov: number
  /** Sampling pattern: random (smooth once converged, grainy while moving) or an 8×8 Bayer
   *  matrix (an ordered, screen-locked pattern that doesn't crawl). */
  bayer: boolean
  /** Posterize through the Bayer matrix to this many levels per channel (0 = off): a retro look. */
  ditherLevels: number
}

export const DEFAULTS: GemParams = {
  shape: 'brilliant', sides: 8, table: 0.55, crown: 0.32, pavilion: 0.86, girdle: 0.04, crownFacets: true, pavilionFacets: true, culet: 0, length: 1.6, tip: 0.8,
  ior: 2.0, dispersion: 0.08, color: '#c58cff', density: 1.0, glow: '#c48cff', diffusion: 0.15, frost: 0, bounces: 6, surface: 1, edge: 0, edgeWidth: 0.02, edgeColor: '#ffffff', edgeRainbow: 0,
  lights: 8, lightSize: 0.14, lightPower: 6.5, contrast: 0.45, lightSpin: 0, lightTurn: 0, lightHeight: 0, rainbow: 0.35, hue: 0.72,
  key: 0, keySize: 0.6, keyAzimuth: 40, keyElevation: 35, keyColor: '#ffe9f6',
  sky: '#5b4a7e', floor: '#0a0612', exposure: 1.1,
  bloom: 0.5, bloomRadius: 0.5, bloomThreshold: 1.0, background: '#111111', view: 'tilted', spin: 0, angle: 0, tilt: 0, roll: 0, camAz: 0, camEl: 38.5, fov: 30, bayer: false, ditherLevels: 0,
}

/** Points whose convex hull is the stone (y up). */
function cutPoints(p: GemParams): THREE.Vector3[] {
  const pts: THREE.Vector3[] = []
  const ring = (n: number, r: number, y: number, off: number) => {
    for (let i = 0; i < n; i++) {
      const a = off + (i / n) * Math.PI * 2
      pts.push(new THREE.Vector3(Math.cos(a) * r, y, Math.sin(a) * r))
    }
  }
  if (p.shape === 'd20') {
    // An icosahedron (a D20 die), turned so a face points up: from the top it's a hexagon.
    const f = (1 + Math.sqrt(5)) / 2
    const raw = [[-1, f, 0], [1, f, 0], [-1, -f, 0], [1, -f, 0], [0, -1, f], [0, 1, f], [0, -1, -f], [0, 1, -f], [f, 0, -1], [f, 0, 1], [-f, 0, -1], [-f, 0, 1]]
    const vs = raw.map(([x, y, z]) => new THREE.Vector3(x, y, z).normalize().multiplyScalar(1.15))
    const up = new THREE.Vector3(0, 1, f).add(new THREE.Vector3(0, -1, f)).add(new THREE.Vector3(f, 0, 1)).normalize()
    const q = new THREE.Quaternion().setFromUnitVectors(up, new THREE.Vector3(0, 1, 0))
    vs.forEach((v) => v.applyQuaternion(q))
    // Then turn it so an outline corner points to the top of the screen (-z from above):
    // a pointy-top hexagon at Angle 0.
    const corner = vs.reduce((a, v) => (Math.hypot(v.x, v.z) > Math.hypot(a.x, a.z) + 1e-6 ? v : a))
    const turn = new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0, 1, 0), Math.PI - Math.atan2(corner.x, corner.z))
    return vs.map((v) => v.applyQuaternion(turn))
  }
  const n = Math.round(p.sides)
  const step = Math.PI / n
  if (p.shape === 'crystal') {
    // A quartz-like prism with pyramid tips, standing up.
    const h = p.length / 2
    ring(n, 1, h, 0)
    ring(n, 1, -h, 0)
    pts.push(new THREE.Vector3(0, h + p.tip, 0), new THREE.Vector3(0, -h - p.tip * 0.85, 0))
    return pts
  }
  if (p.shape === 'bipyramid') {
    ring(n, 1, 0, 0)
    pts.push(new THREE.Vector3(0, p.crown + p.tip, 0), new THREE.Vector3(0, -p.pavilion - p.tip * 0.4, 0))
    return pts
  }
  const g = p.girdle / 2
  ring(n, p.table, p.crown, 0)
  if (p.crownFacets) ring(n, (p.table + 1) / 2 + 0.06, g + (p.crown - g) * 0.42, step)
  ring(2 * n, 1, g, step / 2)
  ring(2 * n, 1, -g, step / 2)
  if (p.pavilionFacets) ring(n, 0.52, -g - p.pavilion * 0.48, 0)
  if (p.culet > 0.001) ring(n, p.culet, -g - p.pavilion, 0)
  else pts.push(new THREE.Vector3(0, -g - p.pavilion, 0))
  return pts
}

export interface Cut { geometry: THREE.BufferGeometry; planes: THREE.Vector4[]; radius: number }

export const MAX_PLANES = 160

export function buildCut(p: GemParams): Cut {
  const pts = cutPoints(p)
  const geometry = new ConvexGeometry(pts)
  // One plane per facet: merge the hull's coplanar triangles.
  const pos = geometry.getAttribute('position')
  const planes: THREE.Vector4[] = []
  const a = new THREE.Vector3(), b = new THREE.Vector3(), c = new THREE.Vector3()
  for (let i = 0; i < pos.count; i += 3) {
    a.fromBufferAttribute(pos, i); b.fromBufferAttribute(pos, i + 1); c.fromBufferAttribute(pos, i + 2)
    const n = new THREE.Vector3().subVectors(b, a).cross(new THREE.Vector3().subVectors(c, a)).normalize()
    const d = n.dot(a)
    if (!planes.some((q) => Math.abs(q.x - n.x) + Math.abs(q.y - n.y) + Math.abs(q.z - n.z) < 1e-3 && Math.abs(q.w - d) < 1e-3)) planes.push(new THREE.Vector4(n.x, n.y, n.z, d))
  }
  const radius = Math.max(...pts.map((v) => v.length()))
  return { geometry, planes: planes.slice(0, MAX_PLANES), radius }
}

const vertex = /* glsl */ `
  varying vec3 vObj;
  void main() {
    vObj = position;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`

const fragment = /* glsl */ `
  #define MAX_PLANES ${MAX_PLANES}
  #define MAX_BOUNCES 12
  #define MAX_LIGHTS 12
  #define SAMPLES 3
  uniform vec4 uPlanes[MAX_PLANES];
  uniform int uPlaneCount;
  uniform vec3 uCamObj;
  uniform mat3 uObjToWorld;
  uniform float uIor, uDispersion, uDensity, uDiffusion, uSurface, uExposure, uFrost, uEdge, uEdgeWidth, uSeed, uEdgeRainbow;
  uniform vec3 uAbsorb, uGlow, uSky, uFloor, uEdgeColor, uSpecNorm;
  uniform int uBounces, uLightCount;
  uniform vec3 uLightDir[MAX_LIGHTS];
  uniform vec3 uLightColor[MAX_LIGHTS];
  uniform float uLightSize;
  uniform vec3 uViewer;
  uniform float uContrast;
  uniform vec3 uKeyDir, uKeyColor;
  uniform float uKeySize;
  uniform float uBayer;
  varying vec3 vObj;

  // ---- random numbers: a per-pixel, per-frame stream ----
  uint rngState;
  uint pcg(uint v) { uint s = v * 747796405u + 2891336453u; uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u; return (w >> 22u) ^ w; }
  float rnd() { rngState = pcg(rngState); return float(rngState) / 4294967295.0; }
  vec3 rndDir() { float z = rnd() * 2.0 - 1.0; float a = rnd() * 6.2831853; float r = sqrt(1.0 - z * z); return vec3(r * cos(a), z, r * sin(a)); }

  // Ordered dither threshold in [0, 1), from an 8×8 Bayer matrix (built from 2×2 steps).
  float bayer2(vec2 a) { a = floor(a); return fract(a.x / 2.0 + a.y * a.y * 0.75); }
  float bayer4(vec2 a) { return bayer2(0.5 * a) * 0.25 + bayer2(a); }
  float bayer8(vec2 a) { return bayer4(0.5 * a) * 0.25 + bayer2(a); }

  // Wavelength 0 (red, bends least) to 1 (violet, bends most), as an RGB weight that averages
  // to white over the whole range.
  vec3 spectrum(float x) {
    float r = exp(-pow((x - 0.1) / 0.22, 2.0)) + 0.3 * exp(-pow((x - 0.98) / 0.12, 2.0));
    float g = exp(-pow((x - 0.45) / 0.19, 2.0));
    float b = exp(-pow((x - 0.8) / 0.19, 2.0));
    return vec3(r, g, b) * uSpecNorm;
  }

  // The world the stone sees: a sky-to-floor gradient, a ring of small lights, a big soft key.
  vec3 env(vec3 dirObj) {
    vec3 d = normalize(uObjToWorld * dirObj);
    vec3 c = mix(uFloor, uSky, smoothstep(-0.6, 0.8, d.y));
    for (int i = 0; i < MAX_LIGHTS; i++) {
      if (i >= uLightCount) break;
      float k = dot(d, uLightDir[i]);
      c += uLightColor[i] * smoothstep(cos(uLightSize), cos(uLightSize * 0.55), k);
    }
    float kk = dot(d, uKeyDir);
    c += uKeyColor * pow(max(0.0, (kk - cos(uKeySize)) / (1.0 - cos(uKeySize))), 1.5);
    // The viewer (head, camera) blocks what's behind them: the dark pattern in gem photos.
    if (uContrast > 0.0) c *= 1.0 - 0.92 * smoothstep(cos(uContrast), cos(uContrast * 0.6), dot(d, uViewer));
    return c;
  }

  float fresnel(float cosi, float etai, float etat) {
    float sint = etai / etat * sqrt(max(0.0, 1.0 - cosi * cosi));
    if (sint >= 1.0) return 1.0;
    float cost = sqrt(max(0.0, 1.0 - sint * sint));
    float rs = (etat * cosi - etai * cost) / (etat * cosi + etai * cost);
    float rp = (etai * cosi - etat * cost) / (etai * cosi + etat * cost);
    return 0.5 * (rs * rs + rp * rp);
  }

  // Where a ray inside the stone leaves it: the nearest plane it's heading out through.
  float exitPlane(vec3 p, vec3 d, out int hit) {
    float best = 1e9;
    hit = -1;
    for (int i = 0; i < MAX_PLANES; i++) {
      if (i >= uPlaneCount) break;
      vec3 n = uPlanes[i].xyz;
      float den = dot(n, d);
      if (den > 1e-5) {
        float t = (uPlanes[i].w - dot(n, p)) / den;
        if (t > 1e-4 && t < best) { best = t; hit = i; }
      }
    }
    return best;
  }

  // How close a point on facet 'own' is to the facet's edge: its distance to the nearest
  // other plane. Lit as a thin glowing line.
  float edgeGlow(vec3 p, int own) {
    if (uEdge <= 0.0) return 0.0;
    float e = 1e9;
    for (int i = 0; i < MAX_PLANES; i++) {
      if (i >= uPlaneCount) break;
      if (i == own) continue;
      e = min(e, uPlanes[i].w - dot(uPlanes[i].xyz, p));
    }
    return uEdge * exp(-max(e, 0.0) / uEdgeWidth);
  }

  // Edge color, shifting through the rainbow along the stone when Edge rainbow is up.
  vec3 edgeTint(vec3 p) {
    if (uEdgeRainbow <= 0.0) return uEdgeColor;
    float h = fract(dot(p, vec3(0.35, 0.5, 0.2)) * 1.3);
    vec3 rgb = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return mix(uEdgeColor, mix(vec3(1.0), rgb, 0.85) * 1.2, uEdgeRainbow);
  }

  vec3 frosted(vec3 n) { return uFrost > 0.0 ? normalize(n + rndDir() * uFrost) : n; }

  // One wavelength through the stone.
  vec3 trace(vec3 p, vec3 d, float ior, vec3 w) {
    float thr = 1.0;
    vec3 acc = vec3(0.0);
    float absorb = dot(uAbsorb, w) / 3.0 * uDensity;
    float glow = dot(uGlow, w) / 3.0;
    for (int b = 0; b < MAX_BOUNCES; b++) {
      if (b >= uBounces) break;
      int hit;
      float t = exitPlane(p, d, hit);
      if (hit < 0) break;
      p += d * t;
      thr *= exp(-absorb * t);
      acc += vec3(thr * uDiffusion * t * glow);       // light scattered inside: the soft glow
      acc += thr * edgeGlow(p, hit) * edgeTint(p);     // back edges, seen through the stone
      vec3 n = frosted(uPlanes[hit].xyz);
      float cosi = clamp(dot(d, n), 0.0, 1.0);
      float F = fresnel(cosi, ior, 1.0);
      if (F < 1.0) acc += thr * (1.0 - F) * env(refract(d, -n, ior));
      thr *= F;
      d = reflect(d, -n);
    }
    return acc + thr * env(d) * 0.35;
  }

  void main() {
    // Random mode: a fresh stream per pixel and frame. Bayer mode: the stream comes from the pixel's
    // place in an 8×8 Bayer matrix and never changes, so the pattern is ordered and still.
    float bay = bayer8(gl_FragCoord.xy);
    rngState = uBayer > 0.5
      ? uint(bay * 64.0) * 2654435761u + 977u
      : uint(gl_FragCoord.x) * 1973u + uint(gl_FragCoord.y) * 9277u + uint(uSeed) * 26699u;
    vec3 n0 = normalize(cross(dFdx(vObj), dFdy(vObj)));
    vec3 v = normalize(vObj - uCamObj);
    if (dot(n0, v) > 0.0) n0 = -n0;
    // Which facet this pixel is on, for its own edges.
    int own = 0; float bestD = 1e9;
    for (int i = 0; i < MAX_PLANES; i++) {
      if (i >= uPlaneCount) break;
      float dd = abs(uPlanes[i].w - dot(uPlanes[i].xyz, vObj)) + (1.0 - dot(uPlanes[i].xyz, n0));
      if (dd < bestD) { bestD = dd; own = i; }
    }
    float edge = edgeGlow(vObj, own);
    vec3 col = vec3(0.0);
    float j = uBayer > 0.5 ? bay : rnd();
    for (int s = 0; s < SAMPLES; s++) {
      float x = (float(s) + j) / float(SAMPLES);
      vec3 w = spectrum(x);
      float ior = uIor + uDispersion * (x - 0.5) * 2.0;
      vec3 n = frosted(n0);
      if (dot(n, v) > 0.0) n = n0;
      float cosi = clamp(-dot(v, n), 0.0, 1.0);
      float F = fresnel(cosi, 1.0, ior);
      vec3 spec = F * uSurface * env(reflect(v, n));
      col += w * (spec + (1.0 - F) * trace(vObj, refract(v, n, 1.0 / ior), ior, w));
    }
    col = col / float(SAMPLES) + edge * edgeTint(vObj);
    gl_FragColor = vec4(col * uExposure, 1.0);
  }
`

/** Normalizes spectrum() so its average over all wavelengths is white. */
function spectrumNorm(): THREE.Vector3 {
  const s = new THREE.Vector3()
  const N = 2000
  for (let i = 0; i < N; i++) {
    const x = (i + 0.5) / N
    s.x += Math.exp(-(((x - 0.1) / 0.22) ** 2)) + 0.3 * Math.exp(-(((x - 0.98) / 0.12) ** 2))
    s.y += Math.exp(-(((x - 0.45) / 0.19) ** 2))
    s.z += Math.exp(-(((x - 0.8) / 0.19) ** 2))
  }
  return new THREE.Vector3(N / s.x, N / s.y, N / s.z)
}

export function makeMaterial(): THREE.ShaderMaterial {
  return new THREE.ShaderMaterial({
    vertexShader: vertex,
    fragmentShader: fragment,
    uniforms: {
      uPlanes: { value: Array.from({ length: MAX_PLANES }, () => new THREE.Vector4()) },
      uPlaneCount: { value: 0 },
      uCamObj: { value: new THREE.Vector3() },
      uObjToWorld: { value: new THREE.Matrix3() },
      uIor: { value: 2 }, uDispersion: { value: 0.05 }, uDensity: { value: 1 }, uDiffusion: { value: 0.1 }, uSurface: { value: 1 }, uExposure: { value: 1 },
      uFrost: { value: 0 }, uEdge: { value: 0 }, uEdgeWidth: { value: 0.02 }, uSeed: { value: 0 }, uEdgeRainbow: { value: 0 },
      uAbsorb: { value: new THREE.Vector3() }, uGlow: { value: new THREE.Color() }, uSky: { value: new THREE.Color() }, uFloor: { value: new THREE.Color() },
      uEdgeColor: { value: new THREE.Color() }, uSpecNorm: { value: spectrumNorm() },
      uBounces: { value: 6 }, uLightCount: { value: 6 },
      uLightDir: { value: Array.from({ length: 12 }, () => new THREE.Vector3(0, 1, 0)) },
      uLightColor: { value: Array.from({ length: 12 }, () => new THREE.Color()) },
      uLightSize: { value: 0.2 },
      uViewer: { value: new THREE.Vector3(0, 1, 0) },
      uContrast: { value: 0.45 },
      uKeyDir: { value: new THREE.Vector3(0, 1, 0) }, uKeyColor: { value: new THREE.Color() }, uKeySize: { value: 0.6 }, uBayer: { value: 0 },
    },
  })
}

/** Writes everything but the cut into the material's uniforms. */
/** `phase` is the turntable spin of the lights so far, radians, added to lightTurn. */
export function applyLook(m: THREE.ShaderMaterial, p: GemParams, phase: number) {
  const u = m.uniforms
  u.uIor.value = p.ior
  u.uDispersion.value = p.dispersion
  u.uDensity.value = p.density
  u.uDiffusion.value = p.diffusion
  u.uFrost.value = p.frost
  u.uSurface.value = p.surface
  u.uExposure.value = p.exposure
  u.uEdge.value = p.edge
  u.uEdgeWidth.value = Math.max(0.001, p.edgeWidth)
  u.uEdgeColor.value.set(p.edgeColor)
  u.uEdgeRainbow.value = p.edgeRainbow
  u.uBounces.value = Math.round(p.bounces)
  // Body color is what survives a typical trip through the stone (~3 units over its bounces),
  // so the color you pick is about the color you see: absorption = -ln(color) / 3.
  const body = new THREE.Color(p.color)
  const k = 1 / 3
  u.uAbsorb.value.set(-Math.log(Math.max(body.r, 0.002)) * k, -Math.log(Math.max(body.g, 0.002)) * k, -Math.log(Math.max(body.b, 0.002)) * k)
  u.uGlow.value.set(p.glow)
  u.uSky.value.set(p.sky)
  u.uFloor.value.set(p.floor)
  u.uLightSize.value = p.lightSize
  u.uContrast.value = p.contrast
  const count = Math.round(p.lights)
  u.uLightCount.value = count
  for (let i = 0; i < count; i++) {
    const a = (i / count) * Math.PI * 2 + THREE.MathUtils.degToRad(p.lightTurn) + phase
    const el = 0.2 + 1.0 * ((i * 0.618) % 1)
    const el2 = Math.max(-1.4, Math.min(1.5, el + THREE.MathUtils.degToRad(p.lightHeight)))
    u.uLightDir.value[i].set(Math.cos(a) * Math.cos(el2), Math.sin(el2), Math.sin(a) * Math.cos(el2)).normalize()
    const c = new THREE.Color().setHSL((p.hue + (i / count) * p.rainbow) % 1, p.rainbow > 0 ? 0.85 : 0, 0.5)
    u.uLightColor.value[i].copy(new THREE.Color(1, 1, 1).lerp(c, Math.min(1, p.rainbow * 2))).multiplyScalar(p.lightPower)
  }
  const az = THREE.MathUtils.degToRad(p.keyAzimuth), el = THREE.MathUtils.degToRad(p.keyElevation)
  u.uKeyDir.value.set(Math.sin(az) * Math.cos(el), Math.sin(el), -Math.cos(az) * Math.cos(el))
  u.uKeyColor.value.set(p.keyColor).multiplyScalar(p.key)
  u.uKeySize.value = p.keySize
  u.uBayer.value = p.bayer ? 1 : 0
}

/**
 * The last pass, in display space: an optional soft edge falloff (an ellipse touching the canvas
 * sides, solid to 55% then linear to nothing), optional Bayer posterizing (`uLevels` > 0), and
 * otherwise a fine triangular-noise dither of about one 8-bit step so dark glows don't band.
 */
export const finishShader = {
  uniforms: { tDiffuse: { value: null }, uSeed: { value: 0 }, uFalloff: { value: 0 }, uLevels: { value: 0 } },
  vertexShader: 'varying vec2 vUv; void main() { vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }',
  fragmentShader: /* glsl */ `
    uniform sampler2D tDiffuse; uniform float uSeed, uFalloff, uLevels; varying vec2 vUv;
    float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
    float bayer2(vec2 a) { a = floor(a); return fract(a.x / 2.0 + a.y * a.y * 0.75); }
    float bayer4(vec2 a) { return bayer2(0.5 * a) * 0.25 + bayer2(a); }
    float bayer8(vec2 a) { return bayer4(0.5 * a) * 0.25 + bayer2(a); }
    void main() {
      vec3 c = texture2D(tDiffuse, vUv).rgb;
      if (uFalloff > 0.5) c *= 1.0 - clamp((length((vUv - 0.5) * 2.0) - 0.55) / 0.45, 0.0, 1.0);
      if (uLevels > 0.5) {
        float L = uLevels - 1.0;
        c = floor(c * L + bayer8(gl_FragCoord.xy)) / L;
      } else {
        vec2 q = gl_FragCoord.xy + uSeed;
        c += (h(q) + h(q + 17.3) - 1.0) / 255.0 * 1.5;
      }
      gl_FragColor = vec4(clamp(c, 0.0, 1.0), 1.0);
    }`,
}
