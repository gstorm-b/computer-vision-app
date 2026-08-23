# Doc comment style — Doxygen `/** */` + tags

The written rule for how class/struct/enum/function doc comments are written in
C++ headers. Portable by design: nothing below references this project's files,
modules, or paths — it can be copied as-is into another project's `docs/rules/`
(or equivalent) and applied unchanged. Assumes a Doxygen-generated reference
(any `Doxyfile` works; nothing here depends on non-default settings — see §9).

Authority: this doc owns doc-comment MECHANICS (which comment form, which tags,
when). It does not own what the prose should say — writing good, honest,
non-obvious explanations is a separate skill this doc assumes rather than teaches.

## §1 Scope

Applies to every C++ header (`.h`/`.hpp`) meant to be parsed by Doxygen: file
headers, namespaces, classes/structs/enums, functions/methods, and member
variables/constants. `.cpp` files are explicitly OUT of scope for doc comments
(see §6) — they get plain implementation comments only.

## §2 The three comment forms, and when each applies

Doxygen only treats specific comment markers as documentation. Plain `//` and
plain `/* */` are invisible to it — content written that way will never appear
in the generated reference, no matter how well written. This is the single most
common way a doc comment silently fails: it compiles, it reads fine in the
source, and it produces an empty page.

1. **`/** ... *​/` (with tags)** — the default for anything with real content:
   every class/struct/enum/namespace, and every function/method that takes a
   parameter, returns something non-trivial, or has a behavior worth explaining.
   Opens the door to `@param`, `@return`, `@pre`/`@post`, `@see`, `@note` (§3).
2. **`///` (one or a few lines, no tags)** — for a brief that genuinely needs no
   structure: a zero-argument function whose name + one sentence says everything,
   or a short multi-line note above a member variable. If you catch yourself
   wanting to add `@param` or `@return`, that is the signal to switch to form 1.
3. **`///<` (trailing, same line, always exactly one line)** — member variables,
   enum values, and constants. Goes after the declaration on the same line.

Never use plain `//`/`/* */` for anything meant to reach the generated docs.
Reserve plain `//` for local, implementation-only remarks inside a function body
(mainly in `.cpp` files — see §6) — notes that are true only because of how the
code is currently written, not part of the type/function's public contract.

Do not write two competing descriptions of the same thing (e.g. a decorative
`//` banner above a class AND a separate `///` brief below it). Say it once, in
the form Doxygen actually reads. A second, undocumented copy drifts out of sync
silently and just wastes a reader's time figuring out which one is current.

## §3 Tag reference

| Tag | Use for |
|---|---|
| `@file` | One per header, top of file: what this file is / contains (§4). |
| `@brief` | First line of every `/** */` block. Always explicit — do not rely on an auto-brief setting inferring it from the first sentence; explicit `@brief` reads the same regardless of Doxygen config. |
| `@class` / `@struct` / `@enum` / `@namespace` | Optional but recommended immediately above the matching declaration when the doc block is long enough to want a heading in the generated page. |
| `@param[in]` / `@param[out]` / `@param[in,out]` | One per parameter, always with a direction. A function with 3 parameters gets 3 `@param` lines, never a single sentence covering all three. |
| `@return` | General description of the return value. |
| `@retval <value>` | Use instead of / alongside `@return` when specific return values each mean something distinct (e.g. an enum, a sentinel, true/false with different reasons) — one `@retval` line per meaningful value. |
| `@pre` | A precondition the caller must satisfy before calling. |
| `@post` | A postcondition guaranteed after the call returns — including "what fires asynchronously as a result" for callback/event/signal-driven APIs (the call dispatches work, and the real result surfaces later through a callback/event/signal named here). |
| `@throws` | An exception type the function may throw, and when. |
| `@see` | Cross-reference to a related type/function. Use liberally — this is what makes a generated reference feel connected instead of a flat alphabetical list. |
| `@note` | A non-obvious fact worth calling out separately from the flowing prose — renders as a distinct highlighted block. Good home for threading/ownership/lifetime rules. |
| `@warning` | Same as `@note` but for something that causes real damage if ignored (data loss, security, a crash) — renders more prominently. |
| `@code` / `@endcode` | A short usage example, only when the calling pattern is not obvious from the signature + brief alone. |
| `@deprecated` | Marks a symbol as on its way out, with what to use instead. |

## §4 File-level doc

Every header gets a `@file` block at the very top (before includes, or after —
be consistent within the project):

```cpp
/**
 * @file connection_manager.h
 * @brief Pooled network connections with async connect/query calls.
 */
```

One line is enough. Its job is to make the "Files" index in the generated
reference useful instead of a bare list of filenames.

## §5 Type-level doc (class / struct / enum / namespace)

`@brief` one line, then a blank comment line, then as much detail as the type
actually needs: what it owns, its lifecycle, its threading contract, its state
model if it has one. End with `@see` for closely related types.

```cpp
/**
 * @class ConnectionManager
 * @brief Owns a pool of reusable connections and dispatches queries across them.
 *
 * Thread-safe for connect()/query() calls; teardown (close()) must run on the
 * thread that constructed the manager.
 *
 * @see Connection, QueryResult
 */
class ConnectionManager {
```

Enums get the same treatment at the enum level; each enumerator still gets its
own `///<` (§7) — the enum-level `@brief` explains what the enum as a whole
represents, the per-value comments explain each case.

## §6 Function / method doc

Lives in the header, immediately above the declaration. `.cpp` definitions do
NOT repeat it — Doxygen's brief/detail from the header is what's shown; a
`.cpp` gets plain `//` comments only, and only for something non-obvious about
*how* it's implemented (an algorithm choice, a workaround, a "why not the
obvious way" — never a restatement of what the header already documents).

```cpp
/**
 * @brief Runs @p sql against the pool and returns the result asynchronously.
 *
 * Borrows an idle connection (opens a new one if the pool has room), then
 * queues @p sql on it and invokes @p onDone once it completes.
 *
 * @param[in]  sql    Query text.
 * @param[in]  onDone Callback invoked with the result. Called at most once.
 * @return false when the pool is full and no connection is available;
 *         true once the query has been dispatched.
 * @pre Must not be called after close().
 * @post On a true return, @p onDone(result) fires exactly once, from a
 *       worker thread.
 * @see QueryResult
 */
bool query(const std::string &sql, std::function<void(QueryResult)> onDone);
```

A trivial one-line accessor does not need the full block — form 2 (`///`) is
enough:

```cpp
/// @return Number of currently open connections.
int openCount() const;
```

The dividing line: once a doc comment needs `@param` or more than one `@return`
case, use form 1. A single self-evident line never needs a tag at all.

## §7 Member variable / constant / enum value doc

Always `///<`, always trailing, always one line:

```cpp
int m_maxConnections;           ///< Upper bound on pooled connections.
std::vector<Connection> m_pool; ///< Currently open connections (idle or busy).
```

If a member genuinely needs more than one line of explanation, put a `///`
block directly above it instead of forcing it onto one line or wrapping it in
`/** */` — member docs never get the tag treatment, they are always prose:

```cpp
/// Guards the pool during resize(): held for the shortest span that keeps
/// openCount() and m_pool consistent for a concurrent query().
std::mutex m_poolMutex;
```

## §8 Grouping related members

Use Doxygen's own member-group syntax instead of a plain-`//` visual divider —
the plain-`//` version is invisible to the generator (§2); this version renders
as a labeled section in the generated page:

```cpp
/// @name Pool lifecycle
/// @{
/// @brief Closes every pooled connection. Idempotent.
void close();
/// @return Number of currently open connections.
int openCount() const;
/// @}
```

## §9 Formatting conventions

- `/**` on its own line, each continuation line starts with a single space + `*`
  aligned under the second `*` of `/**`, closing `*/` on its own line (see the
  examples above) — this is the layout Doxygen's own examples use and most
  editors auto-continue it.
- One blank `*` line between the `@brief` line and the detail paragraph(s) that
  follow it, and between the detail paragraph(s) and the tag block (`@param`
  onward) — keeps the brief from bleeding into the detail in the rendered page.
- Wrap comment prose at the same column width the project already uses for code.
- This style does not require any particular Doxygen config. `@brief` is always
  written explicitly (§3), so it renders correctly whether or not the project's
  `Doxyfile` has an auto-brief-from-first-sentence setting turned on — the rule
  does not depend on it either way.

## §10 Anti-patterns (things that silently produce empty docs)

- **Plain `//` where Doxygen expects `///`/`/** */`.** Compiles fine, reads
  fine in the source, generates nothing. If a class or method's generated page
  shows no description at all, this is almost always why — check the comment
  marker before assuming the prose is missing.
- **A decorative banner comment duplicating a real doc comment.** Only one of
  them is real; the other is dead weight that will eventually say something
  different from the one Doxygen actually reads.
- **One sentence covering multiple parameters.** Doxygen can only build a
  Parameters table from one `@param` per parameter; a combined sentence in the
  detail text does not populate it.
- **Undirected `@param`.** Always `@param[in]`, `@param[out]`, or
  `@param[in,out]` — omitting the direction loses information a reader (or a
  future maintainer skimming signatures) would otherwise get for free.

## §11 Worked example (complete, generic)

```cpp
/**
 * @file connection_manager.h
 * @brief Pooled network connections with async connect/query calls.
 */

namespace app::net {

/**
 * @class ConnectionManager
 * @brief Owns a pool of reusable connections and dispatches queries across them.
 *
 * Thread-safe for connect()/query() calls; teardown (close()) must run on the
 * thread that constructed the manager.
 *
 * @see Connection, QueryResult
 */
class ConnectionManager {
public:
    /**
     * @brief Constructs the manager; connections open lazily on first use.
     * @param[in] maxConnections Upper bound on pooled connections. Must be > 0.
     */
    explicit ConnectionManager(int maxConnections);

    /**
     * @brief Runs @p sql against the pool and returns the result asynchronously.
     *
     * Borrows an idle connection (opens a new one if the pool has room), then
     * queues @p sql on it and invokes @p onDone once it completes.
     *
     * @param[in] sql    Query text.
     * @param[in] onDone Callback invoked with the result. Called at most once.
     * @return false when the pool is full and no connection is available;
     *         true once the query has been dispatched.
     * @pre Must not be called after close().
     * @post On a true return, @p onDone(result) fires exactly once, from a
     *       worker thread.
     * @see QueryResult
     */
    bool query(const std::string &sql, std::function<void(QueryResult)> onDone);

    /// @name Pool lifecycle
    /// @{
    /// @brief Closes every pooled connection. Idempotent.
    void close();
    /// @return Number of currently open connections.
    int openCount() const;
    /// @}

private:
    int m_maxConnections;            ///< Upper bound on pooled connections.
    std::vector<Connection> m_pool;  ///< Currently open connections (idle or busy).
};

} // namespace app::net
```
