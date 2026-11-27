# Build messages - errors and warnings from the terminal, as places to jump to

Written down 2026-11-18. **Steps 1-3 built** (2026-11-19, `feature/build-messages`); step 4's matchers (python, rust, ld) built
2026-11-20 (`feature/build-messages-step4`), the rest of step 4 the same day (`feature/build-messages-step4-ui`): every step
of §7 is built. Hitlist #52 (done). Decisions: §9.

## 1. The idea

Run a build in the terminal panel (`cmake --build .`, `make -j`, `ninja`, `cargo build`, a test run). The editor reads
what came back, picks out the errors and warnings with their file and line, and keeps them as a list of places. You
move through that list and fix them one by one, without copying a path out of the terminal or hunting for the line.

A prototype of this existed years ago (before this repo): a parser hooked onto the strings coming back from the build,
a hidden list of "jump points", keys to go to the next / previous one. It worked. What it lacked was UI - the list
couldn't be seen. Now there are modals, the code index's coloured source lines, a jump stack, mouse bindings and the
status line, so the list can be shown and used.

## 2. What it looks like (the user's side)

- **A modal of the last run's messages.** One row per message: severity, `file:line`, the message, and the source line
  itself in its colours (`ActionHelper::SourceLine` - the same as Go to symbol's preview). Errors first, then
  warnings; type to filter (`QueryListModal`). Enter opens the place.
- **Next / previous without the modal.** Two actions (`NextMessage` / `PreviousMessage`), so after the first fix the
  next one is one key away. The status line shows where you are: `error 2/7`.
- **Every jump is a jump.** Through `ActionHelper::JumpTo`, so `Cmd/Ctrl+[` comes back as from Go to definition.
- **A count after the run**, e.g. `3 errors, 12 warnings` in the status line, so you know there is something to look
  at without opening the modal.
- **Later:** a mark in the gutter / minimap on lines with a message; `Cmd/Ctrl+click` a `file:line` in the terminal
  itself (a `mouse:` binding in the terminal's keymap).

## 3. Where the text comes from

What exists today:
- `TerminalScreen::GetLastOutputText()` - the last command's output, row by row (scrollback + live grid). Its start
  is the committed command line, or OSC 133;C exactly when shell integration is on.
- `TerminalAPI::GetLastOutput()` - the same, reachable from JS.
- OSC 133;D carries the exit code, but `TerminalController::OnSemanticPrompt` only handles C today. The block index
  (`docs/done/terminal-scrollback.md` §9 sketched exactly this feature over `GetBlockOutputText`) went with the gvt
  rewrite - only "the last output" remains.

Two ways to read it - **open question 1**:
- **(a) After the fact.** Parse the last output when the command ends (OSC 133;D or the next prompt), or when the
  modal opens. Simple, and nothing runs on the pty thread.
- **(b) Live, as it streams** (what the prototype did). The list fills while a long build is still running, so you can
  start on the first error. Rows are complete only when they scroll or the cursor leaves them, and the pty thread
  feeds the screen under `screenLock` - the parser must not slow that down (hand the finished rows to another thread
  or to the runloop).

A suggestion: (a) first, built so (b) can feed the same parser row by row later.

Which commands - **open question 2**: every command (cheap - a few regexes per line, and `ls` output matches
nothing), or only ones that look like builds (`make`, `cmake --build`, `ninja`, `cargo`, ...), or a dot-command that
runs the build and collects (`.build`, with the command from the project config).

## 4. Recognising a message

The formats that matter, with their quirks:

| Tool | Shape | Notes |
|---|---|---|
| gcc / clang | `path:line:col: error: msg` / `warning:` / `note:` | `note:` and `In file included from` / `required from` lines belong to the message above them; `[-Wfoo]` at the end is the flag; the same header warning comes once per translation unit - de-duplicate |
| MSVC / clang-cl | `path(line): error C2065: msg`, `path(line,col): warning: msg`, `x.obj : error LNK2019: msg` | **built 2026-11-27** (Windows port §4.4): the code goes to the end (`msg [C2065]`); cl's notes are lines of their own (`path(line): note:`), clang-cl's source + caret as clang's; `cl : Command line warning D9002 :` and the LNK lines have no place; MSBuild's ` [x.vcxproj]` suffix is dropped |
| ld / lld | `path:(.text+0x..): undefined reference to ...`, `ld: error: ...` | often no line - the file is still worth a row |
| CMake | `CMake Error at path:line (cmd):` then indented lines | the message is on the lines after |
| make | `make[2]: Entering directory '/x'` / `Leaving directory` | not a message - it changes what relative paths are relative to; `make: *** [...] Error 1` is noise next to the real error |
| ninja | `FAILED: obj.o` then the compiler's lines | paths relative to the build dir |
| rustc / cargo | `error[E0308]: msg` then `--> path:line:col` | the place is on the next line |
| Python | `File "path", line N, in f` | a traceback - the last frame is usually the one |
| trun / unit tests | `=== RUN  \t_test_x` then `*** ASSERT ERROR: path:line\t'exp'` (also `GENERAL` / `FATAL ERROR`); the summary `  [Tma]: _test_x, path:line, msg` | **built 2026-11-19**: the case named in the message; the summary repeats each failure (one row); a case that only returned fail has no place - counted, not stepped to |

**As data, not code** - the same choice as languages-as-data (`support-languages.md`): matchers in
`Assets/Resources/build_messages.yml` (a regex, which group is the path / line / column / severity / message, whether it
continues the message above or carries the place on the next line), overridable in the user/project config. VS
Code's `problemMatcher` is the same idea. A new tool is then a few lines of YAML, not a rebuild.

## 5. Paths

The hard part, as in the prototype. A relative `path` is relative to whatever directory the tool was in:
- the shell's directory when the command ran - OSC 7 (`OnWorkingDirectory`; the bootstrap must report it), else the
  directory the editor last knew;
- make's `Entering directory` lines (a stack, with `-j` they interleave - but each one still names the directory);
- the build directory for ninja / `cmake --build <dir>`.

Then: absolute paths as they are; a relative one tried against the candidates above, then against the project root;
the first that exists wins. A file that isn't found is still a row, shown dimmed, not dropped.

## 6. Model

```
struct BuildMessage {
    kSeverity severity;          // error, warning, note (info)
    std::string path;            // resolved, absolute; empty when unknown
    uint32_t line, column;       // from 1, as the tool printed them; 0 = none
    std::u32string message;      // the first line
    std::vector<std::u32string> detail;   // continuation lines (notes, include chain)
    uint64_t outputRow;          // where in the terminal output - to show it there too
};
```

A small editor-level owner (`BuildMessages`: the list of the last run, the current index, next/previous) - not inside
`TerminalController`, which only hands over text. The parser is a plain function, text rows in, messages out, so it is
unit-tested on captured build logs (`Assets/testfiles/build-logs/`) without a terminal.

**Lines move.** After you fix the first error by adding two lines, the rest of that file's messages are two lines off.
Options - **open question 3**: ignore it (re-run the build, as in the prototype); shift a message's line by the edits
made in that document since the run (the document knows its edits); or anchor messages like the code index anchors
symbols. Ignoring it is fine for a first version.

## 7. Steps

1. Parser + matchers (gcc/clang, CMake, make directories) on captured logs, unit tests. No UI.
2. `BuildMessages` owner, fed from the last output on command end; next/previous actions, status-line count. This is
   the prototype again.
3. The modal (source-line preview, filter, Enter jumps).
4. More matchers (rust, python, ld, trun), live parsing (§3 b), gutter marks, Cmd+click in the terminal, JS access
   (`Terminal.GetMessages()`).

## 8. Open questions

1. Parse after the command ends, or live while it runs (§3)?
2. Every command's output, only build-like commands, or a `.build` dot-command (§3)?
3. Messages in a file after you've edited it - shift them, or leave until the next build (§6)?
4. Keys for next / previous message and for the modal - functionality, so `Cmd/Ctrl` (`F8` / `Shift+F8` is the common
   convention elsewhere)?
5. Does the list survive a restart (session), or is it only the last run's?

## 9. Decisions (2026-11-19)

1. **A streaming parser.** Bytes in, as they come off the pty (escapes, `\r`, a line split across reads), messages out
   as each line completes. The same parser reads the last output after the fact, so a later Build tab (#53) or a build
   button fills its list while the command still runs. v1 feeds it when the command ends.
2. **The commands we support.** Matchers are built in (code, as `.cpp` is a built-in language) for the tools we know -
   gcc / clang (also through make, cmake --build, ninja), CMake, make's directories. Matchers described as data
   (`build_messages.yml`, §4) come later, if they can be.
3. **Lines moving after an edit:** not in v1 - the next build gives the right lines.
4. **Keys:** `F8` / `Shift+F8` next / previous; the list as a modal - or the Messages tab when the panel has one (#53 step 5, 2026-11-20) - showing the last
   run's messages if there is one.
5. **Not kept across a restart** - the last run only.

Found while building it:
- **Fed from the pty's bytes, not the screen's rows.** Scrollback rows don't keep the soft-wrap flag, so a long
  gcc line that wrapped in the panel came back from `GetLastOutputText` as two broken rows. The pty thread hands each
  read to `BuildMessages::Feed` (its own lock, nothing while no run is running) - so v1 is live already (§3 b).
- **A run** starts when a build command is committed (`BuildMessages::IsBuildCommand`: `make`, `cmake`, `ninja`,
  `cc`, `gcc`, `clang++`, `g++-13`, ... after `VAR=x` / `time` / `env`), ends when its foreground job does (once
  seen - the bytes can come before the job is in the foreground) or at the next command. Another command (`ls`)
  ends it but keeps its list. The summary ("build: 2 errors, 1 warning") goes to the status line.
- **F8 goes errors first, then warnings** - the list's order, so the two agree ("error 1/2"). In output order clang's
  warning on line 2 came before the errors.
- **The shell's directory** is read from the shell process when the command is committed (`Shell::GetWorkingDirectory`:
  `proc_pidinfo` on macOS, `/proc/<pid>/cwd` on Linux) - no OSC 7 needed.
- **clang wraps its messages to the terminal's width** (`-fmessage-length` follows the pty's columns). A long path
  leaves `x.cpp:3:4: warning:` at the end of the line and all of the text on the lines below; a short one breaks the
  text over lines indented by 6. In a narrow panel `make -j` on testrunner listed 1 of its ~6 warnings (the line's
  trailing blank is trimmed, so `: warning: ` wasn't found; the wrapped text went to detail). Now: the marker counts
  at the end of a line, and indented-by-6 lines right after a message (not a `  12 |` source line) are its text; the
  repeat check is redone on the whole text (`clang-wrapped.log`, real captures at 60 / 50 columns). The cost: an old
  clang without the line-number gutter whose source line starts with exactly 6 spaces would have it read as text.
- **Step 4's matchers (2026-11-20)**, each on a fixture in `Assets/testfiles/build-logs/`:
  - **Python** (`python-traceback.log`, `python-syntax.log` - real captures): a traceback is one error, at the last
    frame that isn't Python's library (`/lib/python`, `site-packages`, `<frozen ...>`) - the program's own line that led
    there - else the last frame; the message is the exception line, the frames are its detail. A SyntaxError has no
    `Traceback` line, its `File` line starts it. A warning (`main.py:5: DeprecationWarning: x`) is a warning.
    `python`, `python3`, `python3.N`, `py`, `pytest` are build commands - running a script fills the list.
  - **rustc / cargo** (`cargo.log` - real captures): `error[E0308]: msg` takes its place from the ` --> ` line under
    it; the code goes to the end (`msg [E0308]`, as gcc's `[-Wfoo]`). Cargo's summaries (`could not compile`,
    `generated 1 warning`, `aborting due to`) are not messages.
  - **MSVC / clang-cl (2026-11-27)** (`msvc-cl.log`, `clang-cl.log` - real captures on Windows, CRLF): the place is
    `path(line)` or `path(line,col)` (`/diagnostics:column`, clang-cl's default), the severity may carry a code
    (`error C2065`, `fatal error C1083`, `Command line warning D9002`), and MSVC's tools put a blank before the colon
    (`x.obj : error`). The code goes to the end of the message, as rustc's. A temp `.obj` in lld-link's
    `>>> referenced by` is no place.
  - **GNU ld / lld** (`ld.log` - written from the format, no GNU ld on the Mac): `undefined reference to` /
    `multiple definition of`, the place from `file:line:` (with `-g`) or the file alone (`main.cpp:(.text+0x9)`); an
    object or archive (`libfoo.a(foo.o)`) is no place. `in function `main':` is context. lld's place is on its
    `>>> referenced by` line (the absolute path in the parentheses when given).
- **The rest of step 4 (2026-11-20)** - the layout picked by the user:
  - **The source line under each message**, in both lists (the modal, the Messages tab): two lines a message, the code
    from below the place (`BuildMessages::CODE_INDENT`), leading blanks gone, in the list's background. Read once per
    run, the first time it is drawn (`ActionHelper::ListedSourceLine` - the open document's line with its edits, else
    the file's), so it doesn't follow an edit. `QueryListModal` got `LinesPerRow` / `DrawRowLine` for it - generic, the
    UI layer doesn't draw code itself.
  - **Gutter marks:** a line with an error / a warning has its number in `gutter_error` / `gutter_warning` (the theme
    importer maps VS Code's `editorError.foreground` / `editorWarning.foreground`; a theme without them uses the change
    marks' colours). `BuildMessages::LineMarks(path)` is asked on every draw, so it resolves each message's path once
    per run - except, while the build runs, the last message: a line below it can still give its place (rustc's
    ` --> `). The minimap has no mark.
  - **Cmd/Ctrl+click in the terminal** (`OpenPlaceAtClick`, the terminal keymap's `mouse:` - the literal modifier, the
    keymap has no `modifiers:` table): the view keeps the text it drew per row; `TextPlace::At` reads the place at the
    click - `a.cpp:3:16`, ` --> src/main.rs:4:20`, `File "x.py", line 5` (anywhere from `File` to the number, a path
    with blanks in it too), `(/abs/a.cpp:3)`, a path alone. Resolved against the shell's directory now, then where the
    last build ran (§5). A word that is no file does nothing; a `file:line` not found says so. The ANSI backend can't
    send Cmd with a click (SGR mouse has no Super), so on the Mac it is a hand test.
  - **JS:** `Terminal.GetMessages()` - `[{severity, file, path, line, column, message}]` in the list's order, `path`
    resolved (`''` when not found); `null` before the first build.
