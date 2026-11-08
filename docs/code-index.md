# Code index - structural search over the whole code base

Status: **In progress** - steps 0, 1, 1b and 1c shipped, step 2 next (§11). Opened 2026-10-23. Replaces the sketch in
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
- **Incremental:** `FolderMonitor` events re-queue the file (deleted → drop its rows). Open documents
  feed the index from the buffer: the declarations 1b already keeps on each `Line`, no re-read and no
  second lex (D6). A closed file comes from disk.
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

### 5.6 Smart go-to and the navigation stack (planned 2026-11-06)

Jumping around the code from the name under the cursor, and getting back to where the thought started. The views of §5.5
answer "how does this fit together"; this answers "take me there", from the same data.

**Go to, from the name under the cursor:**

| Action | Goes to | Needs |
|---|---|---|
| Go to definition | The body: the function's or the class's `{` | The index today (`isDefinition`) |
| Go to declaration | The declaration, normally in the header | The index today |
| Toggle | The other one of those two, from either side | The index today |
| Go to implementation | From a virtual method or base class: the overrides, or the derived types | `inherits` edges (step 3, §11.10) |
| Go to base | The method or class this one overrides or derives from | `inherits` edges (step 3, §11.10) |
| Go to type | The type of a member or variable | Type on symbol (noted in §11.6) |
| Find references | Where the name is used | Refs (step 6, D3) |

**Finding the target:** a name is a name (§5.2), so this narrows what it can and never guesses:

1. The qualifier as written (`Editor::Instance`, `gedit::Line`) and the enclosing scopes, as in §5.2.
2. The syntax: `a.size()` is a method, `Foo x` a type - the same rules that colour it (§11.6).
3. Proximity, as in §4.5: the same file, the file's own header, the same directory.

One candidate jumps at once. Several open a list in the style of Go to symbol, best first, with the location on each row;
`a.size()` will often land here, since the type of `a` isn't known. A library name has no target (go-to-definition into
system headers isn't a goal, §10.2), so the status line says what it is and the cursor stays.

**The navigation stack:** back and forward through jumps, not through every cursor move.

- **What goes on it:** where a jump started and where it landed: any go-to above, Go to symbol, go to line, a row in a
  relationship view. Moving the cursor doesn't, or the stack fills with noise.
- **Smart about edits:** an entry holds the nearest symbol at or above its line and the distance to it, not only a line.
  Lines above it can come and go and back still lands in the right place. If the symbol is gone it falls back to the line,
  clamped. Not the enclosing symbol: an open file's symbols have no extent in the index, and a function body holds none of
  its own, so the nearest one is the enclosing one where it matters.
- **Smart about noise:** a jump within the same symbol replaces the top entry instead of adding one, and so does going
  back and forth between the same two places.
- **Across files:** an entry for a closed file reopens it.
- **Jumping from the middle:** going back and then jumping somewhere new drops the forward entries, as a browser does. A
  tree that keeps them is possible later; it's a view over the same entries.

**Decided (2026-11-06):**
- **Keys:** back and forward are `Cmd+[` and `Cmd+]`, `Ctrl` on Linux: functionality, so not the `Option` pair, which is
  already mapped.
- **One stack per editor,** not per view.
- **It survives a restart,** through the session cache (`docs/session-cache.md`). An entry holds the path, the symbol and
  the offset, so it needs nothing from a previous run but the file.

- **Go-to key:** the toggle is `Cmd+B`, `Ctrl` on Linux. From a use it goes to the definition, so one key reaches both;
  `GotoDefinition` and `GotoDeclaration` exist without a key of their own.

**Open:** whether `Cmd+click` jumps too.

**Order:** definition, declaration, toggle and the stack need nothing from step 3 and come first, as step 2b. Implementation
and base come with the edges of step 3, and references with step 6.

### 5.7 The context menu at the caret (idea, 2026-11-08)

**An idea, not a plan.** Nothing here is decided or scheduled.

Early Delphi and the Borland C/C++ IDEs had a menu at the caret: one key, and a short list of what could be done with the
word under it, such as go to definition, go to header or follow. Early Visual Assist had the same. Steps 2 and 3 give the
index what it needs to build that menu, so navigation can become one key and a short list instead of a key per action.

**What it could list, from the word under the caret:**

| On | Entries |
|---|---|
| A type | Go to definition, go to declaration, show relationships, go to base, derived types, held by |
| A method | Go to definition, go to declaration. Overrides need an edge kind we don't record yet. |
| An `#include` | Open the file |

- **Only what leads somewhere.** The index can count each entry before the menu opens, so "no base" is greyed out or left
  off, and the menu never offers something that ends in "not found". The counts go on the rows: `Derived (12)`.
- **Most likely first.** From a declaration the implementation comes first, from a use the definition. A type with many
  derived types puts that entry high. The relations of §11.10 are plain data, so this ranking is cheap.
- **One key for the rare ones.** Go to base still has only a stand-in key (`Cmd+Shift+U`); it could live here instead.

**Key:** `Cmd+Return`, `Ctrl` on Linux: functionality (see `CLAUDE.md`, Key bindings). The old menus' `Cmd+Space` is
Spotlight on macOS, and `Ctrl+Space` switches input sources.

**The UI:** there is no pop-up menu, but a modal can act as one: placed at the caret instead of centred (windows draw in
absolute coordinates on SDL3), no caption, sized to its rows, one level, arrows, Enter and Esc. `ListSelectionModal` is most
of the way there.

**Member completion is a different problem.** For `myStruct.<key>` the old menus listed the members, the likely one first:
after `myStruct.x = 4;` that is `y`, the member declared next. The index knows a type's members and their order, so that
part is easy. What it doesn't know is the type of `myStruct`: the structure pass records members and namespace-scope
variables, not locals (`kSymbolKind`). That is "type on symbol" (§5.6, §11.6) and more, and belongs with editing help, not
with navigation.

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

- **PuckoNew stays on the benchmark list as an outlier:** vendored C drivers in several copies and macro-named types. It showed a
  gap in resolving names that the other trees hide (§11.10).

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

### 10.2 System and library names - shipped as assets (planned 2026-10-25)

Recognise the names a program gets from its platform - `std::vector`, `fopen`, `printf`, `size_t` - so they
get their own colour now, and so on-demand suggestions (type `.` → the members) can be built on the same data
later. Go-to-definition into system headers is **not** a goal. Compilers, versions and standard libraries
differ, so v1 knows nothing about the installed toolchain - only syntax and data the editor ships.

**1. By syntax - no knowledge needed:**
- Anything qualified `std::` / `::std::` is the standard library.
- `#include <...>` is a system/library header, `#include "..."` the project's - the include edges of step 1
  already tell them apart.

**2. Curated name lists - editor assets, one per library:**
- `c_std` (the C standard library, ~500 names), `cpp_std` (the C++ standard library, versioned), later
  `posix`, `sdl`, ... - shipped with the editor like `colors.json`, in `Assets/Resources/Libraries/<name>.yml`; a
  project can add its own in `<root>/.goatedit/libraries/*.yml`.
- A list holds a library's public names, not the library: `c_std` is the complete public surface, `cpp_std` the names
  in common use.
- **When a list applies:** a list naming `headers: [SDL3/SDL.h]` applies only to a file that includes one, as written
  there - the include edges of step 1 say which. A list naming none always applies, as `c_std` and `cpp_std` do.
- **A name in a scope** needs that scope, as with `std`: `fmt::format` with `scope: fmt` in the list, or a `using`.
- An entry: name, kind (function / type / macro / namespace / constant), qualified scope (`std`), the
  standard version it appeared in, and **optionally its members** (`std::string` → `size`, `c_str`,
  `substr`, ...). Members aren't used for colour; they are there so suggestions on `.` / `->` / `::` can
  be built later without changing the format.
- Written by hand from the standards (decided 2026-11-06). A generator stays possible - it would ship and load the
  same way - but libc++'s headers, full of macros and `__internal` names, are a poor source.
- New token classes `library_type`, `library_function`. A theme without them shows `user_type` and `function`, then
  `regular`: with `regular` first, every existing theme would hide the feature. TextMate themes
  already name these `support.type` / `support.class` / `support.function`, so the importer's scope table
  (§11.4) maps them directly.

**Bare names, and `using namespace std;`:** rule 1 only covers what is written `std::`. A bare `vector` needs the list,
but applying it everywhere brings back what §11.6 solved: `count`, `find`, `list`, `map`, `min`, `move`, `string` are
common locals. So:

| Written as | Treated as |
|---|---|
| `std::vector` | Library, always |
| Bare, the file has `using namespace std;` or `using std::vector;` | Library, where a local can't stand (the rule of §11.6) |
| Bare, no `using` in the file | Project or plain, never `std` |

C names are global by nature, so `printf` and `fopen` follow the rule for a bare function: a following `(`. A `using` in a
header reaches the files including it, which v1 doesn't see; those names stay plain.

**In the index:** library names are symbols with **origin = system** - ranked below the project's own in Go
to symbol, coloured apart, never shadowing a name the file or project declares (the file's own meaning wins,
as in §11.3). Lookup is the same hash probe per identifier as 1b.

**Later - suggestions (intellisense-like, on demand):** `x.` needs the type of `x`: members of project types
come from the index (step 2, declared member types); locals need a light scan of the enclosing function;
library types come from the members in the assets. That scan is the larger piece and comes after the
index - the asset format just must not rule it out.

**Later - `codeindex.library_paths`:** folders outside the project (a vendored SDK, `/opt/homebrew/include/SDL3`) fed
through the existing indexer with origin = system. Gives a whole library with no list to maintain, and fits clean C headers
best. After 1d: the hand-made lists come first.

**Experiment, not v1 - scanning real headers:** search paths from `compile_commands.json` (per-file
`-isystem`, `-std`) or the compiler itself (`clang -E -x c++ - -v </dev/null`, `xcrun --show-sdk-path` on
macOS); index only headers reachable through the workspace's includes. Hard part: library headers are
written for the preprocessor (libc++ `_LIBCPP_BEGIN_NAMESPACE_STD`, inline `std::__1`,
`_LIBCPP_HIDE_FROM_ABI`; glibc `__BEGIN_DECLS`, `__THROW`, `__wur`) - a small per-library macro table
(namespace openers, attributes to ignore, `std::__1` → `std`) plus hiding reserved names (`__x`, `_Upper`).
Its output could also *generate* the shipped assets.

### 10.3 Keeping the architecture in view while working with an agent (explored 2026-11-06)

This project started as an editor with a very fast syntax highlighter and has become a workbench. What matters after months
of working with agents is the architectural decisions and being able to dive into the specifics quickly, and the editor
should support that. How it will look isn't known yet; this section records the thinking, not a plan. As in §1, we'll know it
when we see it.

**The problem:** an agent works from what is in its context, and a code base doesn't fit. It reads what it finds and builds
from there, so some decisions come out sloppy without any single line being wrong: the structure pass that re-parsed what the
highlighter had already parsed is the example from this project. A person doesn't hold the whole code base either, but holds
it in layers, can skip what doesn't matter, and remembers why something was decided long ago. The agent has none of that, and
it writes faster than anyone reads. So the thing to watch is the architectural effect of a change, not its lines.

**Questions the editor could answer, all from the index:**

| Question | For whom | What it takes |
|---|---|---|
| What did this change do to the structure? New dependencies between modules, new types, a new base class, who now owns what. | The person reviewing | Edges from the working tree against edges from the base commit. Only changed files need scanning, with `Scan(text)` on what `git show` gives, so no stored index is needed. |
| Does this already exist? What already does this, who holds it, what depends on it? | The agent, before it writes | The queries of §11.10, reachable from outside the UI. `gedit-bench` shows the shape; a command against the running editor would see unsaved work. |
| Does it still follow the rules? | Both | The architecture rules of §10: "nothing in `Core/Graphics` includes `Core/Session`". |
| Why is it like this? | Both | Decisions tied to the structure. The cheap start: which docs and `CLAUDE.md` sections name this symbol, found with the index's own names, with nothing to annotate by hand. |

**The second question is the one to try first:** a helper for the agent, so it can ask before it writes.

**What this asks of step 3, and no more:** the queries stay plain data below the UI, so a view, a command or an agent can ask
them alike, and edges stay names, not ids, so two states of a tree can be compared. The rest waits until the views are in use.

## 11. Steps

| # | Step | Status |
|---|---|---|
| 0 | **Measure.** `TokenizeText` fast path + a release-only benchmark (lines/s) on sqlite3.c and this repo. Decides threading and whether the lexer needs speeding up first. | ✅ §11.1 - plus two tokenizer fixes (~3-4x, highlighter too) |
| 1 | ✅ §11.2 - **C++ structure engine** over strings - the low-hanging fruit only: namespaces, classes/structs, methods/functions, members, `#include`, and the edges visible right in the declaration (`inherits`, `holds` + ownership). Rules as tables in code (§5.4), shape + edge tests (§7). No storage yet - dump symbols/edges for a file and judge on this repo. Nothing from §8.1. | - |
| 1b | **Semantic highlighting from the file itself** (§10.1): new token classes with `regular` fallback, reclassification via the post-line hook, the open file's own structure pass. New theme keys in `colors.json`; no theme-format work. | ✅ §11.3 |
| 1c | **Theme importer** (§10.1, D13): Sublime / VS Code theme → GoatEdit `theme.yml` + `colors.json` via the scope table; `--import-theme`. Independent of the index - can come any time after 1b. | ✅ §11.4 (+ `.theme --import`) |
| 1d | **System / library names** (§10.2, D14): `std::` by syntax, `<...>` includes marked system, hand-made name lists as editor assets (`c_std`, `cpp_std`), bare names by the tiers of §10.2, `library_type` / `library_function` classes. Members in the format for later suggestions. Independent of step 2 (can come before or after). | ✅ §11.8 |
| 2 | **In-memory index + Go to symbol** over the workspace, in the background: indexer threads, resolution (§5.2), query ranking, `SymbolSearchModal`. Kept current by `FolderMonitor` and, for open files, the buffer. Decisions: §11.5. | ✅ §11.6 |
| 2b | **Smart go-to and the navigation stack** (§5.6): go to definition, declaration and the toggle from the name under the cursor, with a list when there are several candidates; back and forward on `Cmd+[` / `Cmd+]`, kept across a restart. Go to implementation and base follow with step 3, references with step 6. | ✅ §11.9 |
| 3 | **Relationship views**: edges into the index, open files included, then hierarchy and composition (trees, §5.5), "derived from / held by" from any symbol. Their edges also give §5.6 go to implementation and base. | ✅ §11.10 |
| 4 | **Modules**: file → module roll-up, dependency view + matrix, cycles; DOT/Mermaid export. | - |
| 5 | **Persistence**: store (D2), skip unchanged files at startup. Only if measuring says startup needs it. | - |
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

**Brace-balance check (follow-up, same branch):** the scanner records braces still open at the end of a file
and stray `}` (`FileStructure::numUnclosedBraces` / `numStrayCloseBraces` + lines); `gedit-bench --check`
lists the unbalanced files, `--braces <file>` prints the lexer's depth per line (diffed against a small
reference counter to find the misread line). First run: 23 unbalanced files over the five code bases.
Causes found and fixed:

- **C++ lexer - escapes in strings/chars:** a word in a string ran to the next `"` and swallowed the
  backslash, so `"\"Summary\":{"` ended at `\"` and its `{` became code. The escapes now end a word in
  the string and char states. Test `cpplang stringescape`. (GoatEdit `TerminalController.cpp`,
  testrunner `reportjson.cpp`, PuckoNew `JSONDecoder.cpp`, ...)
- **C++ lexer - raw strings:** no support at all. Added `R"(...)"` and `R"_(...)_"` (with u8/u/U/L) - the
  lexer has fixed tokens, so other delimiters still aren't handled. The closers are *not* main-state
  tokens: `)"` is ordinary code (`(char *)"text"`) - a first version that listed them broke exactly that
  (caught by the check on testrunner `dynlib_win32.cpp` and PuckoNew `win32/i2cdriver.c`). Test
  `cpplang rawstring`. (vcpu assembler tests)
- **Scanner - continuation over an empty line:** `#define X \` followed by an empty line ended the macro
  in C, but the scanner never saw the empty line (no tokens) and swallowed the next code line as macro
  body. Fixed with a line gap check. (testrunner `responseproxy.cpp`, vcpu `positeigen.h`)

After: GoatEdit, testrunner, vcpu, noc_goa `include`: 0 unbalanced; PuckoNew: only the vendored
`sqlite3.c` (not investigated - likely a preprocessor arm). noc_goa `src`: 10 files, all **ISO-8859
(Latin-1)**, not UTF-8 - see below.

**Found: Latin-1 files lose data in the editor (not fixed - outside step 1).** `UnicodeHelper::
ConvertUTF8ToUTF32String` converts strictly and, on an invalid byte, returns `false` with the output
already sized to the input: the rest is NUL characters. The editor loads line by line
(`TextBuffer::Load` → `AddLineUTF8`), so a line with `ö` becomes the part before it + NULs - and **saving
writes the truncated line back**: `/* g\xf6r om s\xe5 */` was saved as `/* g` (verified with a throwaway
test, removed). Opening and saving a Latin-1 file silently destroys content - and can break code (here
the comment's `*/` is gone). The index needs a lenient decode too (whole-file decode makes the rest of
the file vanish). Options: detect invalid UTF-8 and fall back to Latin-1 (and save back as Latin-1), or
refuse to save / open read-only. Tracked as `docs/open-bugs.md` #15.

**Known limits (not handled, by design for now):** the most vexing parse (`Foo f(1);` is a function
declaration), anonymous members not distinguished from named (`vtxinfo.size` is `GOA_MATERIAL::size`),
locals / local classes / lambdas inside bodies, `#if` arms other than the first, macros that open scopes.

### 11.3 Step 1b - semantic highlighting from the file itself (2026-10-24)

**It runs on the highlighter's terms** - same lines, same regions, no text copy, no second lex:

- **Line-fed scan.** `StructureScanner::Begin / StateAtLineStart / ScanLine / End` read the tokens the
  highlighter left on each `Line` (`Attributes()`: a token runs from one attribute to the next). The
  scanner is streaming, a line at a time; `Scan(text)` (indexer, `gedit-bench`) lexes and feeds the same
  path. A test asserts both give the same symbols.
- **State on the line** (`Line::SetStructure`, `Core/Index/LineStructure.h`): the scanner's state at the
  line's start - only at a line that starts between statements, where it is just the scope stack +
  `#if` arms - plus the names the line declares. Like `stateDepthAtStart`; consecutive lines share one
  immutable state object; not copied with a line.
- **Region scan** (`TextBuffer::ScanStructure`, after the highlighter's `ParseRegion`): back from the edit
  to the closest line with a stored state, forward past the edit until a line starts in the same state as
  before (converged), capped by `regionMaxLines`. An edit in a function body converges right after it.
  A resumed scan doesn't know the symbols of the scopes it is inside - only what the highlighter needs.
- **Counted names** (`SemanticNames`): per (name, class); a rescanned line takes its old declarations out
  and puts its new ones in, a deleted line (`DeleteLineAt`) hands its declarations to the next scan.
  A type wins over its constructors; two other meanings → `regular`.
- **Colour:** the per-line hook applies the names to the lines being parsed. When a name's meaning
  changes, the other lines are recoloured by walking their token lists (no lexing) - at once on a full
  parse, after a 300 ms pause on the parse thread's idle tick for an edit.
- New token classes `user_type`, `function`, `member`, `macro`, `enum_value`, `namespace`; a theme without
  them shows `regular` (`Editor::ConfigureTheme`). Off switch: `languages.cpp.semantic_highlighting`.

**Not handled (by design for now):** locals and parameters aren't symbols (a local named like a member gets
the member colour); namespace-scope variables stay `regular`; names declared in other files came with the
index (§11.6).

### 11.4 Step 1c - theme importer (2026-10-25)

`goatedit --import-theme <file> [--name x] [--force]` converts once and quits (no screen); in the editor
the same is `.theme --import <file> [--name x] [--force]` (`ThemesAPI`, JS global `Themes`):
`<x>.theme.yml` + `<x>.colors.json` next to the user config the editor reads (else where one would be
created) - never the system assets; an existing import is only overwritten with `--force`. The theme file
names the import's source and lists what didn't carry over (YAML comments); the font is the current
theme's. `x` defaults to the theme's name (`Night Owl` → `night-owl`; a `--name` is used as given). Switching
to it stays a `main.theme:` edit (`.cfg --user set main.theme x.theme.yml`) and a restart - the command says so.

- **Plain data in/out** (`Core/Theme/ThemeImporter`): text in, colors.json text + a report out; an
  included parent theme comes through a loader callback. `ThemesAPI::Import` is the file glue, shared by
  the command line (`Editor::ImportTheme` prints its lines) and `.theme` (`theme.js` glues a file name
  with spaces back together - the command line is split on spaces).
- **Readers:** VS Code (`colors`, `tokenColors`, `semanticTokenColors`, `include`; comments and trailing
  commas allowed) and Sublime `.sublime-color-scheme` (`variables`, `globals`, `rules`). `.tmTheme` (XML)
  is refused with a message.
- **The scope table:** each content key lists the scopes its tokens carry in VS Code's C/C++ grammar, best
  first (`known_type` → `storage.type.built-in.primitive.cpp`, `import` → `string.quoted.other.lt-gt.include.cpp`,
  `macro` → `entity.name.function.preprocessor.cpp`, `member` → `variable.other.property.cpp`, ...). A
  selector element matches a scope it is a dot-prefix of; more segments win, then the later rule (TextMate's
  rule). The first scope in the list that any rule matches decides; a key nothing matches copies a related
  key (`block_comment` ← `line_comment`, `user_type` ← `known_type`) and is reported.
- **What maps approximately:** context selectors (`meta.class entity.name.type.class`) apply only where no
  context-free rule matches; `a - b` reads as `a`; `source.cpp x` is just `x` and `source.go x` is dropped;
  rules that only set a font style don't take a colour away; font styles are dropped; semantic token
  entries with modifiers or another language are skipped.
- **Selection and search are overlays** - GoatEdit paints them OVER the text with their alpha, the source
  editors paint them under it. An opaque source colour becomes the least translucent tint (alpha >= 0.35)
  that gives exactly the source colour over the background; a missing one is the foreground at 0.25.
- **Output for tuning:** each literal colour becomes a variable named after the first key that uses it
  (`keyword`, `ui_gutter_foreground`, `ansi1`); Sublime variables and expressions are kept as written
  (validated through the colour script). Colours are written `#rrggbb` / `rgba()` - the colour script
  misreads `#rgb` and can't read `#rrggbbaa` (open-bugs #16), so the importer parses hex itself.
- `globals` gets the same colours as `content`; missing UI/terminal colours fall back to the editor's
  background/foreground and the xterm palette.

Night Owl (the sample in `Assets/Themes/`) imports with one key unmatched (`rule`), 38
context selectors and 29 font styles reported.

### 11.5 Step 2 - decisions taken before starting (2026-11-05)

- **No persistence yet** (D2): re-index at startup; PuckoNew (~560k lines) is the first real test of that.
- **Open files come from the buffer** (D6), closed files from disk.
- **`FolderMonitor` updates are part of step 2** (moved from step 5): otherwise results go stale after a
  `git pull` or an external edit until restart. The macOS backend works; Linux gets live updates when its
  backend does.
- **Cross-file colouring:** names with several meanings in the workspace stay plain. Narrowing by includes
  can come later.
- **Keys and filters:** Cmd+T / Ctrl+T (D7), `Scope::name` only (D4).
- **C/C++ only** (D5): the only structure scanner so far.
- **Memory:** tens of MB per million symbols is accepted; a stats readout checks it on PuckoNew.
- **Order:** step 2 before 1d. Library names then slot in as symbols with origin = system.

### 11.6 Step 2 results (2026-11-05, M-series Mac, release)

`gedit-bench --index <dir>` indexes a tree and times the queries, single-threaded on the finished index. With
`.gitignore` honoured (§11.7) - testrunner drops from 399 files because it was indexing build output:

| Tree | Files | Index time | Symbols | Names | Memory | Worst keypress |
|---|---|---|---|---|---|---|
| GoatEdit `src` | 357 | 149 ms | 14 936 | 10 070 | 2.7 MB | 0.32 ms |
| testrunner | 149 | 10 ms | 2 224 | 1 185 | 0.3 MB | 0.07 ms |
| noc_goa | 178 | 16 ms | 5 822 | 3 842 | 0.9 MB | 0.13 ms |
| vcpu | 167 | 15 ms | 5 356 | 2 836 | 0.7 MB | 0.12 ms |
| PuckoNew | 1 377 | 96 ms | 39 463 | 20 551 | 6.2 MB | 0.83 ms |

So persistence stays out: startup indexing is well under a second. Query time grows with the number of
names, so a short query at 1M symbols extrapolates to ~20 ms - over the §4.5 target, but kernel-sized, and
nothing here has that many. PuckoNew's size cap (D5) skips the vendored SQLite, so its 560k lines aren't all
indexed.

**Where the code went another way than §4.3-4.5:**
- **No trigram table.** A trigram can't help a subsequence (`srch`) or word-start match (`SF`). Each distinct
  name carries a mask of its characters instead; a query walks the names, the mask rejects most, the rest are
  scored.
- **No separate writer thread.** A worker replaces one file under a short write lock. Same effect, less code;
  a query only waits for one file's worth of work.
- **One index across all roots**, not one per root (D8) - per-root was for the cache key, which step 5 can
  still split.
- **Not a project:** the cwd default root, and any folder that is `$HOME` or holds it, are never indexed.
- **Bad UTF-8:** the indexer turns invalid bytes into `?` before decoding, so a Latin-1 comment doesn't cost
  the rest of the file (the editor's own load still has bug #15).

**Keeping current (slice 2):**
- **Open files:** a `Line` keeps its symbols (`LineStructure::symbols`). After a pause in typing (300 ms, as for
  recolouring) the parse thread walks the lines - no text copy, no lexing - and hands the index one
  `FileStructure`. Line numbers come from the walk, so they survive edits above a symbol. Any parse counts as
  a change, because any edit moves the lines below it.
- **The buffer owns an open file:** the indexer leaves it alone until the document closes, so a disk read can't
  put an older version over unsaved work. Closing reads it from disk again.
- **Changes on disk:** `FolderMonitor`'s dirty folders go to the indexer, which compares (mtime, size) and reads
  only what differs; lost events compare every root. Off unless `foldermonitor.enabled` is set; the Linux
  backend is still pending.
- **Markers:** one file can't tell `A::f` of a class from one of a namespace, the index can - a hit whose scope
  names a known class is a method. For functions and methods upper case is the body (`F`, `M`), lower the
  declaration (`f`, `m`).
- **Known, inherited:** the parse thread walks `lines` without a lock against the UI thread, as `ScanStructure`
  and `RecolourSemanticNames` already do. The walk happens at a pause, but typing can still meet it.

**Cross-file colouring (slice 3, 2026-11-06):** the second source of §10.1. `CodeIndex::GetWorkspaceNames()` gives an
immutable name -> class snapshot (`WorkspaceNames`). A parse thread takes the pointer and probes it per identifier
without a lock.
- **Order:** the syntax first, then the file's own meaning - also when that meaning is "ambiguous", a name the file
  disagrees with itself about doesn't fall through to the workspace - then the workspace.
- **The syntax first:** after `.` or `->` the code says what a name is: called, a method; otherwise a member. It needs
  no index and can't be fooled by a name's other meanings, so `a.begin()` and `b.size()` look alike whatever `a` and `b`
  are. Before `<` it can't tell (`a.get<int>()`, `a.count < 3`) and the names decide. Excluded: `Args... rest` and
  a trailing return type (`[]() -> Foo`, a `->` spaced off a `)` or a qualifier). `f()->x` written without the space
  stays an access, so `auto f()->Foo` shows a member.
- **One meaning or none:** a constructor doesn't count as a second meaning; a namespace-scope variable does, although
  it has no colour. GoatEdit: 273 of 10 103 names have several meanings; PuckoNew 1 044 of 20 671.
- **Context, because the index has no locals:** by name alone, a member `path` anywhere colours every local `path`
  everywhere. So for a bare name a function must be followed by `(`, a member stays plain, and a type, macro, enum value
  or namespace starting in lower case (`other`, `current`, `ptr` - hundreds of those on real trees) must stand where a
  local can't: after `::`, or before a name, `(`, `::`, `<`, `*`, `&`. `kFoo` is exempt. A miss stays plain; that is
  the cheap way to be wrong.
- **Reaching the buffers:** `TextBuffer::SetWorkspaceNamesSource`, asked at each full parse and on the idle tick. Any pause
  in typing rebuilds the table (0.8 ms GoatEdit, 3.9 ms PuckoNew), and it almost never differs: the same pointer comes
  back, so nothing recolours. A real change recolours by walking the token lists. While the indexer works the source
  answers nothing: a meaning isn't settled until every file is in.
- **Languages:** only one with a structure scanner takes the names, so `ViewBase` in a markdown file stays text.
- **Noted for later - types on symbols:** the scanner parses a member's full type (`TypeRef`), then `ResolveHeld` keeps
  only the held type for the `kHolds` edge, and edges never reach the index. Keeping the type on `CodeSymbol` (members,
  and return types on methods) is small; resolving `a.b.c` from it gives bare members, Go to definition and `.`
  suggestions a base. No need to parse the standard library for it: `std::vector<Document>` shows it is a template, so
  `std::vector` can go in as a type of unknown origin, outside the project, and the dependency on `Document` still
  resolves. Locals and `auto` stay out of reach - that way lies a compiler front end.
- **Limits:** a member used bare in a `.cpp` (`viewTopLine` inside `Foo::Bar`) stays plain unless that file declares it;
  it needs the file's class or its includes. With `threaded_syntaxparser: no` there is no idle tick, so colours follow
  the next full parse.
- **A very large file:** `gedit-bench --recolour <file> <root>`, SQLite's 8 MB amalgamation (238 189 lines) open with
  PuckoNew's 19 630 names: full parse about 420 ms (the syntax check adds some 25 ms, the names nothing measurable); a
  recolour 19 ms; the walk after each pause in typing 2.6 ms.

### 11.7 Exclusions - `ExclusionRules` (2026-11-05)

`use_git_ignore: yes` had never done anything: `.gitignore` wasn't parsed anywhere, the one branch for it was an
inverted `TODO` in a function nobody called, and `workspace.exclude` was read separately by each consumer. Now a
`ProjectRoot` owns one `ExclusionRules`, shared with the folder scanner, the monitor and the indexer.

| | `workspace.exclude` | `.gitignore` |
|---|---|---|
| Meaning | Never touch this (`.git`) | Not source, but it exists |
| Tree | Not shown | Dimmed; `ToggleIgnored` (Alt+H in the workspace view) hides it |
| Scanned | Never | When expanded |
| Index, monitor | Skipped | Skipped |

**It can't be a merge into the exclude list.** `FsFilter` matches single names, so `/ext/` would also hide `src/ext`
(this repo's rule), and `src/ext/ELFIO/` can't be expressed at all. Rules are git's: anchoring, `!`, `**`, last line
wins, deeper files beat shallower ones, nothing below an ignored folder comes back, and a folder opened inside a
repository follows the repository's files.

**Checked against git itself:** `gedit-bench --ignored` held against `git check-ignore --no-index` on GoatEdit,
testrunner, noc_goa, vcpu and PuckoNew - 20 782 paths, 1 difference, and that one explained: a folder in testrunner
holding its own `.gitignore` of `*` (we ignore what's in it, git also the folder - the same files are skipped).

**Limits:** `.git/info/exclude` and a global file named by `core.excludesFile` aren't read; git's default global file
(`~/.config/git/ignore`) is. A changed `.gitignore` reaches the tree and
the monitor at the next flush, but files it newly ignores or un-ignores stay as they are in the index until a restart
or a lost-events rescan. An expanded ignored folder isn't live - the monitor drops its events. The Linux monitor still
reads the old `foldermonitor.exclude` key (B7 in `foldermonitor_fixes.md`), so it has no exclusions until that backend
is done.

**Default list:** `bld` and `cmake-build-*` left `workspace.exclude`, or they could never be browsed - the hard list
wins. A build folder in a project without a `.gitignore` entry for it now counts as source.

**Colour:** `tree_dimmed` in `colors.json`, from VS Code's `gitDecoration.ignoredResourceForeground` on import. A theme
without the key gets the midpoint of foreground and background.

### 11.8 Step 1d results (2026-11-06, M-series Mac, release)

A name's colour now comes from four places, in this order: the syntax (`.`/`->`, then `std::`), the file, the workspace, the
libraries.
`LibraryNames` (`src/Core/Index/`) is an immutable table built at startup from `c_std.yml` and `cpp_std.yml`.

- **Lists:** 1,413 entries, 1,382 names. Checked against the toolchain with `clang -std=c++2b`: a C macro must be defined,
  every other name must compile, and `using std::x;` works for any kind of C++ name. Four entries don't resolve and stay,
  all correct: `std::chrono::utc_clock` and `std::stacktrace` (libc++ lags), `std::random_shuffle` (removed in C++17, still
  in old code), `setjmp` (a function on macOS, a macro in the standard).
- **Format:** the fields of §10.2, grouped so a list stays writable by hand: each group gives `kind`, `scope` and `since`
  for its `names`, and a name may hold its members. `NULL` is quoted, or YAML reads it as null.
- **Classes:** `function` and `type` get the new `library_function` and `library_type`. `macro` and `namespace` use the
  existing classes, and `constant` (`std::cout`, `std::string::npos`) uses `enum_value`.
- **`std::x` not in a list** goes by what follows: `(` is a function, `::` a namespace, anything else a type. A name in two
  scopes takes its first kind, so `std::filesystem::is_empty(p)` shows as the type `std::is_empty`.
- **`using`** is a pair of edge kinds from the scanner, held as counted declarations on the line like any other, so typing
  or deleting one recolours the file with no new mechanism.
- **Project names win, ambiguous ones too:** `WorkspaceNames` used to drop a name with no single meaning, which made it look
  unknown to the libraries. It now keeps it, plain. The cost: a project with a member `time` loses the colour on the C
  `time()` too. Plain is cheap.
- **`::x`** can't be a local, so `::tolower` is library even without a call. `Foo::remove` never is.

| On `sqlite3.c` (238,189 lines) | Before | With the libraries |
|---|---|---|
| Full parse | ~420 ms | ~420 ms |
| Recolour | 19 ms | 20 ms |
| Walk after a pause | 2.6 ms | 2.7 ms |
| Loading the lists at startup | - | 2.1 ms |

**Project lists and header activation (second slice):** `LibraryNames::Load` adds `<root>/.goatedit/libraries/*.yml` to the
shipped lists, in name order; a file that won't load costs only itself and says so in the log. `#include` travels as a
counted declaration like `using`, so editing one recolours the file. Each name keeps one origin per list and scope, and
the first that applies decides, so two libraries can share a name. Only the root found at startup is read, so a changed list
needs a restart. Nothing in a list runs, so a project list needs no entry in `ProjectConfigPolicy`.

**Left out:**
- **Library names in Go to symbol:** §10.2 ranks them below the project's, but they have no file or line to go to, and
  go-to-definition into system headers is a non-goal. Worth revisiting with suggestions.
- **Standard version:** `since` is in the lists and not read.

**Known limits, plain rather than wrong:** `std::vector<int>::iterator` (a template argument hides the `std`), a namespace
alias (`namespace fs = std::filesystem; fs::path`), a `using` inside a function or reaching a file through a header,
lower-case `stderr` or `errno` outside a type position, a list's name in a file that gets the header through another of the
project's headers: only direct includes count, and the index holds no include edges.

### 11.9 Step 2b results (2026-11-06)

`GoToResolver` narrows the name under the cursor to its candidates, `NavigationStack` keeps the jumps; both are plain code
with no document or UI in them. `ActionHelper` joins them to the documents, and every jump goes through `ActionHelper::JumpTo`.

- **Resolving:** in this order, the qualifier as written, else `.`/`->` syntax, else the scopes around the cursor; then
  constructors give way to their class when the class is a candidate, and definition or declaration is picked by the mode.
  A step that would leave nothing is skipped. On a name's own declaration or definition the toggle goes by the qualified name.
- **A list** is Go to symbol's modal over a fixed set of hits; typing filters it.
- **The status line** had no way to say anything: `Editor::ShowStatusMessage` holds a message for four seconds, for "'x' is
  from a library", "'x' isn't in the index" and "'x' has no other place".
- **A bug found on the way:** the index holds canonical paths and the workspace what it was opened with, so for a project
  reached through a symlink, Go to symbol opened a second document for a file already open, without its unsaved edits.
  `Editor::LoadDocument` now compares canonical paths, which covers every caller.
- **A crash found on the way, older than this step:** in quick mode, `]` or `[` to another view with `leave_automatically` on
  left quick mode twice, and the second leave took the root view off the run loop's handlers. The next key, any key, read an
  empty stack. It showed up as a crash on `Cmd+[`. `Editor::LeaveCommandMode` now does nothing outside quick mode.

**Left out or limited:**
- **Go to line** isn't on the stack: `kActionGotoLine` is mapped to a name but nothing implements it yet.
- **Overloads** in one file share a name, so after an edit the closest to the old line wins. A shift of half the distance
  between them picks the wrong one.
- **The stack reads the index,** so a jump right after an edit, before the pause that feeds the index, anchors on slightly
  old lines. Only the anchor is affected, and the next pause corrects it.
- **Linux, not fixed:** the terminal keymap inherits the default one, so in the terminal `Ctrl+B`, `Ctrl+[` and `Ctrl+]` go to
  the editor, as `Ctrl+T` and `Ctrl+F` already do, and that includes full-screen programs: `ForwardActionToShell` passes on a
  fixed set of actions. `Ctrl+[` is `Esc` to a terminal, so this one will be felt in vim. macOS isn't affected.

### 11.10 Step 3 - design (2026-11-06)

3a, 3b and 3c are done, see their results below. The scanner has always found the edges, 1 156 `holds` and 94 `inherits` in GoatEdit's `src`, and they stop there.

**Two gaps:**
- **No file's edges reach the index.** `CodeIndex::ReplaceFile` reads `structure.symbols` only. The indexer's files are as blind
  as the open ones.
- **Open files have none to give.** `TextBuffer::ScanStructure` reads each edge for its declaration and drops it, so
  `CollectStructure` has only symbols to hand over. Without this, a tree is wrong for the file being edited, which is the one
  that matters.

**Open files - edges ride on the lines, as symbols do.** `LineStructure` gets `edges` next to `symbols`. `ScanStructure` fills
them in the same loop, under the same check for a line deleted meanwhile, and `CollectStructure` walks them with the line number
from the walk. Nothing is lexed or copied beyond what the region scan already does.

**The trap: `idxFrom` doesn't survive a region scan.** It indexes the symbols of one scan, and `Begin` sets every resumed scope's
`idxSymbol` to -1, because those symbols belong to an earlier scan, and `AddHoldsEdge` leaves out an edge with no owner. So an edit
inside a class loses the `holds` edges of every line it scans. Copying edges to the lines as they are would pass on a fresh parse
and go wrong at the first keystroke. **`CodeEdge` carries `fromName`, the qualified name, instead of `idxFrom`:** `ScopePrefix()` still has the
names after a resume, because they are part of the resumable state, and §10.3 wants names anyway. An empty `fromName` is the
file itself.

**The store:** `ReplaceFile` takes all the edges, per file, removed with the file as symbols are.
- Two lookups: by the last component of `toName`, so `ILogger` and `gnilk::ILogger` meet, and by `fromName`. They are separate
  from the `names` table: many targets are never symbols (`gnilk::ILogger`, `_u32`), and a search shouldn't walk them.
- **Resolved at query time,** not stored. §5.2 re-resolves when a target appears or vanishes; asking at query time makes that free.
  Trees are asked per row expanded, not per keystroke. An ambiguous name shows all its candidates, as §5.2 says.
- Plain queries on `CodeIndex`: bases of, derived from, holds, held by. One level each. The view expands lazily, so a cycle in
  composition is one more row, marked, and not a loop.

**Order, each one usable alone:**

| Slice | What | Judged by |
|---|---|---|
| 3a | Edges in the index, open files included. `gedit-bench --edges <name> <root>` asks the queries from the outside. | The answers on GoatEdit and PuckoNew; index time and memory against §11.6 |
| 3b | The views: hierarchy and composition, each row a `JumpTo`, so it lands on the navigation stack. | Use |
| 3c | Go to implementation and go to base (§5.6): the transitive derived types with a symbol of that name. A name is a name, so overloads aren't told apart. | Use |

**Known limits:**
- **`typedef struct { Foo *x; } Bar;`** has no name until the `;`, so its `holds` edges wait for `OnTypedefName`. A region scan
  that stops before the `;` can't name the owner, and those edges are left out until the next full parse. A test holds that they
  never land on the wrong type.
- **`struct { Foo *x; } global;`** at namespace scope: nothing named holds it, so there is no edge. Inside a class the class holds it.
- **`class A::B { ... }`:** `inherits` goes from `A::B` and `holds` from `B`, as the members' own qualified names already do.
- **Inherited members:** `holds` is per class; what a type holds through its bases is the view's to show.
- **Templates:** `template<class T> class A : public T` gives an edge to `T`, as it does today. It resolves to nothing and shows so.

**3a results (2026-11-06, M-series Mac, release).** `gedit-bench --index <dir>`, against §11.6:

| Tree | Files | Index time | Edges | Memory | Relations | Resolved | Outside | Ambiguous | All resolved in |
|---|---|---|---|---|---|---|---|---|---|
| GoatEdit `src` | 365 | ~150 ms | 3 205 | 3.3 MB | 1 256 | 1 185 | 71 | 0 | 0.8 ms |
| testrunner | 149 | 12 ms | 928 | 0.5 MB | 157 | 120 | 37 | 0 | 0.1 ms |
| noc_goa | 178 | 20 ms | 1 067 | 1.1 MB | 456 | 441 | 15 | 0 | 0.2 ms |
| vcpu | 169 | 24 ms | 1 461 | 1.0 MB | 577 | 463 | 93 | 21 | 0.4 ms |
| PuckoNew | 1 385 | ~125 ms | 10 471 | 8.1 MB | 4 057 | 3 784 | 262 | 11 | 2.3 ms |

A relation is an `inherits` or `holds` edge; the rest are includes and `using`. Index time is up from 96 ms on PuckoNew and memory from
6.2 MB, for a few more files and all its edges. Startup stays far from needing a stored index: resolving the whole of PuckoNew takes
2.2 ms, so a tree row costs nothing noticeable, and D2 stays where it is.

- **Checked against grep:** what derives from `ViewBase` is the same 12 types.
- **PuckoNew was the outlier, and worth keeping as one:** 1 823 relations outside the index at first, 45%. MBED-TLS and lwIP weren't
  the cause. 1 574 came from TI's SimpleLink driver, vendored twice, where `#define _u8 unsigned char` names the types. A macro
  now counts when no type has the name, and the count fell to 262: `DWORD`, `mbedtls_*`, template parameters. `--index` prints what
  falls outside by name and folder, so the next outlier explains itself.
- **For 3b:** a macro type says nothing about the architecture, so the composition view should put such rows last or fold them.

**3b results (2026-11-06).** `Cmd+Shift+T`, `Ctrl` on Linux, opens the type under the cursor, or the type the cursor is in.
`RelationshipTree` builds the rows as plain data and `RelationshipModal` shows them.
- **One level at a time:** the tree's lazy fetch asks the index when a row opens. The first type opens on arrival. A name with several
  types shows them all as roots, and an ambiguous target opens into its candidates.
- **Rows:** ordered by owns, shares, borrows, with macro types and names outside the index last and dimmed. The same relation on several
  lines is one row, `borrows ViewBase x3`, which goes to the first. A type already open further up its branch is marked `(above)`
  and doesn't open again.
- **Return** goes through `ActionHelper::JumpTo`, so `Cmd+[` comes back. It lands where the relation is written: the member, or the
  derived class's line.
- **The code itself:** opening a row starts with the lines it stands for, one per place, trimmed, each one a jump:
  `Workspace.h:764  Document::Ref activeDocument = nullptr;`. So `x2` opens into its two members, and you read what a relation is
  before going anywhere. An open file gives its current text, unsaved edits included; a closed one is read from disk. Later: a small
  pop-up with the lines around it, once there is such a thing.
- **In colour:** a code row is drawn with `LineRender`, through a per-row hook on `TreeView`. An open file's line already has its
  colours, semantic ones included. A closed file's is tokenized on its own and then coloured by `WorkspaceNames`, so the middle of a
  block comment reads as code, and a name only that file declares stays plain. The selected row stays one inverted bar.
- **A key that couldn't arrive:** both keyboard drivers dropped every `Shift` + letter key-down, leaving it to text input, but with `Cmd`
  or `Ctrl` held no text comes. `Keyboard::IsShiftedText` now tells the two apart. This is the first binding with two modifiers on a
  letter, and the drivers themselves have no test; only the rule does.
- **Modals on SDL3:** the right border was drawn just inside the window, where the contents painted over it, and every caption was
  blank: nothing set the window context's colours. Both found through this view. SDL2's caption is fixed too; its borders work differently and aren't checked.
- **`TreeSelectionModal`** had no users and laid its tree out over the whole screen. It now fills the modal, as `ListSelectionModal` does.
- **A crash found by a mutation test:** `IsNameOf` underflowed when a type is one character longer than the name (`ab::Item`, `b::Item`).
  Adding a test for a surviving mutation showed it, and any edge written that way would have thrown.

**3c results (2026-11-06).** `Cmd+Shift+B` goes to implementation and `Cmd+Shift+U` to base, `Ctrl` on Linux. `Cmd+Shift` stays the
default for this kind of action. `U` is a stand-in; the key may change.
- **From a type:** implementation lists the derived types at any depth, and base goes to the direct bases.
- **From a method:** implementation lists the bodies below it, and base goes to the nearest base that has the method, where it is
  declared, which is all an interface has. A class in between without the method is passed over.
- **Anywhere else** in a method or class, either key goes from the one the cursor is in. So base works from inside a method body.
- **One place jumps, several open the list,** as for go to definition, and every jump lands on the navigation stack.
- **A bug from 2b, found here:** on a class's own line its constructors were closer than the class, so the class was lost. Go to
  base looked for a method of that name, and go to definition went to the constructor. Constructors now give way before the scopes do.
- **A name is a name (§5.2):** overloads aren't told apart, and a method counts as an override by its name alone, `override` or not.

**Relation rows** show the type name in the user-type colour. A row that doesn't fit, and one outside the index, stays plain.

**Decided (2026-11-06):** a modal first for 3b. `TreeSelectionModal` exists and Go to symbol has the same rhythm: ask, look, jump.
A view that stays open and follows the cursor may be the better end state, and using the modal will say what it should show.

**Open:**
- **The look of the UI.** The toolkit's modals and trees are plain. These views are where the structure gets read, so what they
  look like may need its own work.

## 12. Open decisions

| # | Question | Leaning |
|---|---|---|
| D1 | Plain text search-everywhere: before, after or alongside the symbol index? | After step 2 - symbol search first, as the one you'd prefer. |
| D2 | Persistence: SQLite (FTS5) amalgamation, or a flat versioned binary (`BinBuffer`, like the terminal scrollback) loaded into memory? | Not in step 2 (decided 2026-11-05): re-index at startup, measure on PuckoNew. Format decided at step 5. Name search runs in memory either way. The relationship focus tilts towards SQLite: transitive questions (all derived types, impact, module reach) are recursive CTEs, and ad-hoc structure queries / saved rules (§10) are SQL instead of hand-written traversals. |
| D3 | Index identifier occurrences (refs) from the start? | Collect in step 1 (cheap, same pass), store from step 4. |
| D4 | Query syntax for kind/scope filters (`c:`, `f:`, `Scope::name`)? | **Decided 2026-11-05:** `::` scoping in step 2; kind prefixes only if ranking proves not to be enough. |
| D5 | Limits: max file size, max files, which extensions (by language map only?) | Index only files with a structural scanner (C/C++ in v1); cap ~4 MB per file. |
| D6 | Open, unsaved buffers: index the buffer content (from the editor's parse) or only on save? | **Buffer** (revised 2026-11-05, was: on save). Go to symbol sees what was just typed; no second lex, riding 1b's per-`Line` declarations. |
| D7 | Key for Go to symbol? | **Cmd+T (macOS) / Ctrl+T (Linux)** (2026-11-05), subject to a keymap conflict check. |
| D8 | Several workspace roots: one index per root (cache key per root) or one per workspace? | Per root - matches the session cache key; a query fans out over the open roots. |
| D9 | Structure rules (§5.4): YAML in the config dir (like keymaps/themes, user/project overridable) or tables in the language classes? | Tables in code first (step 1); config once the shape has settled (step 7). |
| D10 | Module = directory, or declared? | Directory by default; declared modules / layers in `.goatedit/` later (§5.3). |
| D11 | Graph pictures: export only (DOT/Mermaid), or a pixel graph view in the SDL backend? | Export first; a drawn view only if the trees/matrix aren't enough. |
| D12 | A full parser (e.g. the hand-written AST parser in `vcpu`, which drives the assembler and a scripting language) as a second structure producer? | Overkill for C/C++ (shallow pass instead); a good fit for languages we control (the vcpu assembler / script). The structure output (symbols + edges) is the interface, so a full parser can plug in next to the shallow pass later. tree-sitter is the third-party equivalent if the shallow pass proves too weak. |
| D13 | Themes: read Sublime `rules` (scope → colour) in addition to GoatEdit's keys? | **No - import instead** (revised 2026-10-24). One runtime format (GoatEdit's own, already diverged from Sublime's); Sublime and VS Code themes are converted once by an importer (step 1c) into an editable GoatEdit theme. Both use TextMate scopes, so one scope table serves both (§10.1). |
| D14 | System / library names: ask the installed toolchain (header scan) or ship curated data? | **Ship data first** (2026-10-25): syntax (`std::`, `<...>`) + curated per-library assets; same on every machine, no compiler knowledge. A header scan stays an experiment - and may generate the assets. Members are in the format from the start, for on-demand suggestions later (§10.2). |
