import type { GemParams } from './gem'

// The locked LOBS logo look (from the gem lab). The site hero, `npm run logo` and the lab's
// "LOBS logo" preset all read this, so changing it here changes all three.
export const LOGO: Partial<GemParams> = {
    shape: 'd20', ior: 2.17, dispersion: 0.552, color: '#a600ff', density: 3.05, glow: '#b98cff', diffusion: 0.15, frost: 0.095, bounces: 5, surface: 0.77,
    edge: 0.82, edgeWidth: 0.002, edgeColor: '#fff5fb', edgeRainbow: 0.21, lights: 11, lightSize: 0.31, lightPower: 19, contrast: 0, lightSpin: 0, lightTurn: 129, lightHeight: 0,
    rainbow: 0.85, hue: 0.7, key: 4.2, keySize: 0.72, keyAzimuth: -1, keyElevation: 20, keyColor: '#fff0f8', sky: '#ffffff', floor: '#c2c2c2', exposure: 0.87,
    bloom: 0.2, bloomRadius: 0, bloomThreshold: 2.37, background: '#000000', view: 'side', spin: 0, angle: 0, tilt: 42, roll: 0, camAz: 0, camEl: 0, fov: 30,
}
