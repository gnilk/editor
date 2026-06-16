# CMake cleanup — work items

> **Planning doc.** Started 2026-06-16. The build is one flat `CMakeLists.txt` (663 lines) that grew
> organically. This logs the analysis + a sequenced plan to modernise it. Same shape as
> [`ui-refactor.md`](ui-refactor.md) / [`session-cache.md`](session-cache.md): §0 status, analysis,
> then sequenced work items (CM-n) with Goal/Scope/Approach/Effort-Risk/Done-when/Depends-on.

---

## §0 — Status / read this first

**Phase: PLANNING — nothing implemented yet.** This is the analysis + plan only; no CMake changed.

Drivers (user-stated): (1) replace the home-grown dependency pre-fetch with **FetchContent**; (2) split
the monolithic `editorsrc` into per-module groups/libs (UI, Graphics, Core, Editor, …); (3) **proper
installers** for macOS + Linux; (4) **per-compiler** flag handling (Clang vs GCC); (5) (open) split the
single `CMakeLists.txt` into sub-CMake files.

**Relationship to other docs:** the ui-refactor deferred a CMake `ui_src`/`appui_src` grouping
([`ui-refactor.md`](ui-refactor.md) §9, "cosmetic, deferred") and explicitly **dropped AI-7** (a
*shipped, exported* `goatui` library with a stable public API). The module split here is **internal
build hygiene** — object/static libs with no install/export and no stable API — which is a *different*
thing and does **not** reopen AI-7. It does, however, turn the ui-refactor's include-discipline gate
(`scripts/check-ui-boundary.sh`) into a **link-time** boundary as a bonus.

---

## §1 — Goal & motivation

A maintainable, reproducible, portable build:
- **Reproducible deps** — pinned versions fetched by CMake itself; no manual `setup_deps.sh` step, no
  configure-time `git clone` side-effects.
- **Legible structure** — the source list reflects the module boundaries the code already has
  (`src/Core/UI/` vs `src/Core/Editor/` vs the rest), ideally as real CMake targets with a one-way
  dependency DAG.
- **Per-compiler correctness** — warning flags chosen by compiler/version, third-party noise scoped to
  third-party targets (not blanket-suppressed across our own code), and build-type-aware optimisation.
- **Real installers** — a `.dmg` (or signed app) on macOS; a sensible package on Linux.

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

**Recommendation for a personal GUI editor:** ship **AppImage as the primary** "works everywhere,
zero-install" artifact, and keep the **`.deb`** (fix it — CM-6) for your own apt machines. Flatpak/Snap
are store-distribution formats with sandbox ceremony (the SDL window, clipboard, pty/`forkpty`, and
arbitrary file access all need portal/permission wiring) — defer unless you actually want Flathub
presence. AppImage's bundling sidesteps the "is SDL3 installed?" problem that bites this project most.

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

## §5 — Proposed module split (the dependency DAG)

The code already has the boundaries; the build should mirror them. Target DAG (arrows = "depends on"):

```
        fmt, gnklog, yaml-cpp, json ─┐
                                      ▼
   duktape, stb, dukglue ──►   goatcore  ──►  goatui  ──►  goatui-sdl2 | goatui-sdl3   (backend)
                                   │             ▲                       ▲
                                   │             └──── goateditor ───────┘
                                   ▼                       ▲
                                 (used by all)         jsengine
                                                           ▲
                              main.cpp  ──►  goatedit (exe) ┘     utests (links the libs, not re-compiles)
```

- **`goatcore`** — `src/Core/` foundation: data model (`Document`, `TextBuffer`, `Line`, `Workspace`),
  config/theme/session, language/syntax, util, and the shared primitives (`Rect`/`Point`/`ColorRGBA`/
  `NamedColors`/`VerticalNavigationViewModel`/`KeyPress`/`Keyboard`/`Cursor`). Deps: fmt, gnklog,
  yaml-cpp, json.
- **`goatui`** — the generic toolkit (`src/Core/UI/`): `ViewBase`, views, `BaseController`, the graphics
  **contract** (`ScreenBase`/`DrawContext`/…), `UIHost`, `ILayoutSink`, `kUIAction`. Deps: goatcore.
- **`goatui-sdl2` / `goatui-sdl3`** — concrete backends (`src/Core/Graphics/SDLn/`), one selected. Deps:
  goatui + SDL + stb (font rendering).
- **`goateditor`** — editor-specific UI (`src/Core/Editor/`): `EditorView`, controllers, `LineRender`,
  etc. Deps: goatcore + goatui.
- **`jsengine`** — `src/Core/JSEngine/` (today's `jsapi` list). Deps: goatcore + goateditor + duktape +
  dukglue.
- **`goatedit`** (exe) — `main.cpp` + a backend; links goateditor + jsengine + goatui-sdlN.

**The hard part (open question, §7):** `src/Core/` root is **not** homogeneous — it mixes true
foundation (`Document`, `Config`) with **app-orchestration** (`Editor`, `RuntimeConfig`, `Runloop`). A
strictly acyclic DAG wants those app types *above* goatcore (an `app`/`goateditor` layer), or goatcore
ends up depending on app concepts. Two granularities:
- **(a) Coarse / low-risk:** CMake **OBJECT libraries** grouped by folder, accepting that Core-root
  mixing isn't resolved. Buys legibility + the utests link-not-recompile win immediately; doesn't fully
  enforce the DAG.
- **(b) Fine / higher-value:** real **STATIC libs** with enforced acyclic deps — requires triaging a
  handful of Core-root files (`Editor`/`RuntimeConfig`/`Runloop`) into the app layer first.

Recommend **(a) first**, evolve to **(b)** opportunistically. (b) makes `check-ui-boundary.sh`
redundant — the linker enforces it.

---

## §6 — Sequenced work items

Ordering principle: safe quick fixes first; FetchContent and warnings are independent and high-value;
the module split unlocks the sub-CMake split + the utests build-time win; packaging is independent.

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
- **Approach:** put declarations in `cmake/Dependencies.cmake`. Document `FETCHCONTENT_SOURCE_DIR_<dep>`
  so existing local `ext/` clones can be reused offline (and for dev against a fork). Drop the CI
  `chmod +x setup_deps.sh` step. **Also fold in the testrunner (`trun`) include** (today the raw
  `-I/usr/local/include` workaround on `utests`, §2): locate it properly via `find_path`/an imported
  target so the `utests` build stops depending on a hand-passed flag — the clean home for the macOS
  SDK-upgrade workaround.
- **Effort/Risk:** medium / low. **Done-when:** a clean checkout configures + builds with **no**
  `setup_deps.sh` and no network `git clone` from inside `CMakeLists.txt`; CI green. **Depends-on:** —

### CM-2 — Per-compiler flags + scoped third-party suppressions
- **Goal:** one source of truth for warnings, chosen by compiler; stop blanket-suppressing our own code.
- **Scope:** collapse the three `-Wno-*` copies into a `cmake/CompilerWarnings.cmake` helper keyed on
  `CMAKE_CXX_COMPILER_ID`. **Scope** dukglue/yaml-cpp/duktape suppressions to *those* targets
  (interface lib / `set_source_files_properties`), not the editor. Add build-type flags (Debug `-O0 -g`,
  Release `-O2 -DNDEBUG`, RelWithDebInfo). Add a `GEDIT_SANITIZE=address|undefined` option to formalise
  the ad-hoc `cmake-build-asan/`.
- **Effort/Risk:** medium / medium (re-enabling warnings on our code may surface real ones — fix or
  re-suppress deliberately). **Done-when:** our code compiles under `-Wall -Wextra -Wshadow` with only
  *intentional*, documented suppressions; third-party noise stays off. **Depends-on:** nicely paired
  with CM-3 (per-target scoping is easier once targets exist).

### CM-3 — Split `editorsrc` into module libraries (§5)
- **Goal:** real CMake targets mirroring the folder modules; utests links libs instead of recompiling.
- **Scope:** `goatcore`, `goatui`, `goatui-sdlN`, `goateditor`, `jsengine` per §5. Start **coarse**
  (OBJECT libs, granularity (a)). `goatedit` + `utests` link them.
- **Approach:** OBJECT libs first (smallest diff, immediate legibility + double-compile fix). Wire
  `target_link_libraries` to express the DAG. Leave Core-root app/foundation triage (granularity (b))
  as a follow-up.
- **Effort/Risk:** high (broad) / medium. **Done-when:** `goatedit` + `utests` build green from the new
  targets; verified-green 236-test set passes; build time drops (utests no longer recompiles the world).
  **Depends-on:** CM-0.

### CM-4 — Split `CMakeLists.txt` into per-module sub-CMake files
- **Goal:** each module owns its `CMakeLists.txt` under its folder, via `add_subdirectory`.
- **Scope:** `src/Core/CMakeLists.txt` (goatcore), `src/Core/UI/CMakeLists.txt` (goatui),
  `src/Core/Editor/CMakeLists.txt`, `src/Core/Graphics/CMakeLists.txt`, `src/Core/JSEngine/`,
  `utests/CMakeLists.txt`. Root keeps project setup, deps, packaging.
- **Effort/Risk:** medium / low (mechanical once CM-3 defines the targets). **Done-when:** root
  `CMakeLists.txt` is short (project + deps + add_subdirectory + packaging); each module file is
  self-contained. **Depends-on:** CM-3.

### CM-5 — `CMakePresets.json`
- **Goal:** standard, discoverable configure/build/test invocations; encode the SDL backend choice,
  build types, and the sanitiser.
- **Scope:** presets for `debug`/`release`/`asan` × (implicit per-platform SDL). `trun` test preset.
- **Effort/Risk:** low / none. **Done-when:** `cmake --preset debug && cmake --build --preset debug`
  works on both dev boxes; README/CLAUDE.md updated; CI can use `--preset`. **Depends-on:** — (nicer
  after CM-1/CM-2 so presets reference real options).

### CM-6 — Linux packaging (fix `.deb`, add AppImage)
- **Goal:** a working `.deb` + a portable AppImage (§3).
- **Scope:** fix install rules (CM-0), set `CPACK_DEBIAN_PACKAGE_DEPENDS` (SDL, ncurses, yaml-cpp);
  add an AppImage step (linuxdeploy/appimagetool, bundling SDL). Flatpak/Snap explicitly deferred.
- **Effort/Risk:** medium / low. **Done-when:** `cpack -G DEB` installs + launches on a clean
  Ubuntu; the AppImage runs on a second distro without installing SDL. **Depends-on:** CM-0 (install
  rules); pairs with CM-3 (clean targets help, not required).

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

---

## §7 — Open questions / decisions log

- **Module granularity (§5):** coarse OBJECT libs (a) vs fine STATIC libs with Core-root triage (b).
  **Leaning (a) first, (b) opportunistically.** Confirm before CM-3.
- **Core-root triage:** do we move `Editor`/`RuntimeConfig`/`Runloop` into an explicit app layer (needed
  for a truly acyclic DAG), or accept the coarse split? Tied to the above.
- **Linux format priority (§3):** AppImage-primary + fix `.deb`, defer Flatpak/Snap — **needs the user's
  nod** (AppImage chosen on the "download-and-run, bundles SDL" merit; revisit if Flathub presence is
  wanted).
- **macOS signing:** personal-use unsigned `.dmg` now, codesign/notarise later? (Assumed yes.)
- **dukglue sourcing:** RESOLVED — FetchContent the **`gnilk/dukglue` fork** at the pinned SHA (§2). It's
  header-only and the worktree is clean, so the fork commit is the whole story; no separate
  pre-processing. (The pre-processed dep is `duktape`, which stays vendored.)
- **AI-7 relationship:** these module libs are **internal** (no install/export, no public API) — they do
  **not** reopen the dropped AI-7 (a shipped `goatui` lib). Recorded so a future reader doesn't conflate
  them.

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
