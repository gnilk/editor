# Mouse double-click support

## Goal

Add double-click handling for raw mouse events. The driving use case: **double-clicking a file row in
the `WorkspaceView` opens it**, exactly as if the row were selected and Enter pressed. The mechanism
should be generic enough that other views (e.g. `EditorView` double-click → select word) can opt in
later, but only the workspace use case is in scope here.

A configurable `dbl_click_speed` (milliseconds) gates how close together two clicks must land.

## Current state (what already exists)

The whole mouse path is already wired; we are adding *meaning*, not plumbing:

- **Backends emit raw press/release.** `SDL2/SDLScreen.cpp:442` and `SDL3/SDLScreen.cpp:418` both
  translate an SDL button event into a `MouseEvent` (`src/Core/MouseEvent.h`) and post it through
  `Runloop::ProcessMouseEvent`. The two backends are near-identical dumb translators.
- **One chokepoint.** `Runloop::ProcessMouseEvent` (`src/Core/Runloop.cpp:133`) is the single
  backend-agnostic place every mouse event passes through before `RootView::DispatchMouse` hit-tests
  and bubbles it to a view. It already carries a note from fkling: *"keeping this as we will implement
  double-click."* This is the right altitude for detection.
- **Main-thread context.** `ProcessMouseEvent` runs on the runloop thread — events reach it drained
  from the `SafeQueue` message pump (`PostMessage` in the backends), not directly from SDL. So any
  state we keep there needs **no locking**.
- **The "open" action already exists.** `WorkspaceView::OnAction` handles `kUIActionCommitLine` (Enter)
  by calling `Editor::Instance().OpenDocumentFromWorkspace(itemSelected)` and optionally switching to
  the editor view (`src/Core/Editor/Views/WorkspaceView.cpp:261`). Double-click only needs to reach
  this same code.
- **`WorkspaceView::OnMousePressedEvent`** (`WorkspaceView.cpp:320`) already maps a press to a row and
  selects it. This is where we branch on click count.

## Design decision

The original sketch proposed a **deferred timer**: on press/release start a `Timer(dbl_click_speed)`;
a second press before it elapses cancels it and fires a double-click; otherwise it elapses and fires a
single click. Three approaches were considered:

| | Approach | Latency | Configurable | Cross-thread | Backend edits |
|---|---|---|---|---|---|
| **A (recommended)** | **Click-count tracker in `Runloop`** | none | yes | no | none |
| B | SDL's own `event.button.clicks` | none | **no** (OS-timed) | no | both backends |
| C | Deferred `Timer` (original sketch) | +`dbl_click_speed` on every single click | yes | **yes** (timer thread → runloop) | none |

**Recommendation: Approach A.** Deferral (C) is only needed when the single-click action must *not*
happen if the gesture turns out to be a double-click. For our use case it never conflicts — selecting
the row you are about to open is harmless and even desirable — so there is no reason to pay the
per-click latency or marshal a `Timer` delegate from its worker thread back onto the runloop. The
`Timer` class (`src/Core/Timer.h`) is therefore **not used**.

Approach A: keep a tiny bit of state at the `ProcessMouseEvent` chokepoint — the timestamp, cell, and
button of the last press. On each new press, if it lands within `dbl_click_speed` ms, on the same cell,
with the same button, it's the *second* click → tag the event with `clicks = 2`. Views read `clicks`
and decide what to do. Single-click fires immediately as today (no behavioural regression); the second
click additionally opens the file. No backend changes, consistent across SDL2/SDL3/platforms, and the
detector is pure logic that unit-tests without SDL or the runloop.

> If a future view needs true deferral (conflicting single/double actions), revisit Approach C *for
> that view only* — the click-count field added here is still the carrier; the view would itself hold
> the debounce timer rather than acting on the first click.

(Approach B — reading SDL's `event.button.clicks`, which both SDL2 and SDL3 populate — is the
two-line-per-backend shortcut, noted for completeness, but it ignores our `dbl_click_speed` config and
leaves timing to the OS, so it's rejected.)

## Event model: counter vs. semantic events

There are two seams here, and we make a deliberate choice at each.

**Seam 1 — raw SDL → our own events.** The backends already synthesize SDL button events into our
`MouseEvent` (Press / Release / Wheel). That stays. The model is intentionally *raw* — Press/Release,
not "Click" — because a click is itself a higher-level gesture.

**Seam 2 — raw events → click semantics.** A "click" (and double-click) is a synthesized gesture on
top of Press/Release. Toolkits split into two mainstream camps on how to surface it:

- **Counter on the event** — `clickCount` / `clicks` carried on the press, the press still firing every
  time. This is **Cocoa's** model (`NSEvent.clickCount`) and **SDL's own** (`SDL_MouseButtonEvent.clicks`).
- **Distinct synthesized event kinds** — a separate `DoubleClick` (Win32 `WM_LBUTTONDBLCLK`) or
  `click`/`dblclick` layered above the raw events (web DOM). Qt similarly has a dedicated
  `mouseDoubleClickEvent` (and, notably, *no* "click" event — a single click is inferred by the widget
  from press+release).

**We take the counter model** (`MouseEvent.clicks`), for two codebase-specific reasons:

1. **It scales to the roadmap as a number, not an enum explosion.** The future items (double-click →
   select word, triple-click → select line) are exactly why text editors use a click count: `2 = word,
   3 = line, 4 = paragraph` is natural as a count and ugly as `DoubleClick`/`TripleClick`/`QuadClick`
   kinds. **`MouseClickTracker` is the single, obvious home for triple-click (and beyond) if we ever
   want it** — just keep counting instead of resetting at 2.
2. **A true first-class "Click" (press+release on the *same* target) only earns its keep once you need
   press/release pairing** — e.g. a button that activates only if pressed *and* released on itself, or
   distinguishing click-from-drag. This editor has no drag and acts on press, so synthesizing a "Click"
   abstraction now would be speculative machinery.

**Upgrade path (not built now).** If a view ever wants to be timing-agnostic, the two camps aren't
mutually exclusive — like the DOM (`mousedown`/`mouseup` *plus* `click`/`dblclick`), `MouseClickTracker`
can keep stamping `clicks` **and** additionally emit `kMouseEventKind_Click` / `_DoubleClick` kinds
derived from the same state, letting a view handle whichever altitude suits it. We add that layer only
when a concrete view needs it; the tracker is the one place it would hang off.

## Architecture of the chosen approach

```
SDL2/SDL3 backend  ──MouseEvent(Press)──▶  Runloop::ProcessMouseEvent  ──▶  RootView::DispatchMouse  ──▶  WorkspaceView
 (unchanged)                                 │  owns MouseClickTracker                                      OnMousePressedEvent:
                                             │  Press → tracker.RegisterPress(now,x,y,btn)                  select row;
                                             │         → me.clicks = 1 or 2                                 if me.clicks>=2 → OpenSelectedItem()
                                             ▼
                                       (main thread, no lock)
```

- **`MouseClickTracker`** — a small, dependency-free class. `RegisterPress(now, x, y, button)` returns
  the click count (1 or 2). It holds the last press's time/cell/button and the threshold (ms). Same
  cell + same button + within threshold ⇒ 2, then it resets so a third quick click counts as 1 again
  (no runaway double-counting). The threshold is **injected** (not read from `Config` inside the
  class) so it stays pure and testable.
- **`MouseEvent.clicks`** — new field, default 1, meaningful only for `kMouseEventKind_Press`.
- **`Runloop`** owns one `MouseClickTracker`, feeds it `dbl_click_speed` from `Config`, stamps each
  press's `clicks`, then dispatches exactly as before.
- **`WorkspaceView`** branches on `clicks` and reuses the existing open path.

## Work packages

### WP1 — Add `clicks` to the mouse event model
- **File:** `src/Core/MouseEvent.h`
- Add `int clicks = 1;` (document: valid for `Press`; 1 = single, 2 = double; default for
  release/wheel/move).
- **Done when:** compiles; release/wheel events are unaffected.

### WP2 — `MouseClickTracker` (core detection logic)
- **New:** `src/Core/MouseClickTracker.{h,cpp}`
- Pure class, no SDL / no singletons. API roughly:
  `int RegisterPress(std::chrono::steady_clock::time_point now, int x, int y, int button)`.
  Threshold (ms) set via ctor or a setter. Returns 1 or 2; resets after a 2.
- Decisions baked in here: same-cell match (coords are character cells, so exact row+col equality —
  see *Edge cases*), same-button requirement, and which button(s) participate.
- This is the **single hook point** for any future click semantics — triple-click (keep counting past
  2 instead of resetting) or synthesized `Click`/`DoubleClick` kinds (see *Event model*). Not built
  now, but the class is structured so that's a local change.
- **Done when:** deterministic for injected timestamps; no dependency on `Config`, `Runloop`, or SDL.

### WP3 — Wire the tracker into `Runloop::ProcessMouseEvent`
- **Files:** `src/Core/Runloop.{h,cpp}`
- Give `Runloop` a static `MouseClickTracker`. On a `Press` event, call
  `RegisterPress(steady_clock::now(), me.x, me.y, me.button)` and set `me.clicks`; pass release/wheel
  through untouched. Then `DispatchMouse` as today.
- Feed `dbl_click_speed` from `Config` into the tracker (read once / refresh on config reload).
- Runs on the runloop thread → no locking needed (note this in a comment, since the rest of the mouse
  path is documented as spatially-routed).
- **Done when:** views receive a correct `clicks`; existing single-click selection behaviour is
  unchanged.

### WP4 — Config: `dbl_click_speed`
- **File:** `Assets/Resources/config.yml`
- Add the setting with a sensible default (e.g. `250` ms) and a comment. Section choice: a small
  top-level `mouse:` block reads cleanest (the `lines_per_scroll_wheel_notch` precedent is *per-view*,
  but double-click timing is global) — confirm in *Open questions*.
- Honours the user-config merge from `~/.local/share/goatedit/config.yml` automatically (existing
  mechanism).
- **Done when:** changing the value visibly changes the detection window.

### WP5 — `WorkspaceView`: act on double-click (the use case)
- **Files:** `src/Core/Editor/Views/WorkspaceView.{h,cpp}`
- Extract the open logic currently inline in `OnAction`'s `kUIActionCommitLine` branch
  (`WorkspaceView.cpp:261-280`) into a private helper, e.g. `OpenSelectedItem()` — file nodes open +
  optional switch-to-editor, folder nodes do nothing (matching Enter today). Call it from both the
  Enter path and the new double-click path (DRY — one open path, two triggers).
- In `OnMousePressedEvent`: select the row as today, then `if (mouseEvent.clicks >= 2) OpenSelectedItem();`.
- **Done when:** double-clicking a file row opens it like Enter; a single click still only selects (no
  open, no lag).

### WP6 — Tests
- **New:** `utests/test_mouseclicktracker.cpp`, registered in `CMakeLists.txt` (the `utestsrc` list,
  alongside `test_treeview.cpp` / `test_workspace.cpp` near line 455).
- Discriminating cases over **injected** timestamps (no real sleeping):
  - two presses within window, same cell, same button → second is `clicks == 2`
  - two presses, gap **>** window → `clicks == 1`
  - within window but **different cell** → `clicks == 1`
  - within window but **different button** → `clicks == 1`
  - triple quick click on same cell → `1, 2, 1` (reset after the double, no runaway)
- The tracker is pure, so this is the high-value test. A `WorkspaceView`-level test is optional (needs
  the view tree); the tracker test is the one that proves the mechanism.
- Add `mouseclicktracker` to the verified-green module list in `CLAUDE.md` once green.

### WP7 — Manual verification & cleanup
- Build/run the **macOS SDL3** target; double-click a file in the workspace panel → opens; single
  click → selects with no perceptible delay. Tweak `dbl_click_speed` and confirm the window changes.
- No backend edits were made, so the SDL2 path inherits the same behaviour — sanity-check it on the
  Linux box if convenient.
- The commented `[MOUSE]` trace in `Runloop::ProcessMouseEvent` can be re-enabled during verification,
  then left commented.

## Edge cases & decisions

- **Same-cell match.** `MouseEvent` coords are already character cells (row/col), not pixels, so
  "same position" is naturally "same cell" — start with exact row+col equality. If that proves too
  strict in practice, relax to same row (a file row is the meaningful target).
- **Button.** Only count a double-click for the **same** button as the prior press; realistically
  restrict double-click detection to the left button (button 1). Right/middle keep `clicks == 1`.
- **Triple-click / runaway.** Reset the tracker after emitting a 2 so the sequence is `1, 2, 1, 2…`,
  never `2, 2, 2`. (Promoting triple-click to a distinct gesture is out of scope.)
- **No deferral ⇒ no latency.** Single-click side effects (row select) fire immediately and are
  intentionally additive with the double-click open — do not suppress them.
- **Threading.** Detection lives on the runloop thread; the tracker needs no lock. The `Timer` class is
  deliberately not involved.

## Out of scope / future

- `EditorView` double-click → select word, triple-click → select line (the `clicks` field is the
  carrier; the view would opt in).
- Click-and-drag selection, mouse-move/hover events (backends don't emit `kMouseEventKind_Move` today).
- Distinct triple-click gesture.

## Open questions

1. **Config section for `dbl_click_speed`** — a new top-level `mouse:` block (global, recommended) vs.
   hanging it under an existing section. Global reads better since timing isn't per-view.
2. **Cell tolerance** — exact cell vs. same-row. Recommend starting exact and relaxing only if needed.
3. **Buttons** — restrict double-click to left button only (recommended) or honour any button.
