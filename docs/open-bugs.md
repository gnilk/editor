# Open bugs / known-wrong code (cross-session tracker)

Durable list of known defects we've chosen NOT to fix in-place yet, with enough context to pick each
up cold. Referenced from CLAUDE.md ("Documentation") so it surfaces each session.

Numbers are stable (commits, docs and notes refer to them) - a fixed bug is removed, never renumbered.
**Next bug is #24.**

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

## 16. The colour script misreads `#rgb` and can't read `#rrggbbaa`

**Where:** `SublimeConfigScriptEngine::ParseWebColor` (colors.json values).

**What's wrong:** the short form takes each hex digit as the whole channel - `#fff` becomes
`(15, 15, 15)` (near black), not `#ffffff`. An 8-digit `#rrggbbaa` (VS Code, Sublime both allow it) is
rejected ("Hex color description to short"). The stock colors.json uses neither, so nothing shows today.

**Workaround in place:** the theme importer (code index step 1c) parses hex itself and writes `#rrggbb` /
`rgba()` only. **Fix:** expand `#rgb` / `#rgba` to two digits per channel; accept 4 and 8 digits with alpha.

**Discovered:** 2026-10-25, writing the theme importer.

---

## 17. A dot-command goes to the shell once Tab has been pressed on its line

**Where:** `TerminalController` - `OnAction` (`kActionShellCompletion`) and `ForwardKeyPressToShell`
(`kUIActionCommitLine`).

**What's wrong:** dot-commands (`.theme`, `.cfg`, ...) are only recognised in `CommitLine`, which offers the
line to `PluginExecutor::ParseAndExecuteWithCmdPrefix` before the shell. The first Tab hands the line to
readline/zsh for completion (`doesShellOwnLineEditing = true`, line + Tab sent to the pty); from then on
Enter is forwarded as a raw CR, the line never passes the dot-command check, and the shell runs it:
`zsh: command not found: .theme`. Typing the same line without Tab works.

**Repro:** in the terminal, type `.theme --import ~/Down` + Tab (complete the path) + `--name x` + Enter.

**Fix (agreed direction 2026-10-25):** check the line before letting the shell execute it. On commit in
shell-owned mode, read the line (`SyncInputLineFromGrid` keeps `inputLine` mirroring readline's line);
if it starts with `cmdlet_prefix`: send Ctrl-U (clear readline's line) instead of CR, `ExitShellOwned()`,
then run it through the plugin executor like `CommitLine` (which sends a newline for a fresh prompt).
Tab completion of paths then works for dot-commands too. Caveat: the mirrored line is read back from the
grid - a wrapped long line or a visible completion menu can garble it; test both.

**Discovered:** 2026-10-25, GUI test of `.theme --import` (code index step 1c).

**Still open (checked 2026-11-11):** half of the fix is in - `ForwardActionToShell` now mirrors the line from the grid
on a shell-owned commit (`SyncInputLineFromGrid`, for history and the `.co`/`.ob` output mark), but it still sends
`\r` to the shell without the `cmdlet_prefix` check.

---

## 19. Build warnings - a cleanup session (mostly `-Wshadow`)

**Where (a clean debug build, macOS/SDL3, 2026-11-09 - 28 distinct warnings):**

- `-Wshadow`, a local or parameter named as a member (12): `Document.cpp` lines 168, 190, 325, 1579;
  `EditorView.cpp:284`; `HexView.cpp:249`; `SDL3/SDLScreen.cpp` `CreateWindow` (its `window` parameter);
  `SDL3/SDLWindow.cpp:212`; `Index/CPPStructureScanner.h:119` (a header - repeats in every file including it);
  `Theme/Theme.cpp:67`, `:75`.
- `-Wshadow`, a local hiding a local (1): `KeyMapping.cpp:372`.
- `-Wmissing-field-initializers` (6): `Index/CPPStructureScanner.cpp` 488, 533, 539, 582, 594, 601 (`name`) -
  aggregate initialisers leaving the last field out.
- One each: `-Winconsistent-missing-override` `macOS/MacOSKBEmulator.h:17` (`Initialize`); `-Wformat-pedantic`
  `Language/LangToken.cpp:103` (an enum through `%d`). (`unix/Shell.cpp`'s unused `signal_handler` is gone - the
  gvt terminal work removed it.)
- Not ours, leave: `stb_image.h` (two unused functions), gnklog's `LogManager.cpp:318` (`-Wunused-result`).

**What's wrong:** nothing breaks today, but the noise hides a warning that matters - a new one in a header repeats in
every file including it (`DrawContext::DrawImage`'s `rect` parameter, from the minimap, was 69 of 98 lines until it
was renamed `cellRect`). A shadowing parameter is also the kind that reads the wrong value silently on the next edit.

**Fix direction:** one session, file by file: rename the shadowing names (the member keeps its name; the local or
parameter says what it is - `newWindow`, `cellRect`), complete the initialisers (or name the fields), add the
`override`, cast the enum. Check the SDL2 build too (Linux/`cmake-build-sdl2`) - its
sources are only compiled there. Then consider making new warnings visible - `-Werror=shadow` on our sources (not
`_deps`/`ext`) so a header one can't slip in again.

**Recount:** `cmake --build --preset debug --clean-first 2>&1 | grep warning: | sort -u`

**Discovered:** 2026-11-09, after the minimap (§7) - "a lot of build warnings now".

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

---

## 21. A file argument before the folder leaves that folder without a folder monitor

`goatedit src/a.txt .` (from the project root) shows the tree but never updates it: no files written by others,
no reload. `goatedit .` works. Both platforms - the consumer side, not a backend.

**Why:** `LoadDocument` for the file runs first and creates the **default root** at the cwd
(`Workspace::GetDefaultRoot`: `ProjectRoot::Create(nullptr, nullptr, ...)` - no reconcile callback, "no
file-monitor" by design). `OpenFolder(".")` then calls `GetOrAddProjectRoot`, which matches by path and returns
that default root, so `StartFolderMonitor` works on a root that can't reconcile. The loose file also shows at the
root's level instead of under `src/`.

**Fix direction:** decide whether a folder open at the default root's path *promotes* it (give it the reconcile /
changed callbacks, re-scan, re-home its loose documents into the tree), or whether `GetOrAddProjectRoot` must
skip the default root and the default root be dropped once empty. Repro in a unit test: open a document, then
`OpenFolder` of the cwd, and assert the root has a reconcile callback / a started monitor (fake factory).

---

## 22. Installing (and building) never removes an asset that was deleted from the repo

After `blocktobuffer.js` / `blocktoclipboard.js` were removed (command blocks gone, `860dee3`), a fresh macOS
`cmake --install bld` still left both in `/Applications/goatedit.app/Contents/SharedSupport/Plugins/Scripts/`.
Removed by hand on 2026-11-10. Harmless this time - `jsplugs.yml` no longer names them, so nothing loads them - but
a stale *loaded* file (a keymap, a theme, a plugin still listed by an older user/project config) would keep its old
behaviour with no sign of where it came from.

**Why:** every copy step only adds or overwrites: the macOS `install(DIRECTORY ${EDITOR_ASSET_DIR}/ ...)` into the
bundle, the `POST_BUILD` `copy_directory` into the build tree's `goatedit.app/Contents/SharedSupport`, and the
`resources` / `plugins` targets (`copy_directory_if_different`) into `EDITOR_ASSET_DIR`. Linux is covered for installs:
`scripts/install.sh` uninstalls by its manifest before installing.

**Fix direction:** on macOS, clear `<bundle>/Contents/SharedSupport` before the asset install (an `install(CODE ...)`
`file(REMOVE_RECURSE ...)` ahead of `install(DIRECTORY ...)`), and do the same for the build tree's bundle copy, or
copy with a mirror step that deletes what the source no longer has. Check it with a file removed from
`Assets/Resources`: after build + install it must be gone from both bundles.

---

## 23. The ANSI backend is sluggish while a full-screen program runs in the embedded terminal

The terminal is generally fast, but a program that takes over the terminal (vim, claude, ...) feels slow: its output
appears in steps instead of as it arrives.

**Why (from the code, 2026-11-11):** the SDL backends had the same symptom. Their `PollEvents` waits up to 250 ms
for an event (`SDL_WaitEventTimeout(nullptr, 250)`); in raw passthrough the pty output never produced one, so each
screen update waited out the timeout. The cure there is `SDLScreen::WakeUp()` - it pushes a wake event so the
wait returns as soon as `Runloop::PostMessage` queues work from another thread. `GansiScreen::PollEvents` has the same
wait (`terminal->PollEvent(250)`) but **does not override `ScreenBase::WakeUp()`**, which is an empty default - so
nothing from the pty thread cuts the wait short.

**Fix direction:** give `GansiScreen` a `WakeUp()` that makes the blocked `PollEvent` return - e.g. a self-pipe the
terminal's poll also watches (gansi side: a wake fd or a `Wake()` on the terminal), written from `WakeUp()`, drained
in `PollEvents`. Keep the "one pending wake" guard the SDL screens use (`isWakePending`) so a busy pty doesn't flood
the pipe. Check: `vim` scrolling and typing in the embedded terminal under Gansi feel the same as under SDL3.
