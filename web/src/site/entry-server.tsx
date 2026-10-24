import { renderToString } from 'react-dom/server'
import { match, Page, ROUTES } from './routes'

export { ROUTES }

const SITE_URL = 'https://streamlobs.com'

const esc = (s: string) => s.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;')

export function render(path: string) {
  const route = match(path)
  const head = [
    `<title>${esc(route.title)}</title>`,
    `<meta name="description" content="${esc(route.description)}" />`,
    `<meta property="og:title" content="${esc(route.title)}" />`,
    `<meta property="og:description" content="${esc(route.description)}" />`,
    `<meta property="og:type" content="website" />`,
    `<meta property="og:site_name" content="LOBS" />`,
    `<meta property="og:url" content="${SITE_URL}${route.path}" />`,
    // The card link previews show (public/og.png, from og.html via `npm run og`); absolute, as Discord and X want.
    `<meta property="og:image" content="${SITE_URL}/og.png" />`,
    `<meta property="og:image:width" content="1200" />`,
    `<meta property="og:image:height" content="630" />`,
    `<meta property="og:image:alt" content="Logan's OBS: OBS with a creative UI for building awesome scenes." />`,
    `<meta name="twitter:card" content="summary_large_image" />`,
    `<meta name="theme-color" content="#111111" />`,
  ].join('\n    ')
  return { html: renderToString(<Page route={route} />), head }
}
