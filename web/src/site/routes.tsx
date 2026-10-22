import '@fontsource-variable/inter'
import './site.css'
import type { ReactNode } from 'react'
import { Layout } from './Layout'
import { Home } from './pages/Home'
import { DocPage, DocsIndex } from './pages/Docs'
import { DOCS } from './docs'

export interface Route { path: string; title: string; description: string; element: () => ReactNode }

const SITE = 'LOBS'
const DEFAULT_DESC = 'A free streaming and recording app for Windows on the OBS engine. Add your webcam once and put it in every scene with its own size, crop and filters.'

export const ROUTES: Route[] = [
  { path: '/', title: "LOBS: Logan's OBS", description: DEFAULT_DESC, element: () => <Home /> },
  { path: '/docs/', title: `Docs · ${SITE}`, description: 'How LOBS works, panel by panel.', element: () => <DocsIndex /> },
  ...DOCS.map((d) => ({ path: `/docs/${d.slug}/`, title: `${d.title} · ${SITE} docs`, description: d.description || DEFAULT_DESC, element: () => <DocPage doc={d} /> })),
]

export function match(pathname: string): Route {
  const p = pathname.endsWith('/') ? pathname : `${pathname}/`
  return ROUTES.find((r) => r.path === p) ?? ROUTES[0]
}

export function Page({ route }: { route: Route }) {
  return <Layout path={route.path}>{route.element()}</Layout>
}
