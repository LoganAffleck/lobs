// LOBS's Twitch sign-in server, as OBS's auth.obsproject.com: the client secret stays here, and
// LOBS trades through it. LOBS opens Twitch's sign-in page with redirect_uri set to
// /twitch/callback, reads the one-time code off that page's address, and POSTs it here.
//   POST /api/twitch/token  {"code": "..."}           -> Twitch's token response
//   POST /api/twitch/token  {"refresh_token": "..."}  -> a refreshed token
// The secret is the TWITCH_CLIENT_SECRET environment variable in Netlify (never in the repo).

const CLIENT_ID = 'y0mck0jqerrkhzvqiy1pzhagusz1rn'
// Must match the redirect_uri LOBS signed in with, and one registered in the Twitch console.
const REDIRECTS = ['https://streamlobs.com/twitch/callback', 'http://localhost:5340/twitch/callback']

const json = (status, body) => new Response(JSON.stringify(body), { status, headers: { 'content-type': 'application/json', 'cache-control': 'no-store' } })

export default async (req) => {
  if (req.method !== 'POST') return json(405, { error: 'POST only' })
  const secret = process.env.TWITCH_CLIENT_SECRET
  if (!secret) return json(500, { error: 'The server has no Twitch client secret set' })
  let body
  try { body = await req.json() } catch { return json(400, { error: 'Send JSON' }) }

  const form = new URLSearchParams({ client_id: CLIENT_ID, client_secret: secret })
  if (typeof body.code === 'string' && body.code) {
    const redirect = typeof body.redirect_uri === 'string' ? body.redirect_uri : REDIRECTS[0]
    if (!REDIRECTS.includes(redirect)) return json(400, { error: 'Unknown redirect_uri' })
    form.set('grant_type', 'authorization_code')
    form.set('code', body.code)
    form.set('redirect_uri', redirect)
  } else if (typeof body.refresh_token === 'string' && body.refresh_token) {
    form.set('grant_type', 'refresh_token')
    form.set('refresh_token', body.refresh_token)
  } else {
    return json(400, { error: 'Send a code or a refresh_token' })
  }

  const r = await fetch('https://id.twitch.tv/oauth2/token', { method: 'POST', body: form })
  const out = await r.json().catch(() => ({ error: 'Twitch sent no JSON' }))
  // Twitch's own answer, minus nothing: access_token, refresh_token, expires_in, scope, or its error.
  return json(r.status, out)
}

export const config = { path: '/api/twitch/token' }
