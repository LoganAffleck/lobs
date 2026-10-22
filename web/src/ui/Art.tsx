import type { Art as ArtKind } from './model'

// Stand-ins for live inputs, drawn at the source's own 1920×1080 so crop math matches the app.
// Plain SVG, sharp at any capture scale, except the facecam, game and Steam window, which are pictures.
export function Art({ kind, uid }: { kind: ArtKind; uid: string }) {
  const id = (n: string) => `${uid}-${n}`
  switch (kind) {
    case 'facecam':
      // A real photo, drifting slightly so it reads as a live camera. Photo by Higor Hanschen on Unsplash.
      return <img className="art-img art-cam" src="/art/facecam.jpg" alt="" draggable={false} />
    case 'game':
      return <img className="art-img" src="/art/game.jpg" alt="" draggable={false} />
    case 'steam':
      return <img className="art-img" src="/art/steam.jpg" alt="" draggable={false} />
    case 'browser-alerts':
      return (
        <svg viewBox="0 0 1920 1080" width="100%" height="100%" preserveAspectRatio="none">
          <g transform="translate(80 900)">
            <rect width="560" height="110" rx="20" fill="#16121f" opacity="0.92" stroke="#8b5cf6" strokeWidth="3" />
            <circle cx="60" cy="55" r="30" fill="#8b5cf6" />
            {/* Lucide heart: it's a follow */}
            <path transform="translate(44 39.5) scale(1.333)" d="M19 14c1.49-1.46 3-3.21 3-5.5A5.5 5.5 0 0 0 16.5 3c-1.76 0-3 .5-4.5 2-1.5-1.5-2.74-2-4.5-2A5.5 5.5 0 0 0 2 8.5c0 2.3 1.5 4.05 3 5.5l7 7Z" fill="#ffffff" />
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
