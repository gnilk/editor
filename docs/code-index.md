# Code index - structural search over the whole code base

Status: **DESIGN, not started** (opened 2026-10-23). Replaces the sketch in
[`done/session-cache.md`](done/session-cache.md) §6 and is step 5 of [`modal_fixes.md`](modal_fixes.md).

## 1. Aim

A **very fast, contextual search** across a large code base: type part of a name and get the classes,
structs, functions, members, macros it can mean - with their scope and where they live - in the time it
takes to type the next key. It doesn't have to be perfect (no compiler, no build system, no type
resolution), but it must be **smart enough that you prefer it to plain text search everywhere**.

Built on the lexer the editor already has, plus a light structural pass, feeding an index.

The longer aim: the index is the foundation for **a different way of working with code, where the
structure is primary and the text secondary** - navigate by what a code base *is* (namespaces, types,
their members, who uses what) rather than by files and lines. Search is the first consumer; the index
must be shaped so the later ones (§10) don't need a redesign.

**Relationships are first-class, not derived.** When development is more about directing structure than
writing lines, what matters is being able to see and check the structure, and still dive into the
specifics: **dependencies, interactions, modularity** - inheritance, composition (what a type holds),
which file/directory/module a thing lives in, what includes / uses / calls what. The index stores these
as **edges** between symbols, files and modules (§4.3, §5), so "who derives from `ViewBase`", "what does
`Document` hold", "what does `Core/Graphics` depend on" are queries, not text searches. Being able to
**search and visualise around** them is the point; the first time a large project is opened this is
built in the background, and the editor is usable meanwhile.

### Approach: low-hanging fruit first

We don't know yet what the right end state looks like - **we'll know it when we see it**. So: build the
cheap, reliable parts first, use them, and let that decide the next step. The hard problems (§8.1) stay
written down and in the back of our heads - the schema must not *preclude* them - but no step tries to
solve them up front.

### Non-goals (v1)

- Exact semantics: overload resolution, templates instantiated, macros expanded, `#include` graph
  resolved, types of expressions. A heuristic that's right for the common declaration shapes is enough.
- A language server / clangd client. That's a different path (external process, needs a compile DB);
  it could feed the same index later, but the index must work without one.
- Plain-text "search everywhere". Still wanted (D1) but secondary; it is not this index.

## 2. What exists

| Piece | Where | Use for the index |
|---|---|---|
| `LangLineTokenizer` | `Core/Language/LangLineTokenizer.*` | Stack-based, per-line, state carried line to line. `ParseLineWithCurrentState(tokens, u32string)` yields `LangToken{string, idxOrigStr, classification}`. |
| Token classes | `Core/Language/LanguageTokenClass.h` | `kKeyword` (C++: `class struct enum union namespace template typedef using operator ...`), `kKnownType`, `kRegular` (identifiers *and* everything unclassified), `kCodeBlockStart/End` (`{` `}`), `kOperator`, `kPreProcessor`, `kMacroIdentifier`, strings, comments. **No identifier class** - an identifier is a `kRegular` token. |
| Languages | `Core/Language/LanguageSupport/` | C++ (also used for C, .h), JSON, Make, Markdown, Default. `Editor::GetLanguageForExtension`. |
| Folder walk | `Core/FileSystem/FolderScanner.*` | `Scan(root, {exclude, followSymlinks, depth})` with `onEnterDir`/`onFile` callbacks; `workspace.exclude` in config. |
| Change feed | `Core/FolderMonitor.h` (+ macOS/Linux backends) | File created/changed/deleted under a root - keeps the index live. Linux backend: step 7 of the folder-monitor work. |
| Project / cache dirs | `.goatedit/` (project), `~/.cache/goatedit/` (planned cache tier, session-cache §3) | Where a persisted index goes (regenerable → cache tier). |
| Result UI | `Core/UI/Modals/QueryListModal.*` | Edit line + list + preview/commit/cancel hooks. Find is its first user. |

**Throughput today:** the release-only `sqlite3-parse` test tokenizes the sqlite3 amalgamation (8.4 MB,
238k lines) in ~6 s on an M1 through the *editor* path (`TextBuffer` → `Line` objects, per-line lock,
attributes). That's ~40k lines/s - fine for one open file, far too slow for a big tree (a 10M-line tree
would take minutes on one core). The index needs its own path (§4.1) and must be measured first (§11,
step 0).

## 3. Shape of the solution

```
 files on disk ──► (1) lexer ──► token stream ──► (2) structural pass ──► symbols + refs
      ▲            per language,                   per language,               │
      │            no Line objects                 scope stack, heuristics     ▼
 FolderScanner                                                          (3) index store
 FolderMonitor ─────────────── (4) indexer: queue, threads, change detection ──┘
                                                                                │
                                                     (5) query engine ◄─────────┘
                                                            │
                                                     (6) UI: symbol search modal (QueryListModal),
                                                         later outline / structure views
```

Each layer has one job and a plain-data boundary, so a better structural pass (or an LSP feed) can
replace (2) without touching (3)-(6).

## 4. The pipeline

### 4.1 Lexer pass (reuse, but not through `TextBuffer`)

A lightweight entry point that tokenizes a whole file held as one buffer and emits tokens with
`(line, col)` - no `Line` objects, no locks, no attributes:

```cpp
// Calls 'onToken' for every token, in order; line/col are 0-based char positions
void LangLineTokenizer::TokenizeText(const std::u32string &text, const TokenSink &onToken);
```

- Same states and identifier lists as the highlighter → one definition of the language, two consumers.
- A tokenizer instance per indexing thread (it is stateful: `stateStack`).
- UTF-8 → UTF-32 once per file; files above a size cap (D5) are skipped or indexed by name only.
- Comments and strings come out classified, so the structural pass can ignore them (a `class Foo` inside
  a comment is not a declaration).

If profiling shows `IdentifierList::IsPartialMatch` (linear scan per token) dominates, that's the place
to speed up (hash for whole-word lists, a trie for operators) - which helps the highlighter too.

### 4.2 Structural pass (per language - the "smart" part)

A small state machine over the token stream that tracks **brace depth and a scope stack** and recognises
declaration shapes. For C/C++ (v1):

| Shape (tokens, comments/strings skipped) | Symbol |
|---|---|
| `namespace` *Name* `{` | namespace - pushes scope |
| `class`/`struct`/`union` *Name* [`final`] [`:` bases] `{` | class/struct/union - pushes scope; bases recorded |
| `class`/`struct` *Name* `;` | forward declaration (kept, low rank) |
| `enum` [`class`] *Name* [`:` type] `{` ... `}` | enum + its enumerators |
| *Name* `::` ... *Name* `(` ... `)` [qualifiers] `{` at namespace scope | out-of-line function/method **definition** (scope from the qualifier) |
| *type-ish tokens* *Name* `(` ... `)` [qualifiers] `{` or `;` | function definition / declaration (in a class scope: method) |
| *type-ish tokens* *Name* [`=`...]`;` inside a class scope | member variable |
| `using` *Name* `=` / `typedef` ... *Name* `;` | type alias |
| `#define` *Name* | macro |

- **Scope stack:** `{` pushes either the symbol that opened it or an anonymous block; `}` pops. A function
  body is entered but its contents aren't declarations (locals are not indexed in v1).
- **Each symbol gets:** name, kind, qualified name (`gedit::Document::SearchFor`), parent symbol, file,
  line/col of the name, extent (start/end line of the body when there is one), declaration vs definition.
- **It will be wrong sometimes** - macros that open braces, `#if` arms with unbalanced braces, K&R C,
  heavy templates. Mitigations: reset the scope stack at file end; treat `#if/#else` arms independently
  for brace counting (take the first arm); never let one bad file stop the indexer. A wrong symbol
  costs a wrong row, not a crash.
- The pass is **per language** behind one interface, so JSON (keys as a path), Markdown (headings as a
  tree), CMake (functions/targets), JS (plugins) can come later with the same output.

Alongside the symbols the pass emits **relationship edges** it can see locally (§5): `class A : public B`
→ A *inherits* B; a member `std::unique_ptr<Foo> foo;` → the class *holds* Foo (owning); `Foo *` / `Foo &`
→ *refers to* Foo; `#include "Core/Foo.h"` → file *includes* file; a method body naming `Bar::` or calling
`baz(` → *uses* / *calls* (by name, unresolved). Edges carry the **name** of the target; resolving a
name to a symbol happens in the index (§5.2), not in the pass - the pass only sees one file.

Because the lexer emits identifiers as `kRegular`, the pass also records **identifier occurrences**
(name, file, line, col) for every `kRegular` identifier token outside comments/strings - the raw
material for "where is this used" (§10). Optional in v1 (D3), but the schema keeps room for it.

### 4.3 Index store

Two concerns, kept apart:

- **Query structure (in memory):** what the search runs against - interned names, symbol records,
  per-file ranges. Built at startup from the persisted store, updated incrementally.
- **Persistence (on disk):** so a restart doesn't re-index the tree. Regenerable → cache tier
  `~/.cache/goatedit/index/<root-key>.*`.

Schema (whether SQLite tables or a flat binary - D2):

```
files(id, path, module_id, mtime, size, hash, lang, indexer_version)
modules(id, path, parent_id)                          -- directories (or declared modules, §5.3)
symbols(id, file_id, parent_id, name, qualified_name, kind, flags, line, col, end_line)
edges(from_kind, from_id, rel, to_name, to_id, file_id, line, confidence, flags)
                                                      -- rel: inherits, holds, refers, includes,
                                                      --      uses, calls, declares/defines, ...
                                                      -- to_id: resolved later (null = unresolved)
refs(name_id, file_id, line, col)                     -- identifier occurrences (D3)
```

`indexer_version` invalidates a file's rows when the structural pass changes.

### 4.4 Indexer

- **Initial build:** `FolderScanner` with the workspace excludes enumerates files; files whose
  `(mtime, size)` match the store are skipped; the rest go to a work queue drained by N threads
  (N = cores - 1). Each thread: read → lex → structural pass → result batch → one writer applies
  batches (single writer, no lock contention on the store).
- **Incremental:** `FolderMonitor` events re-queue the file (deleted → drop its rows). Open documents:
  re-index on save; optionally on idle from the editor's own parse (D6).
- **Progress & priority:** the open/active files and their directory first; a status-line hint
  ("indexing 12 034 / 48 211"); search works on what is indexed so far.
- Cancellable on project close / quit; never blocks the UI thread.

### 4.5 Query engine

- Input: what's typed in the search box. Matching on the **name**, ranked:
  1. exact > prefix > word-boundary/camel-hump (`SF` → `SearchFor`, `srch` → `SearchFor`) > subsequence
  2. kind (types and functions above members above macros; definition above declaration)
  3. proximity (same file / directory as the active document, recently opened)
- **Context in the query:**
  - `Document::Sea` - scoped: the part before `::` matches a parent scope
  - a kind filter, e.g. `c:Doc` (classes/structs), `f:search` (functions), `m:` members (syntax - D4)
- Target: results for a keypress in **< 10 ms** with 1M symbols. That decides the in-memory structure:
  a sorted, interned name table plus a trigram or first-char index to cut the candidate set before
  fuzzy scoring. (SQLite FTS5 handles prefix/substring well but not camel-hump/subsequence ranking, so
  ranking stays in memory either way.)

### 4.6 UI

- **Go to symbol** (`SymbolSearchModal`, a `QueryListModal` subclass in `Editor/Modals/`), own key
  (D7). Row: kind marker, qualified name, `path:line` dimmed on the right.
- Preview: if the file is open, scroll to it as Find does; if not, nothing (or a peek, later).
- Return: open the file (or switch to it) at the symbol. Esc: back to where you were.
- The same modal can host plain search-everywhere results later as a second source (D1).

## 5. Relationships, modularity and language metadata

### 5.1 What we want to see

| Question | Edge(s) |
|---|---|
| What derives from `ViewBase`? What does `RootView` derive from, all the way up? | `inherits` (transitive) |
| What does `Document` hold - owned (`unique_ptr`, by value) vs shared (`shared_ptr`, `Ref`) vs borrowed (`*`, `&`)? Who holds a `Document`? | `holds` with an ownership flag, `refers` |
| What does `Core/Graphics` depend on? Does anything in `Views/` include `Modals/` (other than `RootView`)? | `includes`, rolled up file → module (§5.3) |
| Who calls `SearchFor`? What does `FindModal::OnCommit` call? | `calls` (by name - approximate) |
| Where is `Editor::Instance()` used from - which modules reach the singleton? | `uses` rolled up to modules |
| Where is this declared, where is it defined? | `declares` / `defines` (header vs source) |

The composition decisions of the last weeks are exactly these questions: "a lower layer never depends on
a higher-level service" (`Core/Graphics` must not include `Core/Session` or `Editor.h`), "only `RootView`
under `Views/` may include `Modals/`", "the toolkit modal doesn't see the clipboard". Today they live in
CLAUDE.md as prose and are checked by grep. With the index they are **queries** - and a query can be
saved as a **rule** and checked continuously (§10).

### 5.2 Resolution

The structural pass sees one file and emits edges to **names** (`to_name`). The index resolves them:

1. qualified match (`gedit::ViewBase`), then
2. the same name in the enclosing namespaces of the source, then
3. a unique name in the whole index, else
4. **ambiguous** - kept with all candidates, shown as such, never guessed silently.

Resolution is re-run for affected names when a file changes (an edge's target may appear or vanish).
No overloads, no template arguments, no ADL - a name is a name. Wrong in places, but the ambiguity is
visible.

### 5.3 Modules

A **module** is a directory by default (`src/Core/UI/Modals`), forming a tree. File edges roll up:
`A.cpp includes B.h` gives `module(A) → module(B)`, with a count. That gives a module dependency graph
for free - and cycles, fan-in/fan-out, "which modules reach X". A project may declare modules and
layer rules explicitly later (e.g. in `.goatedit/`: `Core/Graphics` below `Core/Session`), turning
the CLAUDE.md layering rules into checked config.

### 5.4 Language metadata - classification as data

The structural pass needs more than token classes: it needs to know that `class` *opens a type*, that
`:` after it starts *a base list*, that `public` there is an *access specifier and not a base*, that
`std::unique_ptr<T>` means *owns T* and `std::shared_ptr<T>` / `T::Ref` *shares T*, that `#include`
is a *file dependency*. Today the language definitions (e.g. `CPPLanguage.cpp`) carry only what the
highlighter needs - keyword lists and lexer states.

So the languages grow a second, **declarative** layer of meta-data next to the lexer definition -
roles and patterns over the token stream, not more hand-written C++ per language:

```yaml
# sketch - C++ structure rules (shape, not syntax)
roles:
  type_introducer:   [class, struct, union, enum, "enum class"]
  scope_introducer:  [namespace]
  access_specifier:  [public, protected, private]
  base_list_start:   ":"
  alias:             [using, typedef]
  file_dependency:   ["#include", "#import"]
ownership:                       # member type → relationship flag
  owns:     ["std::unique_ptr<$T>", "$T"]          # by value
  shares:   ["std::shared_ptr<$T>", "$T::Ref", "std::weak_ptr<$T>"]
  borrows:  ["$T *", "$T &"]
project_overrides:               # per project: its own idioms
  shares:   ["$T::Ref"]          # GoatEdit's Ref alias = shared_ptr
```

- The generic **structure engine** (scope stack, brace tracking, pattern matching over tokens) is
  written once; a language supplies its roles and patterns. C++ is the first; JS, CMake, Markdown
  (headings as structure) follow by adding meta-data, mostly not code.
- **Projects can extend it** - their own smart-pointer aliases (`Ref`, `Ptr`), factory idioms
  (`Create()` returns an owner), singleton markers (`Instance()`), module/layer declarations. That's
  where "composition decisions" become something the tool understands.
- Whether this lives in YAML (config dir, like keymaps/themes) or in the language classes as tables
  is D9. Start in code (tables in `CPPLanguage`), move to config once the shape has settled.

### 5.5 Visualising

GoatEdit renders a character grid, so the first views are **trees and lists**, which suit it:

- **Hierarchy view**: a type's bases up and derived types down (`TreeView` exists).
- **Composition view**: a type's members grouped by owns / shares / borrows, each expandable into its
  own members.
- **Dependency view**: a module's (or file's) outgoing and incoming dependencies with counts; cycles
  marked.
- **Matrix** of modules × modules (dependency counts) - fits a grid naturally.
- **Export** of any of these as Graphviz DOT / Mermaid for a real graph picture outside the editor;
  a graphical graph view in the SDL backend is possible later (the window is pixels underneath), but
  not first.

Every row in these views is a jump target (Return opens the symbol/file), so the overview and the
deep dive into the specifics are one keypress apart.

## 6. Where it lives in the code

| New | Where | Notes |
|---|---|---|
| `TokenizeText` | `Core/Language/LangLineTokenizer.*` | The fast lexer entry (§4.1). |
| `StructureEngine` + per-language structure rules (`CPPStructureRules` first) | `Core/Index/` (engine), `Core/Language/LanguageSupport/` (rules next to the lexer definition) | Token stream → symbols, edges, refs (§4.2, §5.4). Plain data out; no Editor dependency - testable on strings. |
| `CodeIndex` (store + query + edge resolution) | `Core/Index/` | §4.3 / §4.5 / §5.2. |
| `ModuleGraph` | `Core/Index/` | File edges rolled up to modules, cycles, rules (§5.3). |
| `Indexer` | `Core/Index/` | Threads, queue, FolderScanner/FolderMonitor glue (§4.4). |
| `SymbolSearchModal` | `Core/Editor/Modals/` | §4.6. |
| Hierarchy / composition / dependency views | `Core/Editor/Views/` (real views, not modals) | §5.5 - side panels like the workspace tree. |
| SQLite amalgamation (if D2 = SQLite) | `src/ext/sqlite-<ver>/` | Vendored like Duktape. |

Layering: `Core/Index` depends on the language layer and the file system, never on `Editor` or the UI.
The editor wires it (root, excludes, cache path, open-document hooks) - same rule as the session cache.

## 7. Tests

- Structural pass on **source strings**: one case per declaration shape in §4.2, plus the known-hard ones
  (macro-opened brace, `#if/#else` braces, nested namespaces, out-of-line `A::B::f`, enum class,
  forward declaration vs definition, `class` inside a comment/string). Assert the symbol set (name, kind,
  qualified name, line).
- Edges on source strings: `inherits` (single, multiple, with access specifiers, templates as bases),
  `holds` with the ownership flag for each pattern in §5.4 (incl. the project `Ref` override), `includes`
  (quoted vs angle), `calls` inside a method body. Resolution: qualified / enclosing namespace / unique /
  ambiguous kept with candidates.
- Module roll-up on a temp tree: counts, a cycle found, a layer rule violated and reported.
- Query ranking on a small fixed symbol set: exact > prefix > camel-hump > subsequence; scope and kind
  filters.
- Indexer on a temp tree: initial build, a changed file re-indexed, a deleted file dropped, an excluded
  dir skipped, an unchanged file not re-lexed (count lex calls).
- Persistence round-trip; a bumped `indexer_version` re-indexes.
- Performance smoke (release only, like `sqlite3-parse`): lines/s for the lexer path and query time on a
  generated 1M-symbol set.

### 7.1 Real code bases to judge it on

Unit tests prove the shapes; whether the index is *useful* is judged on real projects the user knows
well (counts: C/C++ files and lines, measured 2026-10-23, build dirs and `.git` excluded). Live,
disciplined code bases come first - they're the ones where a wrong edge is noticed at once.

**Stage 1 - live projects, used daily:**

| Project | Path (`~/src/...`) | Size | Why it's a good test |
|---|---|---|---|
| GoatEdit | `github.com/editor` | 351 files, ~58k lines (C++) | Our own code - the layering rules (CLAUDE.md) are known, so edges can be checked against them. Step 1 target. |
| testrunner | `github.com/testrunner` | 154 files, ~15k lines (C++) | Live and used daily (GoatEdit's test framework). Cross-platform (platform-specific files / `#ifdef`s), plugin-style loading of test libraries. |
| noc_goa | `github.com/noc_goa` | 180 files, ~73k lines (mostly C, since 1998) | Live, still in use. Old C: no classes, structure is structs + function pointers + naming conventions + files. Tests that the index is still useful without OO (modules, includes, struct composition, function-pointer tables). |
| vcpu | `github.com/vcpu` | 130 files, ~23k lines (C++) | An experiment, less disciplined - last of stage 1. Small but complex: CPU + MMU emulator, AST-style assembler, several linkers - hierarchies and interfaces, the §8.1 problems. |

**Stage 2 - a large, modular system:**

| Project | Path (`~/src/...`) | Size | Why it's a good test |
|---|---|---|---|
| PuckoNew | `embedded/libraries/PuckoNew` | ~1400 files, ~560k lines (C/C++) | Multi-MCU meta-OS with its own component/build system: modules and dependencies are the point. External dependencies (mbedTLS, lwIP, SQLite) are all modularised components; the SQLite amalgamation (`sqlite3.c`, 238k lines / 8.4 MB) lives in the source tree but is not really used. **`src/Pucko/Platforms`** (~430 files) is an elaborate platform tree - `arduino/esp32`, `arm/{efr32,nrf52,nrf9160}`, `espidf`, `macos`, `win32`, `zephyr/{nrf52,nrf9160,stm32/...}`, `generic` - each with its own `Components/`; it has repeatedly confused AI audits. |

**Stage 3 - scale:** the Linux kernel (~30M lines).

Notes from the counts:

- **Lines are not structure.** In PuckoNew one vendored file is ~40 % of the lines, and a shallow look
  (file sizes, text search) says SQLite is a big part of the system. A deeper analysis would show the
  opposite: it's barely used and the external dependencies are carefully managed behind components. The
  index must be able to show *that* - who actually depends on a component, through which interface -
  rather than echo the line counts. So: external components are modules with a public surface (their
  headers / the names that are used) and internals indexed shallowly or not at all; excludes and a size
  cap (D5) from step 2; and a "depended on by" count per module that reflects real use.
- **C matters as much as C++** (noc_goa, parts of PuckoNew): the C++ rules must degrade gracefully to C -
  structs, typedefs, function prototypes/definitions, `#include`, function-pointer members.
- Order: stage 1 GoatEdit (step 1) → testrunner → noc_goa (C) → vcpu; stage 2 PuckoNew (modules,
  components, external dependencies); stage 3 the Linux kernel.

## 8. Risks

- **Lexer speed** - the measured editor path is ~40k lines/s. If the fast path can't reach a few hundred
  thousand lines/s per core, the first index of a big tree is slow (but it's a one-off; persistence and
  incremental updates carry the rest).
- **Heuristic quality** - the structural pass decides whether this beats text search. Mitigation: start
  with C++ (the main use), a fixture set of real headers/sources from this repo, judge by use.
- **Memory** - refs (every identifier occurrence) are large on a big tree; that's why D3 keeps them
  optional and they might stay on disk only.
- **Encoding / huge / generated files** - size cap, skip binaries, treat non-UTF-8 leniently.
- **Resolution noise** - name-based edges (`calls`, `uses`) on common names (`Get`, `Init`, `size`)
  resolve badly. Mitigation: show ambiguity instead of guessing; rank by proximity; `inherits` / `holds` /
  `includes` are far more reliable than `calls` and come first.
- **Rule engine creep** - §5.4 can grow into a parser generator. Keep it to roles + a handful of token
  patterns; anything that needs real parsing is a C++ hook in the language's rules, not more YAML.

### 8.1 Known hard problems - parked, not forgotten

Relationships that the token stream of one file does not show. None of these is in scope for the early
steps; each is listed with what the index would need, so the schema leaves room.

| Problem | Example | Why it's hard | What it would need |
|---|---|---|---|
| **Dependency injection** | a service handed in through a constructor or a setter (`SetOnModalActive(fn)`, an injected `autoSaveHandler`) | the dependency is decided at the *call site* that wires it, not where the type is declared | edges from the wiring site: "A is given B here" - a `wires` / `injects` relation on calls into constructors / setters |
| **Interfaces assigned through construction** | a member `ScreenBase::Ref screen` that is an `SDLScreen` at runtime | the declared type is the interface; the concrete type is chosen elsewhere | `holds` stays on the interface; a separate "implemented by" (`inherits` reversed) + "constructed as" at creation sites (`make_shared<SDLScreen>`) |
| **Virtual dispatch** | `view->OnAction(...)` | a call by name hits every override | `calls` to the interface method + `overrides` edges; show the fan-out, don't pick one |
| **Callbacks / `std::function` / lambdas** | `rootView.SetOnModalActive([](bool){...})` | the call edge goes through a stored function object | treat "passes a lambda/function to X" as an edge from the wiring site; the lambda body's calls belong to the enclosing symbol |
| **Singletons / service locators** | `Editor::Instance().GetClipBoard()` | a hidden dependency - not in any member or parameter | `uses` on `Instance()` calls; easy to collect, worth showing per module (who reaches the singleton) |
| **Macros** | a macro that declares a class or opens a scope | the structure is only visible after expansion | lexer sees the macro name; per-project rules can say "this macro declares X" (§5.4) |
| **Templates / CRTP / type aliases** | `class A : public Base<A>`, `using Ref = std::shared_ptr<T>` | the real type is a parameter or an alias | keep the name as written; aliases resolved through the index; no instantiation |
| **Build variants / platform selection** | PuckoNew `src/Pucko/Platforms`: the same interface implemented once per platform (`macos/`, `win32/`, `zephyr/nrf52/`, ...), the build picks one | a text or name index sees N definitions of one thing and calls it ambiguous or duplicated; which one is "real" depends on the build configuration, not the source | a **variant** dimension on files/modules (platform dir = variant of an interface module), edges `implements-for-platform`; group the N definitions as one interface with N variants instead of N conflicts. Ideally read from the build system's selection, at first from directory conventions (project rules, §5.4) |
| **Layered variants** | PuckoNew `zephyr/common` + `zephyr/nrf52`, `zephyr/stm32/stm32h743`: a common layer with per-MCU overrides; platform `Components/` extend a generic component (Bluetooth, file system) for one platform | not alternatives but a stack - treating `common` as one more platform is wrong | variants with a **parent** (like keymap `inherit`): `common` is the base of the MCU variants; platform component dirs are `extends` edges onto the generic component |
| **Abandoned code** | platforms that are no longer maintained - only visible from history and missing features | nothing in the source says "dead" | **hints, not facts**: liveness (last commit per module, `git log` on the dir) + coverage (a variant implementing less of the component interface than the live ones). Shown as "possibly stale"; the user decides |
| **Knowledge the source doesn't carry** | "`zephyr/common` is the base", "this platform is abandoned", "SQLite is not really used" | needs intimate knowledge of the project or a deep analysis | a project file in `.goatedit/` where the user states it - taken as fact by the index, overriding inference. The tool proposes (hints), the user confirms |
| **Cross-language** | JS plugins calling into `EditorAPI`, keymaps naming actions | different languages, a binding layer in between | per-language edges + a binding table (`dukglue_register_method` names) - much later |

The general rule: **when the tool doesn't know, it shows that** (ambiguous, unresolved, "via
interface") rather than guessing. Edges carry a provenance/confidence flag (`declared`, `inferred`,
`ambiguous`) so the views can tell solid structure from guesses.

## 9. Relation to "search everywhere"

Plain text search across files (grep-like) is still useful for strings, comments, config. It's simpler
(no index needed: a background scan with the excludes and a result cap) and can share the modal. The
symbol search should be the **default** you reach for; text search the fallback. Order: D1.

## 10. Later - structure first

What the index enables once it exists (not v1, but the schema must allow them):

- **Outline** of the active file (symbols with extents) - a side view or a picker.
- **Type view**: a class with its members, bases, derived classes; namespaces as a tree.
- **Usages**: where a name occurs (refs), grouped by enclosing symbol - approximate callers.
- **Architecture rules**: saved queries over edges/modules ("nothing in `Core/Graphics` includes
  `Core/Session`", "only `RootView` under `Views/` includes `Modals/`") checked on every re-index, shown
  like diagnostics. The layering decisions written as prose in CLAUDE.md become checked facts.
- **Impact**: before changing a type, what depends on it - transitively, per module.

### 10.1 Semantic highlighting - the visible proof that the index works

Today every identifier is `kRegular` - `Document`, `SearchFor`, `viewTopLine`, `MAX_BUFFER_SIZE` all get
one colour. The structure pass and the index know what each name *is*, so the highlighter can show it.
The colours are mostly a way to **see** that the indexing works - a wrongly coloured name is a bug
report for free.

- **Kinds:** user type (class/struct/union/enum/alias), function/method, member, macro, enum value,
  namespace. New `kLanguageTokenClass` values + theme keys, each **falling back to `regular`** so
  existing themes don't change.
- **Two sources, cheapest first:** (1) the open file's own structure pass (members, local types,
  methods) - no index needed, can come right after step 1 (step 1b); (2) the workspace index for names
  declared elsewhere (`ViewBase` used in `RootView.cpp`) - after step 2. A name → kind lookup is a hash
  probe per identifier, nothing next to the lexing.
- **Hook:** a reclassification pass over a line's `kRegular` identifiers after the tokenizer -
  `LanguageBase::OnPostProcessParsedLine` (the per-line callback markdown uses) is the seam. The
  language layer must not depend on the index: the editor injects a plain `name → kind` lookup.
- **Ambiguous names** (a type here, a member there) stay `regular` or take the local meaning first -
  never a guess.

**Themes - import, don't read foreign formats at runtime (D13).** GoatEdit's `colors.json` borrows
Sublime's colour-scheme *language* (`variables`, `hsl()`/`var()`/`color()`/`alpha()`), but its keys are
GoatEdit's own token-class names in its own sections (`globals`/`content`/`ui`/`terminal`) - it has no
Sublime `rules`, and years of tuning have made it GoatEdit's own format. That stays the **one runtime
format**: the theme loader is a plain token class → colour lookup. Other editors' themes come in through
an **importer** (step 1c) that converts once and saves a GoatEdit theme the user can then tune:

- **Sources:** Sublime `.sublime-color-scheme` (`globals`, `rules` by scope) and VS Code themes (`colors`
  for the UI, `tokenColors` by scope, `semanticTokenColors`). Both name code by **TextMate scopes**, so
  one matcher serves both; VS Code's semantic kinds (`class`, `function`, `method`, `property`,
  `namespace`, `enumMember`, `macro`) map almost 1:1 onto the new token classes, and its `colors`
  (`editor.background`, `editorGutter.*`, `terminal.ansi*`) onto `ui` / `terminal`.
- **The scope table** (one table, importer only): each token class → the scopes it takes its colour
  from, best (longest) matching selector wins: `keyword` → `keyword`, `known_type` → `storage.type`,
  `string` → `string`, `line_comment` → `comment.line`, the new kinds → `entity.name.type` /
  `entity.name.class`, `entity.name.function`, `variable.other.member`, `entity.name.namespace`,
  `constant.other.enum`, `entity.name.macro` / `support.macro` (exact names checked against stock
  schemes when 1c is built).
- **Lossy, and says so:** compound selectors (`source.c++ entity.name.function - meta.block`),
  per-language rules and font styles don't map; the importer reports what it dropped or approximated.
  VS Code themes need a lenient reader (comments, trailing commas) and `include` of a parent theme.
- **Use:** `goatedit --import-theme <file> [--name x]` writes `x.theme.yml` + `x.colors.json` to the
  user config dir (never the system one). An upstream theme change means re-running the import - fine
  for themes.

**Same data, beyond colour:** code folding from symbol extents; a scope breadcrumb
(`gedit › Document › SearchFor`) in the header/status line; a sticky scope line when the enclosing
function's start has scrolled out of view; scope-aware brace matching and indent.
- **Navigate by structure**: next/previous symbol, go to parent scope, jump between declaration and
  definition.
- **Edit in structure**: open a symbol (one function) as the unit of editing rather than a whole file;
  the file becomes the storage format. This is the "code secondary, structure primary" direction - the
  index (symbol extents, parents, refs) is what makes it possible.

## 11. Steps

| # | Step | Status |
|---|---|---|
| 0 | **Measure.** `TokenizeText` fast path + a release-only benchmark (lines/s) on sqlite3.c and this repo. Decides threading and whether the lexer needs speeding up first. | ✅ §11.1 - plus two tokenizer fixes (~3-4x, highlighter too) |
| 1 | ✅ §11.2 - **C++ structure engine** over strings - the low-hanging fruit only: namespaces, classes/structs, methods/functions, members, `#include`, and the edges visible right in the declaration (`inherits`, `holds` + ownership). Rules as tables in code (§5.4), shape + edge tests (§7). No storage yet - dump symbols/edges for a file and judge on this repo. Nothing from §8.1. | - |
| 1b | **Semantic highlighting from the file itself** (§10.1): new token classes with `regular` fallback, reclassification via the post-line hook, the open file's own structure pass. New theme keys in `colors.json`; no theme-format work. | - |
| 1c | **Theme importer** (§10.1, D13): Sublime / VS Code theme → GoatEdit `theme.yml` + `colors.json` via the scope table; `--import-theme`. Independent of the index - can come any time after 1b. | - |
| 2 | **In-memory index + Go to symbol** over the workspace, in the background: indexer threads, resolution (§5.2), query ranking, `SymbolSearchModal`. | - |
| 3 | **Relationship views**: hierarchy and composition (trees, §5.5), "derived from / held by" from any symbol. | - |
| 4 | **Modules**: file → module roll-up, dependency view + matrix, cycles; DOT/Mermaid export. | - |
| 5 | **Persistence + incremental**: store (D2), `FolderMonitor`, re-index on save. | - |
| 6 | **Calls / uses / refs** (D3), usages view; **architecture rules** (§10). | - |
| 7 | **Rules as data**: move the C++ tables to config, project overrides; a second language (§5.4, D9). | - |

### 11.1 Step 0 results (2026-10-23, M-series Mac, release, 10 cores)

**What was built:** `LangLineTokenizer::TokenizeText(text, onToken)` - the highlighter's states over a
whole text, tokens with their line, no `Line` objects / locks / attributes. Tests (`codeindex` module):
same tokens (position + class) as the editor's parse of `ConvertUTF.cpp`; line numbers, CRLF, empty lines,
a block comment carried across lines, an unterminated construct not leaking into the next call.
Mutation-checked (state reset per line → both cases fail). Benchmark: `gedit-bench` (a plain executable -
**not** a trun case: trun links the ASan runtime, which made the same run ~5x slower and mostly measured
ASan's allocator).

| Corpus (no excludes - build dirs included) | Lines | Editor path | Fast path, 1 thread | Fast path, 9 threads |
|---|---|---|---|---|
| GoatEdit `src/Core` | 43k | 66k lines/s | 148k | 379k |
| testrunner | 550k | 74k | 167k | 483k |
| noc_goa | 66k | 61k | 155k | 247k |
| vcpu | 56k | 67k | 154k | 414k |
| sqlite3.c (one file) | 238k | 118k | 289k | 290k (one file = one thread) |
| PuckoNew `src` | 437k | 96k | 232k | 369k |

UTF-8 → UTF-32 is negligible (~2 ms per MB).

**Reading it (before the fixes):** the fast path was ~2.3x the editor path on one thread, but threads
scaled poorly (~3x on 9). Profile (`sample`, no ASan): ~45 % allocation/copying and allocator lock
contention, ~40 % in `GetNextToken`. Two causes in the shared tokenizer code:

1. `IdentifierList::IsPartialMatch` / `IsFullMatch` looped with `for (auto s: tokens)` - a **copy of every
   keyword/operator string**, for every list, for every token. Worse: the postfix check runs per
   *character* of an identifier over ~60 operators - ~60 string copies per character.
2. `stateStack.top()` / a `State::Ref` parameter copied a `shared_ptr<State>` several times per token -
   atomic ref counts on states shared by every tokenizer copy → cache-line contention between threads
   (after fix 1 the threaded run was *slower* than one thread).

**Fixed** (`for (auto &s: ...)`; raw `State *` in the per-token path - the states are owned by the
tokenizer's map and outlive a parse). Same tokens (the `codeindex` equivalence test and the highlighter
tests are green). After:

| Corpus | Lines | Editor path | Fast path, 1 thread | Fast path, 9 threads |
|---|---|---|---|---|
| GoatEdit `src/Core` | 43k | 196k lines/s (3.0x) | 565k (3.8x) | 2.54M (6.7x) |
| testrunner | 550k | 211k (2.9x) | 632k (3.8x) | 3.79M (7.8x) |
| noc_goa | 66k | 167k (2.8x) | 591k (3.8x) | 2.66M (10.8x) |
| vcpu | 56k | 202k (3.0x) | 604k (3.9x) | 2.67M (6.5x) |
| sqlite3.c (one file) | 238k | 262k (2.2x) | 918k (3.2x) | 912k (one file = one thread) |
| PuckoNew `src` | 437k | 230k (2.4x) | 766k (3.3x) | 1.52M (4.1x) |

(factor = vs. before the fixes)

- The **editor's highlighter got ~2.5-3x faster** as a side effect (same code).
- Index lexing: a 500k-line tree in ~0.15-0.3 s on 9 threads; the Linux kernel (~30M lines) roughly
  10-20 s. The lexer is not a concern for the index.
- Threads scale ~5-6x on 9 when files are balanced; one huge file is one unit of work (PuckoNew is
  dominated by sqlite3.c).
- What's left in the profile: `GetNextToken`'s linear scans (operators, whole-word lists), the action
  hash lookup per token, per-token `u32string` in `LangToken`. Further gains need structural changes (a
  trie / first-char table for operators, a token sink with views) - not worth it now.

### 11.2 Step 1 results (2026-10-23)

**What was built:** `src/Core/Index/` - `FileStructure.h` (plain data: `CodeSymbol`, `CodeEdge`),
`StructureScanner` (interface), `CPPStructureScanner` (C and C++). One pass over `TokenizeText`'s tokens:
preprocessor lines first (`#include` → edge, `#define` → macro, conditionals keep the first arm, `#if 0`
keeps the `#else`, `\` continuations skipped, comments/strings dropped), then a scope stack + a statement
buffer classified at `{` / `;` / `}`; function bodies are skipped by brace counting.

- **Symbols:** namespaces (incl. `a::b`), class/struct/union (incl. anonymous, `typedef struct {...} Name`
  named at the `;` with members re-qualified), enums + enumerators (plain: enclosing scope; `enum class`:
  scoped), methods (in a class) / functions (free or out-of-line `A::f` - the qualifier may be a class or a
  namespace, resolution is step 2), constructors/destructors/operators, members, variables (incl.
  out-of-line static members `T Runloop::bQuit`), aliases (`using X =`, `typedef`, function pointer
  typedefs), macros. Each with qualified name, parent, name position, `beginLine`/`endLine` extent,
  declaration vs definition.
- **Edges:** `inherits` (base lists: access, virtual, template args stripped), `holds` + ownership
  (by value / `unique_ptr` / `T::Ptr` own; `shared_ptr` / `weak_ptr` / `T::Ref` share; `*` / `&` borrow;
  containers take their element's; `std::` library types and builtins give no edge), `includes`
  (quoted / system).
- **Handled noise:** macro call prefixes (`DECLARE_X(Foo) void f();`), macro statements (`REGISTER(x);`),
  macro-headed blocks (`TEST(a, b) {`), export macros, attributes, `template<...>`, initializer braces
  (`= {...}`, `Foo f{1}`, ctor init lists `: a(1), b{2} {`), lambdas, default arguments, `extern "C" {`.
- **Tests:** 9 shape cases in `codeindex` (classes, inherits, holds, out-of-line, enums, C typedefs,
  preprocessor, macros, initializers). Mutation-checked: conditionals ignored, no macro-prefix strip,
  `Ref` as owns, the ctor-init-list marker removed - each fails its case.
- **Dump:** `gedit-bench --dump <file-or-dir>` - judged on GoatEdit `src/Core`, testrunner, noc_goa (C),
  PuckoNew. No crashes over all corpora including PuckoNew's vendored/generated code (`bld/`, `examples/`).
- **Speed:** lexer + structure ~450-620k lines/s on one thread (the structure pass adds ~13 % to the
  lexer). PuckoNew `src` (437k lines): 0.7 s, 30k symbols, 6.9k edges.

**Found on the way (lexer bugs, fixed - affect highlighting too):**

- C++: `{` / `}` were not word-ending postfix characters, so `return {value};` lexed `value}` as one token
  - the braces didn't balance (and the `}` was coloured as text). Test `cpplang braceafterword`.
- JSON: same for `{ } [ ]` - `true}` was one token, not a keyword + `}`. Test `jsonlang closeafterword`.

**Seen in the dumps:**

- `Document` *shares* its `TextBuffer` (`TextBuffer::Ref`) although the comment says it owns it - the kind
  of fact the index is for.
- noc_goa: `#ifdef __GOA_AMIGA__` first arm wins (`float ambient` instead of `GOA_FRGBA`) - the parked
  platform-variant problem (§8.1), as expected.
- PuckoNew: the most-derived bases are interfaces with many platform implementations (`Platform` 15,
  `CPUBoard` 16, `GPIOController` 12, `IWriter` 29) - the variant structure becomes visible as data.

**Known limits (not handled, by design for now):** the most vexing parse (`Foo f(1);` is a function
declaration), anonymous members not distinguished from named (`vtxinfo.size` is `GOA_MATERIAL::size`),
locals / local classes / lambdas inside bodies, `#if` arms other than the first, macros that open scopes.

## 12. Open decisions

| # | Question | Leaning |
|---|---|---|
| D1 | Plain text search-everywhere: before, after or alongside the symbol index? | After step 2 - symbol search first, as the one you'd prefer. |
| D2 | Persistence: SQLite (FTS5) amalgamation, or a flat versioned binary (`BinBuffer`, like the terminal scrollback) loaded into memory? | Decide at step 5. Name search runs in memory either way. The relationship focus tilts towards SQLite: transitive questions (all derived types, impact, module reach) are recursive CTEs, and ad-hoc structure queries / saved rules (§10) are SQL instead of hand-written traversals. |
| D3 | Index identifier occurrences (refs) from the start? | Collect in step 1 (cheap, same pass), store from step 4. |
| D4 | Query syntax for kind/scope filters (`c:`, `f:`, `Scope::name`)? | `::` scoping in step 2; kind prefixes when there's a need. |
| D5 | Limits: max file size, max files, which extensions (by language map only?) | Index only files with a structural scanner (C/C++ in v1); cap ~4 MB per file. |
| D6 | Open, unsaved buffers: index the buffer content (from the editor's parse) or only on save? | On save in v1. |
| D7 | Key for Go to symbol? | Pick at step 2 (e.g. Cmd+T / Ctrl+T, or Cmd+Shift+O). |
| D8 | Several workspace roots: one index per root (cache key per root) or one per workspace? | Per root - matches the session cache key; a query fans out over the open roots. |
| D9 | Structure rules (§5.4): YAML in the config dir (like keymaps/themes, user/project overridable) or tables in the language classes? | Tables in code first (step 1); config once the shape has settled (step 7). |
| D10 | Module = directory, or declared? | Directory by default; declared modules / layers in `.goatedit/` later (§5.3). |
| D11 | Graph pictures: export only (DOT/Mermaid), or a pixel graph view in the SDL backend? | Export first; a drawn view only if the trees/matrix aren't enough. |
| D12 | A full parser (e.g. the hand-written AST parser in `vcpu`, which drives the assembler and a scripting language) as a second structure producer? | Overkill for C/C++ (shallow pass instead); a good fit for languages we control (the vcpu assembler / script). The structure output (symbols + edges) is the interface, so a full parser can plug in next to the shallow pass later. tree-sitter is the third-party equivalent if the shallow pass proves too weak. |
| D13 | Themes: read Sublime `rules` (scope → colour) in addition to GoatEdit's keys? | **No - import instead** (revised 2026-10-24). One runtime format (GoatEdit's own, already diverged from Sublime's); Sublime and VS Code themes are converted once by an importer (step 1c) into an editable GoatEdit theme. Both use TextMate scopes, so one scope table serves both (§10.1). |
