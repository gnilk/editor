# Linux checklist

What to check on the Linux box (SDL2), most important first. Written 2026-11-25 for `feature/editor-agent-ask`.

- **Last full Linux run in the notes:** 2026-11-19, at `e7117ee4`. Over 80 commits have come since; none is recorded as run on Linux.
- **Not merged:** the branch holds #77 (layouts), #78 steps 1-3 (agent, `.ask`), #79 and #82. `dev` waits for this run.
- Every open Linux step in [`hitlist.md`](hitlist.md) is here, with its number. Tick here; the hitlist is updated later.
- **Short on time?** Parts 1-3 are what the merge to `dev` needs. Parts 4-6 can follow while you use the editor.

Linux keys: `Ctrl` for functionality, `Alt` for the UI. `Alt+T` / `Alt+E` / `Alt+P` = terminal / editor / project tree,
`Ctrl+Shift+T` / `Ctrl+Shift+W` = new / close terminal tab, `Alt+1..9` = tab, `Ctrl+[` = jump back.

## 1. Build and automated tests

```sh
git fetch && git switch feature/editor-agent-ask && git pull
sudo apt-get install -y build-essential cmake libyaml-cpp-dev libsdl2-dev libfontconfig-dev    # only if missing
cmake --preset debug
cmake --build --preset debug
cmake --build --preset utests
```

- [ ] **It compiles and links.** New files since the last run: `src/Core/Agent/`, `AgentAsk`, `PanelHost`, `PanelLayout`,
  `StructureDump`; the SDL2 font and screen code and `Shell.cpp` changed. A missing `#include` is the likely error (GCC's
  library pulls in less than clang's).
- [ ] **Unit tests:** `cd cmake-build-debug && trun ./libutests.so` - 0 failed. The Mac has 1078; the number can differ a little.
- [ ] **Terminal library:** `cmake --build cmake-build-debug --target gvttests && cd cmake-build-debug && trun ./libgvttests.so`
- [ ] **App checks**, from the repo root, with the desktop unlocked (one check opens a window):
  `python3 scripts/handtests/checks.py --exe cmake-build-debug/goatedit`
  - The Mac: 37 pass, 0 fail, 4 skipped. Three of those four are for Linux and should run here: `install_stale`,
    `sdl_no_display`, `desktop_window` (needs `xdotool`). `real_agent_ask` stays skipped - it costs tokens.
  - It takes around ten minutes. No check talks to a real agent.
  - **A failed "quits" line can be an old crash.** The checks now fail on a bad exit status; until 11-25 a crash at quit
    went unnoticed.
  - `editor_agent_template` failed once on the Mac on its "answering" line, probably from a slow start. Run it alone
    before believing it.

An odd compiler crash or a test that segfaults once: **try again first.** This box has logged CPU machine-check errors
(`journalctl -k | grep -i mce`).

## 2. Crashes and hangs

Install a release build, then use it on a real project:
`cmake --preset release && cmake --build --preset release && scripts/install.sh --user --build-dir cmake-build-release`

- [ ] **Quit is fast and clean** (open-bugs #37's fix; it stops the folder monitor first). Quit about ten times: at once after
  start-up, with a build running, with `claude` running, and while files change in the project
  (`while sleep 0.05; do touch x; done` in another terminal). Under a second or so, no crash.
- [ ] **Quit and tab close with several tabs** (#79: they hung). Three terminal tabs, `claude` or `top` in two of them: close one
  with `Ctrl+Shift+W`, then quit.
- [ ] **A shell that ends** (#82): `exit` in the Shell tab - the notice, then Enter: a new prompt below it.
- [ ] **... with a background job:** `sleep 60 &`, then `exit`. The notice should come at once. **Suspected problem:** on Linux
  it may only come when the `sleep` ends. Note what happens.
- [ ] **... in the Claude tab** (#82): quit `claude`, `exit`, Enter - `claude` starts again.
- [ ] **`Ctrl+D` doesn't kill the panel** (fixed 11-25): type `echo one two`, `Left` four times, `Ctrl+W` (`one` goes); try `Ctrl+U`,
  `Ctrl+K`, `Ctrl+Y`; clear the line, `Ctrl+D` - the shell stays. `Ctrl+T` still opens Go to symbol.
- [ ] **Ordinary use for a while,** with `claude` in a tab: typing is quick, nothing freezes, no drawing left behind after resizing.

## 3. New on this branch (#77, #78)

Layouts - in `~/.config/goatedit/config.yml`: `main.layout: landscape`, `panel_right: { tabs: [ { name: Claude, config: ai_agent } ] }`,
`ai_agent: { inherit: terminal, bootstrap: ["claude"], promote_target: yes }`; a project with no session.

- [ ] **Landscape** (#77): Claude full height on the right, Shell at the bottom, the status line across the whole width. Drag the
  right divide; `Alt+A` / `Alt+D` in the right panel move it. `Alt+T` from the editor goes to the panel used last, again to the
  other. A restart keeps both.
- [ ] **`auto`** (#77): `main.layout: auto` - maximised starts landscape, half the screen portrait. Drag the edge across 3:2: one
  switch, no flicker, `claude` keeps running with its output intact.
- [ ] **`.layout`** (#77): `.layout portrait` / `landscape` / `auto`, and `.layout` alone (prints it), while `claude` runs.
- [ ] **Portrait start** (#77): `main.layout: portrait`, same session - Claude is a bottom tab; back to landscape: on the right again.
- [ ] **`.panel move`** (#77): the right panel's tab goes to the bottom (the right panel closes with its last tab) and back; a
  restart keeps it. `Esc` in a running agent's tab is the agent's: focus that tab, `Alt+E`, then `Esc` and `.panel move`.
- [ ] **Agent tab** (#53): `Ctrl+Enter` on a line in the editor puts it in the Claude tab; a restart brings the tabs back.
- [ ] **`.agent`** (#78): without `main.ai_integration`, and with `claude` - the transcript folder it names exists (`ls` it).
- [ ] **Editor Agent's tab** (#78, starts a real agent): `.agent start` - a bold violet label, the banner stays above Claude, its title
  doesn't rename the tab. `Ctrl+Shift+W` asks, then stops it. `.panel move` says it stays. A restart doesn't bring it back.
- [ ] **Questions** (#78, **costs tokens**, each one asks first): `.ask why` on a line; `.agent model sonnet` while it runs - the tab
  comes back, the focus stays, your own `claude` keeps its model; `.ask structure` - the consent names `sonnet`. Needs
  `python3`, and Claude Code trusted in the folder.

## 4. Build messages and clicks

- [ ] **A build with an error** (#52): `make` or `cmake --build` - "build: N errors, M warnings" when it ends. `F8` / `Shift+F8` go
  through them, errors first, at the right line and column; `Ctrl+[` comes back.
- [ ] **A long build** (#52, this repo, `-j`): `Ctrl+F8` while it runs lists what has come, "(running)"; an `ls` afterwards keeps the
  list; the next build replaces it.
- [ ] **The list** (#52): `Ctrl+F8` - the command and counts in the caption, type to filter, Enter goes there, `F8` goes on from it.
- [ ] **The code row** (#52): each message's code on the row under it, coloured - in the list and the Messages tab.
- [ ] **The gutter** (#52): after a build, an error line's number is red, a warning's orange / yellow.
- [ ] **`Ctrl+click` in the terminal** (#52) on `file:line` (gcc / clang, rustc's ` --> `, a Python traceback) opens it; `Ctrl+[` comes
  back; a plain word does nothing.
- [ ] **Messages tab** (#53): `- view: messages` in `panel.tabs`, a project with no session - 'Messages (N)' while `make` runs, `Ctrl+F8`
  shows it, a double-click jumps, `F8` moves the selection.
- [ ] **trun** (#52): a failing `trun --sequential ./libutests.so` - each failure counted once; `F8` goes to the `TR_ASSERT` line.

## 5. Everyday editing in the window

- [ ] **Save** (#58): `Ctrl+S` saves, the `*` goes, also with the terminal or tree focused; a file another program rewrote: it
  asks (Overwrite / Reload / Cancel).
- [ ] **`nano` in the terminal** (#58): `Ctrl+S` is nano's save, not the editor's.
- [ ] **Select with the mouse** (#41): drag selects, backwards too; below the view it scrolls; a click drops it; `Shift+click` extends.
- [ ] **Clicks** (#41): `Ctrl+click` still jumps, without selecting; double-click in the tree still opens.
- [ ] **A dot-command at your own prompt** (#57): `.theme list` + Enter - the command stays on its line, the list right below,
  then one new prompt; no blank line.

## 6. Looks, and Python (deferred)

- [ ] **Switch themes** (#19): `.theme list`, `.theme use <one>` - editor, gutter, status line, tree, terminal and minimap all change;
  `.theme use default theme` back; a restart keeps the choice.
- [ ] **Project theme** (#59): in a project whose `.goatedit/config.yml` sets `main.theme`, `.theme use <other>` asks. Keep leaves it and
  says so; Remove takes it out and switches; `.theme list` names where the theme comes from.
- [ ] **Themes folder** (#18): after installing, the default theme as before; `.theme --import <a VS Code theme>` lands in
  `~/.config/goatedit/themes/` and works as `main.theme`; one imported earlier still loads.
- [ ] **Dark 2026** (#20): `.theme use vscode-dark-2026` next to VS Code - a C++ file, `ls --color`, `git diff`.
- [ ] **Python colours** (#11, deferred): keywords, names, strings (`f"..."`, `"""`), numbers, comments, decorators; Enter after `:`
  indents 4.
- [ ] **Python errors** (#11, deferred): `python3 x.py` that raises - "build: 1 error", `F8` goes to your own line; a SyntaxError too.

## When something goes wrong

Please keep these, they are enough to work from later:

| What | Where |
|---|---|
| A failed app check | the whole output: `python3 scripts/handtests/checks.py --exe cmake-build-debug/goatedit <check> \| tee check.txt` |
| A failed unit test | `trun --sequential -m <module> -t <case> ./libutests.so` |
| A crash | `coredumpctl list goatedit`, then `coredumpctl info <pid>` |
| A hang | don't kill it first: `gdb -p <pid> -batch -ex "thread apply all bt" > hang.txt` |
| The editor's log | `~/.local/state/goatedit/goatedit.log` |
| Heap corruption suspected | `cmake --preset asan && cmake --build --preset asan`, run `cmake-build-asan/goatedit` |

Which build it is: `git log --oneline -1`. **Not the version on the status line** - CMake reads that hash when it configures, so
a rebuild can show an older one.
