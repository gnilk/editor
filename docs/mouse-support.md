# SIMPLE mouse-support in the editor

Looking at wiring in mouse-support in the graphics backend to solve some basic stuff.

* Clicking in a view set's it active
* Scroll-wheel scrolls the view/cursor
* Click in the editor view repositions the cursor
* Click in the workspace view repositions the cursor


There are obvious several different ways to do this. Today there is NO routing of mouse-related 
actions at all (no functions, not enum's etc...) - so this is where we can do architectural stuff..

Two options (top of my head):
1) Classic, route mouse-messages directly to the view-hierarchy and let each view figure out what to do
2) Use 'current' kUIAction and have a translation-layer (with view-support functions where needed)

Important is that src/Core/Graphics base-classes need to be enhanced.
The two main backends (src/Core/Graphics/SDL2 and SDL3) must be enhanced to trap mouse actions and 
handle them properly - here I would imagine we translate them and toss them to the RunLoop for further
processing and routing..

---

# Analysis (cold-start notes for implementation)

## How input flows today (keyboard)

The existing keyboard path is the template to mirror:

1. `SDLScreen::PollEvents()` (the **single event pump**, on the main thread) drains the SDL queue —
   `SDL2/SDLScreen.cpp` and `SDL3/SDLScreen.cpp`. Mouse events currently fall through `default:` and
   are dropped.
2. Keyboard events → `SDLKeyboardDriver::ProcessEvent` → a backend-agnostic `KeyPress`
   (`Core/KeyPress.h`).
3. The `KeyPress` is **marshalled onto the main thread via the Runloop message queue**:
   `Runloop::PostMessage(0, [kp]{ Runloop::ProcessKeyPress(kp); })`. (It's already on the main thread,
   but going through the queue keeps a single ordering for input + cross-thread messages.)
4. `Runloop::ProcessKeyPress` → `Runloop::DispatchToHandler` (`Core/Runloop.cpp:183`): resolves the
   keypress to an `EditorAction` via the active keymap, then sends it to the **top
   `KeypressAndActionHandler`** on `kpaHandlers` (RootView, or a modal). RootView forwards to its
   current *top view*.

**Key insight:** keyboard dispatch is **focus-routed** (goes to whoever is active) and the action
vocabulary is **coordinate-free**. Mouse is the opposite — it is **spatially-routed** (goes to whatever
is *under the pointer*, which may not be the focused view) and **carries an (x,y)**. This mismatch is
the whole architectural question; see Options below.

## Pieces that already exist and we can reuse

- **Pixel → row/col translation:** `SDLTranslate::PixelToRowCol(Point)` (SDL2 + SDL3), factors set on
  open/resize in `ComputeScalingFactors`. Mouse events arrive in window-logical pixels — exactly what
  these factors are derived from (`SDL_GetWindowSize`), so the conversion is direct. (HiDPI render
  scaling is applied separately via `SDL_SetRenderScale` and does not affect event coordinates — verify
  on the macOS retina box.)
- **View rects are ABSOLUTE screen row/col.** Splitters compute child rects by `Move()`-ing off the
  parent's absolute origin (`VSplitView::UpdateRightViewRect`). So hit-testing is a plain tree walk:
  find the deepest *visible* view whose `viewRect.PointInRect(row,col)` is true. `Rect::PointInRect`
  already exists (`Core/Rect.h:69`).
- **Click → buffer position:** `Line::VisualToCharIndex(visualCol, tabSize)` (`Core/Line.cpp:218`) is
  **already written and commented "for mapping a screen column (e.g. a mouse click) back to a buffer
  position"** — someone anticipated this. Combined with `lineCursor.viewTopLine + localRow` for the
  line, and `Document::SetCursorPosition(idxLine, idxChar)` (`Document.h:173`, "the single canonical
  way to set the cursor from outside navigation"), click-to-position is mostly plumbing.
- **Scroll:** `LineCursor.viewTopLine/viewBottomLine` (`Core/Graphics/Cursor.h`) +
  `VerticalNavigationViewModel::OnNavigateUp/Down`. The editor, the TreeView (workspace), and the
  terminal scrollback all already have a vertical-scroll concept to hook the wheel onto.
- **Focus switching:** the three focusable things are RootView *top views*
  (`glbEditorView`/`glbTerminalView`/`glbWorkSpaceView`, `Core/Editor.h:28`), switched with
  `RootView::SetActiveTopViewByName`. Note the registered editor top view is the
  **`EditorViewContainer`**, not the inner `EditorView` — so "focus the view I clicked" must resolve to
  the *enclosing top view*, not the leaf.

## What must be added

- A backend-agnostic **`MouseEvent`** plain-data struct (mirrors `KeyPress`):
  `{ kind (Press/Release/Wheel/Move), int x, int y /*row,col*/, int button, int wheelDelta, uint8_t modifiers }`.
- **Backend capture** in both `PollEvents` (SDL2 + SDL3 — keep in sync): trap
  `SDL_EVENT_MOUSE_BUTTON_DOWN/UP`, `_MOUSE_WHEEL` (and later `_MOUSE_MOTION` for drag-select), convert
  px→rc via `SDLTranslate`, reuse `TranslateModifiers(SDL_GetModState())`, post to the Runloop.
- A **Runloop entry point** (`ProcessMouseEvent`) + a **dispatch/routing** step.
- A per-view **`OnMouseEvent`** handler.

`ScreenBase` itself needs little/no new virtual — like keyboard, the backend posts straight to the
Runloop. The pixel→rc factors are a legitimate graphics concern and stay in the backend (SDLTranslate).
NCurses is out of build; Headless ignores mouse.

---

# Proposed solutions

## Option A — Spatial hit-test routing  ✅ recommended

A second, parallel dispatch path that is honest about mouse being spatial.

```
SDL mouse event ──px→rc──> MouseEvent ──Runloop queue──> Runloop::ProcessMouseEvent
   └─> RootView::DispatchMouse(me):
         1. if a modal is active → restrict hit-test to the modal subtree (else swallow)
         2. hit-test: deepest visible view with viewRect.PointInRect(me.x,me.y)
         3. activate the enclosing TOP VIEW (focus follows click) — RootView owns this
         4. call hitView->OnMouseEvent(me); bubble to parent while it returns false
```

- `ViewBase`: add `virtual bool OnMouseEvent(const MouseEvent&) { return false; }` and a generic
  `HitTest(row,col)` walk (splitters/stacks get it for free since rects are absolute).
- **RootView owns focus-on-click** (it already owns `topViews` + active state) — the leaf view never
  reaches back up to change focus; it just handles the event. This keeps the existing inversion style.
- Per-view behaviour lives where the knowledge is:
  - **EditorView** — Press: local `(col,row)`; `line = viewTopLine+row`;
    `char = LineAt(line)->VisualToCharIndex(col, tabSize)`; `document->SetCursorPosition(line,char)`.
    Wheel: nudge `viewTopLine` through the nav model.
  - **WorkspaceView/TreeView** — Press: `treeLineCursor.idxActiveLine = viewTopLine+row`; (double-)click
    opens via the existing `OpenDocumentFromWorkspace`/`SwitchToEditorView` path
    (`WorkspaceView.cpp:266`). Wheel: scroll the tree.
  - **TerminalView** — Wheel: scrollback (it already renders scrollback); click: just focus initially.

**Pros:** matches how real GUI toolkits route the mouse; click/drag/wheel all fit; per-view semantics
stay local; reuses every primitive listed above. **Cons:** introduces a second dispatch path + a
hit-tester (small — the rects are already absolute).

## Option B — Translate mouse → existing `kUIAction`/`kAction`, reuse keyboard dispatch

Map e.g. wheel-up → `kUIActionLineUp`×N and feed it through the existing `DispatchToHandler`.

- **Works cleanly only for the wheel** (scroll = the nav actions the focused view already handles).
- **Breaks for click:** actions are coordinate-free and focus-routed. To click-to-position you must
  (a) smuggle an (x,y) target into `EditorAction`, and (b) **still hit-test** to pick the recipient
  (the view under the pointer ≠ the focused view). At that point you've built the hit-tester anyway and
  polluted `EditorAction` with coordinates 95% of actions ignore — net negative.

**Verdict:** Option B is attractive *only* if mouse support were scroll-only. Since the requirement
includes click-to-position and click-to-focus (both inherently spatial), B degenerates into "A plus an
awkward coordinate-carrying action."

## Recommendation

**Go with Option A**, and borrow B's one good idea: for the **wheel**, have the hit view translate into
the *same* navigation primitives the `kUIActionLineUp/Down`/`PageUp/Down` path uses, so scroll
behaviour stays identical between keyboard and wheel. Clicks use the spatial path + `SetCursorPosition`.

## Suggested phasing

0. **Plumbing:** `MouseEvent` type; capture+translate in SDL2 **and** SDL3 `PollEvents`;
   `Runloop::ProcessMouseEvent`; `ViewBase::OnMouseEvent` + `HitTest`; `RootView::DispatchMouse` with
   focus-on-click. Prove routing with a temporary `fprintf` (which view + local row/col).
1. **EditorView click-to-position** (+ focus).
2. **Wheel scroll** — EditorView, then TerminalView scrollback, then WorkspaceView tree.
3. **WorkspaceView click-to-select / click-to-open.**
4. **Later:** drag-select, double-click word-select, gutter click, mouse inside modals
   (`ListSelectionModal`/`TreeSelectionModal`).

## Open questions

- Double-click detection — time/dist threshold in the backend, or a click-count in `MouseEvent`?
  - Defer to later
- Should the wheel move the **cursor** or only the **viewport** (most editors: viewport only)?
  - **Move the cursor** for now (reuses `OnNavigateUp/Down`; caret stays on-screen by construction).
    Viewport-only is deferred — it needs a new clamped scroll primitive AND a caret-off-screen draw
    policy (`EditorView::SetWindowCursor` does not clamp the caret to the view today).
- Drag-select now or later (needs `_MOUSE_MOTION` + a button-held state machine)?
  - Defer to later
- Do we want any of this keymap-configurable (wheel lines-per-notch, natural/inverted), or hardcoded
  for the "SIMPLE" first cut?
  - Hardcoded, but with constants

---

# Work items (Option A — spatial routing)

Decision locked: **Option A** (spatial hit-test routing); do **not** translate mouse → keyboard/
`kUIAction`. End-features in scope: (1) click changes the active view, (2) click in editor & workspace
moves the cursor/selection, (3) scroll-wheel moves the cursor up/down — **directly**, not via
`kUIActionLineDown`. Reuse existing nav support functions; add one where workspace lacks it. Terminal
is out of scope for now.

**Build order:** P0 (capture+translate) → P1 (hit-test + focus-on-click; gives "click changes view")
→ P2 *click-to-position* (best first payoff) → wheel items (P2/P3) → P3 workspace click. **Wheel decision:
move the cursor**, reusing `OnNavigateUp/Down` (viewport-only scroll deferred — see Open questions).

## P0 — Plumbing & translation
- [x] Add `Core/MouseEvent.h`: plain struct `{ kKind kind (Press/Release/Wheel); int x,y /*row,col*/; int button; int wheelDelta; uint8_t modifiers }` (mirrors `KeyPress`). [design: "What must be added"]
- [x] SDL3 `PollEvents` (`SDL3/SDLScreen.cpp`): trap `SDL_EVENT_MOUSE_BUTTON_DOWN`/`_WHEEL`, px→rc via `SDLTranslate::PixelToRowCol`, modifiers via `TranslateModifiers(SDL_GetModState())`, post to Runloop. [design: "How input flows today"]
- [x] SDL2 `PollEvents` (`SDL2/SDLScreen.cpp`): same as above with SDL2 event names (`SDL_MOUSEBUTTONDOWN`/`SDL_MOUSEWHEEL`) — **keep both backends in sync**.
- [x] `Runloop::ProcessMouseEvent(MouseEvent)` + queue marshalling, mirroring `ProcessKeyPress`/`PostMessage` (`Core/Runloop.cpp`).

## P1 — Routing & focus (RootView)
- [x] `ViewBase`: add `virtual bool OnMouseEvent(const MouseEvent&) { return false; }` (`Core/UI/Views/ViewBase.h`).
- [x] `ViewBase::HitTest(x,y)`: generic walk returning the deepest **visible** subview with `viewRect.PointInRect` (rects are absolute, so splitters/stacks work unchanged). [design: "Pieces that already exist"]
- [x] `RootView::DispatchMouse(me)`: modal-guard → hit-test → activate enclosing **top view** (focus-follows-click, resolve leaf→ancestor top view via `FindEnclosingTopViewName`) → call `OnMouseEvent`, bubbling up while false (`Core/UI/Views/RootView.h`).
- [x] Wire `Runloop::ProcessMouseEvent` → `RootView::DispatchMouse`; temporary `fprintf` trace (hit view + row/col) to prove routing, **not yet removed** - keep until manual GUI verification (P4) confirms routing. [design: "Suggested phasing" P0]
- [x] **End-feature: click changes active view** — manually verified in a running GUI session: focus-follows-click confirmed (clicking switches the active top view).

## P2 — EditorView: click-to-position + wheel-moves-cursor
- [x] `EditorView::OnMouseEvent` Press: local `(col,row)=(me.x-contentLeft, me.y-contentTop)`; `line=viewTopLine+row`; `char=LineAt(line)->VisualToCharIndex(col,tabSize)`; `document->SetCursorPosition(line,char)`. [design: reuses `Line::VisualToCharIndex` + `Document::SetCursorPosition`]
- [x] Add a public Document seam `MoveCursorByLines(int delta)` that reuses `verticalNavigationViewModel->OnNavigateUp/Down(|delta|, viewRect, Lines().size())` + `UpdateDocumentFromNavigation(true)` (the support fns behind `OnActionLineUp/Down`) — **no `kUIAction` path** (`Core/Document.{h,cpp}`).
- [x] `EditorView::OnMouseEvent` Wheel: call `document->MoveCursorByLines(±notch)` then `InvalidateView()`. [end-feature: wheel moves editor cursor]

## P3 — WorkspaceView: click-to-select/open + wheel-moves-cursor
- [x] Add `TreeView` support fns (none exist for spatial use): `SetSelectionAtRow(int flattenedRow)` and `MoveSelectionByRows(int delta)` reusing `verticalNavigationViewModel.OnNavigateUp/Down` + `treeLineCursor` (`Core/UI/Views/TreeView.h`).
- [x] `WorkspaceView::OnMouseEvent` Press: `treeView->SetSelectionAtRow(viewTopLine+localRow)`; (open on second click / Enter still uses existing `OpenDocumentFromWorkspace`→`SwitchToEditorView`, `WorkspaceView.cpp:266`).
- [x] `WorkspaceView::OnMouseEvent` Wheel: `treeView->MoveSelectionByRows(±notch)` then redraw. [end-feature: wheel moves workspace selection]

## P4 — Verification
- [x] Unit test the pure mapping (no SDL): px→rc + `VisualToCharIndex` + `viewTopLine` → expected (line,char); covers tabs (use `Assets/testfiles/ConvertUTF.cpp`). [`utests/test_document.cpp`: `test_document_mouseclick_maps_to_charpos`, `test_document_mouseclick_maps_with_scroll`]
- [x] Manual GUI pass (SDL3/macOS dev box): click switches view; click positions editor caret & workspace selection; wheel scrolls cursor in both; HiDPI coords land correctly. [design: open question re: retina event coords]

## Explicitly out of scope (for now)
- Terminal mouse (click/scrollback), drag-select, double-click word-select, gutter click, mouse inside modals, keymap-configurable wheel. [design: "Suggested phasing" P4 / "Open questions"]
