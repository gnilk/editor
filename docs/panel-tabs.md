# Tabs in the lower panel - several terminals, Messages, and more

Written down 2026-11-18. **Idea, not built.** Hitlist #53.

## 1. The idea

The lower panel holds one terminal today (`RootView > HSplitViewStatus[ upper, lower = terminal ]`). Make it a
**tabbed container** instead, as CLion's bottom tool window: a row of tabs, one view shown at a time.

What would live there:
- **Several terminals** - a build shell, a shell in another directory, and one set aside for an AI agent (`claude`,
  hitlist #32), each with its own scrollback and history.
- **Messages** - the build errors / warnings list (`build-messages.md`, hitlist #52) as a view to keep open, not only a
  modal.
- **CMake** (or "Build") - the configure / build output with its own actions, as CLion has.
- Later, other output: a test run, search results (hitlist #30 workspace search), the log.

## 2. A generic container first

`TabbedView` (name open) in the UI toolkit, `src/Core/UI/Views/` - it knows nothing about terminals:
- holds N child views, shows one; a tab row of labels on top (one row - the editor's header row is the model);
- actions: next / previous tab, go to tab N, new tab (what kind - asked of the owner), close tab;
- the mouse: click a tab to show it; a close button per tab (the button widget, hitlist #26, and its hover plumbing -
  the same as the per-file close button, #27);
- a tab label can change (a terminal's title from OSC 0/2 - `OnTitle` exists; a `*` or a count, e.g. `Messages (3)`).

The editor-specific part (which tabs exist, what "new" makes) goes in `src/Core/Editor/`, as the split views'
`HSplitViewStatus` does today. Possibly the same container for the editor's own files later (the header row already
lists them) - not a goal, but worth not designing out.

## 3. What assumes ONE terminal today

These have to learn "the active terminal" or "a named one":
- `Editor::glbTerminalView` + `GetTopViewByName` - `GetTerminalView()` in `Editor.cpp`, used by
  `PromoteToTerminal` (#47) and session save.
- `kActionSwitchToTerminal` (`Option/Alt+T`) - which terminal: the last used one, probably.
- `RuntimeConfig::OutputConsole()` - one `IOutputConsole`; `TerminalAPI` (JS) reads the last output through it.
- Session persistence of the scrollback and command history (`terminal-scrollback.md` §8) - one file each; needs one
  per terminal, and the set of tabs saved and restored.
- The shell: one `Shell` per `TerminalController`, so several terminals are several ptys and reader threads - that
  part already holds, each `TerminalView` owns its controller.
- Build messages (#52) parse "the last output" - of which terminal: the one the command ran in, so each terminal's
  messages feed the one Messages list, marked with where they came from.

## 4. The AI terminal

One terminal set aside for an agent: started with `claude` (or a configured command) instead of a plain shell, so
`PromoteToTerminal` (#47) sends there - not to the build shell - when it exists. Config, e.g.
`terminal.tabs: [ { name: Shell }, { name: Claude, command: claude } ]`. Ties in with #32 (`claude` running well in gvt)
and the architecture-workbench idea (index data handed to agents).

## 5. Steps

1. `TabbedView` in the toolkit with plain test views; tab row, switching, keys; unit tests for layout and switching.
2. The lower panel becomes a `TabbedView` with the one terminal in it - nothing else changes; the "one terminal"
   places (§3) go through "the active terminal".
3. More terminals: new / close, per-terminal session files, tab titles from OSC 0/2.
4. Configured tabs (§4 - the AI terminal, the `PromoteToTerminal` target).
5. Messages as a tab (with #52); a CMake / build tab.
6. Mouse: click a tab, close buttons (after #26).

## 6. Open questions

1. Keys: tab next / previous / N are UI control - `Option/Alt` (`UINavigationModifier`); which letters / digits are free
   on both platforms?
2. Does `Option/Alt+T` go to the last used terminal, or cycle through them?
3. Tabs configured in the config (fixed set), made at run-time (new / close), or both?
4. Which terminal does `PromoteToTerminal` send to - the AI one when configured, the last used, or ask?
5. An ANSI-backend limit: the tab row costs one row of a small terminal - hide it when there is only one tab?
