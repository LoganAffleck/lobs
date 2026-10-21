// Writes one static HTML file per route (dist/<path>/index.html) from the SSR build, so the
// site works on any static host and every page has its content without JavaScript.
import { readFileSync, writeFileSync, mkdirSync, rmSync } from 'node:fs'
import { join, dirname } from 'node:path'
import { fileURLToPath, pathToFileURL } from 'node:url'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const dist = join(root, 'dist')
const template = readFileSync(join(dist, 'index.html'), 'utf8')
const { render, ROUTES } = await import(pathToFileURL(join(root, 'dist-ssr/entry-server.js')).href)

for (const r of ROUTES) {
  const { html, head } = render(r.path)
  const page = template.replace('<!--head-->', head).replace('<!--app-->', html)
  const dir = join(dist, r.path)
  mkdirSync(dir, { recursive: true })
  writeFileSync(join(dir, 'index.html'), page)
  console.log(r.path)
}
rmSync(join(root, 'dist-ssr'), { recursive: true, force: true })
