# Build Log — Session 3 (14:00–16:00)

**Note:** reconstructed from chat order, not real timestamps.

## Focus
Rebuilding `frontier` on top of the project's own `Linkedlist<T>`
instead of duplicating list logic; fixing `seekstore`; wiring the full
BFS crawl loop into a `crawler` class; implementing relative→absolute
URL resolution so extracted links are actually fetchable.

## Issues found and fixed

### 1. `frontier` switched to wrap `Linkedlist<node>`
Avoids a second hand-rolled linked list. Surfaced a real correctness
bug: the project's existing `Linkedlist::push()` appends at the
**tail**, and its existing `pop()` *also* removes from the **tail**
(walks to the node before `tail`, frees `tail`). Both ends being the
same makes it a LIFO stack, not a FIFO queue — using it as-is would
have made the crawl silently depth-first instead of breadth-first,
with no error to flag it.

**Fix:** added a new `Linkedlist::pop_front()` (removes from head),
paired with the existing tail-appending `push()`, giving true FIFO.
Iterated through several broken drafts of `pop_front()` (missing
`count--`, wrong destructor call — `temp.~T()` instead of
`temp->~Node<T>()`, unconditional `head=head->next` after already
nulling `head` in the one-element case, incorrectly marked `const`
while mutating members) before landing on a correct version matching
the file's existing conventions (malloc + placement-new allocation,
manual destructor call, `std::underflow_error` on empty).

### 2. `frontier::peek()` accessor mismatches
Went through three incorrect attempts:
- `ls.head->value()` — `head` isn't publicly exposed (only `begin()`
  is), and `.value()` isn't a method, it's a public data member
  `value`.
- `ls.head->value` — right member access syntax, wrong access path
  (`head` still not public).
- `ls[ls.Size()-1]` — technically works via `operator[]`, but reads
  the wrong end once `pop_front()` existed (this was written before
  `pop_front()` was added, while `pop()` still removed from the tail).

**Fix:** `return ls.begin()->value;`, matching the head that
`pop_front()` actually removes from. Added an empty-check throwing
`std::out_of_range`, mirroring `Linkedlist::pop()`'s own behavior.

### 3. `seekstore` — `HashMap` open question flagged
Noted but not resolved: whether `HashMap::push` rehashing invalidates
previously-returned `HashNode*` pointers (relevant to
`seekstore::get()`'s `&n->Value`) depends on internal chaining vs.
bucket-array-move semantics not yet confirmed against the real header.

### 4. `crawler::start()` wired up
Full BFS loop: seed → frontier → seekstore dedupe check → fetch →
save → extract links → re-push. Confirmed `height > 2` depth cap
behavior and counter-based page naming
(`"index_" + to_string(i) + "_" + to_string(height)`) from the
previous session's fix.

### 5. URL resolver / normalizer implemented
`pageload` gained a `lastUrl` member (set in `loadpage()` alongside
`lastHtml`) so `linktag()` has a base to resolve relative hrefs
against. Added `resolveUrl(base, href)` handling:
- already-absolute (scheme://...)
- protocol-relative (`//host/path`)
- root-relative (`/path`)
- document-relative (`path`, `./path`, `../path`), with `.`/`..`
  segment collapsing

`linktag()` now dedupes on the *resolved* URL, not the raw href
string, and strips `#fragment`s before comparison.

Flagged as still open: no same-domain filtering (crawl can follow
off-site links until the depth cap stops it); resolver only handles
`http`/`https`, not a full RFC 3986 implementation.

## State at end of session
Frontier now genuinely FIFO/BFS. `seekstore` compiles and functions,
with one unverified assumption about `HashMap` rehash/pointer
stability. Crawler loop runs end-to-end against a live site. URL
resolution closes the largest functional gap from earlier sessions
(most relative links were previously silently unfetchable).