# Build Log — Session 4 (16:00–18:00)

**Note:** reconstructed from chat order, not real timestamps.

## Focus
Explaining the `mkdir()` behavior underlying the earlier path bug,
updating the design spec document to match the actual implementation,
then diagnosing a fresh "pages not saving" report against a new (much
harder) seed URL.

## Work done

### 1. `mkdir()` explained
Clarified that `mkdir()` (POSIX syscall, not a shell command) creates
exactly one directory level per call and never creates missing
parents — which is why `savepage()` needs both `mkdir("./include",...)`
and `mkdir("./include/pages",...)` in order, and why the return value
being ignored means real failures (permissions, disk full, name
collision) get masked and only surface later as a generic `ofstream`
open failure.

### 2. Design spec updated to v1.3
Rewrote the appendix and touched up sections 8–16 of the design
document to reflect what's actually been built vs. the original v1.2
draft: `frontier` now correctly documented as `Linkedlist`-backed FIFO
(with the LIFO-stack near-miss recorded as implementation history),
`seekstore` documented against the real `HashMap`, URL Normalizer
section split into "relative→absolute: done" vs. "case/slash/query
canonicalization: not yet done", Link Extraction section updated to
note the resolver integration, Crawler Controller section documents
the `crawler` class and its two intentional divergences from the
original pseudocode (depth cap independent of page-count limit; a URL
is marked seen before its fetch is attempted, not after success).
Added a consolidated outstanding-work list (25.9): URL
canonicalization, retry/delay controller, page-count limit,
URL→filename manifest, same-domain restriction, lazy Chrome launch,
automated tests, Tier 2/CDP path still unexercised by any real crawl,
WebSocket thread-safety/leak/timeout issues.

### 3. New "pages not saving" report — different root cause this time
Seed URL changed to a live Google search-results page with
session-bound query parameters (`sxsrf`, `ei`, `ved` — tokens tied to
the original browser session, meaningless on a fresh fetch).

**Diagnosed two likely causes, not mutually exclusive:**
- The session-bound Google URL itself — refetched cold, Google has no
  reason to serve a normal results page back (redirect, interstitial,
  or block page all possible).
- No `User-Agent` set on the curl request at all — default/blank
  libcurl UA is commonly blocked or treated differently by production
  sites, especially Google.

**Root visibility gap identified:** `fetch()` only ever checked
`curl_easy_perform`'s transport-level result, never the actual HTTP
status code. A 403/429/redirect-to-consent-page all return
`CURLE_OK` with a non-empty body — so the existing code couldn't
distinguish "worked" from "got a block page" and would have happily
`loadpage()`'d and saved garbage.

**Fixes applied to `Browser::fetch()`:**
```cpp
curl_easy_setopt(curl, CURLOPT_USERAGENT,
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
...
long httpCode = 0;
curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
if (httpCode < 200 || httpCode >= 300) { /* treat as failure */ }
```
Recommended swapping the test seed to `https://books.toscrape.com/`
(a scraping-practice site, no anti-bot hardening) to isolate whether
the crawler itself works before testing against Google specifically.

### 4. HTTP/2 loading issue raised
Two possible causes distinguished:
- Broken/mismatched HTTP/2 (nghttp2/ALPN) support in the local libcurl
  build — worked around by forcing `CURLOPT_HTTP_VERSION,
  CURL_HTTP_VERSION_1_1`.
- A TLS/handshake failure that only *looks* like an HTTP/2 problem —
  added `CURLOPT_VERBOSE, 1L` temporarily to get curl's real
  connection trace and tell the two apart definitively.

## State at end of session
`fetch()` now reports HTTP status and body length on every call,
forces HTTP/1.1, sends a real browser User-Agent, and (temporarily)
traces the full connection. Root cause of the Google-specific failure
not yet confirmed from real output — next step is running with
`CURLOPT_VERBOSE` on and reading the trace, or switching to a
non-adversarial test target first.