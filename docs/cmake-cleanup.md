# CMake cleanup — work items

> **Planning doc.** Started 2026-06-16. The build is one flat `CMakeLists.txt` (663 lines) that grew
> organically. This logs the analysis + a sequenced plan to modernise it. Same shape as
> [`ui-refactor.md`](ui-refactor.md) / [`session-cache.md`](session-cache.md): §0 status, analysis,
> then sequenced work items (CM-n) with Goal/Scope/Approach/Effort-Risk/Done-when/Depends-on.

---

## §0 — Status / read this first

**Phase: PLANNING — nothing implemented yet.** This is the analysis + plan only; no CMake changed.

Drivers (user-stated): (1) replace the home-grown dependency pre-fetch with **FetchContent**; (2) group
the monolithic `editorsrc` into **named source-list variables** (UI, Graphics, Core, Editor, …) — *not*
separate libraries; (3) **proper installers** for macOS + Linux; (4) **per-compiler** flag handling
(Clang vs GCC); (5) move the rarely-changed **noise** (deps, flags, packaging) into `cmake/*.cmake`
includes — keeping **one** main `CMakeLists.txt`, not splitting the source tree across per-folder
`CMakeLists.txt`.

**Relationship to other docs:** the ui-refactor deferred a CMake `ui_src`/`appui_src` grouping
([`ui-refactor.md`](ui-refactor.md) §9, "cosmetic, deferred") — that's exactly this (CM-3). It also
**dropped AI-7** (a *shipped, exported* `goatui` library with a stable public API). This cleanup is
**source-list grouping + noise extraction only** — no new libraries/targets — so it does **not** reopen
AI-7. The UI boundary stays enforced at the source level by the include gate
(`scripts/check-ui-boundary.sh`).

---

## §1 — Goal & motivation

A maintainable, reproducible, portable build:
- **Reproducible deps** — pinned versions fetched by CMake itself; no manual `setup_deps.sh` step, no
  configure-time `git clone` side-effects.
- **Legible structure** — the source lists reflect the areas the code already has (`src/Core/UI/` vs
  `src/Core/Editor/` vs the rest) as **named `list()` groups feeding one target** (no separate libs); and
  the noise (deps/flags/packaging) lives in `cmake/*.cmake` so the main file is mostly source + targets.
- **Per-compiler correctness** — warning flags chosen by compiler/version, third-party noise scoped to
  third-party *sources* (not blanket-suppressed across our own code), and build-type-aware optimisation.
- **Real installers** — a `.dmg` (or signed app) on macOS; a sensible package on Linux.
- **Single-source version** — `project(VERSION …)` is the one place (SemVer — there's a public API); a
  generated `Version.h`, the macOS bundle version, and the package version all derive from it.

Non-goal: rewriting working behaviour. The current build *works*; this is hygiene + packaging.

---

## §2 — Current state (grounded inventory)

One `CMakeLists.txt`, no `cmake/` module dir, no `CMakePresets.json`. CI
(`.github/workflows/cmake.yml`) runs `chmod +x setup_deps.sh` → `apt-get install` → `cmake -B build
-DGEDIT_BUILD_SDL2=ON` → build `goatedit` → `check-ui-boundary.sh`.

### Dependency sourcing — **two overlapping home-grown mechanisms**
1. `setup_deps.sh` — manual `git clone` of `dukglue`, `gnklog`, `json` into `ext/`.
2. **Inline `execute_process(COMMAND git clone …)`** at configure time (CMakeLists L37–67) for `json`,
   `gnklog`, `dukglue`, `fmt` (fmt pinned to `12.1.0`; the rest unpinned → float on upstream `HEAD`).

`ext/` is gitignored. Vendored-in-tree (correctly, not fetched): `src/ext/duktape-2.7.0` and
`src/ext/stb`. Stale leftovers in `ext/`: `fmt-10.1.0`, `logger` (superseded by `gnklog`) — removable
(CM-0). This whole area is what FetchContent replaces.

#### Known-working dependency versions — the pins for CM-1
Captured 2026-06-16 from the current (working) `ext/` clones; **all worktrees clean** (no local
uncommitted edits). FetchContent must pin to *these exact commits* so we reproduce the known-good build,
not float on upstream `HEAD`.

| Dep | Source repo | Commit (pin) | Tag / note |
|---|---|---|---|
| `json` (nlohmann) | `github.com/nlohmann/json` | `0457de21cffb298c22b629e538036bfeb96130b7` | v3.11.3 **+6 commits** — pin the SHA, not the tag |
| `gnklog` | `github.com/gnilk/gnklog` | `10f002a182c46cd0c95f715f3c83bd100fe7d1cc` | **no tag** (own lib, 2026-05-19) — SHA-pin mandatory |
| `dukglue` | `github.com/gnilk/dukglue` ⚠️ **FORK** | `452d043bba2f480866204d2319d34eda6a408a77` | no tag (2023-11-19, "fix: return types and gcc warnings") |
| `fmt` (fmtlib) | `github.com/fmtlib/fmt` | `407c905e45ad75fc29bf0f9bb7c5c2fd3475976f` | = tag **`12.1.0`** (can pin by tag) |
| `duktape` | vendored `src/ext/duktape-2.7.0` | — (not a repo) | **2.7.0**, **pre-processed** (see below) — stays vendored |
| `stb` | vendored `src/ext/stb` | — (not a repo) | `stb_truetype` + `stb_rect_pack` single-headers — stays vendored |

**Two gotchas that break a naïve FetchContent:**
- ⚠️ **`dukglue` is a personal fork (`gnilk/dukglue`), not upstream.** It carries source fixes over the
  original (return types + gcc warnings). FetchContent **must** point at the fork at the pinned SHA —
  pulling upstream `Aloshi/dukglue` would *not* reproduce the working build. (It's plain header-only C++;
  the fork *is* the customization — there is no separate "pre-processing" step for it. The worktree is
  clean, so the fork commit is the whole story.)
- **The genuinely *pre-processed* dependency is `duktape`, not dukglue.** Upstream Duktape ships a Python
  amalgamation/configure tool that *generates* `duktape.c`/`duktape.h`/`duk_config.h`; the generated 2.7.0
  output is vendored in `src/ext/duktape-2.7.0` (that's the CMakeLists L62 "this won't fly… need to
  pre-process this further" comment). **Keep it vendored** under FetchContent too — do not try to fetch
  + build raw upstream. *(The pre-processing was done once, long ago, by the user; provenance otherwise
  unknown — treat the vendored tree as the source of truth.)*

### `editorsrc` — one flat list (~150 `list(APPEND …)` lines, L318–464)
Mixes every layer: Core foundation, the generic UI toolkit (`src/Core/UI/`), editor-specific UI
(`src/Core/Editor/`), the graphics contract, language/syntax, config/theme/session, Sublime, util,
primitives. `jsapi` and `duktape` are separate lists; the SDL backend is appended conditionally. The
folder split from the UI refactor exists physically but CMake still lumps it together.

### Compiler flags — **three near-duplicate copies of the same `-Wno-*` set**
- macOS branch (always Clang): one copy (L177–189).
- Linux branch: a Clang/GCC `if/else` (L238–277) with two more copies.
Most suppressions exist for **third-party** code — dukglue (`-Wno-unused-local-typedef[s]`), yaml-cpp
(`-Wno-shadow`), duktape — but are applied **globally** to the whole editor, hiding real warnings in our
own code. `-Wpedantic` is commented out on Linux. No build-type-specific optimisation flags. The ASan
build is an untracked side `cmake-build-asan/` (per CLAUDE.md), not a formalised option.

### Packaging — half-wired
- **Linux:** `install()` rules + CPack `DEB` generator (L284–300). **Two real bugs:** `DESTINATION
  DESTINATION share/...` (doubled keyword, L287–288 — the `.desktop` + icon install rules are broken).
- **macOS:** builds a `MACOSX_BUNDLE` `.app` with icon + `Info.plist`, copies resources to
  `SharedSupport` via a POST_BUILD step. The CPack `DragNDrop` (`.dmg`) block is entirely commented out
  (L192–206). No codesigning/notarisation.

### Smaller smells / actual bugs
- `install(... DESTINATION DESTINATION ...)` ×2 — **broken** (see above).
- Duplicated source entries (CMake dedupes, so harmless but sloppy): `ThemeAPI.cpp` listed twice with
  no `.h` (L343); `Keyboard.cpp` twice (L371); `RuntimeConfig.cpp` twice (L378); `JSONLanguage.cpp`
  twice (L459).
- `execute_process(COMMAND mkdir …)` (L18, L39) — non-portable; use `file(MAKE_DIRECTORY …)`.
- **Version scattered across ~4 places, inconsistent:** `project(goatedit VERSION 0.1)` (L6), macOS
  bundle `"0.1"` ×2 (L158–159), and `GEDIT_VERSION_MAJOR/MINOR/PATCH = 0/1/0` (L308–310) — format
  mismatch (`0.1` vs `0.1.0`), no single source, no dev/release suffix. CPack has no explicit
  `CPACK_PACKAGE_VERSION` (defaults to `0.1`). The app composes the string from the macros
  (`xstrver(...)` in `Editor.cpp`) — CM-9 replaces all of this with a generated `Version.h` derived from a
  single `project(VERSION)` (and deletes the stringify macros).
- `target_compile_options(utests PUBLIC -I/usr/local/include)` (L609) — **intentional, do not "fix".**
  A raw `-I` is used deliberately to pull in the **testrunner (`trun`) headers** installed under
  `/usr/local/include`: after a macOS system-SDK upgrade, the normal include mechanism stopped picking
  `/usr/local/include` up (the SDK assumes it / CMake drops it), and the raw compile-flag `-I` forces it
  verbatim where `target_include_directories` did not. Leave as-is **for now** — but note it was the
  *easy path*: the flag was enforced to make the problem go away, not after finding the idiomatic fix, so
  a cleaner solution very likely exists. The proper fix is to *locate the testrunner include properly*
  (a `find_path`/dependency target) — actually investigate it in CM-1 rather than assuming the workaround
  is the only option; don't just convert the flag.
- macOS pins `CMAKE_OSX_SYSROOT` to a hard Xcode path (L112) — fragile across Xcode updates.
- `utests` recompiles **every** source (it's `add_library(utests SHARED ${editorsrc} ${jsapi} …)`), so
  the whole tree builds twice. Module libs (CM-3) would let utests *link* instead of recompile.

---

## §3 — Linux packaging options (primer — you asked what these are)

There is no single "Linux installer"; the format depends on *how* you want it distributed. The four
realistic options, with the trade-offs that matter here:

| Format | What it is | Installs / runs how | Bundles deps? | Best for | Effort |
|---|---|---|---|---|---|
| **`.deb`** | Native Debian/Ubuntu package | `apt`/`dpkg`, system-wide | No — declares apt deps (SDL2, ncurses, yaml-cpp) | Your own Debian/Ubuntu boxes | **Low** (CPack `DEB`, half-done already) |
| **AppImage** | One self-contained executable file | `chmod +x` and run; no install | **Yes** — bundles libs into the file | "Download one file, runs on any distro" | **Low–Med** (linuxdeploy / appimagetool) |
| **Flatpak** | Sandboxed app + shared runtime | `flatpak install`, via Flathub or a local repo | Yes (runtime/SDK) | Wide distribution via a store; sandboxing | **Med–High** (manifest, runtime, permissions) |
| **Snap** | Canonical's sandboxed format | `snap install`, via Snap Store | Yes | Ubuntu-centric store distribution | **Med–High** (snapcraft.yaml; store account) |

**DECIDED (2026-06-16): `.deb` + AppImage, both CI-built and published to GitHub Releases. No app-store
presence (Flatpak/Snap explicitly out).** The intent is "if someone wants to try it, they download an
easy binary off GitHub" — same model the user already runs happily for another tool's auto-generated
`.deb`s. So: AppImage = the portable "download one file, runs on any distro" artifact (bundles SDL,
sidesteps the "is SDL3 installed?" problem that bites this project most); `.deb` = the apt-native package
(fix it — CM-6).

**Why Flatpak/Snap are out — not just "no store wanted", there's a hard technical reason:** GoatEdit's
**embedded terminal uses `forkpty`**, and a sandboxed package runs that shell *inside the sandbox*, not
on the host — wrong for a coding editor (you want host `$PATH`/toolchain). Un-sandboxing it needs
`flatpak-spawn --host` + portal/filesystem permission plumbing, which is ongoing manifest ceremony that
largely defeats the sandbox you took on the complexity for. AppImage (no sandbox) makes the embedded
terminal Just Work against the host. Combined with "no store presence wanted", Flatpak/Snap buy nothing
here.

---

## §4 — macOS packaging options (primer)

- **`.app` bundle** — already built (`MACOSX_BUNDLE`). This is the unit of distribution.
- **`.dmg`** — the standard "drag to Applications" disk image; CPack `DragNDrop` generator wraps the
  `.app`. This is the natural target (CM-7) — the commented-out block at L192–206 is the starting point.
- **Codesigning + notarisation** — required only to run on *other people's* Macs without Gatekeeper
  warnings (needs an Apple Developer ID + `codesign`/`notarytool`). **Optional for personal use**; note
  it but don't block on it.
- **Homebrew cask** — a later option for `brew install --cask goatedit`; only worth it with a public
  release cadence.

---

## §5 — Source-list grouping (NOT separate targets)

**Scope clarification (2026-06-16, user):** the goal is **not** separate CMake libraries/targets with a
dependency DAG, and **not** splitting the source tree across multiple `CMakeLists.txt`. It's simply
**grouping the one flat `editorsrc` list into several named source-list variables** that all still feed
the single `goatedit` (and `utests`) target — exactly today's `editorsrc` / `jsapi` / `duktape` pattern,
just more of them. One main `CMakeLists.txt` stays the home for the source lists + targets; the *noise*
(deps, flags, packaging) moves to `cmake/*.cmake` includes (CM-4).

Proposed source-list variables (names illustrative), combined at the end via
`target_sources(goatedit PUBLIC ${coresrc} ${uisrc} ${editorsrc} ${graphics_sdlN} ${jsapi} ${duktape})`:

| List var | Holds |
|---|---|
| `coresrc` | `src/Core/` foundation: data model (`Document`/`TextBuffer`/`Line`/`Workspace`), config/theme/session, language/syntax, util, primitives (`Rect`/`Point`/`ColorRGBA`/`NamedColors`/…), app glue (`Editor`/`RuntimeConfig`/`Runloop`) |
| `uisrc` | generic toolkit `src/Core/UI/`: `ViewBase`, views, `BaseController`, graphics **contract**, `UIHost`, `ILayoutSink`, `kUIAction` |
| `editorsrc` | editor-specific UI `src/Core/Editor/`: `EditorView`, controllers, `LineRender`, … |
| `graphics_sdl2` / `graphics_sdl3` | the per-backend `src/Core/Graphics/SDLn/` sources (already conditionally appended — just give them their own named lists) |
| `jsapi` | `src/Core/JSEngine/` (exists already) |
| `duktape` | vendored duktape TUs (exists already) |
| `platformsrc` | the `if(APPLE)`/`elseif(UNIX)` platform files (`macOS/*`, `Linux/*`, `unix/Shell`) |

This is **purely organisational** — same single binary, same compile, just legible groups instead of one
~150-line wall. No DAG, no link-time boundary, no `utests` recompile change (utests keeps compiling the
same sources; that was a *library*-split benefit and libraries are explicitly out of scope here).

*(For the record: a real library split — `goatcore`/`goatui`/backend/`goateditor` with an enforced
acyclic DAG — was considered and is **not wanted**. It's a much bigger change for a benefit the user
doesn't need, and it would resurrect the dropped AI-7 shape. The include-discipline gate
`scripts/check-ui-boundary.sh` already guards the UI boundary at the source level; it stays the
enforcement mechanism.)*

---

## §6 — Sequenced work items

Ordering principle: safe quick fixes first; FetchContent and warnings are independent and high-value;
source-list grouping (CM-3) + extracting the noise into `cmake/*.cmake` (CM-4) are mostly cosmetic and
can land any time; packaging is independent.

### CM-0 — Quick fixes / correctness (safe, mostly no behaviour change)
- **Goal:** kill the outright bugs + sloppiness with zero structural risk.
- **Scope:** the `DESTINATION DESTINATION` install rules (L287–288); duplicate source entries
  (L343/371/378/459); `execute_process(mkdir)` → `file(MAKE_DIRECTORY)` (L18/39); delete stale
  `ext/fmt-10.1.0` + `ext/logger`. **Leave** the `utests` `-I/usr/local/include` flag alone (it's an
  intentional testrunner-include workaround — see §2; addressed properly in CM-1).
- **Effort/Risk:** trivial / none (the install-rule fix is a *behaviour fix* — those rules are currently
  broken). **Done-when:** `cpack -G DEB` produces a package with the `.desktop` + icon correctly placed.
  **Depends-on:** —

### CM-1 — FetchContent for source deps
- **Goal:** delete `setup_deps.sh` + the inline `git clone` blocks; CMake fetches pinned deps itself.
- **Scope:** `json`, `gnklog`, `dukglue`, `fmt` → `FetchContent_Declare`/`MakeAvailable`, each pinned to
  the **exact commit in the §2 pin table** (json/gnklog/dukglue have no usable tag → SHA-pin; fmt = tag
  `12.1.0`). **`dukglue` points at the `gnilk/dukglue` fork**, not upstream (it carries fixes — §2).
  `duktape` (pre-processed, §2) + `stb` stay vendored (`src/ext/`). Keep `ext/` gitignored.
- **Approach:** put declarations in `cmake/CMakeDeps.cmake` (CM-4). Document `FETCHCONTENT_SOURCE_DIR_<dep>`
  so existing local `ext/` clones can be reused offline (and for dev against a fork). Drop the CI
  `chmod +x setup_deps.sh` step. **Also fold in the testrunner (`trun`) include** (today the raw
  `-I/usr/local/include` workaround on `utests`, §2): locate it properly via `find_path`/an imported
  target so the `utests` build stops depending on a hand-passed flag — the clean home for the macOS
  SDK-upgrade workaround.
- **Effort/Risk:** medium / low. **Done-when:** a clean checkout configures + builds with **no**
  `setup_deps.sh` and no network `git clone` from inside `CMakeLists.txt`; CI green. **Depends-on:** —

### CM-2 — Per-compiler flags + scoped third-party suppressions
- **Goal:** one source of truth for warnings, chosen by compiler; stop blanket-suppressing our own code.
- **Scope:** collapse the three `-Wno-*` copies into `cmake/CMakeBuildFlags.cmake` (CM-4), keyed on
  `CMAKE_CXX_COMPILER_ID`. **Scope** the dukglue/yaml-cpp/duktape suppressions to *those sources*
  (`set_source_files_properties` per file/dir, since there are no separate targets), not the whole
  editor. Add build-type flags (Debug `-O0 -g`, Release `-O2 -DNDEBUG`, RelWithDebInfo). Add a
  `GEDIT_SANITIZE=address|undefined` option to formalise the ad-hoc `cmake-build-asan/`.
- **Effort/Risk:** medium / medium (re-enabling warnings on our code may surface real ones — fix or
  re-suppress deliberately). **Done-when:** our code compiles under `-Wall -Wextra -Wshadow` with only
  *intentional*, documented suppressions; third-party noise stays off. **Depends-on:** — (note: with no
  per-target split, third-party suppressions are scoped via `set_source_files_properties`, not target
  options).

### CM-3 — Group the flat `editorsrc` into named source-list variables (§5)
- **Goal:** legible source lists — one main `CMakeLists.txt`, but grouped `LIST(APPEND <group> …)`
  variables instead of one ~150-line `editorsrc` wall. **No separate targets/libraries**, no DAG.
- **Scope:** carve `editorsrc` into `coresrc`/`uisrc`/`editorsrc`/`graphics_sdl2`/`graphics_sdl3`/
  `platformsrc` (+ keep existing `jsapi`/`duktape`) per the §5 table; combine them in one
  `target_sources(goatedit PUBLIC …)` (and the `utests` library). Names are the user's call.
- **Approach:** pure move of `list(APPEND …)` lines between named lists; zero behaviour change. Same
  files compiled, same single binary.
- **Effort/Risk:** low / very low (mechanical re-grouping). **Done-when:** `goatedit` + `utests` build
  identically; verified-green 236-test set passes; the source lists read as labelled groups.
  **Depends-on:** CM-0 (nice-to-have).

### CM-4 — Move the rarely-changed *noise* into `cmake/*.cmake` includes
- **Goal:** keep **one** main `CMakeLists.txt` focused on the source-list groups + targets; hoist the
  stuff the user seldom touches into separate include files `include()`d from the top. **Not**
  `add_subdirectory` per source folder — the source tree stays in one CMake file.
- **Scope:**
  - `cmake/CMakeDeps.cmake` — the FetchContent declarations + vendored-dep setup (CM-1 lands here).
  - `cmake/CMakeBuildFlags.cmake` — the per-compiler warning/flag logic + build-type/sanitiser options
    (CM-2 lands here).
  - `cmake/CMakePackaging.cmake` — install rules + CPack `.deb`/`.dmg` config (CM-6/CM-7 land here).
  - Main `CMakeLists.txt` keeps: `project()`, the §5 source-list groups, the target(s), and three
    `include(cmake/…)` lines.
- **Effort/Risk:** low / low. **Done-when:** the top-level file is mostly source lists + targets;
  deps/flags/packaging live in `cmake/`. **Depends-on:** naturally absorbs CM-1/CM-2 (and CM-6/CM-7's
  packaging) — i.e. those work items *write into* these files rather than the root.

### CM-5 — `CMakePresets.json`
- **Goal:** standard, discoverable configure/build/test invocations; encode the SDL backend choice,
  build types, and the sanitiser.
- **Scope:** presets for `debug`/`release`/`asan` × (implicit per-platform SDL). `trun` test preset.
- **Effort/Risk:** low / none. **Done-when:** `cmake --preset debug && cmake --build --preset debug`
  works on both dev boxes; README/CLAUDE.md updated; CI can use `--preset`. **Depends-on:** — (nicer
  after CM-1/CM-2 so presets reference real options).

### CM-6 — Linux packaging: `.deb` + AppImage, CI → GitHub Releases  *(DECIDED — §3)*
- **Goal:** two downloadable binaries on the GitHub Releases page — an apt-native `.deb` and a portable
  AppImage — **built in CI**, not by hand. Mirrors the user's existing happy setup for another tool.
  Flatpak/Snap are **out** (no store presence wanted; `forkpty`/sandbox conflict — §3).
- **Scope:**
  - **`.deb`:** fix the broken install rules (CM-0); set `CPACK_DEBIAN_PACKAGE_DEPENDS` (SDL, ncurses,
    yaml-cpp) so apt resolves runtime deps; correct `.desktop` + icon placement.
  - **AppImage:** bundle the app + SDL (and other non-glibc libs) via `linuxdeploy` + `appimagetool`.
    **Build on an older base image** (e.g. an LTS Ubuntu a couple of releases back) so the bundled glibc
    is old enough to run on most distros — the classic AppImage portability rule.
  - **CI/release:** a workflow (tag-triggered) that runs `cpack -G DEB`, builds the AppImage, and
    **uploads both as GitHub Release assets**. Keep PR/CI builds producing them as artifacts too.
- **Effort/Risk:** medium / low. **Done-when:** pushing a version tag produces a Release carrying a
  `.deb` (installs + launches on a clean Ubuntu, apt-resolves deps) and an AppImage (runs on a *different*
  distro with no SDL installed). **Depends-on:** CM-0 (install rules); pairs with CM-3/CM-5 (clean
  targets + presets help the CI invocation; not strictly required).

### CM-7 — macOS packaging (`.dmg`)
- **Goal:** a distributable `.dmg` wrapping the `.app` (§4).
- **Scope:** revive + fix the CPack `DragNDrop` block; ensure `SharedSupport` resources land inside the
  bundle for an installed copy (the POST_BUILD copy is dev-tree only). Note codesign/notarise as a
  documented optional follow-up.
- **Effort/Risk:** medium / low. **Done-when:** `cpack -G DragNDrop` yields a `.dmg` that mounts,
  drags to Applications, and launches (resources resolve from the bundle). **Depends-on:** —

### CM-8 — (optional) CI alignment
- **Goal:** CI uses presets (CM-5), drops `setup_deps.sh` (CM-1), optionally publishes packages as
  build artifacts (CM-6/7).
- **Depends-on:** CM-1, CM-5 (+ CM-6/7 for artifacts).

### CM-9 — Single-source SemVer via `project(VERSION)` + a generated `Version.h`
- **Goal:** define the version **once**; everything (compile-time access, bundle, package) derives from
  it. SemVer because GoatEdit exposes an API. *(For now it's visual/user-info only — not driving ABI,
  SONAME, or package-dep resolution.)*
- **Decision (2026-06-16):** use the **CMake-idiomatic** form, not the raw trun `-D`/macro pattern:
  - **`project()` is the single source** — `project(goatedit VERSION 0.1.0)`. CMake auto-populates
    `PROJECT_VERSION` + `PROJECT_VERSION_MAJOR/MINOR/PATCH`; everything reads those. No parallel
    `set(..._MAJOR)` vars (that's the duplication the trun pattern carries).
  - **`configure_file` → a generated `Version.h`**, replacing the `-D GEDIT_VERSION_*` defines **and** the
    `xstr`/`xstrver` stringify macros in `Editor.cpp` (delete those — you get real string literals).
- **Approach:**
  ```cmake
  project(goatedit VERSION 0.1.0)                  # ← the one place to bump
  if(DEFINED ENV{GITHUB_REF} AND "$ENV{GITHUB_REF}" MATCHES "refs/tags/")
      set(GEDIT_VERSION_SUFFIX "")                 # tag build → release
  else()
      set(GEDIT_VERSION_SUFFIX "-dev")            # else → -dev (optionally + short git SHA, below)
  endif()
  configure_file(${CMAKE_SOURCE_DIR}/cmake/Version.h.in
                 ${CMAKE_BINARY_DIR}/generated/Version.h @ONLY)
  target_include_directories(${CUR_TARGET} PUBLIC ${CMAKE_BINARY_DIR}/generated)   # ← THE GOTCHA
  ```
  ```cpp
  // cmake/Version.h.in   (@VAR@ placeholders; @ONLY so C++ ${...} isn't touched)
  #pragma once
  #define GEDIT_VERSION_MAJOR  @PROJECT_VERSION_MAJOR@
  #define GEDIT_VERSION_MINOR  @PROJECT_VERSION_MINOR@
  #define GEDIT_VERSION_PATCH  @PROJECT_VERSION_PATCH@
  #define GEDIT_VERSION_STR    "@PROJECT_VERSION@@GEDIT_VERSION_SUFFIX@"   // e.g. "0.1.0-dev"
  ```
- **Why your earlier `Version.h.in` "didn't generate":** almost always **the missing include-dir step** —
  `configure_file` writes the header into the *build* tree (`${CMAKE_BINARY_DIR}/generated/`), so unless
  that dir is on the target's include path the `#include "Version.h"` fails and it looks like nothing was
  produced. (Secondary traps: vars must be `set()` *before* the `configure_file` call; output to the
  build dir, never the source tree; use `@ONLY` so the C++ in the template isn't mangled.)
- **Two nuances:** (1) the macOS bundle keys (`CFBundleShortVersionString`) and the `.deb` package
  version must use the **clean** `${PROJECT_VERSION}` (no `-dev` — Apple rejects non-numeric short
  versions); `CPACK_PACKAGE_VERSION = ${PROJECT_VERSION}`. The `-dev` suffix is **display-only**
  (`Version.h`'s `GEDIT_VERSION_STR`). (2) Manual bump chosen over `git describe` — matches the trun
  habit, predictable for tagless builds.
- **Optional (recommended for "user info"):** append the **short git SHA** to the `-dev` string
  (`execute_process(COMMAND git rev-parse --short HEAD …)`, degrade gracefully outside a checkout) so a
  bug report's version pins the exact commit. ~2 lines; the one git-derived bit genuinely worth it.
- **Effort/Risk:** low / low. **Done-when:** bumping the single `project(VERSION)` changes the version
  everywhere (about/title, `.deb`/`.dmg`, bundle); a tag build drops `-dev`; the `xstr*` macros are gone.
  **Depends-on:** the suffix/SHA logic lands in CM-4's `cmake/`; feeds CM-6/CM-7 package versions.

---

## §7 — Open questions / decisions log

- **Source organisation (§5):** RESOLVED (2026-06-16) — **named source-list groups in one
  `CMakeLists.txt`** (CM-3), with deps/flags/packaging hoisted to `cmake/*.cmake` (CM-4). **Separate
  libraries / a dependency DAG / per-folder sub-`CMakeLists.txt` are explicitly NOT wanted** — too big a
  change for no needed benefit, and it would resurrect the dropped AI-7. The list-variable names are the
  user's call.
- **Linux format (§3):** RESOLVED (2026-06-16) — **`.deb` + AppImage, CI-built → GitHub Releases; no
  store presence.** Flatpak/Snap out (no store wanted + `forkpty`/sandbox conflict). See CM-6.
- **macOS signing:** personal-use unsigned `.dmg` now, codesign/notarise later? (Assumed yes.)
- **dukglue sourcing:** RESOLVED — FetchContent the **`gnilk/dukglue` fork** at the pinned SHA (§2). It's
  header-only and the worktree is clean, so the fork commit is the whole story; no separate
  pre-processing. (The pre-processed dep is `duktape`, which stays vendored.)
- **UI boundary enforcement:** stays with the source-level include gate `scripts/check-ui-boundary.sh`
  (there are no separate libs to enforce it at link time, by choice).

---

## Changelog
- **2026-06-16** — Initial analysis + plan (CM-0…CM-8). Grounded inventory of the current 663-line
  `CMakeLists.txt` (incl. the `DESTINATION DESTINATION` install bug, duplicate source entries, stale
  `ext/` leftovers), Linux/macOS packaging primers, the module DAG (§5), and the sequenced work items.
  No CMake changed.
- **2026-06-16** — Corrected the `utests -I/usr/local/include` entry: it's an **intentional**
  workaround for finding the testrunner headers after a macOS SDK upgrade (user clarification), not a
  bug. Removed from CM-0; folded the proper fix (locate `trun` includes via `find_path`) into CM-1.
- **2026-06-16** — Captured the **known-working dependency pins** (§2 table, SHAs from the current clean
  `ext/` clones) as the CM-1 targets. Clarified two FetchContent gotchas: `dukglue` is the **`gnilk`
  fork** (not upstream — must pin the fork SHA), and the genuinely **pre-processed** dep is `duktape`
  (stays vendored), not dukglue. Resolved the §7 dukglue-sourcing question.
- **2026-06-16** — **Linux packaging DECIDED:** `.deb` + AppImage, CI-built and published to GitHub
  Releases; no app-store presence. Flatpak/Snap ruled out (no store wanted + the `forkpty` embedded
  terminal conflicts with a sandbox). Rewrote CM-6 to be concrete (CPack DEB + linuxdeploy AppImage on an
  old base + a tag-triggered release workflow); resolved the §7 Linux-format question.
- **2026-06-16** — **Scope correction (user):** CM-3 is **source-list grouping** (named `list()` vars
  feeding one target), **not** separate libraries/targets or a dependency DAG; CM-4 is **hoisting the
  noise into `cmake/*.cmake` includes** (`CMakeDeps`/`CMakeBuildFlags`/`CMakePackaging`), **not**
  per-folder sub-`CMakeLists.txt` — one main CMake file stays. Rewrote §5, CM-3, CM-4; aligned CM-1/CM-2
  filenames; resolved the §7 source-organisation question (libraries explicitly out).
- **2026-06-16** — Added **CM-9 (single-source SemVer)**; then revised it to the **CMake-idiomatic** form
  (user asked "is there a more conventional way", fine to change): `project(VERSION)` as the single source
  + a `configure_file`-generated `Version.h` (replacing the `-D` defines and the `xstr`/`xstrver` macros),
  rather than the raw trun `set()`/`-D` pattern. Recorded the gotcha that broke the user's earlier
  `Version.h.in` attempt (the generated header lands in the build tree → its dir must be on the include
  path). Kept the `-dev`-via-`GITHUB_REF` suffix; added optional short-git-SHA-on-`-dev` for bug-report
  traceability. Bundle/package still use clean `${PROJECT_VERSION}`; `-dev` display-only.
