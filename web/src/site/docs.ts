import { Marked } from 'marked'
import { SHOTS } from '../shots/catalog'

// Docs are Markdown files in web/docs with a small frontmatter block (title, order,
// description). `![Caption](shot:name)` embeds public/shots/name.png, rendered by
// `npm run shots` from src/shots/catalog.ts.
export interface Doc { slug: string; title: string; order: number; description: string; html: string; headings: { id: string; text: string }[] }

const files = import.meta.glob('../../docs/*.md', { query: '?raw', import: 'default', eager: true }) as Record<string, string>

const slugify = (s: string) => s.toLowerCase().replace(/<[^>]+>/g, '').replace(/&[#a-z0-9]+;/g, '').replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '')

function parse(path: string, raw: string): Doc {
  const slug = path.split('/').pop()!.replace(/\.md$/, '')
  const m = /^---\r?\n([\s\S]*?)\r?\n---\r?\n?/.exec(raw)
  const meta: Record<string, string> = {}
  for (const line of (m?.[1] ?? '').split(/\r?\n/)) {
    const i = line.indexOf(':')
    if (i > 0) meta[line.slice(0, i).trim()] = line.slice(i + 1).trim().replace(/^["']|["']$/g, '')
  }
  const body = m ? raw.slice(m[0].length) : raw
  const headings: Doc['headings'] = []
  const marked = new Marked({
    renderer: {
      heading({ tokens, depth }) {
        const text = this.parser.parseInline(tokens)
        const id = slugify(text)
        if (depth === 2) headings.push({ id, text: text.replace(/<[^>]+>/g, '') })
        return `<h${depth} id="${id}">${text}</h${depth}>\n`
      },
      image({ href, text }) {
        const name = href.startsWith('shot:') ? href.slice(5) : null
        const src = name ? `/shots/${name}.png` : href
        // Shots are 2x; show them at their CSS size so panel close-ups aren't blown up.
        const shot = name ? SHOTS[name] : undefined
        const [w, h] = shot?.clip ? shot.clip.slice(2) : shot?.size ?? [1520, 900]
        const dims = shot ? ` width="${w}" height="${h}"` : ''
        return `<figure><img src="${src}" alt="${text}" loading="lazy"${dims} />${text ? `<figcaption>${text}</figcaption>` : ''}</figure>`
      },
      paragraph({ tokens }) {
        // A paragraph that's only an image becomes a bare <figure> (no <p> around a block).
        const inner = this.parser.parseInline(tokens)
        return inner.startsWith('<figure>') ? inner : `<p>${inner}</p>\n`
      },
    },
  })
  const html = marked.parse(body.replace(/^#\s+.*\r?\n/, '')) as string
  return { slug, title: meta.title ?? slug, order: Number(meta.order ?? 99), description: meta.description ?? '', html, headings }
}

export const DOCS: Doc[] = Object.entries(files).map(([p, raw]) => parse(p, raw)).sort((a, b) => a.order - b.order)
