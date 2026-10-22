import type { ReactNode } from 'react'

export const REPO = 'https://github.com/LoganAffleck/lobs'
export const VERSION = '0.3.1'
/** The installer itself, so the button downloads instead of opening the release page. */
export const DOWNLOAD = `${REPO}/releases/download/v${VERSION}/LOBS-Setup-${VERSION}.exe`

export function GitHubIcon() {
  return <svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M12 .5a11.5 11.5 0 0 0-3.64 22.41c.58.1.79-.25.79-.56v-2c-3.2.7-3.88-1.37-3.88-1.37-.53-1.33-1.28-1.69-1.28-1.69-1.05-.71.08-.7.08-.7 1.16.08 1.77 1.19 1.77 1.19 1.03 1.77 2.7 1.26 3.36.96.1-.75.4-1.26.73-1.55-2.55-.29-5.24-1.28-5.24-5.68 0-1.26.45-2.28 1.19-3.09-.12-.29-.52-1.46.11-3.05 0 0 .97-.31 3.17 1.18a11 11 0 0 1 5.77 0c2.2-1.49 3.17-1.18 3.17-1.18.63 1.59.23 2.76.11 3.05.74.81 1.19 1.83 1.19 3.09 0 4.41-2.69 5.39-5.25 5.67.41.36.78 1.06.78 2.14v3.17c0 .31.21.67.8.56A11.5 11.5 0 0 0 12 .5Z" /></svg>
}

export function DownloadIcon() {
  return <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" aria-hidden="true"><path d="M12 3v12m0 0-4.5-4.5M12 15l4.5-4.5M4 17v2a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-2" /></svg>
}

export function Layout({ children }: { path: string; children: ReactNode }) {
  return (
    <>
      <header className="nav">
        <div className="wrap">
          <a className="logo" href="/"><img src="/brand/lobs-logo.svg" alt="" width={21} height={24} />LOBS</a>
          <nav className="links">
            <a className="sbtn primary small" href={DOWNLOAD} download>Download</a>
          </nav>
        </div>
      </header>
      <main>{children}</main>
      <footer className="footer">
        <div className="wrap">
          <p>
            LOBS {VERSION} is free software under the GPL-2.0-or-later. Capture, encoding and streaming come from libobs, the open-source engine behind{' '}
            <a href="https://obsproject.com">OBS Studio</a>. LOBS is the editor on top. Not affiliated with or endorsed by the OBS Project.
            {' '}Facecam photo by <a href="https://unsplash.com/@higorrss">Higor Hanschen</a> on <a href="https://unsplash.com/photos/woman-in-white-shirt-wearing-blue-headphones-holding-black-microphone-3Bz1yBpI3GI">Unsplash</a>.
          </p>
        </div>
      </footer>
    </>
  )
}
