# Modal dialogs - state, defects and a plan

Status: **in progress.** Proposal 2026-10-22; steps 1-3 (host, confirm, `.cb` / `.q`) done 2026-10-23, merged,
GUI-verified on SDL3 (macOS); SDL2 (Linux) not yet. Step 4 (Find) implemented on `feature/find-modal`,
waiting for the GUI check. Decisions are in §4 (D2 for confirms only).

Modals are wanted for several things already on the list:
- "changed on disk - reload?" (deferred in `foldermonitor_fixes.md` §4.4);
- closing or exiting with unsaved changes (today `.cb` refuses with a console message, `.q` doesn't check
  at all);
- **Find** (Cmd+F): a normal, modal way to search the active file - today search only exists inside
  quick-command mode - and later **search everywhere**;
- pickers: a buffer list (`listbuffers.js` only prints), files, themes, languages;
- plugin prompts from JS;
- features the author wants to explore (TBD).

---

## §1 — What exists today

| Piece | Where | What it does |
|---|---|---|
| `ModalView` | `Core/UI/Modals/ModalView.{h,cpp}` | A `ViewBase` with its own window: centred, half the screen, border + caption ("ModalView"). Optionally wraps one view (`viewPtr`), forwards actions to it or a `dispatchView`. Esc (as a raw keypress) or `CycleActiveView` closes it (`CloseModal()` = `SetActive(false)`). |
| `ListSelectionModal` + `ListView` | `Core/UI/Modals/ListSelectionModal.{h,cpp}` | A list of strings with up/down/page navigation and an optional header line. |
| `TreeSelectionModal<T>` | `Core/UI/Modals/TreeSelectionModal.h` | A `TreeView<T>` inside a `ModalView`. |
| `Runloop::ShowModal(modal)` | `Core/Runloop.cpp:162` | A **nested** run loop: snapshots the screen, pushes the modal on `kpaHandlers`, then loops `ProcessMessageQueue()` + redraw (snapshot + modal) until the modal goes inactive. Blocks the caller. |
| `ViewBase::modal` / `RootView` routing | `ViewBase.h:429`, `RootView.h:127-181` | If `modal` is set: `Draw` draws only the modal; `RootView::OnAction` / `HandleKeyPress` / `DispatchMouse` send everything to it (mouse hit-test restricted to it). |
| Screen snapshot | `ScreenBase::CopyToTexture` / `ClearWithTexture` (SDL2, SDL3, gansi) | Save the frame, restore it as the modal's background. |
| `kUIActionCloseModal` | `UIAction.h`, `KeyMapping.cpp:92` | Exists; the keymap binding is commented out ("doesn't work unless we are in modal-mode"). |

**Nothing uses any of it.** `ShowModal` has no caller, and nothing ever assigns `ViewBase::modal`. The two
halves (nested loop vs. RootView routing) are separate attempts that never met.

The closest working precedent is **quick-command mode**: an Editor state (`QuickCommandState`) with its
own keymap, drawn by the normal loop as an overlay (`UIHost::SetQuickCmdView` /
`SetQuickCommandActive`), non-blocking. That is the shape the modal should take.

---

## §2 — Defects in the skeleton

| # | Defect | Where | Effect |
|---|---|---|---|
| M1 | Two disconnected mechanisms; `ViewBase::modal` is never set, `ShowModal` never called. | `Runloop.cpp`, `RootView.h` | Nothing to build on as-is. |
| M2 | **The nested loop re-enters `ProcessMessageQueue`.** Every caller (an action handler, a JS command, a posted message) already runs *inside* `ProcessMessageQueue` - keypresses arrive as posted messages. The inner loop calls `SwapQueues()` while the outer one is still draining `processingQueue`. | `Runloop::ShowModal` | The outer loop's remaining messages and the new ones get mixed / reordered; the same class of bug as the old `SwapQueues` race (fixed-bugs #6), now by re-entrancy instead of threads. |
| M3 | **The world keeps moving under a blocked caller.** While the modal is up, the inner loop still runs autosave, folder-monitor flushes, terminal output, plugin messages. | same | The caller resumes with stale assumptions (e.g. asked "save X?" and X was closed meanwhile). A modal opened from inside such a message nests again. |
| M4 | **The background snapshot is blank.** SDL2/SDL3 `CopyToTexture` uploads `screenAsSurface->pixels`, but nothing ever renders into that surface. A resize while the modal is up isn't handled either. | `SDLScreen.cpp` (both) | The modal would sit on a black screen. |
| M5 | **Esc can't close it.** `ModalView::OnKeyPress` checks for a raw Esc, but Esc is bound to `EnterCommandMode` in the default keymap, so it arrives as an *action*. `CloseModal` isn't bound. Closing on `CycleActiveView` is accidental. | `ModalView.cpp`, keymaps | No reliable way out. |
| M6 | **Mouse goes to the views underneath** while `ShowModal` runs (it doesn't set `RootView::modal`). | `RootView::DispatchMouse` | Clicks act behind the dialog. |
| M7 | **No result.** `ModalView` has no concept of "closed with X / cancelled", no callback. `ListSelectionModal::idxSelectedItem` is never set, so `GetSelectedItemIndex()` is always empty; Enter closes without recording anything. | `ListSelectionModal.cpp` | A picker that can't report the pick. |
| M8 | **Ownership.** `ListView`, `VStackView`, `ListHeaderView` are raw `new`, never freed; `listHeader` is a file-static shared by every instance; `viewPtr` isn't owned. `GetSelectedItem()` on an empty list indexes out of bounds. | `ListSelectionModal.*`, `ModalView.h` | Leaks; UB; violates the RAII rule. |
| M9 | Hard-coded look: caption "ModalView"/"Tree", header text "Header", fixed size (½ screen), default colours (not theme-aware). | `ModalView.cpp`, `ListSelectionModal.cpp` | — |
| M10 | `ShowModal` sets `isRunning = true` and re-installs the keymap delegate; it never restores anything. Keymap is the editor's (no modal keymap). | `Runloop.cpp` | Minor; goes away with the rewrite. |

---

## §3 — Proposed architecture

> **A modal is an overlay owned by the RootView, not a second run loop.** Showing one is
> non-blocking: the caller passes a completion callback, and the normal loop draws and routes input to
> the modal until it closes. The result comes back through the callback on the main thread.

```
 caller (action / JS / posted msg)          RootView (main loop, unchanged)
   ModalHost::Show(modal, onClose) ───────► modal stack: [ ... , modal ]
   returns immediately                       Draw:   tree, then the top modal on top
                                             Input:  keys/actions/mouse -> top modal only
                                             keymap: 'modal_keymap' while a modal is up
   onClose(result)  ◄──── modal->Close(r)    pop (deferred to after the current dispatch),
                                             restore keymap, callback once, then destroy
```

### 3.1 Host and lifetime

- **`RootView` keeps a modal stack** (`std::vector<std::unique_ptr<ModalView>>`), replacing the unused
  `ViewBase::modal` pointer. The existing routing in `RootView::OnAction` / `HandleKeyPress` /
  `DispatchMouse` becomes "top of the stack". A stack, not one slot, so a modal can open another (a
  picker → a confirm).
- **`Show(std::unique_ptr<ModalView>, OnClose)`** takes ownership. `ModalView::Close(result)` only
  *marks* it closed. The host pops it after the current dispatch returns, so a modal never deletes
  itself from inside its own `OnAction`. It then restores the keymap and calls `onClose` exactly once.
- **Result type:** a small `ModalResult { bool isCancelled; int choice; std::string text; }` (or per-modal
  typed callbacks - §4 D4).
- **Delete `Runloop::ShowModal`** and the snapshot calls. The main loop redraws the whole tree anyway; the
  modal is drawn after it. (`CopyToTexture` / `ClearWithTexture` can go, or stay unused.)
- Follows the layering rule: `ModalView` / host live in `Core/UI` (toolkit). Editor policy (which modal,
  what to do with the answer) stays in `Core/Editor` / the JS layer.

### 3.2 Input

- **A `modal_keymap`** (macOS + Linux) pushed while a modal is up, restored on close, like quick-command
  mode does. Esc → `CloseModal` (cancel), Enter → `CommitLine`, arrows / page keys → navigation. It
  doesn't inherit `default_keymap`, so editor actions (Cmd+N, Alt+t, `.` commands) can't fire behind a
  dialog.
- **Choice hotkeys:** a confirm's choices carry a letter (`[S]ave  [D]iscard  [C]ancel`); a plain letter
  picks it. Arrows + Enter also work.
- **Mouse:** already restricted to the modal subtree by `DispatchMouse` once the stack is wired. Click
  outside = ignored (not cancel).

### 3.3 Look

- Centred box, sized from its content (clamped to the screen), with border and caption. It's re-laid out
  on resize (`ReInitView`).
- Colours from the theme: new keys (`modal_foreground/background`, `modal_caption_*`,
  `modal_selection_*`). The theme doesn't carry these today; the same change can add the strikethrough
  colour noted in `foldermonitor_fixes.md` ("Later").
- The gansi backend needs nothing special: it's the same overlay drawn into the cell grid.

### 3.4 Building blocks (in order of need)

1. **`ConfirmModal`** - a message (one or more lines) + 2-4 choices with hotkeys. Covers save/discard,
   reload/keep, yes/no.
2. **`FindModal`** - an edit line on top (reuse the quick-command edit-line controller) and a result list
   below (the reworked `ListView`). Each edit re-runs the search over the active document
   (`Document::SearchFor`) and lists the hits as `line: text`. Up/Down pick a hit and scroll the editor to
   it (preview); Enter closes and leaves the cursor there, with the search results kept, so
   `NextSearchResult` / `PrevSearchResult` (Cmd+n / Cmd+p) keep working; Esc closes and puts the cursor
   back. Opened by Cmd+F (macOS) / Ctrl+F (Linux, `CopyPasteModifier`); the field starts with the
   selection, if any. "Search everywhere" later = the same modal with a result source over all workspace
   files (a background search with the `workspace.exclude` filter, capped results) - its own design note.
3. **`ListSelectionModal`** (reworked, shares the list with 2) - a picker (buffers, languages, themes).
4. **`TextInputModal`** - the edit line alone ("save as", rename, plugin prompts).
5. `TreeSelectionModal` - later (file picker).

### 3.5 Testability

The host is plain logic over views: a test can show a modal, feed `EditorAction`s through
`RootView::OnAction`, and assert the callback fired once with the right result, that input didn't reach
the views underneath, and that closing from inside `OnAction` is safe. That needs views without a real
screen - the headless backend (`Core/Graphics/Headless/HLScreen`) is the candidate; check how far it
gets before step 1.

---

## §4 — Decisions needed

| # | Question | Recommendation |
|---|---|---|
| D1 | **Nested loop (blocking `ShowModal`, caller gets the answer as a return value) vs overlay + callback (non-blocking).** | **Decided (step 1): overlay + callback.** The nested loop re-enters the message queue (M2) and lets the world change under a blocked caller (M3). The callback costs a lambda at each call site; JS gets the same shape (`Editor.Confirm(text, choices, function(choice) {...})`). |
| D2 | Look: centred box with border/caption, or a bar like quick-command (VSCode-style palette at the top)? | **Decided: centred box.** Confirms sized from their content (step 2). Find (2026-10-23): a box with the input line and a result list, anchored in the upper part of the screen; the editor centres the previewed hit so it lands below the box. A top bar was rejected - it would duplicate quick-command `/` search and can't grow into "search everywhere". |
| D3 | Esc in a modal = cancel (via `modal_keymap`), even though Esc is "command mode" elsewhere? | **Decided: yes.** There is no command mode inside a modal. |
| D4 | One `ModalResult` struct for all modals, or a typed callback per modal kind (`OnChoice(int)`, `OnPicked(index)`, `OnText(string)`)? | **Decided (step 1): typed callbacks.** The host knows only closed / cancelled (`ModalView::OnClosed(bool wasCancelled)`); each kind overrides it and calls its own typed callback. |
| D5 | First consumer to drive the work. | **Decided: the confirm.** `.cb` on a modified buffer and `.q` with unsaved buffers. |
| D6 | Your features that need modals - which building block do they need first? | **Decided: Find.** Cmd+F opens a search box over the active file and lists the hits as you type; "search everywhere" (all files) builds on it later. That needs a text input + result list (§3.4 items 2 and 3, combined). |

---

## §5 — Steps (each its own commit, tests first where possible)

| # | Step | Test |
|---|---|---|
| 1 ✅ | **Host:** modal stack on `RootView` (replaces `ViewBase::modal`), `Show(modal, onClose)`, deferred close, input routing (actions, keys, mouse), drawing after the tree, `modal_keymap` push/pop. Delete `Runloop::ShowModal`. | Unit: actions reach only the top modal; the callback fires once; closing inside `OnAction` is safe; a nested modal closes back to its parent; the keymap is restored. |
| 2 ✅ | **`ConfirmModal`** (message + choices with hotkeys) + theme keys for the modal colours (and the strikethrough colour). | Unit: hotkey → choice; Enter → selected choice; Esc → cancelled. GUI: look on SDL3 (and SDL2 on Linux). |
| 3 ✅ | **First consumer: `.cb` on a modified buffer** → Save / Discard / Cancel; `.q` with unsaved buffers → Save all / Discard / Cancel. Needs a JS binding (`Editor.Confirm`). | Unit for the JS wrapper; GUI for the flow. |
| 4 ✅ | **`FindModal`** (§3.4 item 2): the list part of the `ListSelectionModal` rework (M7, M8), the edit line, live search over the active document, preview / commit / cancel, Cmd+F / Ctrl+F. Needs read access to the document's hits (positions + line text). | Unit: typing narrows the hits; Up/Down moves the preview; Enter keeps the cursor + results; Esc restores the cursor; empty pattern / no hits. |
| 5 | **Search everywhere** - design note started: [`code-index.md`](code-index.md) (structural code index + Go to symbol first, plain text search after). Original note: design note first (result source over unopened files, threading, excludes, limits, opening a hit). Revisit the **code index** idea with it: `docs/done/session-cache.md` §6 sketches a SQLite/FTS5 index (`files`, `symbols(name, kind, line, col)`) fed by a background indexer from the tokenizer. Wanted on top (2026-10-23): **context-aware** search - classes / structs / members / functions - which needs more than the line tokenizer's token classes (a structural pass per language). Basics (Find) first. | — |
| 6 | Pickers / `TextInputModal` as needed (buffer picker, `Editor.Prompt`). | Unit: navigation + pick + cancel; empty list. |
| 7 | **Changed on disk → reload?** The folder monitor reconciles folders, not file contents: on a flush, compare each open document's mtime/size in a dirty folder with what was loaded, and ask `Reload / Keep mine` (a buffer with no edits could just reload - §4 decision then). Cross-ref `foldermonitor_fixes.md` §2.2 "hook". | Unit: a changed open file in a flushed folder triggers the callback; an unchanged one doesn't. |
| 8 | Clean up: `TreeSelectionModal` onto the new host, remove `CopyToTexture` / `ClearWithTexture` if unused. | Green set. |

---

## §6 — Done

### Step 1 — the host (2026-10-23)

- **`RootView`** owns `std::vector<ModalView::Ptr> modals` (the back gets input). `ShowModal(Ptr)` takes
  ownership, sets the root as the modal's *overlay parent* (`ViewBase::SetOverlayParent`: invalidation
  and unhandled keys bubble to the root, but the modal is not a subview - not drawn, hit-tested or
  resized with the tree), initializes and activates it (the modal below is deactivated).
- **Routing:** `OnAction` (handled or not, returns true - nothing passes to the tree or the global
  handler), `HandleKeyPress` and `DispatchMouse` (hit-test inside the top modal, bubbling stops at the
  modal, a click outside is swallowed) go to the top modal only.
- **Deferred close:** `ModalView::Close()` / `Cancel()` only mark it (and invalidate). `ReapClosedModals`
  runs after each dispatch and at the start of `RootView::Draw` (a close from a posted message): it
  pops, reactivates the new top, then calls `OnClosed` once and destroys it. A callback may show the
  next modal. Esc = `CloseModal` = cancel.
- **Draw:** the tree, then the modals bottom to top. `RootView::ReInitView` re-centres them (`Resize`).
  The look is still the old one (half the screen, centred, a cleared content area) - step 2.
- **Keymap:** `UIHost::SetOnModalActive` (empty <-> non-empty stack) → `Editor::SetModalActive`.
  While a modal is up `GetActiveKeyMap` returns `modal_keymap` (no `inherit` - Esc, Return, arrows,
  page, home/end only); a `SetActiveKeyMapping` underneath is recorded but held back, and applies when
  the last modal closes. `modal_keymap` registered in `config.yml` (`macos:` / `linux:`).
- **Runloop:** `ShowModal` (nested loop) deleted. `ActiveHandler()` routes keys/actions to the root
  while a modal is up, even when quick-command mode has its hook pushed - a modal opened from a `.`
  command gets the input; quick-command mode is still there underneath when it closes.
- `ViewBase::modal` and `ViewBase::CloseModal()` deleted (`ListSelectionModal` uses `Close()`).
- Tests: `utests/test_modal.cpp` (module `modal`) - routing, close in action, cancel, nested, show from
  close, close outside a dispatch, keymap (incl. the held-back change and what the run loop is handed).
  Mutation-checked: keys falling through, unhandled actions falling through, no reap in `Draw`, the
  parent not reactivated, and no keymap hold-back each fail a test.

GUI-verified together with steps 2 + 3 (SDL3).

### Steps 2 + 3 — the confirm and its first consumers (2026-10-23)

- **`ConfirmModal`** (`Core/UI/Modals/ConfirmModal.{h,cpp}`, toolkit): a caption, a message (`\n` = more
  lines) and choices. A choice's hotkey is its first letter, drawn `[S]ave`; a letter picks it (either
  case; shift is fine, ctrl/alt/cmd aren't). Left/Right move the selection (inverted), Return picks it,
  Esc cancels. Typed callbacks: `SetOnChoice(int)`, `SetOnCancel()`.
- **Sizing:** `ModalView::ComputeRect(screenRect)` is virtual (default: half the screen, centred) with a
  `CenterIn(screen, w, h)` helper that clamps to the screen. The confirm sizes itself from its text:
  caption, blank, message lines, blank, choices, blank; 2 columns of margin.
- **Colours:** `ui.modal_background` / `ui.modal_foreground` in `colors.json` (fallback: `ui.background` /
  `ui.foreground`). The border and caption are still the window decoration's (white line, inverted row).
  The strikethrough colour is not part of this.
- **JS:** `Editor.Confirm(message, choices, function(choice) {...})` - returns at once; the function is
  called when the dialog closes, with the choice's index or -1 when cancelled. The `DukValue` keeps the
  function alive (heap stash) until then; the call is a `dukglue_pcall`, a script error is logged. Glue:
  `EditorAPI::Confirm` (utf8 -> u32, builds the modal, `RootView::ShowModal`); without a root view it
  cancels right away.
- **`.cb`** on a modified buffer: Save (then close; a failed save keeps it open and says so) / Discard /
  Cancel. `.cb !` still closes at once.
- **`.q`** with modified buffers: lists them, Save all (then exit; stops at the first failed save) /
  Discard / Cancel. New: `.q !` exits at once.
- Tests: `modal` - confirmhotkey, confirmselect, confirmcancel, confirmsize; `jsengine` - closecmd
  (rewritten: Esc keeps the buffer, Discard closes and drops the node, Save writes then closes, `!` and
  unmodified close without asking), quitcmd (asks, Cancel keeps running). Mutation-checked: the hotkey
  modifier rule.
- `Runloop::IsQuitRequested()` added (for the test).

### Step 4 — Find (2026-10-23)

- **`QueryListModal`** (`Core/UI/Modals/`, toolkit): an edit line (`Find: ...`, a hit counter `3/17` /
  `no hits` on the right) and a result list below it. Every edit runs the query again and selects the
  first row. Up/Down/PageUp/PageDown move the selection (clamped, the list scrolls), each move is a
  *preview*; Return commits the selected row (does nothing without one); Esc cancels.
  Left/Right/Home/End/Backspace/Delete edit the line (`BaseController::DefaultEditLine`); typing with
  ctrl/alt/cmd is not text. Virtual hooks: `OnQuery(query) -> rows`, `OnPreview(idx | -1)`,
  `OnCommit(idx)`, `OnCancel()` - "search everywhere" is another subclass. Placed in the upper part of
  the screen: 60% wide, 35% high, 5% from the top.
- **`FindModal`** (`Core/Editor/Modals/`): case-insensitive plain text over the active document
  (`Document::SearchFor(item, isCaseSensitive)` - the quick-command `/` search stays case-sensitive;
  still one hit per line, the first). Rows `  12: text` (numbers right-aligned, indent dropped, tabs as
  spaces). Preview = jump to the hit + `Document::ScrollActiveLineIntoLowerPart` (unless it is already in
  the lower half of the view, it goes two thirds down - below the dialog). No hits = the cursor goes back
  to where it was. Commit = cursor on the hit, the selection is dropped, the results stay (highlighted,
  `Cmd+n`/`Cmd+p` continue from that hit), the editor view gets the focus. Cancel = results cleared,
  cursor *and* view top restored exactly (`Document::RestoreCursorAndViewTop`). Starts with the
  selection when it is on one line.
- **Binding:** action `Find` (`kActionFind`, global in view state) → `ActionHelper::ShowFind()`;
  `CopyPasteModifier + 'f'` in both default keymaps = Cmd+F (macOS) / Ctrl+F (Linux). Workspace and
  terminal keymaps inherit it.
- Tests: new module `find` (caseinsensitive, narrows, preview, commit, cancel, nohits, scroll,
  selection) + `keymapping` find. Mutation-checked: no lower-part scroll, view top not restored, the case
  flag ignored.
- Not done here: the `ListSelectionModal` rework (M7, M8) - the list lives in `QueryListModal`; the
  picker (step 6) can reuse it or the rework happens there.
- **Paste (follow-up):** `PasteFromClipboard: CopyPasteModifier + 'v'` in `modal_keymap` (which got its
  own `modifiers:` - aliases are per keymap). The clipboard is an editor service, so the toolkit
  `QueryListModal` only offers `InsertText` (at the field cursor, up to the first line break, query run
  again); `FindModal::OnAction` takes the editor action, refreshes from the OS pasteboard
  (`UpdateClipboardData`, as `Document::PasteFromClipboard`) and inserts `Top()->AsText()`. Tests:
  `find` paste (clipboard filled via `CopyFromExternal`, E.18) + `keymapping` find; mutation-checked:
  no cut at the line break, appended instead of at the cursor.
- Known: on Linux the terminal keymap inherits Ctrl+F, so it opens Find instead of reaching the shell
  (readline forward-char) - the same as the other CopyPasteModifier bindings there.

