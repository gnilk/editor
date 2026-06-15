# Markdown language support — implementation plan

Plan for adding `.md` syntax highlighting to GoatEdit. Written 2026-06-15 after an architecture
read of `src/Core/Language/`. Pick up cold from §0.

Goal: **decent** highlighting (a syntax highlighter, not a CommonMark parser). Spec-exact emphasis,
setext headings, and reference-link resolution are explicit non-goals — see §5.

---

## 0. Resume point / status

**Status: skeleton + §3 + §2.A + §2.B DONE.** `.md`/`.markdown` routes to `MarkdownLanguage`. §3 added 8
generic document/markup classes + `tokenNames` + theme colors. §2.A push/pop states: `in_fence`
(persists across lines), `in_code_span`, `in_strong` (`**`), `in_em` (`*`), `in_link` (`[text]`).
§2.B line-anchored block syntax via `OnPostProcessParsedLine`: ATX headings `#`..`######`, blockquote
`>`, unordered (`-`/`*`/`+`) + ordered (`1.`/`1)`) list markers, thematic breaks (`---`/`***`/`___`),
all fence-guarded (a line starting inside ``` is left as code). Test module `markdown` = 11 cases green;
broad language/theme/indent regression green (126/0).

**Plumbing added for §2.B (note for future markup langs):** the tokenizer gained an injected
`PostLineCallback` (`LangLineTokenizer::SetPostLineCallback`), invoked at the end of `ParseLine` (while
the line is locked) for every line `ParseLines`/`ParseRegion` touches. `LanguageBase`'s ctor binds it to
the virtual `OnPostProcessParsedLine` (base = no-op). The make/command parser is unaffected — it uses
`ParseLineFromState` (not `ParseLine`) and still calls `OnPostProcessParsedLine` explicitly. The callback
must NOT re-lock the line or re-enter the tokenizer (state stack is live mid-parse).

**Known limitations (acceptable v1, candidates for later):**
- A `*` bullet's item text is reset to regular (the leading `*` mis-tokenizes as emphasis with no
  line-start awareness); inline code/emphasis *inside* a `*`-bulleted item won't highlight. `-`/`+`
  bullets keep full inline highlighting. (Prefer `-` bullets, or do the §6 lexer extension.)
- `[text](url)` colours only `[text]` (the `(url)` trailer stays regular).
- See §5 for the hard non-goals (setext, reference links, CommonMark-exact emphasis).

**Next move:** GUI eyeball with `Assets/testfiles/support-markdown.md`, retune the placeholder colors in
`colors.json`, add `markdown` to the verified-green `-m …` set in CLAUDE.md. Optionally §6 (lexer
line-start awareness) to fix the `*`-bullet limitation properly.

Suggested scope for a first sitting: **§2 + §3 + register the language** (≈ one focused session,
comparable effort to writing `CPPLanguage`). §4 tests and §5 niceties can follow.

---

## 1. Why the existing construct fits (and where it doesn't)

`LangLineTokenizer` (`src/Core/Language/LangLineTokenizer.{h,cpp}`) is a **stack-based, line-at-a-time
lexer**. A `LanguageBase` subclass only configures states in `Initialize()` (see `CPPLanguage.cpp` /
`JSONLanguage.cpp` as templates). Relevant properties:

- State persists across lines (the stack is NOT reset per line) → multi-line constructs like C++ block
  comments work for free. **Fenced code blocks get this for free too.**
- Tokenizing splits on whitespace + operator/postfix chars, longest-match-first; per-token push/pop
  actions move between states; an EOL action can pop a line-terminal state.
- Two escape hatches already exist:
  - per-state pluggable matcher `NumberMatcherBase::Match(view) -> length`, consulted before
    identifier matching (currently only numbers).
  - `LanguageBase::OnPostProcessParsedLine(Line::Ref)` — a whole-line post-pass that can rewrite
    attributes. Only `MakeBuildLang` uses it today (error detection). **This is our main lever for
    Markdown's line-anchored block syntax.**

**The one structural gap:** the tokenizer has **no beginning-of-line awareness** — a `#` is classified
identically mid-sentence and at column 0. C++ tolerates this (stray `#` is rare); Markdown can't (`#`,
`>`, `-`, `*` mid-line are common prose). We resolve this in the post-process pass, NOT the core lexer
(see §2.B). There is also no lookahead/lookbehind and no whole-document pass — that bounds the non-goals
in §5.

---

## 2. Construction

### 2.A  State config (in `MarkdownLanguage::Initialize()`) — handles inline + fenced spans

These are symmetric-delimiter, push/pop constructs that map directly onto the JSON/CPP string &
block-comment pattern:

- **Fenced code ``` ``` ``` / `~~~`** — state `in_fence`, pushed on the fence open, **no EOL action** so
  it persists across lines, popped on the closing fence. Model after CPP `in_block_comment`.
  `regularTokenClass = kMDCodeSpan` (or a dedicated `kMDCodeBlock`).
  - CAVEAT: do **not** classify the fence as `kCodeBlockStart/End` — `ParseLine` keys auto-indent off
    those classes (`LangLineTokenizer.cpp` ~line 140), which would inject spurious indentation. Use a
    Markdown-specific class.
- **Inline code `` `…` ``** — state `in_code_span`, pushed/popped on `` ` ``, EOL action = pop (inline
  code does not span lines). Model after `in_string`.
- **Emphasis `**`/`__`/`*`/`_`** — states `in_strong` / `in_em`, push/pop. Longest-match-first in the
  lexer already makes `**`(2) win over `*`(1). Accept that this is approximate (see §5).
- **Links `[text](url)`** — operators `[ ] ( )`; optionally a state for the URL portion. Low priority;
  a flat operator classification already reads fine.

Register start state `main`, call `ConfigFromNodeName("markdown")` (mirrors the other languages — lets
`autopairs.yml` / config hang off the node).

### 2.B  `OnPostProcessParsedLine(Line::Ref)` — handles block-level (line-anchored) syntax

This is the part the core lexer can't do. After the generic parse, inspect the **first non-whitespace
token** and rewrite the line's attributes:

- `#` … `######` at line start → heading (whole line, or just through the text).
- `>` at line start → blockquote.
- `-` / `*` / `+` / `1.` `2.` … at line start (allowing leading indent) → list marker.
- 4-space / tab leading indent on an otherwise-plain line → indented code block.
- `---` / `***` / `___` alone on a line → thematic break (horizontal rule).

`MakeBuildLang::OnPostProcessParsedLine` is the precedent for iterating a line's attributed parts
(`Line::IterateWithAttributes` / `Line::Attributes()`); reclassify by editing the `LineAttrib` entries
(set `.tokenClass` + colors via `Editor::Instance().ColorFromLanguageToken`). Keep this guarded against
empty lines and the `in_fence` state (don't reinterpret `#` etc. inside a fenced code block — check the
line's state-stack depth / starting state before reclassifying).

---

## 3. Token classes + theme (mechanical, but multi-file — do first so nothing `exit(1)`s)

`LanguageTokenClassToString` (`LangToken.cpp`) calls `exit(1)` on any class missing from the
`tokenNames` map, and `Editor::ConfigureTheme` warns on any class missing a theme color. So adding a
class is a 3-touch change:

1. `src/Core/Language/LanguageTokenClass.h` — add entries **before** `kLastTokenClass` (it's the numeric
   end-sentinel; `IsLanguageTokenClass` iterates `[0, kLastTokenClass)`). Renumber `kLastTokenClass` and
   `kFunky` accordingly.
2. `src/Core/Language/LangToken.cpp` — add a matching `{kMD…, "md_…"}` entry to the `tokenNames` map.
3. Theme files — add a color for each new key (e.g. `md_heading`, `md_emphasis`, `md_code`,
   `md_list_marker`, `md_quote`, `md_link`) under `contentColors` in the `.theme.yml` (and the runtime
   default in `Assets/Resources/config.yml` if colors are seeded there). `hexview` is the precedent for a
   recently-added content color — grep it to find every file that needs the key.

**Decision to make:** new dedicated classes (cleaner theming, more files) vs. reuse existing
(`kKeyword` for heading, `kString` for code, `kOperator` for markers — zero enum/theme work, but
Markdown shares colors with code). Recommend **a small set of new classes** (heading, emphasis, code,
marker/quote) for distinct theming; reuse `kOperator` for the rest.

---

## 4. Registration + tests

- **Register:** in `Editor::ConfigureLanguages()` (`Editor.cpp` ~line 619), mirror the JSON block:
  `auto md = MarkdownLanguage::Create(); md->Initialize(); RegisterLanguage(".md|.markdown", md);`
- **Files:** `src/Core/Language/LanguageSupport/MarkdownLanguage.{h,cpp}` (`Create()` factory +
  `Ref` alias, like the others).
- **Tests:** new `utests/test_markdown.cpp`, module `markdown`. Follow `test_cpplang` / `test_jsonlang`.
  Assert the *property* (per the test conventions in CLAUDE.md), e.g.:
  - fence open → following lines carry the code class until the closing fence (cross-line state).
  - `# Heading` → heading class; a `#` mid-sentence does NOT.
  - inline `` `code` `` classed as code; `*em*` / `**strong**` distinguished.
  - a `#` inside a fenced block stays code (the §2.B guard).
  - Add `markdown` to the verified-green `-m …` set in CLAUDE.md once it passes.
- A fixture `Assets/testfiles/sample.md` (mixed headings/lists/fences/emphasis) doubles as the GUI
  eyeball file.

---

## 5. Explicit non-goals (architecture can't, and that's fine for "decent")

- **Setext headings** (`===` / `---` underline under text) — needs next-line lookahead; the lexer is
  strictly forward, one line at a time. (A `---` on its own line will read as a thematic break, which is
  an acceptable approximation.)
- **Reference-style links** `[text][ref]` … `[ref]: url` — needs whole-document resolution.
- **CommonMark-exact emphasis** (flanking rules, nested `***`, intraword `_`) — greedy push/pop is
  approximate; literal `*`/`_` in prose will occasionally false-trigger. Acceptable for highlighting.

If any of these later become must-haves, they imply a different (document-level, multi-pass) parser —
out of scope for this construct.

---

## 6. Optional later: line-start matching in the lexer itself

If the post-process approach (§2.B) feels too much like a second parser, the "proper" alternative is a
contained tokenizer extension: pass the in-line column/offset (or a "first-token-on-line" flag) into the
per-state matcher hook, so a Markdown matcher can match `#`/`>`/list-markers only at line start. This
keeps block logic in the lexer instead of the post-pass. Not needed for v1; noted so the option isn't
lost. Touch point: `LangLineTokenizer::GetNextToken` (the matcher is consulted there) +
`NumberMatcherBase` signature (would generalize to a `LineMatcher`).
