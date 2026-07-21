/**
 * Cloudflare Pages Function — Firebase RTDB proxy
 * Catches all requests o /firebase/… and forwards them to the Firebase
  * Realtime Database, appending the database secret server-side.
 */
export async function onRequest(context) {
  const { request, env } = context;

  const fbBase = (env.FIREBASE_URL || '').replace(/\/$/, '');
  if (!fbBase || !env.FIREBASE_SECRET) {
    return new Response('Firebase not configured on server', { status: 503 });
  }

  const url = new URL(request.url);
  // Strip the /firebase prefix; keep the rest of the path (e.g. /thermostat/state.json)
  const fbPath = url.pathname.replace(/^\/firebase/, '');
  const sep = url.search ? '&' : '?';
  const target = fbBase + fbPath + sep + 'auth=' + env.FIREBASE_SECRET;

  const init = { method: request.method };
  if (request.method !== 'GET' && request.method !== 'HEAD') {
    init.body = request.body;
    init.headers = { 'content-type': 'application/json' };
  }

  const resp = await fetch(target, init);
  const body = await resp.text();
  return new Response(body, {
    status: resp.status,
    headers: { 'content-type': 'application/json' },
  });
}
