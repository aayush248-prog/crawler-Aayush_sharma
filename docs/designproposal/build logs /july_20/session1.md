# Build Log — Session 5: Live-Site Fetch Debugging

## Focus
Diagnosing a fresh "pages not saving" report after the seed URL was
changed to a live, hardened target — a different root cause than the
filesystem path bug from Session 2.

## Issues found and fixed

### 1. Session-bound Google search URL as seed
The new seed carried query parameters (`sxsrf`, `ei`, `ved`) tied to
the original browser session that generated them — meaningless on a
fresh, cookie-less `curl` fetch. Refetched cold, Google has no reason
to serve back a normal results page.

### 2. No `User-Agent` set on the curl request
Default/blank libcurl UA is commonly blocked or treated differently
by production sites, especially Google.

**Fix:**
```cpp
curl_easy_setopt(curl, CURLOPT_USERAGENT,
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
```

### 3. HTTP status code never checked
`fetch()` only checked `curl_easy_perform`'s transport-level result.
A 403/429/redirect-to-consent-page all return `CURLE_OK` with a
non-empty body, so the code couldn't distinguish "worked" from "got a
block page."

**Fix:**
```cpp
long httpCode = 0;
curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
if (httpCode < 200 || httpCode >= 300) { /* treat as failure */ }
```

Recommended swapping the test seed to `https://books.toscrape.com/`
(a scraping-practice site with no anti-bot hardening) to isolate
whether the crawler itself works before testing against Google
specifically.

### 4. HTTP/2 loading issue raised
Distinguished two possible causes:
- Broken/mismatched HTTP/2 (nghttp2/ALPN) support in the local libcurl
  build — worked around with `CURLOPT_HTTP_VERSION,
  CURL_HTTP_VERSION_1_1`.
- A TLS/handshake failure that only *looks* like an HTTP/2 problem —
  added `CURLOPT_VERBOSE, 1L` temporarily to get curl's real
  connection trace.

## State at end of session
`fetch()` now reports HTTP status and body length on every call,
forces HTTP/1.1, sends a real browser User-Agent, and (temporarily)
traces the full connection. Root cause of the Google-specific failure
not yet confirmed from real output — next step is running with
`CURLOPT_VERBOSE` on and reading the trace, or switching to a
non-adversarial test target first.