# Build Log — Session 2 (11:00–13:00)

**Note:** reconstructed from chat order, not real timestamps.

## Focus
Getting `main.cpp` past compile errors caused by name collisions with
the project's own `List.h`/`Hashmap.h`, then diagnosing why crawled
pages weren't being saved to disk.

## Issues found and fixed

### 1. `class Node` collides with `include/List.h`'s template `Node<T>`
The crawler's own linked-list node (used by `frontier`) was named
`Node` — same name as the DS library's own template `Node<T>`, now
reachable because `include_directories` puts `include/` on the search
path. Every plain `Node` use in the crawler code got resolved to the
template instead, breaking `Node* front`, `new Node()`, etc.

**Fix:** renamed the crawler's own type from `Node` → `FrontierNode`
(later superseded — see Session 3) to remove the ambiguity.

### 2. `Hashmap` vs. `HashMap`
`main.cpp` declared `Hashmap<string,node>` (lowercase m); the real
header defines `class HashMap` (capital M). Also, `hs.find(val)`
returns a `HashNode<K,V>*`, not a `V*` — `seekstore::get()` needed to
dereference `n->Value` explicitly.

**Fix:** corrected the class name and `get()`'s return logic.

### 3. `crawler`'s "constructor" wasn't one
```cpp
void crawler(string url){ seedurl=url; }
```
A `void`-returning method named after the class is just a regular
method, not a constructor — `crawler cr("...")` had nothing to bind
to. Fixed by dropping `void`.

### 4. `fr.front()` called where `fr.peek()` was meant
`front` is a data member (a raw pointer), not a callable — calling it
like a function doesn't compile. Corrected to the actual accessor.

## Running the crawler
Provided the full build/run sequence:
```bash
rm -rf build
cmake -B build
cmake --build build
./build/DSLibrary
```
Flagged: hardcoded macOS Chrome path in `Browser()`'s constructor,
live network dependency (fetches `login.salesforce.com` on run),
AddressSanitizer linked in (expect ASan reports, not crashes, if the
known pointer-stability issues trigger).

## Bug: pages not saving to disk
Console showed repeated `"File cannot open: ./include/pages/https://
www.salesforce.com/...txt"` for every crawled URL.

**Root cause:** `savepage(name)` was being called with the raw URL as
`name`. Every `/` in a URL is interpreted by the filesystem as a
directory separator, not a literal character — so
`https://www.salesforce.com/agentforce/operations/` was parsed as a
deeply nested directory path that `savepage()`'s two `mkdir()` calls
(which only ever create `./include` and `./include/pages`, not
arbitrary nested dirs) never created. `ofstream::open` then silently
fails to open, and the exception is caught and logged.

**Fix (two options given):**
- Sanitize the URL into a safe filename (replace unsafe chars with
  `_`), keeping it human-readable.
- Hash the URL into a filename — avoids collisions but loses
  readability without a separate manifest.

Recommended sanitization for active debugging. Iterated through a
user-submitted `namenormalizer` attempt with compile errors
(`inr` typo, only kept the last path segment → empty/colliding names
on every trailing-slash URL) before landing on a counter-based scheme
(`"index_" + to_string(i) + "_" + to_string(height)`) in the next
session, after a separate `to_string(index)` bug (shadowed by the
POSIX `index()` function via `using namespace std;`) was also fixed.

## State at end of session
Compile errors from the Node/Hashmap collisions resolved. Page-save
path bug diagnosed and fix direction chosen (counter-based filenames).
Frontier's underlying data structure (raw pointers vs. wrapping the
project's own `Linkedlist`) still an open question going into the next
session.