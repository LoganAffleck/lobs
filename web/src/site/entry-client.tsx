import { createRoot, hydrateRoot } from 'react-dom/client'
import { match, Page } from './routes'

const route = match(location.pathname)
const el = document.getElementById('root')!
// Prerendered pages hydrate; the dev server serves the bare shell, so render there.
if (el.firstElementChild) hydrateRoot(el, <Page route={route} />)
else {
  document.title = route.title
  createRoot(el).render(<Page route={route} />)
}
