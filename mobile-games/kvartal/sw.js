// offline cache. The page itself is fetched fresh whenever there is a connection (so updates show at once);
// the saved copy is used only offline or when the network is too slow. Libraries and icons come from the cache.
const CACHE = "kvartal-6dd6b1027d";
const FILES = ["./", "index.html", "manifest.webmanifest", "app-icon-180.png", "app-icon-192.png", "app-icon-512.png",
  "lib/three.min.js", "lib/CopyShader.js", "lib/LuminosityHighPassShader.js", "lib/EffectComposer.js", "lib/RenderPass.js", "lib/ShaderPass.js", "lib/UnrealBloomPass.js", "lib/BufferGeometryUtils.js"];
self.addEventListener("install", e => { e.waitUntil(caches.open(CACHE).then(c => c.addAll(FILES)).then(() => self.skipWaiting())); });
self.addEventListener("activate", e => { e.waitUntil(caches.keys().then(ks => Promise.all(ks.filter(k => k !== CACHE).map(k => caches.delete(k)))).then(() => self.clients.claim())); });
const keep = (req, r) => { const u = new URL(req.url); if (r && (r.ok || r.type === "opaque") && (u.origin === location.origin || u.hostname.startsWith("fonts."))) { const cp = r.clone(); caches.open(CACHE).then(c => c.put(req, cp)); } return r; };
self.addEventListener("fetch", e => {
  const req = e.request; if (req.method !== "GET") return;
  const page = req.mode === "navigate" || /\/(index\.html)?$/.test(new URL(req.url).pathname);
  if (page) {
    // network first, with a 6 s limit, then the saved copy
    e.respondWith(new Promise(resolve => {
      let done = false; const fallback = () => caches.match(req, { ignoreSearch: true }).then(h => h || caches.match("index.html"));
      const t = setTimeout(() => { if (!done) { done = true; resolve(fallback()); } }, 6000);
      fetch(req, { cache: "no-store" }).then(r => { keep(req, r); if (!done) { done = true; clearTimeout(t); resolve(r); } })
        .catch(() => { if (!done) { done = true; clearTimeout(t); resolve(fallback()); } });
    }));
    return;
  }
  e.respondWith(caches.match(req, { ignoreSearch: true }).then(hit => hit || fetch(req).then(r => keep(req, r))));
});
