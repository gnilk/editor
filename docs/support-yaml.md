# YAML language support - implementation plan

Plan for adding `.yml` / `.yaml` syntax highlighting to GoatEdit. Written 2026-11-11 after a read of
`src/Core/Language/` and the Markdown effort (`done/support-markdown.md`), which this follows closely. Pick
up cold from §0.

Goal: **good** highlighting for the YAML people actually write (config files, CI workflows, the editor's
own `config.yml` / keymaps / themes). It is a highlighter, not a YAML parser. The hard non-goals are in §7.

---

## 0. Resume point / status

**Status: Phases 1-3 done on `feat/yaml-lang`** (2026-11-11). Phase 1: `YAMLLanguage` for `.yml|.yaml`,
comments + quoted strings, `#` line comment, `tabsize: 2`. Phase 2: `kConfigKey` class (+ `config_key` theme colour,
importer row), post-pass `YAMLLinePass` (§2.B steps 1-6), Markdown's span helpers lifted into
`src/Core/Language/LineAttribEdit`. Phase 3: block scalars through two tokenizer additions (§4 as built
below). Module `yamllang` (38 cases) green, verified-green set 684; lexer throughput unchanged
(`gedit-bench`, ~860k lines/s single thread before and after). Module `yamllang` is 44 cases now, verified-green set 693.
Phases 1-3 verified by the user in the app,
ANSI backend (2026-11-11). Keys are teal (`config_key` = `blue5`, as C++ class names): red read as errors,
and the macro yellow is taken by anchors/aliases. Phase 4 built (not yet tried in the app): see below.
Next move: try Enter / reformat in a .yml file, then merge.

**Phase 4 as built:** `indent.yml` `yaml:` section (indent after `: | > [ {`, dedent on `] }`) and
`autopairs.yml` `yaml: { inherit: generic }` (`don't` was already safe: no quote pair after a word char).
Two additions the plan didn't have:
- `reindent: no` (`IndentTable::isReindentable`): reformat leaves every YAML line alone (-1, as a frozen line).
  The opener/closer walk sees `key:` open and nothing ever close, so it would staircase the file.
- `LanguageBase::GetContinuationColumn` -> `IndentEngine::Context::referenceColumn`: the column Enter measures
  from. YAML: `- name: x` -> the key's column (the item's next key lines up with it); `- a` -> the dash, for
  `- b`. Text-based (quotes and ` #` respected), on the text before the cursor.
Known gap: `|-` / `>+` headers don't indent the next line (the trigger is the last char; `-` would also fire on
`---`).



**§4 as built:** `kAction::kPushStateAtEOL` - the header marks a pending push; a later token at the same
stack depth (taken before that token's own action, so `| "x"` cancels) that is not a comment or another
header cancels it; the EOL flush performs it. `State::SetIndentScoped` + a `scopeIndents` stack kept in
lockstep with `stateStack` by `PushState`/`PopState`. `PopIndentScopedStates` runs at the start of each
non-blank line, **before** `ParseLine` stamps the depth (so the line that ends a block is clean) and in
`TokenizeText`. The pushed indent comes from `SetScopeIndentFunction`: YAML's is the key's column (past
`- ` markers), or the dash's column for a bare `- |` - with the plain line indent, `- run: |` would have
swallowed its sibling `name:` in every GitHub workflow. Mutations checked: no pop in `ParseLine` (9 red), no pop
in `TokenizeText` (1 red), pop after the stamp (9 red).

Found on the way:
- `'it''s'` needs no escape token - the doubled quote closes and reopens the string, which colours the
  same. Only `\"` / `\\` in double quotes are real escapes.
- `kConfigKey` is **27, not appended**: `[kUserType, kLastTokenClass)` is the semantic range
  (`SemanticNames::IsSemanticClass`, the structure scanner), so a class added at the end would count as a
  code-index name. The semantic classes moved up by one; the only on-disk class numbers are in terminal
  scrollback (`SaveWithAttributes`), which never holds semantic classes.
- The keymaps' multi-line `{ … }` blocks come out right without multi-line flow support: each line is
  `key: value,` and reads as a block key.

Decisions (all taken as recommended, 2026-11-11):
- D1 (§3): one new token class `kConfigKey`, rather than reusing `kKeyword` for keys. **Recommended: new class.**
- D2 (§4): block scalars through a small tokenizer extension (indent-scoped states). **Recommended: yes,
  as Phase 3, after the line-local parts ship.**
- D3 (§2.A): quoted strings end at end-of-line. **Recommended: yes for v1** (multi-line quoted scalars are
  rare, and an unclosed `'` must not colour the rest of the file).

---

## 1. Why the existing construct mostly fits, and where it doesn't

`LangLineTokenizer` is a stack-based lexer, one line at a time; state persists across lines; per-state
push/pop actions, an EOL action, and a post-line hook (`LanguageBase::OnPostProcessParsedLine`, added for
Markdown) that can rewrite a line's attributes. See `done/support-markdown.md` §1 for the full read.

YAML splits cleanly in three:

| Construct | Scope | Where it goes |
|---|---|---|
| Comments `#`, quoted strings `"…"` `'…'` | inside one line, delimiter driven | tokenizer states (§2.A) |
| Keys `key:`, list markers `- `, document markers `---` `...`, typed scalars (`true`, `42`, `null`), anchors / aliases / tags | line anchored, needs "what comes before the `:`" | post-pass (§2.B) |
| Block scalars `key: |` / `key: >` | **several lines, ended by indentation** | tokenizer extension (§4) |

The first two are the same split Markdown used. The third is new: the lexer has no idea of indentation,
and a block scalar ends at the first line that is not indented deeper than its key. Without §4, the body of
a `run: |` script is highlighted as YAML (a `foo: bar` inside a shell script reads as a key). That is
wrong but harmless, which is why §4 can come last.

---

## 2. Construction (`YAMLLanguage`)

Files: `src/Core/Language/LanguageSupport/YAMLLanguage.{h,cpp}` (`Create()` factory, `Identifier()` =
`U"yaml"`, `GetLineComment()` = `U"#"` so line-comment toggling works, `OnPostProcessParsedLine` override).
Register in `Editor::ConfigureLanguages()` (`src/Core/Editor.cpp`): `RegisterLanguage(".yml|.yaml", …)`;
add to `CMakeLists.txt` next to the Markdown line. `ConfigFromNodeName("yaml")`.

### 2.A Tokenizer states (in `Initialize()`)

- **`main`**: identifiers `#` (comment), `"`, `'`; flow punctuation `{ } [ ] ,` as `kOperator`. Postfix set
  `: , { } [ ] #` so `key:` splits into `key` + `:` and `[a,b]` splits at the commas.
  - Flow brackets are **`kOperator`, not `kCodeBlockStart/End` / `kArrayStart/End`**: `ParseLine` counts
    those classes for its indent bookkeeping (`LangLineTokenizer.cpp`, `ParseLine`). Same caveat as Markdown's
    fences.
  - `#` only starts a comment at the start of a token (after whitespace), which is exactly YAML's rule
    (`url#frag` is not a comment). `a:#b` would misfire; acceptable.
- **`in_comment`**: regular class `kLineComment`, EOL action pop. (Copy of JSON's `in_line_comment`.)
- **`in_dq_string`**: regular class `kString`, pop on `"`, EOL pop (D3). Escapes: add `\"` and `\\` as
  identifiers that do NOT pop, longest match first, so `"a\"b"` stays one string.
- **`in_sq_string`**: regular class `kString`, pop on `'`, EOL pop. YAML escapes a quote by doubling it
  (`'it''s'`): add `''` as a non-popping identifier (longest match beats `'`).
  - A `'` inside a plain scalar (`msg: don't`) does not open a string: it is mid-token, and identifiers
    only match at token start. Check with a test.

No number matcher and no keyword list in the tokenizer: `true`, `null`, `42` only mean something when they
are the **whole** value (`name: true story` is a string). That check is line-level, so it lives in §2.B.

### 2.B Post-pass (`OnPostProcessParsedLine`)

Runs after the tokenizer on every parsed line, reads the line's buffer **and its existing attributes** (to
know which characters are already string or comment), and reclassifies ranges. Reuse Markdown's helpers -
`makeAttrib`, `classAt`, `reclassifyRange` are static in `MarkdownLanguage.cpp` today; lift them into a
small shared helper (e.g. `src/Core/Language/LineAttribEdit.{h,cpp}`) rather than copying them.

Guard first: a line that starts inside a block scalar (depth > root, see §4) is left alone.

Then, walking from the first non-blank character:

1. **Document markers**: `---` or `...` at column 0 (followed by space / EOL) -> `kRule`. `%YAML` /
   `%TAG` directives at column 0 -> `kPreProcessor`.
2. **List markers**: one or more `- ` prefixes (`- - a` is legal) -> `kListMarker`; continue after them.
3. **Key**: the first `:` that is followed by a space or EOL, and is not inside a string or comment
   (`classAt` says so). The range from the current position to the `:` -> `kConfigKey`; the `:` stays
   `kOperator`. A quoted key (`"a b": 1`) becomes `kConfigKey` too (it's a key first). A `:` with no space after
   (`http://x`, `a:b`) is not a key separator - YAML agrees.
4. **Value typing**: if what follows the `: ` (or the `- `) is a single plain scalar up to a comment or
   EOL, classify the whole thing:
   - `true false yes no on off null ~` (case-insensitive; `yes`/`no` because the editor's own config uses
     them) -> `kKeyword`;
   - an integer / float / `0x…` / `0o…` / `.inf` / `.nan` -> `kNumber`.
   Anything else stays plain (`kRegular`). Plain text values staying uncoloured is what most themes do and
   keeps the keys readable.
5. **Anchors, aliases, tags**: a token starting `&`, `*` or `!` (at token start, outside strings) ->
   `kMacroIdentifier` for the whole token. Theming-wise this is "a reference, not data"; a dedicated class
   is not worth it.
6. **Flow mappings** on one line (`{ a: 1, b: 2 }`): after a `{` or `,`, run step 3 again so `a` and `b`
   are keys. Cheap and common in GitHub Actions (`with: { node-version: 20 }`).

Everything here is line-local, so the region reparse (`ParseRegion` -> `SeekClean*`) needs nothing new.

---

## 3. Token class + theme (D1)

YAML's defining colour is the key. Options:
- **Reuse `kKeyword`** for keys: zero enum/theme work, but then `true`/`null` and keys share a colour, and
  keys look like language keywords.
- **New `kConfigKey`** (chosen): the key in a config / data file, also right for JSON / TOML / INI keys later
  (today JSON keys are plain strings). First built as `kKey`, renamed 2026-11-11: a bare "key" is too generic
  - semantic highlighting of hash-map keys, or anything about crypto keys, would want the name. Same 3-touch
  change as Markdown's classes:
  1. `LanguageTokenClass.h`: `kConfigKey = 27`, before the semantic block (see §0), which moves up by one.
  2. `LangToken.cpp`: `{kLanguageTokenClass::kConfigKey, "config_key"}` in `tokenNames` (a missing entry `exit(1)`s).
  3. `Assets/Resources/colors.json`: a `key` colour in **both** colour sections (the one with `heading` near
     line 53 and the `content` one near line 80).
  4. `src/Core/Theme/ThemeImporter.cpp`: a mapping row,
     `{"config_key", {"entity.name.tag.yaml", "support.type.property-name"}, "keyword"}`, so imported VS Code /
     Sublime themes colour keys too (fallback `keyword`).

Everything else reuses existing classes: `kLineComment`/`kCommentedText`, `kString`, `kOperator`,
`kListMarker`, `kRule`, `kKeyword`, `kNumber`, `kMacroIdentifier`, `kPreProcessor`, and (§4) `kCode` or
`kString` for block scalar bodies.

---

## 4. Block scalars - an indent-scoped state (D2)

```yaml
run: |
  echo "build: done"     # all of this is one string
  make -j
next: value              # the scalar ended here: indent <= the key's
```

**What the tokenizer needs:** a state that is popped at the start of a line whose indentation is not
deeper than the line that pushed it. Proposed contained extension of `LangLineTokenizer`:

- `State::SetIndentScoped(true)`: when such a state is pushed, record the pushing line's leading-blank count
  next to it (the stack becomes `std::stack<StackEntry{State::Ref, int pushIndent}>`, or a parallel
  `std::stack<int>`; only indent-scoped entries use the number).
- At the **start** of each line, before anything else: while the top is indent-scoped and the line is not
  blank and its indent `<= pushIndent`, pop. Blank / whitespace-only lines never pop (they belong to the
  scalar).
- That check must happen in **one** function called from both `ParseLine` (before `SetStateStackDepth`, so
  the dedented `next:` line is stamped clean at depth 1) and `TokenizeText` (the index fast path), so the
  two paths never disagree.
- Inside the state: regular class `kString` (or `kCode`, pick by eye), no identifiers, no actions - the body
  is opaque.

**Pushing it:** `|` and `>` with their indicators (`|-`, `|+`, `>-`, `|2`, …) are block-scalar headers only
when they are the **last** thing on the line (bar a comment). `a > b` is a plain scalar. So the action is a
*deferred* push: matching the header marks a pending push, any later non-comment token on the line cancels
it, and the EOL flush performs it. That is one more `kAction` value (`kPushStateAtEOL`) handled in
`ParseLineWithCurrentState`'s EOL block. Edge case accepted: `key: text >` at end of line would wrongly open
a block (YAML reads it as plain text).

**Why this is the right place, not the post-pass:** the post-pass sees one line and has no state across
lines; keeping state in the language object breaks region reparse (it restarts at a "clean" line it knows
nothing about) and the threaded index copy of the tokenizer. With the state on the tokenizer stack, depth > 1
inside a block scalar means `SeekCleanBackward/Forward` already reparse from before the `key: |` line -
region parsing works unchanged (no whole-buffer pass, no re-lex per edit).

This is also the "line-start awareness" option Markdown deferred (`done/support-markdown.md` §6); the same
start-of-line hook would let Markdown drop its `*`-bullet workaround later. Python would want it too.

---

## 5. Phases

Each phase ships on its own and is useful on its own.

**Phase 1 - skeleton + tokenizer (half a day).**
`YAMLLanguage` with §2.A, registration, CMake, `GetLineComment`. Config in `Assets/Resources/config.yml`:
```yaml
  yaml:
    indent: 2
    tabsize: 2
    insert_spaces: yes   # YAML forbids tabs in indentation
```
Result: comments and strings coloured, `Cmd+/` comments lines, 2-space indents.

**Phase 2 - post-pass + `kConfigKey` (the bulk).**
§3 class and theme plumbing first (so nothing `exit(1)`s), lift the attribute helpers out of
`MarkdownLanguage.cpp`, then §2.B steps 1-6 in order, a test per step.

**Phase 3 - block scalars.** §4 tokenizer extension with its own tokenizer-level tests (push, survive blank
lines, pop on dedent, region reparse from inside a block, `TokenizeText` agrees with `ParseLines`), then wire
YAML's `|` / `>`.

**Phase 4 - editing niceties (small, optional).**
- `indent.yml`: `yaml: { indent_after: [":", "|", ">", "[", "{"], dedent_on: ["]", "}"], suppress_in:
  [string, comment] }` - a line ending in `key:` indents the next one. Check what `- ` should do by using it.
- `autopairs.yml`: `yaml: { inherit: generic }`; confirm `don't` doesn't auto-close (the
  `auto_close_before` guard should stop it, test it).

---

## 6. Tests

New `utests/test_yamllang.cpp`, module `yamllang`, modelled on `test_markdown.cpp` (load text into a
buffer, parse, assert the class at a position - the *property*, not attribute counts). Cases, by phase:

- P1: `# c` and `a: b # c` comment from `#`; `url#frag` is not a comment; `"a\"b"` one string; `'it''s'`
  one string; `don't` is not a string; an unclosed `'` does not leak to the next line.
- P2: `key: v` -> `key` is `kConfigKey`; `- key: v` -> marker + key; `"a b": 1` -> key; `http://x` not a key;
  `a: true` keyword but `a: true story` not; `port: 8080` number but `v: 1.2.3` not; `---` rule only at
  column 0; `&anchor` / `*alias` / `!!str`; `{ a: 1, b: 2 }` two keys; a `:` inside a string is not a key.
- P3: block body is string; `foo: bar` inside it is NOT a key; blank lines inside don't end it; the dedented
  line after it IS a key; an edit inside the block reparses correctly (region path); `a > b` does not open a
  block.

Fixture `Assets/testfiles/sample.yml` (all of the above in one file) doubles as the GUI eyeball file. Real
files to eyeball as well: `Assets/Resources/config.yml`, a keymap, `.github/workflows/cmake.yml` (block
scalars in every `run:`). Add `yamllang` to the verified-green `-m …` set in CLAUDE.md once green.

---

## 7. Non-goals

- **Complex keys** (`? key` / `: value`), multi-line keys.
- **Multi-line quoted scalars and multi-line flow collections** (D3: both end at EOL). Revisit only if real
  files need it; it would be a persisting state plus the unclosed-quote risk.
- **Multi-line plain scalars**: continuation lines just read as plain text, which is what they are.
- **Validation** (duplicate keys, bad indentation, tabs) - a highlighter does not report errors.
- **Files without a YAML extension** (`.clang-format`, `.clangd`): registration is by extension today;
  a filename map would be a separate small change in `Editor::GetLanguageForExtension`.
- **Embedded languages in block scalars** (shell in `run: |`): the per-block highlighting work
  (`syntax-blocks.md`) is the natural home if ever wanted.
