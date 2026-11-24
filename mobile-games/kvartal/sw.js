// offline cache: the game opens from the home screen without a connection; a new version is fetched in the background
const CACHE = "kvartal-fcfbee3cf0";
const FILES = ["./", "index.html", "manifest.webmanifest", "icon.png", "icon-192.png", "icon-512.png",
  "lib/three.min.js", "lib/CopyShader.js", "lib/LuminosityHighPassShader.js", "lib/EffectComposer.js", "lib/RenderPass.js", "lib/ShaderPass.js", "lib/UnrealBloomPass.js", "lib/BufferGeometryUtils.js"];
self.addEventListener("install", e => { e.waitUntil(caches.open(CACHE).then(c => c.addAll(FILES)).then(() => self.skipWaiting())); });
self.addEventListener("activate", e => { e.waitUntil(caches.keys().then(ks => Promise.all(ks.filter(k => k !== CACHE).map(k => caches.delete(k)))).then(() => self.clients.claim())); });
self.addEventListener("fetch", e => {
  if (e.request.method !== "GET") return;
  e.respondWith(caches.match(e.request, { ignoreSearch: true }).then(hit => {
    const net = fetch(e.request).then(r => { const u = new URL(e.request.url); if (r && (r.ok || r.type === "opaque") && (u.origin === location.origin || u.hostname.startsWith("fonts."))) { const cp = r.clone(); caches.open(CACHE).then(c => c.put(e.request, cp)); } return r; }).catch(() => hit);
    return hit || net;
  }));
});
