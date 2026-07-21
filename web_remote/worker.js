const WRITE_METHODS = new Set(['POST', 'PUT', 'PATCH', 'DELETE']);

export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url);

    if (url.pathname.startsWith('/firebase/')) {
      const fbBase = (env.FIREBASE_URL || '').replace(/\/$/, '');
      if (!fbBase || !env.FIREBASE_SECRET) {
        return new Response('Firebase not configured on server', { status: 503 });
      }

      // Enforce token for all write operations.
      // Set SITE_TOKEN as a Cloudflare secret. Share via QR code pointing to /?token=<SITE_TOKEN>.
      if (WRITE_METHODS.has(request.method) && env.SITE_TOKEN) {
        const token = request.headers.get('X-Auth-Token') || '';
        if (token !== env.SITE_TOKEN) {
          return new Response('Unauthorized', { status: 401 });
        }
      }

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
        headers: {
          'content-type': 'application/json',
          'access-control-allow-origin': '*',
        },
      });
    }

    return env.ASSETS.fetch(request);
  },
};
