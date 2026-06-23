# Deferred items

Work that was **explicitly deferred or left out of scope** from otherwise-shipped features. Each entry
points back to the source doc (now under [`done/`](done/) or [`partially_done/`](partially_done/)) and
the section that has the full context. This is *deferred* work — distinct from active bugs
([`open-bugs.md`](open-bugs.md)) and the planned-but-unstarted efforts indexed in
[`work-log.md`](work-log.md).

---

## From [`done/folder-scanner.md`](done/folder-scanner.md)

- **FS-5 — monitor reuse: scan rooted at a newly-created directory.** Deferred because the folder
  monitor is disabled; the continuous-producer path is also gated on `open-bugs.md` #6 (the
  `Runloop::SwapQueues` race). See §0.1 / §7.

## From [`done/session-cache.md`](done/session-cache.md) — Phase 1 shipped, the rest deferred

- **Step 2 — live session registry + cold-start restore of multiple instances.** The registry / restore
  / paths machinery (incl. FNV-1a root-key, central outside-home fallback, `persist_external`) doesn't
  exist yet; deliberate decision (2026-06-16) to do other work first. See §3.5 / §10.
- **Relativise document paths.** Doc paths are stored as-is (likely absolute) — relativise-to-root for
  portability later (§0 "Known/deferred").
- **Consolidate `Editor::LoadDocument` ↔ `Workspace::ReopenDocument`.** Both run the same open sequence;
  extract one shared open primitive. Left as its own separately-verified change, not mid-feature
  (`FIXME(consolidation)` in `ReopenDocument`, §9).
- **Gated undo-history persistence** — not carried in Phase 1.

## From [`done/support-markdown.md`](done/support-markdown.md) — v1 shipped, optional/aesthetic tail

- **§6 line-start matching in the lexer itself.** Push a first-token-on-line flag into the matcher hook
  so block markers (`#`/`>`/list markers) match only at line start, instead of the §2.B post-pass —
  fixes the `*`-bullet limitation properly. Not needed for v1.
- **Link `[text](url)` token support.** Low priority; the flat operator classification already reads
  fine (§2.A).
- **GUI color retune** of the `md_*` placeholder colors — purely aesthetic (§0).

## From [`done/mouse-support.md`](done/mouse-support.md) — explicitly out of scope (for now)

- Terminal mouse (click / scrollback), drag-select, double-click word-select, gutter click, mouse inside
  modals, and keymap-configurable wheel (lines-per-notch / natural-vs-inverted). See "Explicitly out of
  scope" + "Open questions".

## From [`done/cmake-cleanup.md`](done/cmake-cleanup.md) — all CM items done, two non-blocking follow-ups

- **Verify the release workflow** by pushing a `v*` tag.
- **Full fix for `open-bugs.md` #7** — drop the `cwd → $HOME` fallback.

---

## Dropped (not deferred)

- **ui-refactor AI-7 — a shipped, exported `goatui` library.** *Dropped*, not deferred: the in-tree
  boundary already serves both drivers, so a shipped library isn't worth its carrying cost without a
  real second consumer. The cosmetic `ui_src`/`appui_src` CMake grouping it implied landed as
  cmake-cleanup CM-3. See [`done/ui-refactor.md`](done/ui-refactor.md) §0.
