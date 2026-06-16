# UI Refactor — Working Notes

> **Living document.** Started 2026-06-16. This is the analysis-and-decision log for splitting
> `src/Core/Views` (+ the controllers, + the graphics seam) into a **generic, reusable UI layer**
> and an **editor-specific UI layer**. Append findings as we go; keep §0 current.

---

## §0 — Status / read this first

**Phase: IN PROGRESS — branch `refactor-ui`, AI-0 ✅ + AI-1 ✅ + AI-2 ✅ DONE (committed `97ce381`,
`7dce4e1`, AI-2 pending commit).** Analysis done (§3–§7); sequenced
action-item plan in **§8**. User direction (2026-06-16): wants the **clean separation** regardless of
whether a physical library is ever cut — **start with the folder split (AI-1)**, treat the **`kAction`
split (AI-4)** as its own work item, and harden the new **`SessionManager`/`ViewBase` seam via
`ILayoutSink` (AI-5)** — the user flagged that one as a dependency that crept in recently and was
overlooked. Also captured: the **`UIHost`** seam (AI-3). The library itself (AI-7) is optional ("I
might not do it").

**AI-0 done (2026-06-16, on `refactor-ui`):** removed the dead `#include "Core/Line.h"` from
`src/Core/Graphics/DrawContext.h`; swept the other contract headers (`ScreenBase.h`, `WindowBase.h`,
`NativeWindow.h`, `Cursor.h`, `KeyboardDriverBase.h`) — no other editor-model leaks found, only
primitives (`Rect`/`Point`/`ColorRGBA`/`TextAttributes`) and intra-Graphics includes. `goatedit` +
`utests` both rebuilt clean.

**AI-1 done (2026-06-16, on `refactor-ui`):** the **full split** (user chose: move editor views out of
`Views/` too, not just the generic set) into `src/Core/UI/{Graphics,Views,Controllers}` (generic) and
`src/Core/Editor/{Views,Controllers}` + `src/Core/Editor/LineRender.{h,cpp}` (editor-specific); old
`src/Core/Views/` and `src/Core/Controllers/` no longer exist. **Correction to the original §10
sketch:** `Rect`, `Point`, `ColorRGBA`, `TextAttributes`, `NamedColors`, `VerticalNavigationViewModel`,
`KeyPress`, `Keyboard`, `Cursor` did **NOT** move into a `UI/Primitives/`(`/Input/`) folder as §10
guessed — grepping their use sites showed they're genuinely **shared** with the core document/text
model (`Document.h`, `Line.h`, `TextBuffer.h`, `DocumentViewState.h`, `ClipBoard.h`, `HexProjection.h`,
`TerminalScreen.h`, `LanguageBase.h`, `Theme.h` all include one or more of these). They stay in
`src/Core/` root as a de facto shared/common layer; no `UI/Primitives/` or `UI/Input/` folder was
created. `CMakeLists.txt` updated in place (no separate `ui_src`/`appui_src` CMake groups yet — still
one `editorsrc` list, just with the new paths; that grouping is deferred, not needed for AI-1's
goal of a physical, visible split). Full rebuild (`goatedit` + `utests`) clean; verified-green 235-test
set passes 0 failures.

**AI-2 done (2026-06-16, on `refactor-ui`):** `scripts/check-ui-boundary.sh` + a non-blocking CI step
in `.github/workflows/cmake.yml`. Current leak set: **19 includes / 14 files**, all matching the §4
chokepoints exactly (16× `RuntimeConfig.h`/`Editor.h` → AI-3, 2× `Core/Session/*` → AI-5, 2×
`Core/Config/*` → AI-6; zero `Document.h`/`TextBuffer.h`/`Workspace.h`/`Plugins` hits). Full list in
the AI-2 §8 entry. **Next: pick one of AI-3/`UIHost`, AI-4/`kAction` split, or AI-5/`ILayoutSink` per
user priority.**

**Verdict in brief (see §7 for the argument):**
- The layout engine + the rendering contract + the selection/tree widgets **are** genuinely generic
  and reusable. A real char-grid retained-mode toolkit is hiding in here.
- BUT a *physically separate library* is a speculative payoff ("reuse elsewhere"). The *concrete*
  payoff is reason (2) — the graphics refactor — and the discipline that enables a clean library
  split (kill the `RuntimeConfig` service-locator coupling, a generic action set, draw-list
  primitives) is the **same** discipline the graphics refactor needs.
- **Recommendation: do the boundary-hardening as a logical module split first** (`src/Core/UI/` vs
  the editor views), enforce a one-way include rule with a grep CI smell-test (mirroring the
  existing `Core/Graphics → Core/Session` check), and only cut a separate CMake target *after* the
  includes are clean. Low-regret: serves both goals, defers the speculative bit.

**Three structural chokepoints** stand between today's code and a clean generic UI (§4): the
`RuntimeConfig` service-locator (screen access), the monolithic `kAction` enum, and the `Session`
hooks baked into `ViewBase`. Everything else is leaf-level.

**Quick wins already identified** (§11): `DrawContext.h` includes `Core/Line.h` but never uses it.

---

## §1 — Goal & motivation

Two drivers, stated by the user:

1. **Overview + reuse.** Get a clean separation so the generic UI *could* be split into a standalone
   library and reused in other projects.
2. **Graphics-backend refactor (the real near-term win).** Streamline the graphics backend and push
   more work down into base classes that emit **primitive draw-lists**. A clean UI/Graphics boundary
   is a precondition for that.

Explicit asks folded in:
- Deliver the **external-dependency list** for the generic parts (what outside the UI they touch).
- Decide the **Graphics seam**: leave it as "interfaces you implement", or consolidate the backend
  *into* the UI library.
- Treat **`BaseController` as part of the UI**.
- Accept that the honest answer might be **"not worth extracting"** — but walk the route to find out.

---

## §2 — Method

- Enumerated `src/Core/Views/` (35 files), `src/Core/Controllers/` (5), `src/Core/Graphics/` (the
  seam).
- Extracted every local `#include` per file → built the dependency graph (§3, §4).
- Read the load-bearing headers: `ViewBase`, `VisibleView`, `RootView`, `RuntimeConfig`,
  `WindowBase`, `DrawContext`, `ScreenBase`, `Action`/`EditorAction`, `KeypressAndActionHandler`,
  `BaseController`, `LineRender`.
- Grepped the actual *use sites* of the suspicious couplings (`Editor::Instance()`,
  `RuntimeConfig::Instance()`, `Config::`, `NamedColors::`, `Line`) to separate "includes it" from
  "genuinely needs it".

Build reality: there is **no library boundary today** — everything is appended to one `editorsrc`
list in `CMakeLists.txt` and linked straight into the `goatedit` executable (and `utests`). So
"splitting into a library" is a genuine structural change, not a relabel.

---

## §3 — Inventory & classification

Buckets: **GENERIC** (UI-library candidate), **APPUI** (editor-specific views), **BACKEND** (concrete
graphics impls — already separate-ish), **SUPPORT** (render glue / primitives).

### Views (`src/Core/Views/`)

| File | Bucket | Why / key external deps |
|---|---|---|
| `ViewBase.{h,cpp}` | GENERIC (base) | The base class. Couples to: `Action`/`EditorAction`, `Session/SessionState`, and (in .cpp) `Editor`, `RuntimeConfig`, `Runloop`, `SessionManager`. **The chokepoint.** |
| `VisibleView.{h,cpp}` | GENERIC | `RuntimeConfig::GetScreen()` to create windows. |
| `StackableView.h` | GENERIC | **Clean** — only `ViewBase.h`. |
| `VStackView.h` / `HStackView.h` | GENERIC | `RuntimeConfig` (screen), `StackableView`. Layout containers. |
| `VSplitView.h` / `HSplitView.h` | GENERIC | `RuntimeConfig` (screen), `VisibleView`. Splitter logic (`ClampSplitterPos`, etc.). |
| `RootView.h` | GENERIC* | Top-view manager (generic concept) but reaches `Editor`/`Config` for quick-command leave. |
| `ModalView.{h,cpp}` | GENERIC | `RuntimeConfig`. Overlay base. |
| `ListSelectionModal.{h,cpp}` | GENERIC* | `VerticalNavigationViewModel`, `VStackView`, `SingleLineView`, `Config`, `NamedColors`. Reusable picker; pulls theme/config. |
| `TreeView.h` | GENERIC* | `VisibleView`, `VerticalNavigationViewModel`, `Editor::GetTheme()` (colors), `RuntimeConfig` (invalidate). |
| `TreeSelectionModal.{h,cpp}` | GENERIC | `ModalView`, `TreeView`, `VStackView`, `VerticalNavigationViewModel`. |
| `SingleLineView.h` | GENERIC* | `VisibleView`, `Editor::GetTheme()` (colors). Single-line input widget. |
| `TestView.{h,cpp}` | GENERIC (demo) | `Config`, `ColorRGBA`, `Editor`. Scratch/demo view. |
| `EditorView.{h,cpp}` | APPUI | `Document`, `EditController`, `VerticalNavigationViewModel`, `LineRender`. The core editor. |
| `EditorViewContainer.h` | APPUI | `Editor`, `ActionHelper`. Owns text↔hex swap + buffer cycling. |
| `EditorHeaderView.h` | APPUI | `SingleLineView`. Editor header. |
| `HexView.{h,cpp}` | APPUI | `Document`, `HexProjection`, `ByteStreamReader`. Hex projection view. |
| `GutterView.{h,cpp}` | APPUI | `Document`, `EditController`. Line numbers (stands down in hex mode). |
| `TerminalView.{h,cpp}` | APPUI | `TerminalController`, `TerminalScreen`, `DrawContext`, `LineRender`. |
| `WorkspaceView.{h,cpp}` | APPUI | `Workspace`, `TreeView`, `RootView`. File browser. |
| `CommandView.{h,cpp}` | APPUI | `CommandController`, `LineRender`. (Currently commented out of the build.) |
| `HSplitViewStatus.{h,cpp}` | APPUI | `Editor`, `RootView`. Status line drawn on the splitter row. |

`*` = "generic in concept, but currently reaches an app singleton for theme/config/invalidate" — the
break is shallow (inject a color/host provider). See §4.

### Controllers (`src/Core/Controllers/`)

| File | Bucket | Why / key external deps |
|---|---|---|
| `BaseController.{h,cpp}` | GENERIC (per user) | Single-line edit primitives over `Cursor` + `Line` + `KeyPress`. **Pulls the editor `Line` type** (see §6). |
| `EditController.{h,cpp}` | APPUI | `Document`, `TextBuffer`, `UndoHistory`, `KeyMapping`, `AutoPairEngine`, `IndentEngine`. |
| `TerminalController.{h,cpp}` | APPUI | `Shell`, `TerminalScreen`, `VTermParser`, `TerminalHistory`. |
| `CommandController.{h,cpp}` | APPUI | `Shell`, `MakeBuildLang`, plugins. |
| `QuickCommandController.{h,cpp}` | APPUI | `Editor`, plugins. |

### Support / render glue

| File | Bucket | Why |
|---|---|---|
| `LineRender.{h,cpp}` | SUPPORT→APPUI | Renders a tokenized `Line` with `Document` selection overlays into a `DrawContext`. Knows the editor text+selection model → stays app-side (or becomes the bridge that the generic `DrawContext` is fed from). |

### Graphics seam (`src/Core/Graphics/`)

| File | Bucket | Role |
|---|---|---|
| `ScreenBase.{h,cpp}` | CONTRACT | Abstract screen: create/update windows, geometry, poll events, clipboard. Already a clean interface (geometry already inverted — see CLAUDE.md layering pattern). |
| `WindowBase.h` | CONTRACT | Abstract window: decoration, DC accessor, cursor. |
| `DrawContext.{h,cpp}` | CONTRACT | Abstract draw surface: `DrawStringAt`, attributes, colors, overlays. **The primitive-emitter target for reason (2).** |
| `NativeWindow.h`, `Cursor.h` | CONTRACT | Opaque native handle + cursor struct. |
| `KeyboardDriverBase.{h,cpp}` | CONTRACT | Abstract keyboard event source. |
| `SDL2/*`, `SDL3/*` | BACKEND | Concrete impls (built per-platform). |
| `NCurses/*`, `Headless/*` | BACKEND | Out-of-build / headless impls. |

---

## §4 — External dependencies of the GENERIC parts (the core deliverable)

What the generic UI touches *outside itself*, ranked by how much it blocks extraction. "Break" =
how to sever it.

### A. Graphics backend contract — `ScreenBase`/`WindowBase`/`DrawContext`/`NativeWindow`/`Cursor`/`KeyboardDriverBase`
- **Status:** Already abstract base classes; the UI already renders through them. This is the UI's
  *rendering contract*, not an app dependency.
- **Break:** none needed — but **decide ownership** (§5). These should *belong to* the UI library.

### B. `RuntimeConfig` (service locator) — **CHOKEPOINT #1**
- **Where:** `VisibleView`, `V/HStackView`, `V/HSplitView`, `RootView`, `ModalView`,
  `ListSelectionModal`, `TreeView`, `ViewBase.cpp`. Almost always just to call `GetScreen()` (create
  windows) or `GetRootView()` (invalidate).
- **Problem:** `RuntimeConfig` is an app god-object — its header drags in `Document`, `Plugins`,
  `FolderMonitor`, platform monitors. The generic UI transitively depends on the whole app through it.
- **Break:** introduce a slim **`UIHost`/`UIContext`** that carries only what the UI needs
  (`ScreenBase::Ref`, the root view, a `PostMessage` callback, an invalidate hook). Inject it (ctor or
  a UI-owned context), *not* the app singleton. The app wires its `RuntimeConfig` to the `UIHost`.
  Effort: mechanical but broad (touches every `InitView`/`ReInitView`).

### C. Action / input model — `Action.h` (`kAction`, `kActionModifier`, `ActionItem`), `EditorAction`, `KeyMapping`, `KeypressAndActionHandler`, `KeyPress`, `Keyboard` — **CHOKEPOINT #2**
- **Where:** `ViewBase::OnAction(EditorAction)` switches on `kAction::kActionIncreaseViewWidth`,
  `…CycleActiveView`, `…MaximizeViewHeight`, etc.; `KeypressAndActionHandler` is the base mixin.
- **Problem:** `kAction` is **one monolithic enum** mixing ~12 UI-layout/navigation/modal actions
  with dozens of editor actions (`Indent`, `ReformatLine`, `ViewModeHex`, `ShellCompletion`,
  `Undo`…). The generic UI references the same enum the editor defines.
- **Break (options):**
  - **(C1, recommended)** Split into a UI-owned `kUIAction` (resize/cycle/modal-close/navigation —
    the subset `ViewBase`/`RootView` actually handle) + an app `kEditorAction`. The UI library only
    knows `kUIAction`; app actions are opaque to it.
  - **(C2)** Keep one enum but pass an **opaque action id (int)**; the UI matches only the IDs it
    owns. Less invasive, weaker typing.
  - `KeyPress`/`Keyboard` are reasonable to ship *with* the UI (input is a UI concern). `KeyMapping`
    (YAML-config-driven) is more app-flavored — could stay app-side and feed resolved actions in.

### D. Persistence hooks — `Session/SessionState` (`LayoutSession`) + `SessionManager` — **CHOKEPOINT #3**
- **Where:** `ViewBase::ToSession/FromSession/CollectLayout/ApplyLayout/NotifySessionChanged`;
  `RootView` persists the focused top-view; `ViewBase.cpp` calls `SessionManager::NotifyChanged()`.
- **Problem:** the generic base view knows the app's session schema.
- **Break:** replace `LayoutSession&` with a small **UI-owned visitor/sink interface**
  (`ILayoutSink` with `put(sessionId, splitterPos)` / `get(...)`) that `SessionManager` implements
  app-side; replace `NotifySessionChanged()` with an injected `onLayoutChanged` callback. The UI
  keeps the *generic* fact (a view has a stable id + a splitter position); the app owns serialization.

### E. App singleton — `Editor::Instance()`
- **Where:** `RootView` (quick-command leave + `Config`), `ViewBase.cpp` (`SetWindowCursor`
  quick-command redirect), `TreeView`/`SingleLineView` (`GetTheme()` colors), `TestView`.
- **Break:** the *theme/colors* uses → inject a **color/theme provider** into the widgets. The
  *quick-command* uses are editor policy → push behind a hook or move those specializations to the
  APPUI subclass. The `Runloop::PostMessage` use → the `UIHost` `PostMessage` callback (see B).

### F. Config — `Config`/`ConfigNode` (YAML)
- **Where:** `ListSelectionModal`, `RootView`, `TestView`.
- **Break:** inject the few values (colors, "leave_when_switching_view" flag) rather than reading the
  global config from inside a widget.

### G. Geometry/color primitives — `Rect`, `Point`, `ColorRGBA`, `TextAttributes`, `NamedColors`
- **Status:** small, self-contained value types. **These should simply move into the UI library**
  (they're UI vocabulary). Low risk.

### H. ViewModel — `VerticalNavigationViewModel`
- **Status:** generic list-navigation view-model used by both modals and the editor view. Reusable;
  **move into the UI library.**

### I. Text-model leakage — `Line` (and `Document` via `LineRender`)
- **Where:** `BaseController` operates on `Line::Ref`; `DrawContext.h` *includes* `Line.h` (unused —
  §11); `LineRender` needs `Line` + `Document`.
- **Problem:** `Line` carries syntax tokens — an editor concept leaking into the generic
  `BaseController`. `LineRender`/`Document` are firmly app-side (keep there).
- **Break:** decide whether the UI owns a slim text-row type (`std::u32string` + attributes) or
  promotes `Line` as UI vocabulary. See §6. (`DrawContext`'s include is just dead — remove it now.)

### J. External libs — `gnklog` (`logger.h`), `fmt`
- Fine to depend on from a library. No action.

### K. Message pump — `Runloop`
- **Where:** `ViewBase::PostMessage`. Break via the `UIHost.PostMessage` callback (B/E).

**Summary:** the generic UI's *legitimate* outward dependencies are just the **graphics contract**
(A) + small **primitives** (G, H) + **input** (C, partially). Everything else (B, D, E, F, I, K) is
**accidental coupling through app singletons** that injection removes.

---

## §5 — The Graphics seam: interface vs consolidate

**The question:** leave Graphics as "interfaces you implement" (separate from the UI lib), or fold
the backend into the UI library?

**Finding:** the abstract seam already exists and the UI already renders exclusively through it
(`window->GetWindowDC()` → virtual `DrawContext`). What's *missing* is ownership clarity: the
abstract contract currently sits in `Core/Graphics/` alongside the concrete SDL backends, and is
reached through the `RuntimeConfig` app singleton.

**Recommendation — consolidate the *contract*, not the *backend*:**
- The **abstract** types (`ScreenBase`, `WindowBase`, `DrawContext`, `NativeWindow`, `Cursor`,
  `KeyboardDriverBase`) are the UI's **rendering contract** → they belong **inside the UI library**.
- The **concrete** backends (`SDL2/`, `SDL3/`, `NCurses/`, `Headless/`) are **implementation
  modules that depend on the UI library**, selected at build/link time. They stay separate and
  swappable — "interfaces you implement" remains true, but the interface lives with the UI, not with
  the SDL code.

**Why this serves reason (2):** the "push more into base classes that emit primitive draw-lists"
direction means `DrawContext` (and friends) grow a richer *primitive* vocabulary that the UI base
views target, while the backend shrinks to a thin consumer of that primitive list. That richer
vocabulary **is** the UI's contract — so it must live with the UI. This also satisfies the existing
hard invariant (CLAUDE.md): nothing under the rendering layer may depend on an app service; data in
via setters, out via callbacks. Keeping the contract in the UI lib and backends below it preserves
that one-way flow by construction.

**Net:** one-way dependency `backend → UI-lib(contract) → (nothing app-level)`. The app wires a
concrete backend into the `UIHost` at startup (today's `SetupSDL2/3` glue is already this shape).

---

## §6 — `BaseController` as UI

The user wants `BaseController` treated as part of the UI. It fits: it's generic single-line editing
(`DefaultEditLine`, `AddCharToLine`, `RemoveCharFromLine`) over `Cursor` + a line + `KeyPress`, with
a `editTabSize` knob — no `Document`, no app singleton. Its subclasses (`EditController`,
`TerminalController`, …) are the app-specific parts and stay in APPUI.

**The one snag:** it operates on `Line::Ref`, and `Line` is the editor's tokenized text line. To put
`BaseController` in a clean UI library, decide:
- **(a)** Promote a slim UI text-row (`std::u32string` + cell attributes) and have `BaseController`
  work on *that*; the editor's `Line` adapts to it. Cleanest boundary, more work.
- **(b)** Accept `Line` as UI vocabulary (move `Line` into the UI lib). Pragmatic; drags the token
  attribute model along.
- **(c)** Leave `BaseController` in APPUI for now (it's small) and revisit. Lowest effort.

Recommend **(c) for the first pass**, **(a) as the eventual target** — `Line`'s syntax-token payload
is squarely an editor concern and shouldn't anchor the UI library long-term.

---

## §7 — Is it worthwhile? (honest verdict)

**Yes to the boundary work; "not yet / maybe never" to a shipped separate library.**

Arguments **for** a real generic UI:
- The layout engine (ViewBase tree + Stack/Split containers + splitter clamp logic + Modal +
  Tree/List selection widgets + the rendering contract) is **not editor-specific**. It's a coherent
  char-grid retained-mode toolkit. Someone *could* build another TUI/grid app on it.
- The coupling that blocks extraction is concentrated in **three chokepoints** (§4 B/C/D), all
  fixable by injection / a generic action set / a sink interface — no deep redesign.

Arguments **against** (the sober side):
- "Reuse in other projects" is **speculative**. The UI was grown to fit one editor; some of its
  ergonomics (top-view cycling, quick-command redirect, the status-line-on-splitter trick) are
  editor-shaped even where the mechanism is generic.
- A separate CMake target/library has **carrying cost** (versioning, a stable public API, a demo/test
  harness) that only pays off if a second consumer actually appears.
- The **concrete** value is reason (2): the graphics refactor. That needs the *boundary* clean, not a
  *shipped library*.

**Therefore:** the high-value, low-regret move is to **harden the boundary in place** (logical
split + include discipline + CI smell-test). That:
1. gives the better overview the user wants (reason 1, the half that's real today),
2. is a prerequisite for the graphics refactor (reason 2), and
3. makes the eventual library a near-trivial CMake-target cut **if** a second consumer ever
   materializes — decided then, with evidence, not now on spec.

If after the logical split the UI lib's include list is genuinely app-free, extracting the library is
a formality. If it *isn't* clean, we'll have learned the UI is more editor-bound than it looks — also
a useful answer.

---

## §8 — Plan: make the toolkit reusable (sequenced action items)

The aim the user actually wants right now is a **clean separation**, with a physical library as an
optional later payoff ("I might not do it"). So the plan front-loads the parts that buy clean
separation regardless of whether a library is ever cut.

**Ordering principle:** the *folder split* (AI-1) comes first because it's a pure move that makes the
boundary visible and turns the include-discipline gate (AI-2) into a concrete, shrinking work queue.
Each dependency-break (AI-3/4/5) then chips at that queue, one seam at a time, tree green after each.

**Sequencing caveat:** AI-1/2 are safe to do anytime. The dependency-breaks (AI-3+) share surface
with the planned **graphics-backend refactor** (CLAUDE.md "Graphics layer refactor — PLANNED") —
especially the `DrawContext` primitive work. Confirm with the user before AI-3+ so the two efforts
don't fight over the same files.

Each item: **Goal · Scope · Approach · Effort/Risk · Done-when · Depends-on.**

---

### AI-0 — Quick wins (safe, no behavior change)  ✅ DONE (2026-06-16)
- **Goal:** remove dead coupling that costs nothing to drop.
- **Scope:** `src/Core/Graphics/DrawContext.h`.
- **Approach:** delete the unused `#include "Core/Line.h"` (§11 — confirmed no `gedit::Line` use in
  the contract). Grep for any other editor-model includes in `Core/Graphics/*` contract headers.
- **Effort/Risk:** trivial / none. **Done-when:** builds; `Core/Graphics` contract headers carry no
  editor-model include. **Depends-on:** —

---

### AI-1 — Folder split: UI toolkit vs editor views  ✅ DONE (2026-06-16)
- **Goal:** make the boundary physical and visible. *No dependency-breaking yet* — just move files
  and fix include paths.
- **Scope:** all of `src/Core/Views` + `BaseController` + the graphics **contract** headers + the
  small primitives.
- **Approach:** create `src/Core/UI/` and move the GENERIC set (§3) into it:
  - `UI/Views/` — `ViewBase`, `VisibleView`, `StackableView`, `V/HStackView`, `V/HSplitView`,
    `RootView`, `ModalView`, `ListSelectionModal`, `TreeView`, `TreeSelectionModal`, `SingleLineView`,
    `TestView`.
  - `UI/Graphics/` — the **contract** only: `ScreenBase`, `WindowBase`, `DrawContext`,
    `NativeWindow`, `Cursor`, `KeyboardDriverBase`. (Backends stay in `Core/Graphics/SDL2|SDL3|…`.)
  - `UI/Controllers/` — `BaseController` (per user; §6 — keep `Line` dep for now, option-(c)).
  - `UI/Primitives/` — `Rect`, `Point`, `ColorRGBA`, `TextAttributes`, `NamedColors`,
    `VerticalNavigationViewModel`.
  - Editor views (`EditorView`, `HexView`, `GutterView`, `TerminalView`, `WorkspaceView`,
    `CommandView`, `HSplitViewStatus`, `EditorViewContainer`, `EditorHeaderView`, `LineRender`) +
    editor controllers → an editor-UI folder (name TBD, §10) **or** left in `Views/` for the first
    pass to minimize diff. Recommend moving them too, so `Views/` disappears and the split is total.
  - Fix `#include` paths; regroup the `editorsrc` lists in `CMakeLists.txt` into a `ui_src` group +
    an `appui_src` group (still one link target for now).
- **Effort/Risk:** medium churn, low risk (mechanical). The compile still works because the moved
  headers keep their existing `Core/Editor.h`/`RuntimeConfig.h` includes — those get broken later.
- **Done-when:** `goatedit` + `utests` build and the verified-green 235 set passes; `src/Core/UI/`
  exists with the generic set; CMake has distinct `ui_src`/`appui_src` groups.
- **Depends-on:** AI-0 (nice-to-have).

---

### AI-2 — Include-discipline smell-test (the work queue)  ✅ DONE (2026-06-16)
- **Goal:** a cheap CI gate that *names* every remaining app→toolkit leak, mirroring the existing
  `grep -rl "Core/Session\|Editor.h" src/Core/Graphics/` invariant.
- **Scope:** a script (`scripts/check-ui-boundary.sh` or a CMake/CTest step).
- **Approach:** assert `src/Core/UI/` includes **none** of: `Editor.h`, `Document.h`, `TextBuffer.h`,
  `Workspace.h`, `RuntimeConfig.h`, `Core/Session/*`, `Core/Plugins/*`, `Core/Config/*`. It **will
  fail at first** — the failure list is the precise queue for AI-3/4/5/6.
- **Effort/Risk:** small / none. **Done-when:** the gate runs and reports the current leak set.
  **Depends-on:** AI-1.
- **Built:** `scripts/check-ui-boundary.sh` (executable) — walks every `.h`/`.cpp` under
  `src/Core/UI/`, flags `#include "..."` of the forbidden set, and annotates each hit with the
  action item that resolves it. Wired into `.github/workflows/cmake.yml` as an **informational**
  step (`continue-on-error: true`) right after the `goatedit` build — non-blocking on purpose since
  it's expected to fail until AI-3/4/5/6 land; flip `continue-on-error` to `false` once the leak
  count hits 0 (that's also AI-6's done-when).
- **Current leak set (19 includes across 14 files, captured 2026-06-16 — the precise AI-3/5/6
  queue):**
  - **AI-3 (`RuntimeConfig.h`/`Editor.h` → `UIHost`), 16 hits:** `Graphics/DrawContext.cpp`
    (`Editor.h`), `Views/ModalView.cpp` (`RuntimeConfig.h`), `Views/ViewBase.cpp` (`Editor.h` +
    `RuntimeConfig.h`), `Views/RootView.h` (`Editor.h` + `RuntimeConfig.h`),
    `Views/SingleLineView.h` (`Editor.h` + `RuntimeConfig.h`), `Views/TestView.cpp` (`Editor.h`),
    `Views/TreeView.h` (`Editor.h`), `Views/VisibleView.cpp`, `Views/HSplitView.h`,
    `Views/VSplitView.h`, `Views/VStackView.h`, `Views/HStackView.h` (all `RuntimeConfig.h`).
  - **AI-5 (`Core/Session/*` → `ILayoutSink`), 2 hits:** `Views/ViewBase.cpp`
    (`Core/Session/SessionManager.h`), `Views/ViewBase.h` (`Core/Session/SessionState.h`).
  - **AI-6 (`Core/Config/*` → injected theme/colors), 2 hits:** `Views/ListSelectionModal.cpp`,
    `Views/TestView.cpp` (both `Core/Config/Config.h`).
  - **Zero hits** for `Document.h`/`TextBuffer.h`/`Workspace.h`/`Core/Plugins/*` — confirms §4's
    read that those never leaked into the toolkit in the first place.
  - Re-run `./scripts/check-ui-boundary.sh` any time to refresh this list — it's the live source of
    truth; the bullets above are a snapshot.

---

### AI-3 — `UIHost`: sever the `RuntimeConfig` service-locator  *(chokepoint #1 — "a note about the UIHost")*
- **Goal:** the toolkit reaches the screen / root / message-pump through a **slim UI-owned context**
  carrying *only* UI concerns — not the app god-object that drags in `Document`/`Plugins`/`FolderMonitor`.
- **Scope:** new `UI/UIHost.h`; call sites in `VisibleView`, `V/HStackView`, `V/HSplitView`,
  `RootView`, `ModalView`, `ListSelectionModal`, `ViewBase.cpp`.
- **Approach (sketch):**
  ```cpp
  // src/Core/UI/UIHost.h   — the toolkit's only "ambient" dependency
  namespace gedit {
      class UIHost {
      public:
          static UIHost &Instance();                       // UI-owned singleton (or inject a ref)
          ScreenBase::Ref GetScreen() const;
          ViewBase *GetRootView() const;                   // for InvalidateAll / routing
          bool IsRootView(const ViewBase *v) const;
          void PostMessage(std::function<void()> cb) const;// was Runloop::PostMessage
          // wiring (called once at startup by the app/glue):
          void SetScreen(ScreenBase::Ref);
          void SetRootView(ViewBase *);
          void SetPostMessage(std::function<void(std::function<void()>)>);
      };
  }
  ```
  The app keeps `RuntimeConfig` but, at startup, **feeds** the `UIHost` (screen, root, a
  `Runloop::PostMessage` shim). Inside the toolkit, replace every
  `RuntimeConfig::Instance().GetScreen()` → `UIHost::Instance().GetScreen()`. Net: the UI headers
  stop including `RuntimeConfig.h` entirely.
- **Decision (open):** UI-local singleton (smallest diff, matches current style) vs true injected
  `UIHost&` through ctors (cleaner, broader). Lean singleton for pass 1.
- **Effort/Risk:** medium (broad, but each edit is a 1-line swap) / low. **Done-when:** no `src/Core/UI`
  file includes `RuntimeConfig.h`; suite green. **Depends-on:** AI-1, AI-2.

---

### AI-4 — Split `kAction` → `kUIAction` (toolkit) + editor actions  *(chokepoint #2 — its own item, per user)*
- **Goal:** `ViewBase::OnAction` / `RootView::OnAction` switch on a **UI-owned** action set; editor
  actions stay app-owned. The toolkit no longer references the monolithic editor enum.
- **Scope:** `Action.h` → split into `UI/Input/UIAction.h` (`kUIAction`) + the editor remainder;
  `EditorAction` struct; `KeyMapping`/`ActionItem`; `ViewBase.cpp`, `RootView.h`, the selection
  widgets; every editor `OnAction`.
- **Approach:** carve into `kUIAction` the values the toolkit actually handles itself —
  `Increase/DecreaseViewWidth`, `Increase/DecreaseViewHeight`, `MaximizeViewHeight`,
  `CycleActiveView{,Next,Prev}`, `CloseModal`. Leave editor actions (`Indent`, `Reformat*`,
  `ViewMode*`, `Undo/Redo`, clipboard, search, command-mode, shell, goto/buffer-cycle, line edits) in
  the app enum.
  - **Sub-decision — the shared navigation set** (`PageUp/Down`, `Line{Up,Down,Home,End,Left,Right}`,
    `LineWord{Left,Right}`, `Buffer{Start,End}`, `GotoTop/BottomLine`): used by **both** the editor
    (cursor) and the list/tree widgets (selection). Either (a) put them in `kUIAction` as generic
    "move" intents that both consume — toolkit-clean, app depends down onto the toolkit enum (fine,
    one-way); or (b) duplicate a minimal nav subset on each side. **Lean (a).**
  - The keymap resolves a keypress into whichever space; the toolkit's `OnAction` only ever *matches*
    `kUIAction` values and ignores the rest (returns false → app handles).
- **Effort/Risk:** medium / medium (touches the action plumbing; easy to mis-route a value). Land
  behind the green suite; the keymapping module tests guard it.
- **Done-when:** `src/Core/UI` references only `kUIAction`; keymapping + layout + workspace tests
  green. **Depends-on:** AI-1 (can run parallel to AI-3).

---

### AI-5 — `ILayoutSink`: sever `SessionManager`/`Session` from `ViewBase`  *(chokepoint #3 — "a note about the ILayoutSink")*
- **Goal:** the base view persists layout through a **UI-owned interface**, not the app's session
  schema. (This is the seam the user flagged — `SessionManager` is the newest dependency to creep
  into `ViewBase`; catch it before more views lean on `LayoutSession` directly.)
- **Scope:** `ViewBase.{h,cpp}` (`ToSession/FromSession/CollectLayout/ApplyLayout/NotifySessionChanged`),
  `RootView.h` (focused-top-view), and `SessionManager` (app side, implements the sink).
- **Approach (sketch):**
  ```cpp
  // src/Core/UI/ILayoutSink.h   — toolkit knows "stable id → splitter pos" + "focused top-view", nothing else
  namespace gedit {
      class ILayoutSink {
      public:
          virtual void PutSplitter(const std::string &id, int pos) = 0;
          virtual bool GetSplitter(const std::string &id, int &pos) const = 0;
          virtual void PutFocusedTopView(const std::string &name) = 0;
          virtual bool GetFocusedTopView(std::string &name) const = 0;
      };
  }
  // ViewBase::CollectLayout(ILayoutSink&) / ApplyLayout(const ILayoutSink&)  — replace LayoutSession&
  // ViewBase::NotifySessionChanged() -> an injected std::function<void()> onLayoutChanged (set via UIHost)
  ```
  `SessionManager` implements `ILayoutSink` backed by its existing YAML `LayoutSession`, and supplies
  the `onLayoutChanged` debounce callback. The toolkit stops including `Core/Session/*` and stops
  calling `SessionManager::Instance()`.
- **Effort/Risk:** small–medium / low (the surface is just the few session hooks). The session +
  layout tests guard the round-trip. **Done-when:** no `src/Core/UI` file includes `Core/Session/*`;
  session/layout tests green. **Depends-on:** AI-1; pairs with AI-3 (uses the `UIHost` callback slot).

---

### AI-6 — Inject theme/color into the widgets (leaf cleanup)
- **Goal:** drop the last `Editor::Instance().GetTheme()` / `Config::` reads from inside generic
  widgets.
- **Scope:** `TreeView.h`, `SingleLineView.h`, `ListSelectionModal.cpp`, `RootView.h`, `TestView`.
- **Approach:** inject a tiny color/theme provider (a few `ColorRGBA` getters) — either via the
  `UIHost` or a `SetColors(...)` on the widget. Pass the "leave_when_switching_view" flag in rather
  than reading global `Config` from `RootView`.
- **Effort/Risk:** small / low. **Done-when:** AI-2 gate is **green** for `src/Core/UI`.
  **Depends-on:** AI-3 (provider can ride on `UIHost`).

---

### AI-7 — (optional) Extract the physical library
- **Goal:** only if a real second consumer appears, or the user wants the hard wall. Cut a CMake
  `goatui` static lib (the `ui_src` group) + `goatui-sdl2`/`goatui-sdl3` backend libs implementing
  the contract; `goatedit` links them.
- **Pre-req:** AI-2 gate green (proves the include list is app-free). Revisit §6(a) (slim UI
  `TextRow`) to lift `Line` out of `BaseController` for a truly self-contained lib.
- **Effort/Risk:** medium (build plumbing) / low once the includes are clean. **Done-when:** `goatui`
  builds standalone with no `goatedit`/app symbols. **Depends-on:** AI-2 green (i.e. AI-3/4/5/6 done).

---

**Decision gate after AI-2-green:** stop at a clean in-tree boundary (achieves reason 1 + unblocks
reason 2) **or** continue to AI-7 (the shipped library). The plan is built so AI-1…AI-6 are worth
doing *even if AI-7 never happens*.

---

## §9 — Open questions / decisions log

- **DECIDED (2026-06-16):** pursue the clean separation; **start with the folder split (AI-1)**.
- **DECIDED (2026-06-16):** the `kAction` split is a go, as its **own action item (AI-4)**.
- **DECIDED (2026-06-16):** harden the `SessionManager`→`ViewBase` seam via **`ILayoutSink` (AI-5)** —
  user confirmed this is a recently-crept-in dependency to catch now.
- **DECIDED (2026-06-16):** physical library (AI-7) is **optional** — value is the clean boundary.
- **DECIDED (2026-06-16):** **full split** — editor views/controllers moved out of `Views/`/
  `Controllers/` too, not left in place. (AskUserQuestion; rejected "smallest diff, keep them in
  `Views/`".)
- **DECIDED (2026-06-16):** editor-specific UI folder name is **`src/Core/Editor/`**
  (`Editor/Views/`, `Editor/Controllers/`). Rejected `AppUI/`, `EditorUI/`, and "keep as `Views/`".
- **RESOLVED (2026-06-16, was open):** `Rect`/`Point`/`ColorRGBA`/`TextAttributes`/`NamedColors`/
  `VerticalNavigationViewModel`/`KeyPress`/`Keyboard`/`Cursor` stay in `src/Core/` root — confirmed
  shared with the document/text model, not UI-exclusive. No `UI/Primitives/` or `UI/Input/` folder.
  See the AI-1 §0 note for the use-site evidence.
- **Open — `kAction` shared-nav set:** put the navigation actions in `kUIAction` (both editor + list
  widgets consume) vs duplicate a nav subset. Leaning shared-in-toolkit (AI-4 sub-decision).
- **Open — `UIHost` shape:** UI-local singleton (smallest diff) vs injected `UIHost&`. Leaning
  singleton for pass 1 (AI-3).
- **Open — `Line` in the UI:** §6 (a)/(b)/(c). Leaning (c)-now → (a)-only-if-AI-7.
- **Open — does `KeyMapping` ship with the UI** or stay app-side feeding resolved actions in? Leaning
  app-side (it's YAML/config-driven).
- **Open — sequence vs the graphics refactor.** AI-3+ share the `DrawContext` primitive work — likely
  the *same* effort. Confirm with the user before AI-3+.
- **Open — `ui_src`/`appui_src` CMake grouping.** AI-1 only moved files/fixed includes; the
  `editorsrc` list in `CMakeLists.txt` is still one flat list (now with the new paths). Splitting it
  into named groups is cosmetic and deferred — revisit alongside AI-7 if the library is ever cut.

---

## §10 — Target layout (AS BUILT, AI-1)

```
src/Core/UI/                 # the generic toolkit
  Graphics/                  #   the rendering CONTRACT (abstract only)
    ScreenBase, WindowBase, DrawContext, NativeWindow, KeyboardDriverBase
                             #   (Cursor.h stayed in src/Core/ root — shared, see below)
  Views/                     #   ViewBase, VisibleView, StackableView,
                             #   V/HStackView, V/HSplitView, RootView, ModalView,
                             #   ListSelectionModal, TreeView, TreeSelectionModal,
                             #   SingleLineView, TestView
  Controllers/               #   BaseController

src/Core/Graphics/SDL2|SDL3|NCurses|Headless   # BACKENDS (depend on UI/Graphics contract)

src/Core/Editor/              # editor-specific UI (DECIDED name, §9)
  Views/                      #   EditorView, EditorViewContainer, EditorHeaderView, HexView,
                              #   GutterView, TerminalView, WorkspaceView, CommandView,
                              #   HSplitViewStatus
  Controllers/                #   EditController, TerminalController, CommandController,
                              #   QuickCommandController
  LineRender.{h,cpp}

src/Core/                     # shared layer (NOT moved — see §9 "RESOLVED" + AI-1 §0 note):
  Rect.h, Point.h, ColorRGBA.h, TextAttributes.h, NamedColors.h,
  VerticalNavigationViewModel.h, KeyPress.h, Keyboard.h, Graphics/Cursor.h
```

`UIHost` (AI-3) and the `kUIAction` split (AI-4) are not built yet — still open work items, sketched
in §8.

---

## §11 — Quick wins found during analysis

- **`src/Core/Graphics/DrawContext.h:11`** `#include "Core/Line.h"` is **unused** — confirmed no
  `gedit::Line` reference in `DrawContext.h` or `DrawContext.cpp` (the `Line`-named things are
  `LineCursor` in `Cursor.h` and overlay-row helpers). Removing it severs a gratuitous
  editor-model → graphics-contract include. *(Safe; do in P0.)*
- `StackableView.h` is already perfectly clean (only `ViewBase.h`) — the model for what a generic UI
  header should look like.

---

## Changelog
- **2026-06-16** — Initial analysis pass. Inventory (§3), full external-dependency map for the
  generic parts (§4), Graphics-seam recommendation (§5), BaseController note (§6), worthwhile-verdict
  (§7). No code moved.
- **2026-06-16** — Turned §8 into a **sequenced action-item plan** (AI-0…AI-7) per user direction:
  folder split first (AI-1), `kAction` split as its own item (AI-4) with design sketch, `UIHost`
  (AI-3) + `ILayoutSink` (AI-5) seam sketches, include-discipline CI gate (AI-2), library extraction
  made optional (AI-7). Updated §0 status + §9 decisions.
- **2026-06-16** — **AI-1 (folder split) DONE**, on branch `refactor-ui`. User chose (AskUserQuestion)
  the **full split** + **`src/Core/Editor/`** as the editor-UI folder name. Moved: graphics contract →
  `UI/Graphics/`, generic views → `UI/Views/`, `BaseController` → `UI/Controllers/`, editor views →
  `Editor/Views/`, editor controllers → `Editor/Controllers/`, `LineRender` → `Editor/`. Fixed every
  `#include` path across `src/`, `utests/`, and the root-level `main.cpp` (the dead, unbuilt
  `tests/testlayout.cpp` `ViewLayout.h` include was left as pre-existing breakage, unrelated to this
  move). Updated `CMakeLists.txt`. **Correction to §10's original sketch:** the shared primitives
  (`Rect`/`Point`/`ColorRGBA`/`TextAttributes`/`NamedColors`/`VerticalNavigationViewModel`/`KeyPress`/
  `Keyboard`/`Cursor`) stayed in `src/Core/` root — confirmed via grep that the document/text model
  depends on them too, so a `UI/Primitives/`+`UI/Input/` folder would have been a false generic/app
  boundary. Full rebuild (`goatedit` + `utests`) clean; verified-green 235-test set: 0 failures.
  §9/§10 updated to match the as-built state.
- **2026-06-16** — **AI-2 (include-discipline smell-test) DONE**, on branch `refactor-ui`. Added
  `scripts/check-ui-boundary.sh` (asserts `src/Core/UI/` carries no `Editor.h`/`RuntimeConfig.h`/
  `Document.h`/`TextBuffer.h`/`Workspace.h`/`Core/Session/*`/`Core/Plugins/*`/`Core/Config/*`
  include) and a non-blocking `continue-on-error` step in `.github/workflows/cmake.yml`. Captured
  the current leak set as the precise AI-3/5/6 work queue: 19 includes / 14 files (16× → AI-3, 2× →
  AI-5, 2× → AI-6, zero editor-model hits). Full breakdown in the AI-2 §8 entry.
