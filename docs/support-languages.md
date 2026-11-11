# Languages as data - a generic syntax engine, then CMake and Python

Status: **plan** (2026-11-11), for review. Nothing built.

More languages - CMake and Python first - and a way to add one **without recompiling**. A language is one file
(`python.yml`): its name, the extensions it claims, who wrote it and when, and the definitions the tokenizer needs. The
aim is that such a file covers most of what a "keywords, comments, strings, numbers" language needs - the 80% - and the
languages that need more stay in C++.

---

## 1. What exists

**`LangLineTokenizer` is already driven by data - it's just written in C++.** A language's `Initialize()` builds
named **states**, and each state holds:
- **identifier lists:** a token class plus a list of tokens, either whole-word (keywords) or not (operators);
- a **postfix set:** what ends a word;
- **actions per token:** push a state, pop, or push at the end of the line;
- an **EOL action:** e.g. a line comment pops at the end of the line;
- a **regular class:** everything in an `in_string` state is a string;
- optionally, a number matcher and indent scoping.

`JSONLanguage::Initialize` is nothing but such calls - 30 lines that a data file says as well. `MakeBuildLang` is
almost the same.

**What needs code** (so stays C++):
- `CPPLanguage`: its number matcher, the structure scanner, semantic highlighting.
- `MarkdownLanguage` and `YAMLLanguage`: line-anchored post-passes (`OnPostProcessParsedLine`). YAML also has
  `blockScalarScopeIndent` and `GetContinuationColumn`.

**Around a language:**
- `Editor::RegisterLanguage(".json|.js", language)` maps extensions.
- `config.yml` `languages.<name>` holds tab size and similar settings.
- `indent.yml` and `autopairs.yml` hold sections per language, keyed by the same name (`ConfigFromNodeName`).

**Token classes have names** (`LangToken.cpp`: `keyword`, `operator`, `string`, `number`, `line_comment`,
`commented_text`, `preprocessor`, ...). The themes use the same names, so a language file uses the theme's vocabulary.

## 2. The language file

YAML, because yaml-cpp is in, the editor highlights it, and the asset files are YAML already. One file per language.

```yaml
# Assets/Resources/Languages/python.yml
language:
  name: python                  # the identifier: config.yml 'languages.python', indent.yml, '.sl python'
  display: Python
  extensions: [".py", ".pyw"]
  filenames: []                 # exact names, e.g. CMake's 'CMakeLists.txt'
  author: gnilk
  date: 2026-11-11
  version: 1                    # of this file
  line_comment: "#"             # for CommentLine (Cmd+/)
  engine: data                  # 'data': the tokenizer below; 'builtin': a C++ language (2.2)
  tabsize: 4                    # what config.yml 'languages.<name>' holds today; the user's config still wins

tokenizer:
  start: main
  states:
    main:
      postfix: "( ) [ ] { } : , . = + - * / % < > ! ~ & | ^ @ # \" '"
      tokens:
        - class: keyword
          whole_word: yes
          words: "def class return if elif else for while in not and or is import from as with try except
                  finally raise pass break continue lambda yield global nonlocal assert del async await"
        - class: known_type
          whole_word: yes
          words: "None True False self cls int str float bool list dict set tuple bytes object"
        - class: operator
          words: "** // == != <= >= -> := += -= *= /= ( ) [ ] { } : , . = + - * / % < > @"
        - { class: line_comment, words: "#", push: in_comment }
        - { class: string, words: '"""', push: in_tq_dq_string }    # one state per quote style, spelt out
        - { class: string, words: "'''", push: in_tq_sq_string }
        - { class: string, words: "\"", push: in_dq_string }
        - { class: string, words: "'", push: in_sq_string }
      numbers: generic           # decimal, hex/octal/binary, float, '_' separators, 'j' suffix
    in_comment:
      regular: commented_text
      eol: pop
    in_dq_string:
      regular: string
      tokens:
        - { class: string, words: "\\\\ \\\"" }               # escapes - before the closing quote
        - { class: string, words: "\"", pop: yes }
    # ... in_sq_string, in_tq_dq_string ('"""' ... '"""', across lines), in_tq_sq_string

indent:                          # what indent.yml holds today - or 'indent: use_default' (2.2)
  indent_after: [":"]
  reindent: no                   # indentation is the syntax, as YAML

autopairs: use_default           # or a section of its own, as autopairs.yml has
```

**Rules the loader enforces, so a file can't trip the known traps:**
- **Longest first:** identifier lists match in order, not longest-first (a known tokenizer trap: `"` before `"""`
  would never let `"""` match). The loader sorts every list longest-first, so the order in the file doesn't matter.
- **Names are checked:** every `class:` must be a known token-class name, and every `push:` must name a state. A bad
  file is rejected as a whole, with `file:line` and the reason (yaml-cpp gives the mark), on the console and in the log.
  A rejected file never half-loads.
- **Each quote style is its own state, spelt out** (decided): `"`, `'`, `"""` and `'''` each push their own state,
  which only its own closer pops. A `"` inside `"""..."""` is then just text. No shorthand: it's four short entries,
  and the file reads as what it does.

### 2.1 Language settings

Switches for the whole language, under `language:` beside the metadata:

| Setting | Default | For |
|---|---|---|
| `case_sensitive` | `yes` | `no`: CMake, SQL, Pascal, Fortran - every whole-word list compares folded |
| `identifier_chars` | letters, digits, `_` | adds characters a word may contain: `$` (PHP/JS variables), `-` (CSS, Lisp), `?`/`!` (Ruby) |
| `escape_char` | `\` | the escape character inside strings: an escaped quote doesn't close the string |
| `nested_comments` | `no` | `yes`: Rust, Swift - `/* /* */ */` counts its depth (a tokenizer addition) |

More are added when a language needs them, not ahead of time.

### 2.2 A language file is self-contained (decided 2026-11-11)

**Everything about a language is in its file:** the metadata, the settings, the tokenizer, the indent rules and the
autopairs. No other asset has to know about it.

**`use_default`:** a file that has nothing special for a part says so instead of copying it:
- `indent: use_default` - the generic indent rules (`indent.yml`'s `generic` section today);
- `autopairs: use_default` - the generic pairs.

The default is a named section of its own (`Languages/_default.yml`, or the generic sections of the existing files
until they move), so "the default" is one place.

**Built-in languages get a file too:** `cpp.yml`, `markdown.yml`, `yaml.yml` with `engine: builtin`.
- The file gives the metadata, the extensions, the settings, the indent rules and the autopairs.
- The C++ class gives the tokenizer and its hooks; a built-in file has no `tokenizer:` section.
- `engine: builtin` names the class through a small registry in C++ (`"cpp" -> CPPLanguage::Create`). That registry
  is the only hard-coded list left.

**Consequences:**
- `Editor::RegisterLanguage(".cpp|.h|.c|.hpp", ...)` goes: every language is registered from its file, and the
  extensions live in data.
- `indent.yml` and `autopairs.yml` dissolve into the language files (`yaml`'s `reindent: no` moves into `yaml.yml`).
- `config.yml`'s `languages:` keeps only the user's overrides: a user's `languages.python.tabsize` still wins over
  the file's `tabsize`.

This is a step of its own (phase 4), after data languages work, so the move doesn't hold them up.

### 2.3 What will fit, realistically

| Fits well | Partly | Not with this engine |
|---|---|---|
| **Formal, regular syntax:** Go, Java, C#, Kotlin, Swift, Rust (minus the next column), Lua, SQL, CMake, TOML/INI, Dockerfile, shell without heredocs | **Python:** f-string `{...}` parts colour as string. **Rust:** a lifetime `'a` vs a char `'a'` needs a look-ahead. **JavaScript/TypeScript:** a regex literal `/x/` vs division needs the previous token; template literals nest `${...}`. **Shell:** heredocs, `$(...)` nesting | **PHP** inside HTML (two languages in one file). **Markdown, YAML structure** (line-anchored - they stay C++). Anything needing a real grammar |

"Partly" means usable highlighting with known wrong spots, written down in the file's header. That's better than
none, and a language can move to C++ (or get a named hook) when the wrong spots hurt.

## 3. The engine - `DataLanguage`

- **`DataLanguage : LanguageBase`.** `Initialize()` walks the parsed definition and makes exactly the
  `GetOrAddState` / `SetIdentifiers` / `GetOrAddAction` / `SetEOLAction` calls a C++ language makes. So there is no
  second tokenizer: same speed, same incremental region reparse, the same open-bugs notes apply.
- **The parse is separate from the build:** `LanguageDefinition` (plain data, from YAML) and `DataLanguage`
  (definition -> tokenizer). The parse is testable from a string, without the asset loader, as `IndentCache`'s
  `LoadFromString` is.
- **Indent and autopairs:** a file's `indent:` / `autopairs:` sections are handed to `IndentCache` / the autopair
  config under the language's name, as if they were in `indent.yml`. A section in `indent.yml` itself wins, so
  a user's own override still works.
- **Registration:** at startup `Editor` loads every `*.yml` in the language directories (section 4) and calls
  `RegisterLanguage` for its extensions and file names.
  - **A built-in C++ language wins** over a data file that claims the same extension, unless the file says
    `override: yes`.
  - A user's file (section 4) wins over a system one of the same `name`.
- **`.ll`** (list languages) shows the metadata: name, extensions, version, author, and where it was loaded from.

## 4. Where language files live

As every asset, with the same layering as config:
1. **System:** `Assets/Resources/Languages/` (installed next to the other assets).
2. **User:** `~/.config/goatedit/languages/`. This is the first target (decided 2026-11-11).
3. **Project, later:** `<root>/.goatedit/languages/`, scanned when the project opens, after the user's - lazily, as
   the rest of a project is. A language file is data only and runs nothing, so unlike the command-running config keys,
   `ProjectConfigPolicy` has no reason to strip it. Not in the first phases.

**Reload:** `.lang_reload` (short `.lr`) re-reads the files and reparses the open documents, so a language can be
written in the editor and tried at once. That is the extensibility the idea is about.

## 5. What the engine needs beyond today's tokenizer

Small additions, each with a C++ language that already benefits or a data language that needs it:

| Addition | For | Notes |
|---|---|---|
| Lists sorted longest-first (at load) | every data file | the loader does it; no tokenizer change |
| **Case-insensitive** whole-word lists | CMake (`add_executable` = `ADD_EXECUTABLE`), SQL | a flag on the list; compare folded |
| A **generic number matcher** | Python, CMake, most languages | decimal, `0x`/`0o`/`0b`, float/exponent, `_` separators, a suffix set from the file. `CPPNumberMatcher` stays C++'s |
| A **prefix rule:** a token that takes the word glued to it | Python decorators `@name`, shell `$VAR`, CMake `${...}` is a push instead | `{ class: macro_identifier, prefix: "@" }` |
| `push_at_eol`, indent-scoped states | already there (YAML) | exposed as `push_at_eol:` and `indent_scoped: yes` (plain indent rule) |

**Out of scope for data, on purpose:**
- regular expressions - the tokenizer's speed and its line-at-a-time model;
- post-passes and line-anchored rules (Markdown headings, YAML keys);
- semantic highlighting and the code index;
- heredocs (shell's `<<EOF` needs the terminator remembered);
- the YAML-specific scope function.

A language that needs one of those is a C++ language - or, later, a data file plus a small named hook ("post-pass:
markdown_headings"). That bridge isn't planned.

## 6. The first data languages

**CMake (`cmake.yml`):** `CMakeLists.txt` and `*.cmake`.
- Commands are case-insensitive, so they need the case-insensitive lists. Two lists:
  - `function`: the built-in commands (`add_executable`, `target_link_libraries`, `set`, `if`, `foreach`, ...);
  - `keyword`: the arguments that are keywords (`PUBLIC`, `PRIVATE`, `INTERFACE`, `REQUIRED`, `VERSION`, ...).
- `#` comments, and `#[[ ... ]]` bracket comments across lines.
- `"strings"`, with `${VAR}` inside them as a push (`macro_identifier`), and `$<...>` generator expressions likewise.
- `indent_after: ["("]`.

**Python (`python.yml`):** as sketched in section 2.
- Triple-quoted strings across lines.
- `f"..."` colours as a string, and its `{...}` parts don't (accepted for v1).
- Decorators through the prefix rule.
- `indent_after: [":"]`, `reindent: no`.

**And JSON, ported to a data file as the proof:** `json.yml` must tokenize every fixture exactly as `JSONLanguage` does.
Once it does, `JSONLanguage.cpp` can go - the first C++ language replaced by data. `MakeBuildLang` is next, but its
post-pass prints build errors, which is a hook, not data; keep it until that's decided.

## 7. Phases

**Phase 1 - the engine.**
- `LanguageDefinition` (parse, validate, longest-first, errors with `file:line`) and `DataLanguage` (build the
  tokenizer).
- The system directory and registration.
- `json.yml` equal to `JSONLanguage` on the fixtures.
- `.ll` showing the metadata.

**Phase 2 - CMake and Python.**
- The tokenizer additions they need: case-insensitive lists, the generic number matcher, the prefix rule.
- `cmake.yml` and `python.yml`.
- Their `indent:` / `autopairs:` sections.

**Phase 3 - extensible without recompiling.**
- The user directory (the project directory later).
- `.lang_reload` / `.lr`.
- `override: yes`.
- A short "writing a language file" section in this doc, with the format reference.

**Phase 4 - every language from a file (2.2).**
- `cpp.yml`, `markdown.yml` and `yaml.yml` with `engine: builtin`, and the builtin registry.
- `use_default`.
- `indent.yml` / `autopairs.yml` sections and the extension lists in `Editor` move into the files.
- `config.yml`'s `languages:` keeps only overrides.

## 8. Tests

**`datalang` (new module):**
- **The parser:** a minimal file loads.
- **The validation:**
  - an unknown class, an unknown `push:` state, and a missing `name` are each rejected with the line;
  - a rejected file registers nothing.
- **Longest-first:** a file listing `"` before `"""` still matches `"""`.
- **The JSON port:** `json.yml` and `JSONLanguage` give identical attributes on every line of the JSON fixtures. This
  test proves the engine adds nothing of its own.
- **Python:**
  - a triple-quoted string spans lines and ends at its own closer, not at a `"` inside it;
  - a decorator is `macro_identifier`;
  - `0x_FF`, `1_000.5e-3` and `3j` are numbers;
  - `# comment` after code.
- **CMake:**
  - `ADD_EXECUTABLE` and `add_executable` are both `function`;
  - `${VAR}` inside a string is its own class;
  - `#[[ ... ]]` spans lines.
- **Registration:**
  - a data file's extensions map to it;
  - a built-in wins without `override: yes`;
  - a user file wins over a system file of the same name.

**`indent`:** a data file's `indent:` section is used, and `indent.yml` wins over it.

**The bench:** `gedit-bench` lexer throughput on Python and CMake trees is in the C++ languages' range. A data language
is the same tokenizer, so a slowdown means the definition built something wrong.

## 9. Open for review

1. **The default's home:** `Languages/_default.yml`, or keep `indent.yml` / `autopairs.yml`'s generic sections until
   phase 4?
2. ~~`push: by_token`?~~ Decided: each quote style is spelt out as its own state.
3. ~~Where indent/autopairs live?~~ Decided: in the language file, with `use_default` (2.2).
4. ~~Retiring `JSONLanguage.cpp`?~~ Decided: yes, once `json.yml` is equal on the fixtures.
5. ~~Project-level language files?~~ Decided: user-level first; project-level later, scanned lazily when a project
   opens.
6. ~~The file's layout?~~ Accepted as a first try (2026-11-11).
