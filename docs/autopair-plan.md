# Auto-pairing engine — plan

A data-driven bracket/quote auto-pairing engine, configured from a standalone YAML file, replacing the
half-baked per-language C++ experiment.

## Why

Auto-pairing exists today only as an embryo wired into the language hooks, and it is ad-hoc C++ per
language with the hard guards missing:

- `CPPLanguage::OnPostInsertChar` (`CPPLanguage.cpp:175`) inserts a matching `}` / `]` / `)` after the
  opener — but every branch carries a `FIXME: check if chars to right are whitespace`, so it happily
  produces `(|)word`.
- `CPPLanguage::OnPreInsertChar` (`:156`) skips over `)` via a heuristic (`line ends with )`) and yanks the
  cursor back a tab on `}`.
- No quotes. No selection-wrap. No backspace-deletes-pair. No "should I auto-close *here*" guard.
- `JSONLanguage` also overrides the hooks (`JSONLanguage.h:27-28`).
- The decision is smeared across **two** hooks (`OnPreInsertChar` *and* `OnPostInsertChar`,
  `EditController.cpp:64,75`), gated by a `FIXME: rename` config flag `enable_pre_post_insert` (`:60`).

The *seam* is right — "the language rule-set controls this" is how modern editors do it — but the
implementation is the wrong shape: imperative, per-subclass, incomplete.

## Target — three clean separations

| Piece | What it is | Notes |
|---|---|---|
| **`AutoPairTable`** | the *data*: pair list (`open`/`close`/`quote`), `autoCloseBefore` set, `suppressIn` token-classes | built from YAML; one per language identifier, `inherit`-resolved |
| **`AutoPairEngine`** | the *logic*: `Decide(context) → Action`, pure, no UI, no tokenizer dependency | the tricky guard logic lives here, written **once** |
| **selection by identifier** | the table is looked up by `language.Identifier()` (`"cpp"`/`"json"`/…), **not** the C++ class | decouples from the class-per-language hierarchy; works for plaintext (no class) |

**Key architectural stance:** the engine is **not** new `virtual` hooks on the language subclass and is
**not** part of `LangLineTokenizer` (which stays classification + indent only). It is a standalone pair
(logic + data) selected by identifier. This is a deliberate step *away* from the class-per-language smell
(behaviour moves from C++ subclass → declarative data), while staying fully independent of whether
languages later become data — we do **not** refactor the language architecture as part of this work.

### Per-line metadata — none needed (settled)

The only context the engine needs beyond the line text + neighbour chars + cursor + selection is **"what
token class is at the cursor"** (to suppress pairing inside strings/comments and to drive quote logic).
That is already available: `Line::LineAttrib` carries `kLanguageTokenClass tokenClass`, stamped onto every
token span during normal parsing (`LangToken::ToLineAttrib`, `LangToken.cpp:66`), queryable via
`Line::AttributeAt(cursorX)->tokenClass`. **No new `Line` fields, no tokenizer prerequisite.**

## The YAML config

A standalone file (`Assets/Resources/autopairs.yml`) referenced from `config.yml` `main.autopairs:`
(mirroring `main.theme:`), self-contained so it is independently unit-testable (like the keymap files). A
`generic` base section; languages `inherit` it and override/append; **intra-file** inheritance (resolved
within this one file, by section name). Keys follow the project's config convention (lowercase sections,
**snake_case** fields), and sections are keyed by the language's **config name** — the same short name as
`config.yml`'s `languages:` section (`cpp`, `json`, `make`, `default`), *not* `Identifier()` (which is
`c/c++`).

```yaml
autopairs:
  generic:
    suppress_in: [string, comment]              # token-class at cursor where auto-close is OFF
    auto_close_before: [")", "]", "}", ",", ";", whitespace, eol]
    pairs:
      - { open: "(",  close: ")" }
      - { open: "[",  close: "]" }
      - { open: "{",  close: "}" }
      - { open: "\"", close: "\"", quote: yes }
      - { open: "'",  close: "'",  quote: yes }
  cpp:
    inherit: generic
    pairs: [ { open: "<", close: ">" } ]         # appended (replaces an inherited pair with the same open)
  json:
    inherit: generic
```

**Selection rule:** look up the section by the language's config name. **No section ⇒ no pairing** —
plaintext (`readme.txt`, the `default` language) gets nothing. `generic` is a base to *inherit*, never
auto-applied. Bridging the runtime language → its config name needs `LanguageBase::GetConfigNodeName()`
(`ConfigFromNodeName` copies the node but doesn't currently retain the name) — added in Step 3.

**Loading/inheritance owner:** one cache (mirroring `KeyMappingCache`): build each table once, resolve the
intra-file `inherit` chain through itself, cache by identifier. The resolver appends the parent's resolved
rules (child first ⇒ child overrides parent). Cycle-guarded.

## The engine — decision model

`AutoPairEngine::Decide(context) → Action`, where:

**context** = `{ lineText, cursorX, selectionActive (+range), trigger, tokenClassAtCursor, table }`
where `trigger` is either a typed `char32_t` or a `Backspace` marker.

**Action** (the controller applies exactly one):
- `None` — proceed with default editing.
- `Insert{ text, cursorCol }` — replace the default insert: e.g. open char ⇒ insert `"()"`, cursor between.
- `Wrap{ openStr, closeStr }` — selection active: put `open` before / `close` after the selection.
- `SkipOver` — don't insert; step the cursor one char to the right (over the existing closer).
- `DeletePair` — backspace on an empty pair: delete the char before **and** the matching closer after.

**Rules (the v1 contract — these are the unit tests):**
- **Open bracket typed:** selection active → `Wrap`. Else if `tokenClassAtCursor ∉ suppressIn` *and*
  (cursor at EOL **or** next char ∈ `autoCloseBefore`) → `Insert` the pair. Else → `None`.
- **Close bracket typed:** next char == this closer → `SkipOver`. Else → `None`.
- **Quote typed:** selection active → `Wrap`. Else if next char == quote → `SkipOver`. Else if the quote
  would open inside an existing string/identifier (heuristic: previous char is identifier/quote, or
  `tokenClassAtCursor == string`) → `None` (handles `don't`, char-in-string). Else → `Insert` the pair.
- **Backspace:** char-before is an opener *and* char-after is its matching closer (adjacent empty pair) →
  `DeletePair`. Else → `None`.

Skip-over uses the simple "next char == closer" heuristic in v1; tracking *which* closers we auto-inserted
(so skip-over only applies to those) is a v2 nicety (see Deferred).

## Wiring into `EditController` — one decision point

Collapse the pre/post split into a single consult. Concrete seams (traced 2026-06-11, the spec for the
GUI-verified pass):

**Prereq — `LanguageBase::GetConfigNodeName()`.** `ConfigFromNodeName(nodeName)` (`LanguageBase.cpp:20`)
copies `languages.<name>` into the language but drops the *name*. Add `std::string configNodeName;` + a
getter; set it at the top of `ConfigFromNodeName`. The engine table is fetched with
`AutoPairCache::Instance().GetTableForLanguage(lang.GetConfigNodeName())`. (Confirm `JSONLanguage`/
`DefaultLanguage::Initialize` call `ConfigFromNodeName`; CPP does, `CPPLanguage.cpp:143`. A language that
never calls it ⇒ empty name ⇒ no section ⇒ no pairing, which is a safe default.)

**InsertPair + SkipOver — in `HandleKeyPress` (`EditController.cpp:56-83`), the one function:**
- Build context: `table` (above), `lineText = line->Buffer()`, `cursorX = cursor.position.x`,
  `selectionActive = false` (selection is already resolved upstream — see Wrap), `tokenClassAtCursor` from
  `line->AttributeAt(cursor.position.x)->tokenClass` (guard empty `Attributes()` / past-EOL → `kRegular`).
- `kInsertPair`: `DefaultEditLine(...)` inserts the opener and advances the cursor + sets `wantedColumn`
  (via `AddCharToLine`, `BaseController.cpp:63-66`); then `line->Insert(cursor.position.x, action.close)`
  drops the closer *without* moving the cursor → `(|)`.
- `kSkipOver`: insert nothing; `cursor.position.x++` and recompute
  `cursor.wantedColumn = line->CharToVisualColumn(cursor.position.x, document->GetTabSize())` (don't leave
  it stale — that drives vertical nav). One undo item still brackets the no-op for symmetry.
- `kNone`: fall through to the existing `DefaultEditLine`.

**WrapSelection — in `OnKeyPress` (`EditController.cpp:154-164`), BEFORE the selection-delete block:** the
existing code deletes the selection then inserts, so wrap must intercept first (it needs the selection
content). If printable + selection active + engine says `kWrapSelection`, surround the selection with
`open`/`close` and return; else keep the current delete-then-insert.

**DeletePair — the in-line backspace path:** `HandleSpecialKeyPress` → `DefaultEditSpecial` does the
single-char delete; intercept the backspace key there (engine `OnBackspace`) and delete both halves of an
adjacent empty pair before the default.

**Delete the experiment:** `CPPLanguage`/`JSONLanguage` `OnPreInsertChar`/`OnPostInsertChar` overrides, the
`LanguageBase` hook decls for those two, and the `enable_pre_post_insert` flag (`config.yml:94` +
`EditController.cpp:60`; repurpose to `enable_autopair` if a kill-switch is wanted). **Keep
`OnPreCreateNewLine`** — indent engine's concern. (This also drops the `}`-dedent-on-type, per the decided
option a.)

**Verification:** drive `HandleKeyPress`/`OnKeyPress` from a unit test (the seam exists —
`test_document_ins_keypress`, `EditController::Create(document)`) to assert buffer + `cursor.position.x`
for insert-pair / skip-over / no-pair-mid-word / suppress-in-comment / backspace-pair. Then the SDL
on-screen confirm (cursor feel, undo grouping, no pairing in `readme.txt`).

**Suggested sub-split for the GUI-verified pass:** 3a = `GetConfigNodeName` + `HandleKeyPress`
InsertPair/SkipOver + delete experiment + unit tests (one function, lowest risk, the headline feature);
3b = DeletePair (backspace) + WrapSelection (the two cross-path cases).

**STATUS (2026-06-11): 3a + DeletePair DONE; WrapSelection deferred to 3b.**
- `LanguageBase::GetConfigNodeName()` added (set in `ConfigFromNodeName`); `configNodeName` member.
- `EditController::BuildAutoPairContext` is the single glue point (table by `GetConfigNodeName()`,
  `tokenClassAt` via `Line::AttributeAt`). `HandleKeyPress` does InsertPair (DefaultEditLine + `line->Insert`
  the closer) and SkipOver (`cursor.x++` + recompute `wantedColumn`); `HandleSpecialKeyPress` does DeletePair
  on backspace between an empty pair. The `CPP`/`JSON` `OnPre/OnPostInsertChar` experiment + the
  `enable_pre_post_insert` flag are deleted (the `}`-dedent dropped, per option a).
- Verified: 4 integration tests in `test_document` (insert-pair / skip-over / no-pair-mid-word /
  backspace-pair) assert the buffer + cursor through `EditController`; engine logic covered by `test_autopair`.
  Verified-green set **172**. **GUI on-screen confirm still pending** (cursor feel, undo grouping, no pairing
  in `readme.txt` / inside a comment).
- **3b: WrapSelection — DONE.** `EditController::TryWrapSelection` runs in `OnKeyPress` before the
  selection-delete block: it asks the engine (selectionActive=true) and, on a wrap, inserts the closer at
  `Selection::GetEnd()` then the opener at `GetStart()` (closer-first so the opener doesn't shift it),
  parks the cursor after the closer, and cancels the selection. Works single- and multi-line. v1 cancels
  the selection rather than re-selecting the wrapped text (a possible later nicety). Covered by
  `test_document_autopair_wrap`. Verified-green set **173**.

**Auto-pairing v1 is feature-complete** (insert-pair, type-through, backspace-pair, suppress-in-string/
comment, quotes, selection-wrap; `<>` intentionally excluded). Remaining: GUI feel-check of wrap + the
eventual indent engine.

## Ordered steps — each compiles, keeps the verified-green set green

- **Step 0 — Pin the contract (test-first).** New `utests/test_autopair.cpp` (`test_autopair` module +
  CMake entry). Discriminating cases against the not-yet-built engine + a hand-built `AutoPairTable`: guarded
  auto-close (EOL vs before-word vs before-closer), skip-over, wrap, backspace-pair, quote open/skip,
  `suppressIn` string/comment, and **empty table ⇒ always `None`** (plaintext). Red first.
- **Step 1 — `AutoPairTable` + `AutoPairEngine`.** Structs + `Decide`. Pure, in-memory table (no YAML yet).
  Turn Step 0 green.
- **Step 2 — YAML loader + inheritance.** `autopairs.yml` (`generic`+`cpp`+`json`), referenced from
  `config.yml`; a build-once/`inherit`-resolving cache keyed by identifier. Loader tests (load → resolve
  `cpp inherit generic` → assert the merged table; no-section → empty). Green.
- **Step 3 — Wire into `EditController`; delete the experiment.** Single decision point + backspace hook;
  delete the `CPP`/`JSON` hook overrides + `LanguageBase` decls + the old flag. Live-verify on SDL (type
  `(`, `"`, wrap a selection, backspace an empty pair, confirm no pairing in a comment / in `readme.txt`).
  Green.

Per repo discipline: reproduce/contract tests precede the code; the engine is fully covered before it is
wired into the live edit path.

## Decisions — locked (confirmed before planning)

1. **Delete the `CPPLanguage`/`JSONLanguage` `OnPre/OnPostInsertChar` experiment**; everything routes
   through the new engine.
2. **Single decision point** in the `EditController` insert path (replaces the pre/post hooks).
3. **v1 rule set** = guarded auto-close + skip-over + selection-wrap + backspace-deletes-pair + quote
   open/skip. Skip-over by "next char == closer" heuristic.
4. **Test-first**: `AutoPairTable` loader + `AutoPairEngine` with a `test_autopair` module *before* wiring.

## Decided — the `}` auto-dedent drops in v1 (option a)

`CPPLanguage::OnPreInsertChar` currently dedents the cursor a tab when you type `}` (`CPPLanguage.cpp:158`).
Deleting the experiment removes that. Dedent-on-type is an **indent-engine** concern, not auto-pairing, and
the indent engine is a separate future task. **Decision: accept the temporary loss** of `}`-dedent-on-type
and re-home it properly when the indent engine is built (it already re-derives indent from the parser in
`Document::NewLine`). No stopgap in the controller — keep the auto-pair work clean of indent logic.

## Out of scope / deferred

- **Indent engine** — newline auto-indent already works (tokenizer brace-depth → `Document::NewLine`); the
  `}`/dedent-trigger consolidation and the `}`-on-type dedent are a *separate* task. `OnPreCreateNewLine`
  stays as-is.
- **v2 auto-pair niceties** — track auto-inserted closer regions (so skip-over/backspace-pair only apply to
  pairs *we* created), multi-char pairs, `surroundingPairs` distinct from `autoClosingPairs`, language
  config for the *tokenizer* itself.
- **Language-architecture refactor** (dissolving class-per-language into data) — this engine is a step in
  that direction but explicitly does not perform it.
