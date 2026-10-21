import { DOCS, type Doc } from '../docs'

function Side({ current }: { current?: string }) {
  return (
    <aside className="side">
      <h5>Docs</h5>
      <a href="/docs/" className={!current ? 'on' : ''}>Overview</a>
      {DOCS.map((d) => <a key={d.slug} href={`/docs/${d.slug}/`} className={d.slug === current ? 'on' : ''}>{d.title}</a>)}
    </aside>
  )
}

export function DocsIndex() {
  return (
    <div className="wrap docs">
      <Side />
      <article className="prose">
        <h1>LOBS docs</h1>
        <p className="desc">How LOBS works, panel by panel. Start with Getting started, then Scenes and sources, which explains the idea the rest of the app is built on.</p>
        <div className="docindex">
          {DOCS.map((d) => <a key={d.slug} href={`/docs/${d.slug}/`}><b>{d.title}</b><span>{d.description}</span></a>)}
        </div>
      </article>
    </div>
  )
}

export function DocPage({ doc }: { doc: Doc }) {
  const i = DOCS.indexOf(doc)
  const prev = DOCS[i - 1], next = DOCS[i + 1]
  return (
    <div className="wrap docs">
      <Side current={doc.slug} />
      <article className="prose">
        <h1>{doc.title}</h1>
        {doc.description && <p className="desc">{doc.description}</p>}
        <div dangerouslySetInnerHTML={{ __html: doc.html }} />
        <nav className="pager">
          {prev && <a href={`/docs/${prev.slug}/`}><small>Previous</small>{prev.title}</a>}
          {next && <a className="next" href={`/docs/${next.slug}/`}><small>Next</small>{next.title}</a>}
        </nav>
      </article>
    </div>
  )
}
