import type { ReactNode } from 'react'

export const REPO = 'https://github.com/LoganAffleck/lobs'
export const VERSION = '0.7.0'
export const DISCORD = 'https://discord.gg/CY5QSXchxH'
/** The installer itself, so the button downloads instead of opening the release page. */
export const DOWNLOAD = `${REPO}/releases/download/v${VERSION}/LOBS-Setup-${VERSION}.exe`

export function GitHubIcon() {
  return <svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M12 .5a11.5 11.5 0 0 0-3.64 22.41c.58.1.79-.25.79-.56v-2c-3.2.7-3.88-1.37-3.88-1.37-.53-1.33-1.28-1.69-1.28-1.69-1.05-.71.08-.7.08-.7 1.16.08 1.77 1.19 1.77 1.19 1.03 1.77 2.7 1.26 3.36.96.1-.75.4-1.26.73-1.55-2.55-.29-5.24-1.28-5.24-5.68 0-1.26.45-2.28 1.19-3.09-.12-.29-.52-1.46.11-3.05 0 0 .97-.31 3.17 1.18a11 11 0 0 1 5.77 0c2.2-1.49 3.17-1.18 3.17-1.18.63 1.59.23 2.76.11 3.05.74.81 1.19 1.83 1.19 3.09 0 4.41-2.69 5.39-5.25 5.67.41.36.78 1.06.78 2.14v3.17c0 .31.21.67.8.56A11.5 11.5 0 0 0 12 .5Z" /></svg>
}

export function DiscordIcon() {
  return <svg viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M20.32 4.37a19.8 19.8 0 0 0-4.89-1.52.07.07 0 0 0-.08.04c-.21.38-.44.87-.61 1.25a18.27 18.27 0 0 0-5.49 0 12.6 12.6 0 0 0-.62-1.25.08.08 0 0 0-.08-.04 19.74 19.74 0 0 0-4.89 1.52.07.07 0 0 0-.03.03C.53 9.05-.32 13.58.1 18.06a.08.08 0 0 0 .03.05 19.9 19.9 0 0 0 5.99 3.03.08.08 0 0 0 .08-.03c.46-.63.87-1.3 1.23-1.99a.08.08 0 0 0-.04-.11 13.1 13.1 0 0 1-1.87-.89.08.08 0 0 1-.01-.13l.37-.29a.07.07 0 0 1 .08-.01c3.93 1.79 8.18 1.79 12.07 0a.07.07 0 0 1 .08.01l.37.29a.08.08 0 0 1-.01.13c-.6.35-1.22.65-1.87.89a.08.08 0 0 0-.04.11c.36.7.77 1.36 1.23 1.99a.08.08 0 0 0 .08.03 19.84 19.84 0 0 0 6-3.03.08.08 0 0 0 .03-.05c.5-5.18-.84-9.67-3.55-13.66a.06.06 0 0 0-.03-.03ZM8.02 15.33c-1.18 0-2.16-1.09-2.16-2.42 0-1.33.96-2.42 2.16-2.42 1.21 0 2.18 1.1 2.16 2.42 0 1.33-.96 2.42-2.16 2.42Zm7.97 0c-1.18 0-2.15-1.09-2.15-2.42 0-1.33.95-2.42 2.15-2.42 1.21 0 2.18 1.1 2.16 2.42 0 1.33-.95 2.42-2.16 2.42Z" /></svg>
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
            <a className="discord" href={DISCORD}><DiscordIcon /><span className="hide-sm">Discord</span></a>
            <a className="sbtn primary small" href={DOWNLOAD} download>Download</a>
          </nav>
        </div>
      </header>
      <main>{children}</main>
      <footer className="footer">
        <div className="wrap">
          <p>
            Questions, bugs or ideas? Come say hi on <a href={DISCORD}>Discord</a>.
            {' '}LOBS {VERSION} is free software under the GPL-2.0-or-later. Capture, encoding and streaming come from libobs, the open-source engine behind{' '}
            <a href="https://obsproject.com">OBS Studio</a>. LOBS is the editor on top. Not affiliated with or endorsed by the OBS Project.
            {' '}Facecam photo by <a href="https://unsplash.com/@higorrss">Higor Hanschen</a> on <a href="https://unsplash.com/photos/woman-in-white-shirt-wearing-blue-headphones-holding-black-microphone-3Bz1yBpI3GI">Unsplash</a>.
          </p>
        </div>
      </footer>
    </>
  )
}
