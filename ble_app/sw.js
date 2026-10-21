// Cache vỏ ứng dụng để mở được khi không có mạng (BLE không cần Internet).
// Đổi CACHE khi sửa file để điện thoại tải bản mới.
const CACHE = 'hop-den-v1';
const SHELL = ['./', 'index.html', 'css/app.css', 'js/app.js', 'js/ble.js', 'js/parser.js', 'js/chart.js', 'manifest.webmanifest', 'icon.svg'];

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL)).then(() => self.skipWaiting()));
});

self.addEventListener('activate', (e) => {
  e.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

// Mạng trước, rớt mạng thì dùng cache (kể cả font Google đã tải lần trước)
self.addEventListener('fetch', (e) => {
  if (e.request.method !== 'GET') return;
  e.respondWith(
    fetch(e.request)
      .then((res) => {
        if (res.ok || res.type === 'opaque') {
          const copy = res.clone();
          caches.open(CACHE).then((c) => c.put(e.request, copy));
        }
        return res;
      })
      .catch(() => caches.match(e.request))
  );
});
