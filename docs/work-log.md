# Work log

Short, reverse-chronological index of completed feature efforts. Each entry summarises *what shipped*
and *the load-bearing decisions*; the linked detail doc carries the full cold-start history. For
known-wrong code deliberately left unfixed, see [`open-bugs.md`](open-bugs.md) (the active tracker, not
logged here).

---

## UI refactor — generic toolkit vs editor-specific UI ✅ (merged to `main`, `d1622a7`)

Split the monolithic `src/Core/Views` + controllers into a **generic UI toolkit** (`src/Core/UI/`) and
**editor-specific UI** (`src/Core/Editor/`). `src/Core/Views/` and `src/Core/Controllers/` no longer
exist. Goal: a clean one-way boundary (toolkit never depends on app services) — both for overview and to
unblock the planned graphics-backend refactor.

- **Folder split** (AI-1): generic views/`BaseController`/graphics *contract* → `UI/`; editor
  views/controllers/`LineRender` → `Editor/`. Shared primitives (`Rect`/`Point`/`ColorRGBA`/…) stayed in
  `src/Core/` root — confirmed shared with the document model, not UI-exclusive.
- **Include-discipline CI gate** (AI-2): `scripts/check-ui-boundary.sh` fails if `src/Core/UI/` includes
  any app header (`Editor.h`/`RuntimeConfig.h`/`Document.h`/`Core/Session|Config|Plugins/*`). Now
  **blocking** in CI; reports clean.
- **Three chokepoints severed by injection, not direct calls:** `UIHost` replaces the `RuntimeConfig`
  service-locator (AI-3); `ILayoutSink` replaces `SessionManager`/`LayoutSession` in `ViewBase` (AI-5);
  theme colors + quick-command policy injected via `UIHost` + a `std::function` hook instead of
  `Editor::Instance().GetTheme()`/state reads (AI-6). Each glued in the init layer (`Editor::Initialize`
  / `SetupSDL*`), mirroring `WireScreenGeometry`.
- **`kAction` split** (AI-4): toolkit-owned `kUIAction` (`src/Core/UI/Input/UIAction.h`) carries the
  shared nav + view-management actions; `kAction` keeps editor actions. `EditorAction`/`ActionItem`
  carry both fields side by side. Follow-up: enumerators renamed to the `kUIAction<Value>` prefix
  (house convention — members repeat the enum's own type name).
- **AI-7 (physical `goatui` library): DROPPED** — the in-tree boundary serves both drivers; a shipped
  library isn't worth its carrying cost without a real second consumer. CMake `ui_src`/`appui_src`
  grouping deferred indefinitely (cosmetic).

Detail + decision log: [`ui-refactor.md`](ui-refactor.md).

---

## Markdown syntax highlighting — v1 ✅ (in the verified-green set)

`.md`/`.markdown` routes to `MarkdownLanguage`. Push/pop tokenizer states for fenced code (persists
across lines), code spans, strong/emphasis, links; line-anchored block syntax (ATX headings, blockquote,
ordered/unordered list markers, thematic breaks) via a new injected `LangLineTokenizer::PostLineCallback`
→ `LanguageBase::OnPostProcessParsedLine`. Goal was *decent* highlighting, not a CommonMark parser
(spec-exact emphasis / setext / reference links are explicit non-goals). Module `markdown` green.

Remaining (optional/aesthetic): a GUI color pass to retune the `md_*` placeholder colors; the §6 lexer
work is optional. Detail: [`support-markdown.md`](support-markdown.md).

---

## Session cache — Phase 1 ✅ (merged; Step 2 deferred)

Per-root session persistence: open documents, layout (splitters + focused top-view + tree
expand/collapse), window geometry, and debounced autosave all round-trip (GUI-verified). The old
*global* window-geometry file (`WindowLocation`/`gedit_lastwinloc.yml`) was removed — geometry is now
per-root session state and **no rendering backend does file I/O**. `SessionManager` (singleton) is the
sole owner of session disk I/O; a session belongs to an open **folder**, never a single file.

**Step 2 — live registry + cold-start restore of multiple instances (§3.5) — is DEFERRED** (decided
2026-06-16): registry/restore/paths files don't exist yet; revisit when the cold-start work is picked
up. Other deferred items: doc paths stored absolute (→ relativise), `LoadDocument`↔`ReopenDocument`
consolidation, gated undo persistence. Detail + phasing: [`session-cache.md`](session-cache.md).
