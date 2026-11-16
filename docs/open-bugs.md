# Open bugs / known-wrong code (cross-session tracker)

Durable list of known defects we've chosen NOT to fix in-place yet, with enough context to pick each
up cold. Referenced from CLAUDE.md ("Documentation") so it surfaces each session.

Numbers are stable (commits, docs and notes refer to them) - a fixed bug is removed, never renumbered.
**Next bug is #32.**

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

## 15. Opening and saving a Latin-1 (non-UTF-8) file silently truncates lines - DATA LOSS

**Where:** `UnicodeHelper::ConvertUTF8ToUTF32String` (strict decode), used per line by
`TextBuffer::Load` → `AddLineUTF8`. Save writes the buffer back as UTF-8.

**What's wrong:** on the first invalid UTF-8 byte the conversion returns `false` with the output already
sized to the input - everything from the bad byte on is NUL characters. Nobody checks the result, so the
line loads as "the part before the first non-ASCII char", and **saving writes the truncated line back**.

**Repro (real file):** noc_goa `src/extras/pcx.c` (ISO-8859-1, Swedish comments), line 296:
```c
				if (pcx->width&1) /* Kolla om oj\xe4mn bredd */
```
Open + save → the line becomes `if (pcx->width&1) /* Kolla om oj` - the comment's `*/` is gone, so the
file no longer compiles. Verified with a throwaway test (`/* g\xf6r om s\xe5 */` saved as `/* g`). Other
Latin-1 files in noc_goa: `src/extras/gms.c`, `quater.c`, `g3d/g3d.c`, `src/windows/*.c` (10+ files).

**Discovered:** 2026-10-24, by the code index brace-balance check (`gedit-bench --check`; the index
decodes whole files, so there the rest of the FILE vanishes). See `docs/code-index.md` §11.2.

**Fix (agreed direction 2026-10-24): detect -> decode -> convert, the save policy configurable.**
1. Load: check the decode result (never NUL-pad). BOM → exact (UTF-8 / UTF-16 LE/BE). Valid UTF-8 → done.
   Otherwise decode with a configured single-byte fallback (default `cp1252`, a superset of Latin-1's
   printable range). Single-byte encodings can't be told apart reliably (noc_goa `src/text.c` looks like
   DOS CP437/850), so the fallback is a setting, not a guess. The buffer is UTF-32 - loading is lossless.
2. Remember the source encoding on the buffer; show it in the status line.
3. Save policy, config `convert_encoding: auto | prompt | keep` (default `prompt`):
   `auto` writes UTF-8; `prompt` asks once on the first save (ConfirmModal: convert to UTF-8 / keep);
   `keep` writes the original encoding back (a live legacy repo: no diff noise, old toolchains). With
   `keep`, a character the encoding can't hold (`€` in Latin-1, any emoji) → prompt again / convert.
4. The code index only needs step 1 (read-only, no policy).

Either way the decode result must be checked, and the code index needs a lenient decode too.
Test: load a fixture with `\xe4`/`\xf6` bytes, save, compare bytes (round-trip must be lossless).

---

## 19. Build warnings - Linux / SDL2 still to check

**macOS / SDL3: clean (2026-11-16).** A clean debug build of `goatedit` + `utests` had 70 distinct warnings in our
code (shadowing, sign-compare, unused variables, missing initialisers, formats); all fixed, none left outside
`_deps` / `ext`. Sign-compare fixes cast the signed side to the unsigned type - the conversion the compiler already
made - so no comparison changed meaning. One real fix on the way: `WorkspaceView`'s status line dereferenced a null
node (a `breakme` placeholder in the `if`); it returns early now.

**Open:** the SDL2 sources only compile on Linux. The SDL2 twins of the SDL3 fixes were made by hand (`UpdateWindow`'s
`targetWindow`, the unused pixel rects, the keyboard driver's cast) but never compiled. Recount on Linux:
`cmake --build --preset debug --clean-first 2>&1 | grep warning: | grep -v "_deps/\|src/ext/" | sort -u`.
Linux's gcc warns about things clang doesn't (it reported more in `Theme.cpp`, `Editor.cpp` on 2026-11-13).

**Linux result (2026-11-16, Linux-Editor session):** builds; gcc-only fixes pushed (57e08d1). Left: ~70 `-Wshadow` from
gcc, almost all constructor/setter parameters named as a member (FindModal, QuickOpenModal, SymbolSearchModal,
RepoMonitor, Process, ExclusionRules, SDL2 SDLFont/SDLDrawContext, HLScreen, ConfirmModal, ...) - clang doesn't flag
those, gcc does. They are not asked for by our CMake: `src/ext/gnklog/CMakeLists.txt:94`
`target_compile_options(gnklog PUBLIC -Wall -Wextra -Wshadow -Wpedantic)` (under `if(UNIX)`) spills into every target
linking gnklog. **Decide first (the user's repo):** make those options PRIVATE in gnklog, or keep `-Wshadow` and rename
the ~70 parameters.

**Then:** consider making new ones visible - `-Werror=shadow` on our sources (not `_deps`/`ext`), so one in a header
can't slip in again.

**Not ours, leave:** `stb_image.h` (two unused functions), gnklog's `LogManager.cpp:318` (`-Wunused-result`).

---

## 20. The terminal version on macOS gets the Cmd keymaps - and the terminal takes the keys first

**Where:** `KeyMapping::LoadKeymapConfig` (`src/Core/KeyMapping.cpp`) picks the keymap files by operating system: the
`macos:` / `linux:` sections of `Assets/Resources/config.yml`. Not by backend. So the ANSI/terminal version (Gansi) on
macOS runs the same `macOS/*_keymap.yml` as the SDL window, with `CopyPasteModifier: KeyCode_Cmd`.

**What's wrong:** in a terminal, `Cmd` belongs to the terminal program first. Gansi turns on the kitty keyboard protocol
(`gansi/src/Capabilities.cpp` - kitty, Ghostty) and decodes Super as `Cmd` (`GansiKeyboardDriver`), so a `Cmd` key the
terminal doesn't bind does arrive. But Ghostty binds many of ours by default (check with `ghostty +list-keybinds
--default`):
- `Cmd+1..9` - its tabs (our `SwitchToBuffer`)
- `Cmd+T` - a new tab (our Go to symbol)
- `Cmd+[` / `Cmd+]` - the previous / next split (our navigation stack, back / forward)
- `Cmd+C` / `Cmd+V` - its copy and paste

The `Option` alternatives (`Option+1..9`, every `UINavigationModifier` key) don't arrive either unless Ghostty has
`macos-option-as-alt` set: by default Option composes characters (`¡™£`), as SDL3 did before its `only_left` hint.

**Found:** 2026-11-09, running the terminal version in a Ghostty tab: `Cmd+1..9` never reached the editor.

**Fix direction (to design):**
- **The terminal version takes the Linux keymaps on macOS too:** `Ctrl` for functionality, `Alt` for the UI - what
  terminals are built around. The section becomes a choice made at startup: `Editor` (the glue that picks the backend)
  tells the keymap cache which section to read, so `KeyMapping` stays free of backends (the layering rule in
  `CLAUDE.md`, as for the screen geometry).
- **With the kitty protocol** `Ctrl+[` and `Esc`, `Ctrl+I` and `Tab` arrive apart, so the Linux keymaps work in Ghostty.
  Without it (Apple Terminal, tmux) the legacy collisions remain - as on Linux today.
- **`Option` as `Alt`** can't be fixed from our side: document it, or say so once at startup when Gansi finds Ghostty
  (`TERM_PROGRAM`) - `macos-option-as-alt = left` matches what SDL3 does.

**Open questions:** all keymaps or only `default`; whether config can override the choice (`keymap_section:`); and the
related Linux one - the terminal keymap inherits the default one, so `Ctrl+B`, `Ctrl+[` and `Ctrl+]` go to the editor
even in a full-screen program in the embedded terminal (`Ctrl+[` is `Esc` to vim; `docs/code-index.md` §11.9). Both are
"which keys are the editor's in a terminal", and may want one answer.

**Research notes (2026-11-11) - how terminal editors do copy and paste.** Most of them treat paste and copy as two
separate problems:
- **Paste needs no key of the editor's own.** The terminal pastes with its own `Cmd+V` (`Ctrl+Shift+V` on Linux) and sends
  the text as one bracketed-paste block. Neovim, Helix, micro and Kakoune all rely on this. Gansi does it already: a
  `PasteEvent` is loaded into the clipboard and posted as `PasteFromClipboard` (`GansiScreen.cpp`, `DispatchEvent`).
- **Copy: the editor pushes the text out itself.**
  - **OSC 52** sets the system clipboard, and works over SSH. Ghostty, kitty, WezTerm and Alacritty support it,
    iTerm2 only when it's turned on, and Apple Terminal not at all.
  - **Locally,** Neovim and Helix try `pbcopy` / `xclip` / `wl-copy` first and use OSC 52 when those are missing.
  - Gansi sends OSC 52 every time the editor's clipboard changes (`SetOnUpdateCallback` in `GansiScreen::Open`). It has no
    local fallback, so copy goes nowhere in Apple Terminal.
- **The copy key:**
  - Neovim and Kakoune use their own keys (`"+y`, `y`); `Ctrl+C` is never copy.
  - Helix uses `Space y` / `Space p`.
  - **micro** uses plain `Ctrl+C` / `Ctrl+X` / `Ctrl+V`, as a GUI editor does. In raw mode `ISIG` is off, so `Ctrl+C`
    reaches the program as a key (0x03), not an interrupt. micro is the closest precedent for us.
  - nano uses `Alt+6` copy, `Ctrl+K` cut, `Ctrl+U` paste.
- **The terminal's own `Cmd+C`** copies what's *on screen*, not the editor's selection. When the editor captures the mouse,
  a terminal selection needs `Option`/`Shift`-drag. That is why the editors have a copy key of their own.
- **For us:** micro's way (`Ctrl+C/X/V` - the Linux keymap) fits with what Gansi already does: `Ctrl+C` -> OSC 52;
  `Ctrl+V` or the terminal's `Cmd+V` -> paste. That leaves two gaps:
  - Apple Terminal: no OSC 52, so it needs a `pbcopy` fallback when local.
  - The embedded terminal view: `Ctrl+C` there must stay the shell's interrupt. The terminal keymap already keeps that
    apart.

---

## 28. A user or project config list overwrites the default list by position

A list in `~/.config/goatedit/config.yml` (or `<root>/.goatedit/config.yml`) neither replaces the default list nor
adds to it: element `i` overwrites the default's element `i`, and the rest of the default stays. With the shipped
`workspace.exclude: [".git", ".github", ".idea"]`, a user config of `workspace: { exclude: ["build"] }` gives
`["build", ".github", ".idea"]` - `.git` is no longer excluded, and nothing says so. The same goes for
`font_fallback`, `bootstrap_scripts` and any list added later (languages as data: pairs, indent triggers).

**Why:** `YAMLMerge::MergeSequences` (`src/Core/Config/YAMLMerge.cpp`) merges by index. It was written for scalars
and maps; lists were never thought through. Every user/project merge goes through it (`Config::TryMergeUserConfig`,
`TryMergeProjectConfig` -> `ConfigNode::MergeNode`).

**Fix direction:** a list from a later layer **replaces** the earlier list whole - what a user who writes a list
expects - with an explicit form for adding to it (to decide: e.g. a `<key>+:` key, or a `{ append: [...] }` map).
Filed rather than fixed straight away: changing it changes what existing user configs mean, so check the users'
and the shipped project configs for lists first. Test: merge a short list over a longer default and assert the
result equals the short list (fails today), plus the append form. Languages as data (`support-languages.md` §9)
does its own inheritance with these rules and doesn't use `YAMLMerge`.

---

## 24. A command finished in readline after Tab can go into the history with the shell's extras

**Where:** `TerminalController::ForwardActionToShell` (shell-owned commit) records the mirror from
`SyncInputLineFromGrid`, which reads the prompt's row from the prompt end to the last non-blank cell.

**What's wrong:** what the shell draws after the cursor - zsh's `RPROMPT`, a zsh-autosuggestions suggestion - is
read as part of the line, so `vim apa.cpp` can be recorded as `vim apa.cpp        12:30`. A line that wrapped, or a
prompt redrawn with other text (a clock) under a completion list, leaves the mirror stale, so the history gets an
older version of the line. (A prompt redrawn unchanged - Tab Tab - is followed: `IsPromptRow` re-anchors.) The shell itself runs its own line, so only the history (Up) is wrong.

**Already handled for dot-commands (#17, 2026-11-11):** `CommitShellOwnedDotCommand` takes the mirror only up to the
cursor, and only while `isLineMirrorValid` (cursor on the prompt's row, row not full); otherwise the shell gets
its own line. **Fix direction:** record the same text the dot-command check uses (up to the cursor), and skip the
history entry when the mirror isn't valid rather than recording a stale one. Limits shared with the dot-command
path: Enter with the cursor mid-line drops the text after it; vi-mode readline may not clear the line with
`Ctrl+E Ctrl+U`; an Enter handled before the shell's echo of the last keys arrives reads the line without them
(not seen in practice - zsh echoes within the same pass; a dot-command then falls back to the shell).

---

## 26. Claude Code's `/usage` screen doesn't render in the embedded terminal - missing glyphs?

**Where:** the SDL draw path - the embedded terminal's text drawing (`GlyphFont`, the `terminal`/`sdl` `font_fallback`
list in `config.yml`, or how the SDL `DrawContext` draws a terminal cell). **Not the emulation:** the same screen
renders correctly in the ANSI backend (`--backend ansi`), which runs the same gvt `Terminal` and only hands the cells
on to the host terminal - whose own font has the glyphs.

**What's wrong:** running Claude Code in the editor's terminal (macOS/SDL3), the `/usage` screen does not render.
Other Claude Code screens (`/context`, the normal prompt) do. Suspected: `/usage` draws its bars with characters the
theme font and the fallbacks don't have (Block Elements U+2580-259F - partial blocks `▏▎▍▌▋▊▉`, shades `░▒▓` - or
similar), so they come out missing. Glyphs are still the lead suspect but unverified - other SDL-only causes to
rule out: wide-char/width-0 cells, or a colour/attribute (truecolour bg, inverse) the SDL path draws differently.

**How to pin it down:** capture what `/usage` writes (run `claude` under a pty logger - `script -q /tmp/usage.log
claude`, open `/usage`, quit) and list the non-ASCII code points in it; for each, check whether `GlyphFont` finds a
glyph (font, then each fallback). If they're all there, look at what `TerminalScreen::GetRow` hands the SDL view for
those cells (width, colours) - the gvt side is cleared by the ANSI backend.

**Found:** 2026-11-11 by the user, while working in Claude Code in the editor's terminal.
2026-11-11: the user checked `--backend ansi` - `/usage` renders correctly there, so gvt is cleared.
2026-11-11, **glyphs ruled out:** a pty capture of `/usage` uses only `· … ↓ █ ▌ ▔ ▝ ◐ ❯` (bars are `█`). Andale Mono
has `· … ↓ █ ▌`; Menlo (first macOS fallback, `.ttc` opened at index 0) has all nine. (JetBrains Mono lacks `◐`.)
The stream also sets mouse tracking (`?1000/1002/1003/1006h`) - not forwarded to the pty by either backend, so not
it - and wraps each frame in synchronized output (`?2026`, ~9 frames), plus 24-bit SGR fg/bg. Next: see what the
SDL failure actually looks like (blank? stale frame? partial?) - replaying the capture with `cat` in the editor's
terminal should reproduce it without Claude Code.
2026-11-11, **`/context` is glyphs:** its usage grid (`⛀ ⛁ ⛶ ⛝`, U+26C0/26C1/26F6/26DD) is blank in SDL
(`screenshots/claude_context-rendering-sdl3.png`). No configured font has them; on this Mac only `STIXTwoMath.otf`
does (CLion finds it through the system's own fallback). Fix: [`system-font-fallback.md`](system-font-fallback.md).
`/usage` itself is still unexplained.
2026-11-11: system font fallback merged; `/context` draws in the app. Open: whether `/usage` still fails.

---

## 31. Parse thread walks `lines` while the main thread inserts/erases lines (heap use-after-free)

**Where:** `TextBuffer::RecolourSemanticNames` (`src/Core/TextBuffer.cpp`), reached from the parse thread
(`ParseThread` -> `ExecuteParseJob(kWorkspaceNames)` or the idle tick -> `TakeWorkspaceNames`). Seen as a segfault in
`Line::Release` on an unmapped address.

**What's wrong (ASan, Linux, 2026-11-16, `origin/main` 0404c48):** a race on the `lines` VECTOR, not a stale thread or
a destroyed buffer. The buffer is alive; its parse thread iterates `lines` (`for (auto &line : lines)`) while the main
thread `push_back`s into the same vector, and the reallocation frees the storage the loop is reading:
- **use** (parse thread, T98): `RecolourSemanticNames` :587 <- `TakeWorkspaceNames` :595 <- `ExecuteParseJob` :258
  (the `kWorkspaceNames` job) <- `ParseThread` :240. Thread created by `StartParseThread` <- `Reparse` <- `SetLanguage`
  <- `Document::LoadData` <- `Editor::OpenDocumentFromWorkspace`.
- **free** (test thread): `std::vector<Line::Ref>::_M_realloc_insert` <- `TextBuffer::AddLine` (`TextBuffer.h:118`) <-
  `AddLineUTF8` <- `CloseAndReopen` (`utests/test_workspace.cpp:429`) <- `test_workspace_reopen`.
- **allocated** by the same test thread: the vector's previous growth in `AddLine` via `CreateEmptyBuffer` /
  `Workspace::EnsureDocumentForNode`.

Opening the document queues a `kWorkspaceNames` job (`SetWorkspaceNamesSource`) and does not wait for it; the test
then appends a line straight away. Trigger confirmed: `trun -m workspace -t reopen` alone crashes 2/30 (plain debug);
`textbuffer,treeview,workspace` minus `reopen` 0/30. The test names in earlier reports (`treeview_*`, `workspace_*`)
lagged because stdout to a file is buffered and lost at the crash.

**Not test-only.** There is no buffer-level lock on `lines`: `AddLine`, `Insert`, `DeleteLineAt` mutate the vector
bare (only `DeleteLineAt` locks the one `Line`), and every parse-thread walk (`RecolourSemanticNames`,
`PublishLineHashes`, `CollectStructure`, the region/full parse) reads it bare. The app takes the same path when a
names job or idle recolour (`isRecolourPending`, or a new answer from the index via `TakeWorkspaceNames` on every idle
tick) overlaps an Enter / paste / undo / delete-line on the main thread - a narrow window, wider on big files.

**Why the guards don't cover it:** the destructor and `Close()` stop and join the thread, which is right, but the
buffer is not being destroyed here. `CanEdit()` is the edit-side guard and is effectively always true
(`bIsReadOnly == false` short-circuits it), and even its `kState_Idle` test would not help: the idle-tick recolour and
`TakeWorkspaceNames` run while the state is `Idle`. Per-`Line` `Lock()`/`Release()` protects a line's contents, not the
vector holding the `Line::Ref`s.

**How it was caught:** utests built with `-fsanitize=address` (CMAKE_CXX/C/linker flags on the configure line; the
`asan` preset only instruments `goatedit`). trun opens the lib with `RTLD_DEEPBIND`, which ASan refuses - preload a
5-line `dlopen` shim that clears that flag, ahead of libasan (`LD_PRELOAD="shim.so $(gcc -print-file-name=libasan.so)"`,
`ASAN_OPTIONS=verify_asan_link_order=0:...`). The report has no symbols then; resolve offsets with
`addr2line -f -C -i -e libutests.so`. Forked (non-`--sequential`) mode reports no results under that preload.

**Repro:** Linux, `cd cmake-build-debug && trun -m workspace -t reopen --sequential ./libutests.so` in a loop.

