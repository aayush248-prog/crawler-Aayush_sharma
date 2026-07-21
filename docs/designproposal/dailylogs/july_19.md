# Daily Build Log — Web Crawler (Project 02)

**Scope:** CMake integration through live-site fetch debugging, in the
order the work actually happened.

---

## Session 1 — CMake Integration

**Focus:** Getting `CMakeLists.txt` to build `main.cpp` (the CDP
scraper), sharing a repo with the existing `DS_LIBRARY` project and
its `runTests` target.

- **`nlohmann/json.hpp` not found** — `DSLibrary` never had
  `nlohmann_json` linked to it; `find_package` finding a library isn't
  enough, the target itself needs `target_link_libraries`. Fixed with
  `find_package(nlohmann_json 3.11 QUIET)` + `FetchContent` fallback,
  linked onto `DSLibrary` directly.
- **Wrong target/source split** — an earlier draft assumed the scraper
  lived in a separate `src/crawler_main.cpp` and created a second
  `WebCrawler` target for it. That file doesn't exist; the scraper
  *is* `main.cpp` at the project root. Removed `WebCrawler`; linked
  `CURL::libcurl`, `nlohmann_json::nlohmann_json`, `Threads::Threads`
  directly onto `DSLibrary` instead.
- **CMP0135 dev warning** — silenced via `cmake_policy(SET CMP0135
  NEW)` + `DOWNLOAD_EXTRACT_TIMESTAMP TRUE`.

Commands given:
```bash
brew install nlohmann-json
rm -rf build && cmake -B build && cmake --build build
```

**End state:** CMake config correct; `main.cpp` itself not yet
compiling clean.

---

## Session 2 — Compile Fixes and Page-Save Path Bug

**Focus:** Clearing `main.cpp` compile errors, first run, diagnosing
why pages weren't saving.

- **`class Node` collided with `include/List.h`'s template `Node<T>`**
  — reachable via `include_directories`; every plain `Node` in the
  crawler resolved to the template. Renamed crawler's type to
  `FrontierNode` (later superseded in Session 3).
- **`Hashmap` vs. `HashMap`** — wrong capitalization plus
  `hs.find(val)` returning `HashNode<K,V>*` not `V*`. Fixed class name
  and `seekstore::get()`'s dereference (`n->Value`).
- **`crawler`'s constructor wasn't one** — `void crawler(string url)`
  is a regular method, not a constructor. Dropped `void`.
- **`fr.front()` called instead of `fr.peek()`** — `front` is a data
  member, not callable. Fixed.
- **Run sequence provided:** `rm -rf build && cmake -B build &&
  cmake --build build && ./build/DSLibrary`. Flagged: hardcoded macOS
  Chrome path, live network dependency, ASan linked in.
- **Pages not saving** — `savepage(name)` was called with the raw URL
  as `name`; every `/` in a URL is a directory separator to the
  filesystem, and `savepage()`'s two `mkdir()` calls only ever create
  `./include` and `./include/pages`, not arbitrary nested paths, so
  `ofstream::open` silently failed on every save. Landed on a
  counter-based filename scheme (`"index_" + to_string(i) + "_" +
  to_string(height)`) after fixing an intermediate `namenormalizer`
  attempt (typo, and a version that only kept the last path segment —
  collided/emptied on every trailing-slash URL) and a `to_string(index)`
  bug where `index` was shadowed by the POSIX `index()` function via
  `using namespace std;`.

**End state:** Compiles clean past the collisions. Save-path bug
diagnosed and fix direction chosen.

---

## Session 3 — Frontier Rebuild, Crawler Wiring, URL Resolver

**Focus:** Rebuilding `frontier` on the project's own `Linkedlist<T>`,
fixing `seekstore`, wiring the full BFS loop, implementing
relative→absolute URL resolution.

- **`frontier` wraps `Linkedlist<node>`** — surfaced that the
  project's existing `Linkedlist::push()`/`pop()` both operate on the
  **tail** (LIFO stack behavior), which would have made the crawl
  silently depth-first instead of breadth-first. Added
  `Linkedlist::pop_front()` (removes from head) paired with the
  existing tail-appending `push()`, giving true FIFO. Iterated through
  several broken drafts (missing `count--`, wrong destructor call,
  null-then-dereference ordering bug, incorrect `const`) before a
  correct version.
- **`frontier::peek()`** — went through `ls.head->value()` (not
  public, not a method), `ls.head->value` (still not public), and
  `ls[ls.Size()-1]` (worked but read the wrong end) before landing on
  `ls.begin()->value`, matching the head `pop_front()` removes from,
  with an `std::out_of_range` guard on empty.
- **`seekstore`** — open question flagged, not resolved: whether
  `HashMap::push` rehashing invalidates previously-returned
  `HashNode*` pointers used by `seekstore::get()`.
- **`crawler::start()` wired up** — full BFS loop: seed → frontier →
  seekstore dedupe → fetch → save → extract → re-push. `height > 2`
  depth cap confirmed.
- **URL resolver implemented** — `pageload` gained `lastUrl`;
  `resolveUrl(base, href)` handles already-absolute,
  protocol-relative, root-relative, and document-relative hrefs, with
  `.`/`..` collapsing and `#fragment` stripping. `linktag()` now
  dedupes on the resolved URL. Flagged as open: no same-domain
  filtering; `http`/`https` only, not full RFC 3986.

**End state:** Frontier genuinely FIFO/BFS. Crawler loop runs
end-to-end against a live site. Largest earlier functional gap
(unfetchable relative links) closed.

---

## Session 4 — mkdir Explanation and Design Spec Update

**Focus:** Explaining the `mkdir()` mechanics behind the Session 2 bug;
bringing the design doc up to date. No code changes this session.

- **`mkdir()` explained** — POSIX syscall, one directory level per
  call, no parent creation, return value previously ignored (masking
  real failures behind a generic downstream `ofstream` error).
- **Design spec → v1.3** — rewrote the appendix and touched up
  sections 8–16: `frontier` documented as `Linkedlist`-backed FIFO
  (with the LIFO near-miss recorded as history), `seekstore` against
  the real `HashMap`, URL Normalization split into
  done-(relative→absolute) vs. open-(canonicalization), Link
  Extraction updated for resolver integration, Crawler Controller
  section documents `crawler`'s two intentional pseudocode
  divergences. Added consolidated outstanding-work list (§25.9): URL
  canonicalization, retry/delay controller, page-count limit,
  URL→filename manifest, same-domain restriction, lazy Chrome launch,
  automated tests, unexercised Tier 2/CDP path, WebSocket
  thread-safety/leak/timeout issues.

**End state:** Documentation reflects actual codebase state.

---

## Session 5 — Live-Site Fetch Debugging

**Focus:** New "pages not saving" report against a hardened live seed
— a different root cause than Session 2's filesystem bug.

- **Session-bound Google search URL as seed** — query params
  (`sxsrf`, `ei`, `ved`) tied to the original browser session,
  meaningless on a cold `curl` fetch.
- **No `User-Agent` set** — default/blank libcurl UA commonly blocked
  by production sites. Fixed with `CURLOPT_USERAGENT` set to a real
  browser string.
- **HTTP status never checked** — `fetch()` only checked
  `curl_easy_perform`'s transport result; a 403/429/redirect all
  return `CURLE_OK` with a non-empty body. Fixed with
  `curl_easy_getinfo(CURLINFO_RESPONSE_CODE, ...)` and treating
  non-2xx as failure. Recommended `https://books.toscrape.com/` as a
  non-adversarial test seed to isolate the crawler's own correctness
  from Google's bot defenses.
- **HTTP/2 loading issue** — distinguished a broken local libcurl
  HTTP/2 build (worked around via `CURLOPT_HTTP_VERSION,
  CURL_HTTP_VERSION_1_1`) from a TLS handshake failure that only looks
  similar (added temporary `CURLOPT_VERBOSE, 1L` for a real connection
  trace).

**End state:** `fetch()` now reports HTTP status/body length, forces
HTTP/1.1, sends a real User-Agent, and traces the connection
temporarily. Root cause of the Google-specific failure not yet
confirmed from real output — pending either the verbose trace or a
switch to the non-adversarial test seed.

---

## Cross-Session Outstanding Items

Carried forward, not yet addressed as of end of day:

- URL canonicalization (lowercase host, strip trailing slash/empty
  query)
- Retry/delay controller for both fetch tiers
- Page-count limit (only depth cap exists)
- URL→filename manifest (no way to trace a saved file back to its
  source URL)
- Same-domain restriction on the frontier
- Lazy Chrome launch (currently unconditional per `Browser` instance)
- Automated tests for `frontier`, `seekstore`, `resolveUrl`, `crawler`
- Tier 2 (headless Chrome/CDP) path still unexercised by any real
  crawl — every test site so far resolved via Tier 1 (curl)
- WebSocket layer: no write synchronization between reader/main
  threads, `sendAndWait` response-map leak on timeout, unbounded
  incoming frame length, no socket/connect timeouts, no
  `curl_global_init()` despite multithreaded curl use
- `HashMap` rehash/pointer-stability assumption in `seekstore::get()`
  unverified against the real header