# Open bugs / known-wrong code (cross-session tracker)

Durable list of known defects we've chosen NOT to fix in-place yet, with enough context to pick each
up cold. Referenced from CLAUDE.md ("Documentation") so it surfaces each session.

Numbers are stable (commits, docs and notes refer to them) - a fixed bug is removed, never renumbered.
**Next bug is #34.**

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

## 33. Test order: after the `document` module, three `jsengine` reformat tests fail

**Repro (2026-11-16, on `main` too):** `trun -m document,jsengine --sequential ./libutests.dylib` - `jsengine_fmfilecmd`
(`test_jsengine.cpp:800`, `LineOf(doc, 1) == "    a();"`), `jsengine_fmblockcmd` (:827) and `jsengine_fmyaml` (:846,
`!doc->IsReindentable()`) fail. `jsengine` alone and the full green set (which runs `jsengine` before `document`) pass.
A `document` case leaves global state behind (a language's indent/reformat setting, or the Config) that the reformat
commands read. Not looked into further; the fix is probably a post-case reset in `test_document` or `test_jsengine`.

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

## 32. A git `Process` thread outlives `exit()` and posts into Runloop's destroyed message queue (use-after-free)

**Where:** `Process::Finish` (`src/Core/Process.cpp:89`) calls `onExit` on the process's own thread, which is
detached (`src/Core/unix/ProcessUnix.cpp:76`). `onExit` posts to the Runloop; its queues are statics
(`Runloop::msgQueueA/B`, `src/Core/Runloop.cpp:24-25`).

**What's wrong (ASan, Linux, 2026-11-16, on `main` and the #31 fix branch):** WRITE of size 8 on the process thread
(`Process::Finish` <- `ProcessUnix::RunLoop` <- the `DoStart` lambda), into a `SafeQueue<unique_ptr<Runloop::Message>>`
freed by the main thread in `__run_exit_handlers` (the statics' destructors). Nothing joins or cancels a running
`Process` before exit, so a `git status` still running when the process exits reports into freed memory.
One complete report (2026-11-16) names the caller: `SafeQueue<Runloop::Message>::push` <- `Runloop::PostMessage` <-
the `Editor::RefreshChangeBase` lambda (`GitBase::Result`) <- the `GitBase::Query` callback - the gutter's change-base
git query.

**Test or app:** seen as test teardown (`test_workspace_reopen` starts a git process; trun exits while it runs), ~4/60
runs under ASan; the report is usually cut to 3 lines because the process is exiting. The app can do the same on quit
with a git status/diff in flight. Delaying the end of exit by 4 s (an `atexit` sleep in a preload shim) gave 0/160.

**Fix direction (not decided):** keep running processes in a registry and stop/join them before exit (Editor
teardown), or give `Process` a "detached from the app" flag that `onExit` checks - a detached thread can't be joined,
so the first needs the thread kept.

**Repro:** Linux ASan build of utests (see #31 for the setup), `trun -m workspace -t reopen --sequential` in a loop,
with an `atexit(sleep(4))` preload so the report gets written in full.

