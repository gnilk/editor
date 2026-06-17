# Open bugs / known-wrong code (cross-session tracker)

Durable list of known defects we've chosen NOT to fix in-place yet, with enough context to pick each
up cold. Pointer to this file lives in CLAUDE.md ("Remaining / deferred") so it surfaces each session.

---

## 1. `Line::AttributeAt(pos)` returns the WRONG span for any position in the last token span

**Where:** `src/Core/Line.cpp`, `Line::LineAttribIterator Line::AttributeAt(int pos)`.

**What's wrong:** the lookup loops over adjacent pairs `(i, i+1)` and returns span `i` when
`attribs[i].idxOrigString <= pos < attribs[i+1].idxOrigString`. For a `pos` at/after the LAST span's
start (`pos >= attribs[last].idxOrigString`) no pair matches, so it falls through to
`return attribs.begin()` — the FIRST span (almost always `kRegular`). So the token class of anything in
the last span of a line is misreported as code. A **trailing comment is always the last span**
(`foo(); // note`), so its class reads as `kRegular`.

**Discovered:** 2026-06-12, while making reformat token-aware (C.10). A `{` in a trailing `// ... {`
read as a real block opener.

**Why it's still open (don't naively "fix" it):** `Document::OnActionWordRight` (word-jump, right)
*depends on the buggy fallback*. With the cursor in the last token, `AttributeAt` returns `begin()`
(idxOrigString 0); the `attrib->idxOrigString < cursor.position.x` test is then true and routes
word-jump into its "jump to end of line" branch. A corrected `AttributeAt` (returning the real last
span) would instead fall through to the `else` branch and do `attrib++` → dereference `end()` (UB) when
the cursor sits exactly at the last token's start. So the method cannot be fixed in isolation.

**Current workaround (what "works" today):** the two reformat/indent token-class lookups do NOT call
`AttributeAt`. Each does a correct ascending scan ("the span whose start is the greatest
`idxOrigString <= x`"):
- `TokenClassAtChar` in `src/Core/Document.cpp`
- `TokenClassAtChar` in `src/Core/Language/IndentEngine.cpp`
(near-duplicate of each other — the duplication is the cost of not fixing the shared method.)

**Proper fix (its own small branch):**
1. Fix `AttributeAt`: when `pos >= attribs.back().idxOrigString`, return `attribs.end() - 1`.
2. Rewrite `Document::OnActionWordRight` so it no longer relies on the `begin()` fallback (and guard the
   `attrib++` against `end()`); re-verify word-jump-right behavior (esp. cursor at the last token's start
   and at end-of-line).
3. Collapse both `TokenClassAtChar` helpers back onto `AttributeAt`.
4. Add a `Line::AttributeAt` unit test covering: pos in first span, middle span, last span, and past EOL.

**Other callers to re-check when fixing:** `Document::OnActionWordLeft` (uses `AttributeAt` too),
`EditController.cpp` `TokenClassAt` (already guards `== attribs.end()`, which `AttributeAt` never returns
today — revisit once it can).

---

## 2. `Document::SetCursorPosition` writes an ABSOLUTE line index into the screen-relative `cursor.position.y`

**Where:** `src/Core/Document.cpp`, `Document::SetCursorPosition(idxLine, idxChar)` — `GetCursor().position.y = idxLine;`.

**What's wrong:** `cursor.position.y` is the SCREEN row the caret is drawn at — `EditorView::SetWindowCursor`
hands `position.y` straight to the native caret with no `viewTopLine` subtraction, and the vertical-nav model
maintains it as `position.y = idxActiveLine - viewTopLine` (see `VerticalNavigationViewModel.cpp`).
`SetCursorPosition` instead assigns the ABSOLUTE `idxLine`. It then calls `RefocusViewArea()`, which adjusts
`viewTopLine`/`viewBottomLine` but does NOT recompute `position.y`. So whenever the target ends up on a
scrolled view (`viewTopLine != 0`) the caret is drawn at the wrong row / off-screen.

**Symptom path:** `JumpToSearchHit` → `SetCursorPosition` — jumping to a search hit deep in the file scrolls
the view (so `viewTopLine` becomes large) yet leaves `position.y` at the absolute hit line, so the caret is
mis-placed. Any other "go to line N" caller is affected the same way.

**Discovered:** 2026-06-12, while fixing the block-surround / inline-wrap "cursor gone" bugs (RF.2f), which
were the same defect class in the reformat paths. Those were fixed locally (write screen-relative y +
`RefocusViewArea`); `SetCursorPosition` itself was left alone as it's a wider-blast-radius shared method.

**Proper fix:** after `RefocusViewArea()`, set `position.y = idxLine - viewTopLine` (clamped >= 0). Re-verify
every caller: `JumpToSearchHit` (search nav), plus any "goto line"/jump callers. Add a unit test:
`SetCursorPosition` to a line below the fold on a short, scrolled view → assert
`position.y == idxActiveLine - viewTopLine` and the line is inside `[viewTopLine, viewBottomLine]`.

**Reference pattern (already fixed this way):** `Document::SurroundLineRangeWithBlock` and
`EditController::TryWrapSelection` both now write `position.y = absoluteLine - viewTopLine` (+ refocus where
the line count changed) — copy that.

---

## 3. Syntax highlighter mis-tags an identifier that directly abuts `{` (no separating whitespace)

**Where:** the tokenizer — `src/Core/Language/LangLineTokenizer.cpp` and/or the CPP config in
`CPPLanguage.cpp` (token boundary handling when an identifier abuts an opener). NOT yet root-caused.

**Symptom (observed, not yet traced):** with the caret at `{|foo()` — i.e. `{` immediately followed by an
identifier with NO space between — the highlighter tags `foo` with a non-identifier class (looks like an
operator: it renders the SAME color as the brace). Typing a single space after the `{` (then it
re-tokenizes) fixes the coloring.

**Repro:**
```cpp
static void func() {foo();
}
```
Place the caret between `{` and `foo` and observe `foo`'s color; insert a space → correct.

**Hypothesis (unverified):** the tokenizer doesn't break a token at the `{`/identifier boundary when they
abut, so `{foo` (or `foo` right after `{`) is matched against the operator/brace rule instead of starting a
fresh identifier token. The space forces a token boundary.

**Discovered:** 2026-06-12 (feel-check). Deferred to a later bug sweep — do NOT fix piecemeal.

**When fixing:** add a `test_cpplang` case asserting the token class of an identifier immediately following
an opener (`{foo`, and likely `(foo`, `[foo`) is `kRegular`/identifier, not operator/brace. Check the
tokenizer's longest-match / boundary logic at operator↔identifier transitions.


## 4. `Workspace::ReadFolderToNode` recursively scans ALL subdirectories — hangs / crashes on build dirs

**Where:** `src/Core/Workspace.cpp`, `Workspace::ReadFolderToNode` (L391) and `Workspace::Node` (header
`src/Core/Workspace.h` L542).

> **Depends on / mostly superseded by the FolderScanner extraction** —
> [`folder-scanner.md`](folder-scanner.md). The fix below (scan-time exclude + depth bound) is intended
> to land *as part of* that extraction (FS-2/FS-4), not as a bolt-on in `Workspace`. The proposed
> `Node::isRead` is premature here — it belongs to the deferred lazy-expansion item (FS-6).
> **When the scanner lands, re-read this entry and close it or trim it to whatever residual the scanner
> didn't cover** (expected: none of the original symptom). The analysis below is retained as the
> cold-start framing for that work.

**What's wrong:** the recursive scan has no depth limit and no per-directory skip. Opening the project
root (`.`) with a `cmake-build-debug/` (or `_deps/`) subtree present causes the scan to descend into
thousands of generated build artefacts, FetchContent downloads, and compiler cache files. On startup
this makes the editor hang or crash before the first frame renders — there is no point caching the
complete build directory into the node tree.

**Discovered:** 2026-06-17 — after CMake's `FetchContent` started writing deps under `_deps/` inside
the project tree (`cmake-build-debug/_deps/`). The build directory is now a multi-thousand-file
subtree that the startup scan tries to ingest in full.

**Workaround (today):** open a specific subdirectory instead of `.` to avoid descending into build
dirs. Not suitable as a permanent state.

### How the model actually works (read before "fixing" with depth/flags)

Two phases, both **eager** — the cost is paid up front, before the first frame (matching the symptom):

1. **Scan** — `OpenFolder` → `ReadFolderToNode` recurses the *entire* subtree into the `Node` tree
   unconditionally. `ApplyFsEntry` (`Workspace.cpp:410`) is the single fs→tree mutator and is documented
   as the shared seam for both the scan and the (future) folder monitor.
2. **View build** — `WorkspaceView::PopulateTree` → `FillTreeView` walks the *whole* already-built
   `Node` tree and materialises a `TreeView` node for every entry. The `hide_dot_files` `"."` filter
   (`excludePrefixList`) is applied **here, at view-build time** — dotfiles are still scanned into the
   `Node` tree, just not displayed.

**Expansion in the WorkspaceView is purely visual.** `isExpanded` on a `TreeNodeRef` toggles visibility
of children that *already exist*; there is **no callback from expand back into the Workspace to scan on
demand**. So lazy expansion is NOT a property of the current model.

### Assessment of the originally-proposed guards

- **`maxDepth` — necessary but not sufficient.** The build-dir problem is *breadth*, not depth:
  `cmake-build-debug/_deps/<dep>-src/...` and `CMakeFiles/` hold thousands of files well within 8
  levels, so a depth cap still ingests most of a build tree. Its real value is as a hard safety bound
  against **symlink-cycle infinite recursion** — `ReadFolderToNode` recurses on `fs::is_directory(entry)`,
  which *follows symlinks*, so a cyclic link gives unbounded recursion → stack overflow (the "crash").
  Keep it for that, not as the fix.
- **`isRead` — premature; doesn't fit the current model.** As originally specified (set true on entry,
  skip if true) it only prevents redundant *full* re-scans. The "hook for lazy expansion later" is
  overstated: lazy expansion needs a *shallow* (one-level) scan mode in `ReadFolderToNode` (today it
  always recurses fully) **and** an on-expand callback from `WorkspaceView` into `Workspace` (today
  expand never calls back). `isRead` is a fine *seed* for that future but is inert until both exist —
  do NOT add it now as a no-op.

### Proper fix: exclude at scan time (fits the model; half-built already)

The offenders are build/generated directories. Exclude them **at scan time**, in/around `ApplyFsEntry`
— the infrastructure already exists:

- `config.yml` already lists the exact offenders under `foldermonitor.exclude:`
  (`.git, .idea, bld, cmake-build-debug, cmake-build-release`) + `use_git_ignore`.
- `ApplyFsEntry` is THE single fs→tree mutator shared by scan and the future monitor — an exclusion
  check there fixes both at once (the established "single chokepoint" pattern).
- `ReadFolderToNode` already does `if (node == nullptr) continue;` *before* the `is_directory`
  recursion check. So if `ApplyFsEntry` returns `nullptr` for an excluded dir, both ingestion AND
  descent are skipped for free — no new branch in the recursion.

**Wrinkle — the FolderMonitor is currently disabled and being reworked.** The scan must NOT depend on
that subsystem. The exclude *list* lives under the `foldermonitor` config section and is read via
`FolderMonitor::GetExclusionPaths()` (`FolderMonitor.cpp:40`). Share the **data, not the monitor**:
lift the exclude list to a scan-owned key (e.g. `workspaceview.exclude` / `workspace.exclude`) or read
the raw config sequence directly from the scan; the monitor reads the same key later.

**Scan-time vs view-time exclusion (deliberate choice):** scan-time exclusion means excluded dirs are
*never in memory* (correct for build dirs — never wanted). View-time exclusion (today's dotfile filter)
keeps them in memory but hidden, so a future "show hidden files" toggle works by view-rebuild without a
rescan. Recommendation: exclude build/heavy dirs at scan time; keep the dotfile hide at view time.

**When fixing (this bug):**
- Add the exclusion check in `ApplyFsEntry` (return `nullptr` for an excluded dir → ingestion + descent
  both skipped). Read the exclude list from a scan-owned config key, not from the disabled monitor.
- Add `maxDepth` (signature change on `ReadFolderToNode`; only caller is `OpenFolder` at
  `Workspace.cpp:379`) purely as a symlink-cycle / stack safety bound; default 8–10. Optionally also
  skip symlinked dirs outright. Consider a config value (`workspace.maxScanDepth`) for deep trees.
- `test_workspace`: synthetic temp tree containing a `cmake-build-debug/`-like subtree → assert those
  nodes are absent and node count is bounded; plus a depth-bound test.

**Deferred as its own effort (NOT this bug):** true lazy expansion for genuinely huge *legitimate*
trees (monorepo) — `Node::isRead` + a shallow-scan mode + an on-expand callback from `WorkspaceView`,
designed as one piece. Build output is handled by exclusion, not by lazy expansion.

---

## 5. FIXED — Open a non-active file from the project/workspace view in a restored session (where the file has been previously opened) will open a new tab with same file

**Symptom:** open a non-active file from the project/workspace view in a restored session (where the file has been previously opened) will open a new tab with same file name
resulting in a new tab with same file name.

Closing and opening the session again will now have two files with same name. The session file will also contain
two references to the same file.

**Root cause:** `Editor::OpenDocumentOrFolder` calls `Workspace::OpenFolder` (scans the folder into
path-only nodes) BEFORE `Editor::RestoreSession` runs. `Workspace::FromSession` → `ReopenDocument` →
the single-arg `NewDocumentWithFileRef(path)` always parented the new node under `GetDefaultWorkspace()`
— a separate, lazily-created "default" `ProjectRoot`, distinct from the `ProjectRoot` `OpenFolder` just
scanned for the same on-disk directory. So each restored document landed on a brand-new node under that
disconnected "default" tree, while the *displayed* (scanned) node for the same path stayed doc-less.
Selecting that displayed node in `WorkspaceView` → `Editor::OpenDocumentFromWorkspace` →
`EnsureDocumentForNode` then built a **second** `Document` for the same file (the doc-less node had
never been told one already existed) and opened it as a new tab — the duplicate session entries on
save followed from there.

**Fix:** added `Workspace::FindNodeForPath` (searches every `ProjectRoot`'s tree, via
`Node::FindNodeWithPath`, for a node already covering the absolute path) and call it first in the
single-arg `NewDocumentWithFileRef`; if a node already exists (scanned, or already open elsewhere) it's
reused — `EnsureDocumentForNode` attaches/returns its `Document` — instead of creating a duplicate.
`Node::FindNodeWithPath` itself was made to compare `lexically_normal()` forms, since an `OpenFolder(".")`
root (and everything scanned under it) carries a literal `/./` that a freshly-`absolute()`'d query path
doesn't. Also guarded `Editor::LoadDocument` to activate-not-reload when `NewDocumentWithFileRef` returns
an already-open document (mirrors the existing early-return in `OpenDocumentFromWorkspace`), so the
direct "open file" path can't clobber an open buffer's unsaved edits via the same mechanism.

**Regression test:** `test_session_workspace_reopen_reuses_scanned_node` (`utests/test_session.cpp`) —
`OpenFolder(".")`, then `ReopenDocument` a session entry for a file already scanned into the tree;
asserts no second `ProjectRoot` is created and the scanned node ends up holding the restored `Document`.
Verified failing on pre-fix code (extra root committed) before the fix, per the repo's
reproduce-before-fix discipline.

---

## 6. `Runloop::SwapQueues` swaps the message-queue pointers non-atomically (data race vs cross-thread `PostMessage`)

**Where:** `src/Core/Runloop.cpp`, `Runloop::SwapQueues` (L37) — already carries
`// FIXME: must be atomic or thread-safe`. The `incomingQueue`/`processingQueue` pointers live at
`Runloop.h:89`; the underlying `SafeQueue` (`src/Core/SafeQueue.h`) is itself thread-safe.

**What's wrong:** the message pump double-buffers two `SafeQueue`s and, once per frame, swaps the
`incomingQueue`/`processingQueue` **raw pointers** with three plain (non-atomic) assignments on the main
thread. A background producer calls `Runloop::PostMessage` → `incomingQueue->push(...)`, dereferencing
`incomingQueue` concurrently. The queue *contents* are mutex-protected, but *which* queue a producer
pushes into is read without synchronisation against the swap — a torn/stale pointer read can push into
the queue the main thread is simultaneously draining (or the just-swapped one): undefined behaviour and
lost/misrouted messages.

**Why it (mostly) hasn't bitten yet:** the only frequent cross-thread producer today is the async
tokenizer/parser in `TextBuffer` (it `PostMessage`s parse results); its posts are sparse relative to the
per-frame swap, so the window is rarely hit. The main-thread SDL pump posts from the same thread that
swaps, so it never races.

**When it WILL bite:** the folder-monitor / folder-scanner threading model
([`folder-scanner.md`](folder-scanner.md) §7) makes a background thread a *continuous* producer — turning
a rare window into a live data race. **This is a hard prerequisite for enabling any continuous background
producer (the live folder monitor); it must be fixed first.**

**Discovered:** 2026-06-17, while analysing the folder-monitor/scanner threading model — the message-pump
hand-off is the intended cross-thread mechanism, and `SwapQueues` is the one un-synchronised link in it.

**Proper fix (its own small branch):** make the swap mutually exclusive with `push` — a dedicated pump
mutex held during both `SwapQueues` and the pointer read in `PostMessage`; OR collapse to a single
`SafeQueue` drained "pop-until-empty snapshot" per frame (bounds the drain to what was queued at frame
start); OR make the two pointers `std::atomic` with acquire/release. Add a stress test: N producer
threads hammering `PostMessage` while the pump swaps/drains, assert no message is lost and no UB under
TSan/ASan.