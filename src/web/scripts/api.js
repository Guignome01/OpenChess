// Low-level API utilities — fetch wrappers
// Included by all pages. Domain-specific functions live in provider.js.

window.getApi = (url) => fetch(url);

window.postApi = (url, body) => fetch(url, {
    method: 'POST',
    body,
    headers: !!body ? { 'Content-Type': 'application/x-www-form-urlencoded' } : {}
});

window.deleteApi = (url, body) => fetch(url, {
    method: 'DELETE',
    body,
    headers: !!body ? { 'Content-Type': 'application/x-www-form-urlencoded' } : {}
});

// Health check with timeout — used for OTA reboot polling.
window.pollHealth = (timeoutMs) => fetch('/health', { signal: AbortSignal.timeout(timeoutMs) });