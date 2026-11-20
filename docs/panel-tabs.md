# Tabs in the lower panel - several terminals, Messages, and more

Written down 2026-11-18. Hitlist #53. **Steps 1-3 built 2026-11-19** (`feature/panel-tabs`, `feature/terminal-tabs`): `TabbedView` in the toolkit, the lower panel is one, several terminals in it.

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
   **Built:** `src/Core/UI/Views/TabbedView` - add / remove / next / previous / select, a click on a label, a
   tab-changed handler; the top view for its place (focus, keys, actions, status go to the shown tab, focus moves with
   a switch so the tab's keymap installs, as `EditorViewContainer` does). Label row as the editor's header (active
   underlined, ` | `). Keys come with step 3 (they select tabs only once there are several). Tests `layout -t tabs*`.
2. The lower panel becomes a `TabbedView` with the one terminal in it - nothing else changes; the "one terminal"
   places (§3) go through "the active terminal". **Built:** `main.cpp`; the container is the `glbTerminalView` top
   view (`Option/Alt+T`, focus saved in the session by that name); `GetTerminalView()` (`Editor.cpp` - promote,
   scrollback save) asks it for its shown tab when that is a terminal, else its first terminal. The rest of §3 holds
   with one terminal and changes in step 3.
3. More terminals: new / close, per-terminal session files, tab titles from OSC 0/2. **Built:**
   `src/Core/Editor/Views/TerminalPanelView` (a `TabbedView` that owns its `TerminalView`s; `PanelView` since step 4) - `NewTerminal` /
   `CloseTerminal` actions in `terminal_keymap.yml` (asks when `HasRunningJob`, the last one stays, a closed tab's
   scrollback and history files go); `SwitchToBuffer` / `CycleActiveBuffer*` pick tabs while the panel has focus and
   two or more tabs (taken before the terminal, so an app in it doesn't get them). Session `terminal.tabs` (id, name,
   scrollbackFile) + `activeTab`, additive - a session without it is one tab with `scrollbackFile`; the controller's
   files by tab id (`SetSessionTab`: `terminal_scrollback[.id].bin`, `terminal_history[.id]`, id "" the old names).
   Labels: OSC 0/2 title (`TerminalController::OnTitle`, posted to the UI thread) cut to 24, else 'Shell N'. The
   focused terminal is the output console (dot-commands, JS). Build runs belong to their terminal
   (`BuildMessages::OnCommand(..., terminal)`). Check `terminal_tabs`. Not done: renaming the actions to
   `SwitchToTab` / `CycleTab*` (they work as they are named today).
4. Configured tabs (§4 - the AI terminal, the `PromoteToTerminal` target) - **designed 2026-11-19, §7**. In order:
   a. `inherit:` for config sections (generic, in `Config`; resolved once, leaf wins, multi-level, cycle-guarded - as
      `KeyMappingCache`).
   b. The terminal is handed its resolved settings at creation (`TerminalView` / `TerminalController` / `Shell` /
      `TerminalScreen` / session code read that node, never `Config::Instance()["terminal"]`).
   c. `ProjectConfigPolicy` strips the command keys (`shell`, `init`, `bootstrap`) from every section of a project
      config, not only `terminal`.
   d. `TerminalPanelView` -> `PanelView`; ownership of the tab views moves into `TabbedView` (generic, UI toolkit); the
      panel builds its tabs from `panel.tabs` when there is no session; the session keeps each tab's `view` / `config`.
   e. `promote_target` - `PromoteToTerminal` sends to that tab when one exists.
   **Built 2026-11-20:** a. `ConfigNode::ResolveSection` (a copy; maps merge key by key, a list or scalar is the
   leaf's, a loop or missing parent logged and ends the chain). b. `TerminalController::SetSettings` (shell, init,
   bootstrap, alt_arrows, shell_integration, persistence, promote_sends_enter), `TerminalView` (keymap, wheel),
   `TerminalScreen::SetScrollbackCap` (the pty thread used to read the config tree to trim); without settings the
   code's defaults. c. `ProjectConfigPolicy`: shell / init / bootstrap stripped at any depth in any section, lists of
   maps included (`panel.tabs`); a non-map section dropped. d. `TabbedView::AddTab(unique_ptr)` owns, `RemoveTab`
   deletes (after the tab is out of the layout - losing the focus lays the tabs out); `PanelView` keeps a `TabSpec`
   per tab (id, name, view, config); tabs from the session, else `panel.tabs` (`PanelView::ConfiguredTabs`; a view
   kind it doesn't have is left out), else one Shell; the session's `tabs` carry `view` / `config`. e.
   `PanelView::PromoteTarget` - the first tab whose settings say `promote_target: yes`, shown; else the shown terminal.
   Also: a theme switch refreshes every tab's terminal, not only the shown one. Check `agent_tab`. A bootstrap's
   output is not shown (it runs before the prompt) - the check sees what it ran by the files it touches.
5. Messages as a tab (with #52); a CMake / build tab.
   **Built 2026-11-20 (Messages):** `src/Core/Editor/Views/MessagesView` - `view: messages` in `panel.tabs` (name
   'Messages', section 'messages'). The last build's errors then warnings (`BuildMessages::Order`, rows as the modal's -
   `BuildMessages::RowText`), a caption line with the command and its counts ('(running)' while it runs). Read when
   drawn - it fills while the build runs; the selection follows F8 / Shift+F8 (`BuildMessages::Current`), a new run
   (`RunNumber`) starts at the top. Up / Down / Page / Home / End, Enter or a double-click goes there, F8 goes on from
   it. The label counts: 'Messages (3)'. `Cmd/Ctrl+F8` shows the tab, focused, when there is one, else the modal. Its
   keymap `messages_keymap` (default_keymap + the panel's new / close tab keys - default_keymap's close would have
   closed the editor's file). A Messages tab closes like any tab; the last terminal stays, and a panel always has one
   (a `panel.tabs` with only Messages gets a Shell too). Check `messages_tab`. Not done: the CMake / build tab, the
   filter the modal has (typing in the tab).
6. Mouse: click a tab, close buttons (after #26).

## 6. Decisions (2026-11-18)

- **"The one terminal" becomes "the one container".** Everything in §3 that names the terminal view names the lower
  panel's container instead; `Option/Alt+T` goes to the container, which shows the tab that was active last time
  (also across a restart - the active tab is saved with the session).
- **The existing `SwitchToBuffer` keys select tab N in the container that has focus.** Today they are bound to the
  editor's files only and handled globally (`Editor.cpp`, `kActionSwitchToBuffer` -> `ActionHelper::SwitchToBufferNumber`):
  Mac `Cmd+1..9` (the macOS standard) and `Opt+1..9`, Linux `Alt+1..9` (the Linux standard). They become the focused
  container's: the lower panel's tabs when it has focus, the editor's files otherwise (9 = the last, as now). On Linux
  `Alt+digit` must stay the editor's in a terminal tab, not go to the shell (readline uses `Alt+digit` as a count - it
  loses that, as it does for `Alt+T` today).
- **Default tabs come from the config** (e.g. `terminal.tabs: [ { name: Shell }, { name: Claude, command: claude } ]`);
  new / close at run-time on top of that.
- **`PromoteToTerminal` sends to the last used terminal** - for a start; an "AI terminal" target can come later.
- **The tab row is hidden when there is only one tab** - the panel looks as it does today until a second tab exists.

- **Next / previous tab follow the focused container too:** `CycleBufferNext` / `CycleBufferPrev` (`Opt/Alt+\` /
  `Opt/Alt+'`) - renamed for what they then do, e.g. `CycleTabNext` / `CycleTabPrev` (and `SwitchToBuffer` ->
  `SwitchToTab`), the old names kept as aliases in `KeyMapping` so a user keymap still loads.

Decided 2026-11-19 (step 3):
- **New / close tab keys - the terminal standard**, in `terminal_keymap.yml` (only while a terminal has focus): Mac
  `Cmd+T` / `Cmd+W` (`Cmd+T` stays Go to symbol elsewhere), Linux `Ctrl+Shift+T` / `Ctrl+Shift+W` (GNOME Terminal,
  Konsole - `Ctrl+T` / `Ctrl+W` are readline's).
- **A restart brings back what was open** - the tabs, their names, the shown one; each tab its own scrollback and
  history. The config's tabs (step 4) only when there is no session yet, so a configured tab that was closed stays
  closed.
- **A build run belongs to the terminal that started it** (#52): only its bytes feed the run, and only its next
  command ends it - another terminal's output (an agent's) or its `ls` leaves the run alone.

## 7. Configured tabs - decided 2026-11-19

```yaml
panel:
  tabs:
    - name: Shell
      view: terminal          # what draws it: terminal (step 4) | messages (step 5) | ...
                              # config: defaults to the view's name - the 'terminal' section
    - name: Claude
      view: terminal
      config: ai_agent        # the section the tab is handed (resolved)

ai_agent:
  inherit: terminal           # what it doesn't set comes from 'terminal'
  bootstrap: [ "claude" ]     # run in the shell when it starts
  promote_target: yes         # Cmd/Ctrl+Enter sends here
```

- **One terminal view for any long-lived console application** (a shell, an agent in a shell). What differs is
  configuration, not view types - no `TerminalShell` / `TerminalAgent` classes.
- **`view` and `config` are separate:** what draws a tab, and the settings it is handed. Two agents (Claude, Codex) are
  the same view with two sections; a Messages / CMake view can get sections of its own later.
- **Explicit `inherit:`** in config sections, as keymaps and language files have it: the leaf's keys win, the parent
  fills the rest, several levels deep (`codex: { inherit: ai_agent }`). A section without `inherit:` stands alone (the
  code's defaults fill the gaps).
- **Settings are handed over at creation**, resolved - the view never looks up a global section by name.
- **An agent is a shell with `bootstrap`**, not the tab's process: the terminal's key routing asks "is a job running in
  the shell's foreground?" (an agent as the process would be edited as a prompt line), the shell's rc files give the
  agent its `PATH` (npm / nvm / brew installs), and when the agent quits the tab is a prompt again, not dead. Not
  `exec claude` either, for the same reason.
- **Split later:** a terminal base (screen, pty, drawing) and a shell part (local line edit, prompt, build messages)
  when the first tab whose process is not a shell comes (a REPL, `ssh` as the process).
- **Security:** a project's `.goatedit/config.yml` can be committed to a repo - it never sets a command, in any section
  (`shell`, `init`, `bootstrap` stripped everywhere). Keys and secrets live in the user config (never touched by build
  or install) or the environment; agents keep their own credentials. A separate secrets file, if one is ever needed:
  installed once as a skeleton, never overwritten, never merged.
- **`PanelView`:** the generic part is `TabbedView` (UI toolkit) - the container with the tab row; it takes ownership of
  its tab views. `PanelView` (editor, today's `TerminalPanelView` renamed) is the editor's part: which tab kinds exist,
  building them from `panel.tabs` and the session, asking before closing a tab with a running job.
- **Restart:** the session wins (as decided for step 3); `panel.tabs` is used when there is no session yet. The session
  keeps each tab's `view` and `config`, so a Claude tab comes back as an agent tab (its `bootstrap` starts the agent
  again - deliberate).

