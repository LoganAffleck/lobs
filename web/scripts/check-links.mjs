// After `npm run build`: every internal link, #anchor and screenshot in dist must resolve.
import { readdirSync, readFileSync, statSync, existsSync } from 'node:fs'
import { join, relative, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const dist = join(root, 'dist')
const pages = {}
;(function walk(d) {
  for (const f of readdirSync(d)) {
    const p = join(d, f)
    if (statSync(p).isDirectory()) walk(p)
    else if (f === 'index.html') {
      const rel = relative(dist, d).split('\\').join('/')
      pages[rel ? `/${rel}/` : '/'] = readFileSync(p, 'utf8')
    }
  }
})(dist)

const ids = (html) => new Set([...html.matchAll(/id="([^"]+)"/g)].map((m) => m[1]))
let bad = 0
for (const [url, html] of Object.entries(pages)) {
  for (const [, ref] of html.matchAll(/(?:href|src)="(\/[^"]*)"/g)) {
    const [path, hash] = ref.split('#')
    if (path.startsWith('/assets/')) continue
    // Files (images, icons): must exist in the built site.
    if (/\.[a-z0-9]+$/i.test(path)) {
      if (!existsSync(join(dist, path))) { console.log(`${url}: missing file ${path}`); bad++ }
      continue
    }
    const target = pages[path || url]
    if (!target) { console.log(`${url}: broken link ${ref}`); bad++; continue }
    if (hash && !ids(target).has(hash)) { console.log(`${url}: missing anchor ${ref}`); bad++ }
  }
}
console.log(`${Object.keys(pages).length} pages, ${bad} problems`)
process.exitCode = bad ? 1 : 0
