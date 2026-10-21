import type { Art as ArtKind } from './model'

// Stand-ins for live inputs, drawn at the source's own 1920×1080 so crop math matches the app.
// Plain SVG: no images to load, sharp at any capture scale.
export function Art({ kind, uid }: { kind: ArtKind; uid: string }) {
  const id = (n: string) => `${uid}-${n}`
  switch (kind) {
    case 'facecam':
      return (
        <svg viewBox="0 0 1920 1080" width="100%" height="100%" preserveAspectRatio="none">
          <defs>
            <radialGradient id={id('room')} cx="0.3" cy="0.3" r="1">
              <stop offset="0" stopColor="#4a3a5e" /><stop offset="0.6" stopColor="#241c33" /><stop offset="1" stopColor="#120e1a" />
            </radialGradient>
            <linearGradient id={id('skin')} x1="0" y1="0" x2="1" y2="1">
              <stop offset="0" stopColor="#e7b896" /><stop offset="1" stopColor="#b27a5a" />
            </linearGradient>
            <linearGradient id={id('shirt')} x1="0" y1="0" x2="0" y2="1">
              <stop offset="0" stopColor="#3b3f58" /><stop offset="1" stopColor="#1e2030" />
            </linearGradient>
            <filter id={id('blur')}><feGaussianBlur stdDeviation="24" /></filter>
          </defs>
          <rect width="1920" height="1080" fill={`url(#${id('room')})`} />
          {/* out-of-focus room: a shelf light, a monitor glow, a plant */}
          <g filter={`url(#${id('blur')})`} opacity="0.9">
            <circle cx="300" cy="260" r="70" fill="#ffcf8a" opacity="0.55" />
            <circle cx="430" cy="300" r="40" fill="#ff9ec7" opacity="0.4" />
            <rect x="1400" y="180" width="380" height="240" rx="20" fill="#5a7bff" opacity="0.35" />
            <ellipse cx="1650" cy="820" rx="160" ry="260" fill="#2f5a3a" opacity="0.7" />
            <rect x="120" y="520" width="420" height="30" fill="#6b4a32" opacity="0.6" />
          </g>
          {/* person */}
          <path d="M560 1080 C 600 860 760 790 960 790 C 1160 790 1320 860 1360 1080 Z" fill={`url(#${id('shirt')})`} />
          <rect x="890" y="690" width="140" height="140" rx="40" fill="#b9805f" />
          <ellipse cx="960" cy="540" rx="190" ry="230" fill={`url(#${id('skin')})`} />
          <path d="M770 520 C 760 330 860 270 975 270 C 1110 270 1170 350 1150 520 C 1120 420 1050 380 960 390 C 870 395 800 430 770 520 Z" fill="#2a1d18" />
          <ellipse cx="895" cy="560" rx="16" ry="11" fill="#2a1d18" />
          <ellipse cx="1030" cy="560" rx="16" ry="11" fill="#2a1d18" />
          <path d="M900 660 Q 962 700 1025 660" stroke="#8a4b3a" strokeWidth="12" fill="none" strokeLinecap="round" />
          {/* headset */}
          <path d="M760 520 C 760 260 1160 260 1160 520" stroke="#111" strokeWidth="28" fill="none" />
          <rect x="730" y="480" width="60" height="130" rx="24" fill="#151515" />
          <rect x="1130" y="480" width="60" height="130" rx="24" fill="#151515" />
        </svg>
      )
    case 'game':
      return (
        <svg viewBox="0 0 1920 1080" width="100%" height="100%" preserveAspectRatio="none">
          <defs>
            <linearGradient id={id('sky')} x1="0" y1="0" x2="0" y2="1">
              <stop offset="0" stopColor="#0b1d3a" /><stop offset="0.7" stopColor="#1d4a78" /><stop offset="1" stopColor="#3f7fb0" />
            </linearGradient>
            <radialGradient id={id('glow')} cx="0.7" cy="0.35" r="0.4">
              <stop offset="0" stopColor="#bfe8ff" stopOpacity="0.8" /><stop offset="1" stopColor="#bfe8ff" stopOpacity="0" />
            </radialGradient>
          </defs>
          <rect width="1920" height="1080" fill={`url(#${id('sky')})`} />
          <rect width="1920" height="1080" fill={`url(#${id('glow')})`} />
          <path d="M0 700 L180 520 L320 640 L520 420 L760 660 L980 480 L1220 700 L1460 500 L1700 640 L1920 540 L1920 1080 L0 1080 Z" fill="#12304f" />
          <path d="M0 820 L240 700 L460 800 L700 690 L940 820 L1180 720 L1420 830 L1660 740 L1920 820 L1920 1080 L0 1080 Z" fill="#0a1f36" />
          <rect y="900" width="1920" height="180" fill="#061426" />
          {/* the hero: a little horned knight */}
          <g transform="translate(860 760)">
            <path d="M20 140 L40 40 L120 40 L140 140 Z" fill="#1b2433" />
            <ellipse cx="80" cy="30" rx="56" ry="50" fill="#f2f4f8" />
            <path d="M40 0 L22 -70 L56 -10 Z M120 0 L138 -70 L104 -10 Z" fill="#f2f4f8" />
            <ellipse cx="60" cy="34" rx="12" ry="20" fill="#0a0d14" />
            <ellipse cx="100" cy="34" rx="12" ry="20" fill="#0a0d14" />
          </g>
          {[[300, 300], [520, 220], [1180, 260], [1500, 180], [1720, 320], [760, 160]].map(([x, y]) => <circle key={x} cx={x} cy={y} r="5" fill="#cfefff" opacity="0.8" />)}
          {/* HUD */}
          <g transform="translate(70 70)">
            <circle cx="40" cy="40" r="40" fill="#e8eef7" opacity="0.9" />
            {[0, 1, 2, 3, 4].map((i) => <path key={i} transform={`translate(${110 + i * 56} 14)`} d="M20 0 L40 24 L20 52 L0 24 Z" fill={i < 4 ? '#f2f4f8' : '#3a4a60'} />)}
          </g>
        </svg>
      )
    case 'desktop':
      return (
        <svg viewBox="0 0 1920 1080" width="100%" height="100%" preserveAspectRatio="none">
          <rect width="1920" height="1080" fill="#0f1115" />
          <rect x="0" y="0" width="1920" height="56" fill="#181b22" />
          {['#ff5f57', '#febc2e', '#28c840'].map((c, i) => <circle key={c} cx={36 + i * 30} cy="28" r="9" fill={c} />)}
          <rect x="0" y="56" width="300" height="1024" fill="#14171d" />
          {Array.from({ length: 14 }, (_, i) => <rect key={i} x={36 + (i % 3) * 18} y={100 + i * 52} width={160 - (i * 23) % 70} height="16" rx="8" fill="#2a2f3a" />)}
          {Array.from({ length: 22 }, (_, i) => {
            const w = [520, 380, 640, 300, 700, 460, 220, 580][i % 8]
            const c = ['#7aa7ff', '#c792ea', '#9ece6a', '#e0af68', '#5a6170'][i % 5]
            return <rect key={i} x={360 + (i % 4) * 40} y={100 + i * 42} width={w} height="18" rx="9" fill={c} opacity="0.8" />
          })}
          <rect x="1300" y="56" width="620" height="1024" fill="#12141a" />
          <rect x="1340" y="110" width="540" height="300" rx="14" fill="#1d2230" />
          <rect x="1340" y="440" width="540" height="200" rx="14" fill="#1d2230" />
        </svg>
      )
    case 'browser-alerts':
      return (
        <svg viewBox="0 0 1920 1080" width="100%" height="100%" preserveAspectRatio="none">
          <g transform="translate(80 900)">
            <rect width="560" height="110" rx="20" fill="#16121f" opacity="0.92" stroke="#8b5cf6" strokeWidth="3" />
            <circle cx="60" cy="55" r="30" fill="#8b5cf6" />
            <text x="114" y="48" fill="#ffffff" fontFamily="Inter, sans-serif" fontSize="30" fontWeight="700">mossy_kettle</text>
            <text x="114" y="84" fill="#c4b5fd" fontFamily="Inter, sans-serif" fontSize="24">just followed</text>
          </g>
        </svg>
      )
    case 'webcam-utility':
      return (
        <svg viewBox="0 0 1920 1080" width="100%" height="100%" preserveAspectRatio="none">
          <rect width="1920" height="1080" fill="#000" />
          <text x="960" y="460" textAnchor="middle" fill="#fff" fontFamily="Inter, sans-serif" fontSize="80" fontWeight="800" letterSpacing="6">EOS WEBCAM UTILITY</text>
        </svg>
      )
    default:
      return null
  }
}
