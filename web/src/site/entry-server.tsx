import { renderToString } from 'react-dom/server'
import { match, Page, ROUTES } from './routes'

export { ROUTES }

const esc = (s: string) => s.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;')

export function render(path: string) {
  const route = match(path)
  const head = [
    `<title>${esc(route.title)}</title>`,
    `<meta name="description" content="${esc(route.description)}" />`,
    `<meta property="og:title" content="${esc(route.title)}" />`,
    `<meta property="og:description" content="${esc(route.description)}" />`,
    `<meta property="og:image" content="/shots/editor.png" />`,
    `<meta name="theme-color" content="#111111" />`,
  ].join('\n    ')
  return { html: renderToString(<Page route={route} />), head }
}
