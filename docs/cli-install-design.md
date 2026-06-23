# GoatEdit — Command-Line Launch & Self-Install Design

Discussion summary covering: making the macOS `.app` bundle launchable from the
shell, and evolving that into a JS boot-script-driven self-install feature
(`:install-cli`).

---

## 1. Problem

GoatEdit is packaged as a proper macOS `.app` bundle (CMake/CPack), which is
nice for double-click use but unusable from a terminal as-is — there's no
`goat` command on `PATH`.

---

## 2. Baseline solution — shell wrapper script

The standard pattern used by VS Code (`code`), Sublime (`subl`), etc.: a thin
script that `exec`s the real binary inside the bundle.

```bash
#!/usr/bin/env bash
exec /Applications/GoatEdit.app/Contents/MacOS/GoatEdit "$@"
```

Key points:
- Use `exec` so the shell process is replaced, not forked.
- Forward `"$@"` so file arguments reach GoatEdit's normal argv parsing.
- Script-based launch (vs. Finder/`open`) gives reliable CWD inheritance,
  which matters if GoatEdit resolves relative paths against the shell's CWD.

**Tested and confirmed working.**

### Install location options

| Location | Pros | Cons |
|---|---|---|
| `~/bin` or `~/.local/bin` | No `sudo` | Not on `PATH` by default on macOS — needs dotfile setup |
| `/usr/local/bin` | On `PATH` by default, not the Homebrew prefix on Apple Silicon (`/opt/homebrew`) | Needs `sudo` to write |
| Symlink to bundle binary directly | Zero extra files | Brittle if app moves/reinstalls; can't inject default flags |

**Recommendation:** `/usr/local/bin` for zero dotfile maintenance, via a
wrapper script (not a raw symlink) so default flags can be added later.

### `$EDITOR` / `$VISUAL` integration

```bash
export EDITOR="/usr/local/bin/goat"
export VISUAL="$EDITOR"
```

Requires the wrapper to **block until the editor exits** (no early
fork/detach in `main()`) for tools like `git commit` to work correctly.

---

## 3. Packaging the wrapper — CPack options

| Approach | Mechanism | Trade-off |
|---|---|---|
| `.pkg` postflight script | CPack `productbuild` generator, `CPACK_POSTFLIGHT_<COMPONENT>_SCRIPT` | Fully automatic for end users, but only works for `.pkg`, not `.dmg`; more "installer magic" |
| Ship wrapper inside bundle, install manually/once | `install()` into `Contents/Resources/bin/goat`, then user symlinks it out | Transparent, inspectable, matches project philosophy; less automated |
| Skip entirely (drag-to-`Applications` `.dmg`) | N/A | No install-time hook exists for `.dmg` |

```cmake
# Postflight option (.pkg only)
set(CPACK_POSTFLIGHT_GOATEDIT_SCRIPT "${CMAKE_SOURCE_DIR}/packaging/postinstall.sh")
```

```bash
#!/bin/bash
# packaging/postinstall.sh
cat > /usr/local/bin/goat << 'EOF'
#!/usr/bin/env bash
exec /Applications/GoatEdit.app/Contents/MacOS/GoatEdit "$@"
EOF
chmod +x /usr/local/bin/goat
```

**Decision for current single-user daily-driver use:** ship the wrapper
inside the bundle (`Contents/Resources/bin/goat`) and install via a manual
step or `make install-cli`-style target. Confirmed as the preferred,
philosophy-consistent approach — install behavior should be explicit and
inspectable, not silent installer magic. Reserve `.pkg` postflight automation
for if/when GoatEdit is ever distributed to other people.

---

## 4. Evolving the idea — self-install via the JS boot-script

Rather than a manual install step, use GoatEdit's own scripting layer
(DukTape + dukglue) to detect, prompt, and install itself. Good fit for the
script engine: small, self-contained, does real OS/filesystem work rather
than a toy demo, and is unit-testable in the Headless backend.

### Command shape

```
:install-cli            " interactive — detect, prompt, install
:install-cli --check    " report status only, no prompt (useful for boot-script auto-check)
```

### Required native binding surface (new dukglue bindings)

No generic file-write/chmod bindings exist yet — this requires new ones.
Keep them minimal and generic (reusable beyond this one feature):

```cpp
namespace JSBindings {
    std::string App_bundlePath();                     // NSBundle mainBundle path, macOS-only

    bool Fs_writeFile(const std::string& path, const std::string& content);
    bool Fs_chmod(const std::string& path, int mode);  // e.g. 0755
    bool Fs_exists(const std::string& path);
    bool Fs_isWritableDir(const std::string& dirPath);
}
```

Design notes:
- **Platform scope** — `App_bundlePath()` is macOS-only; needs a defined
  behavior (empty string / exception / `#ifdef`'d out) on Linux, where
  `:install-cli` is largely meaningless since there's no `.app` bundle.
- **Error model** — `Fs_*` functions should follow whatever
  return-vs-throw convention existing dukglue bindings already use, not a
  new one-off convention.
- **Headless testability** — native side should be "dumb" file I/O on a
  path supplied by JS. All path-selection/fallback-chain logic
  (`~/bin` → `~/.local/bin` → `/usr/local/bin`) lives in JS so it can be
  unit-tested in Headless mode against a fake `$HOME`/`PATH` without
  touching the real filesystem or mocking C++.

### The privilege escalation constraint

`/usr/local/bin` is often user-writable on single-user Macs (especially
Apple Silicon where Homebrew may have already `chown`'d it) but this isn't
guaranteed.

**Principle adopted:** never build silent privilege escalation
(`sudo`-equivalent, `osascript ... with administrator privileges`) into an
auto-running boot script. The detect-and-offer flow is good UX; silent
elevation triggered by an automatic startup check is not. If no writable
location is found, the script should print the exact command and let the
user run `sudo` themselves in a real terminal.

### JS-side logic sketch

```js
const Cli = {
    candidatePaths: function() {
        return [
            Env.home() + "/bin",
            Env.home() + "/.local/bin",
            "/usr/local/bin"
        ];
    },

    findInstallTarget: function() {
        for (const dir of this.candidatePaths()) {
            if (Fs.exists(dir) && Fs.isWritableDir(dir)) {
                return dir + "/goat";
            }
        }
        return null; // nothing writable found
    },

    isInstalled: function() {
        for (const dir of this.candidatePaths()) {
            if (Fs.exists(dir + "/goat")) { return true; }
        }
        return false;
    },

    install: function() {
        const target = this.findInstallTarget();
        if (!target) {
            Editor.statusMessage("No writable bin dir found. Run manually: sudo sh -c 'cat > /usr/local/bin/goat ...'");
            return false;
        }
        const script = "#!/usr/bin/env bash\nexec " + App.bundlePath() + "/Contents/MacOS/GoatEdit \"$@\"\n";
        if (!Fs.writeFile(target, script)) { return false; }
        Fs.chmod(target, 0o755);
        Editor.statusMessage("Installed: " + target);
        return true;
    }
};
```

### Boot-script hook

```js
if (!Cli.isInstalled()) {
    Editor.promptYesNo("goat: enable cmd-line access? [y/n]", function(yes) {
        if (yes) { Cli.install(); }
    });
}
```

`:install-cli --check` calls `Cli.isInstalled()` directly with no prompt.

### Open gap identified during discussion

The Editor scripting API currently has **no yes/no or status-line input
prompt mechanism** exposed to JS (`Editor.promptYesNo` above is aspirational,
not real yet). This is described as straightforward to add, but is a
prerequisite piece of UI plumbing — not just a JS-side concern — and should
be scoped alongside the new `Fs_*`/`App_bundlePath()` bindings, since both
are new native/script boundary surface going into the same feature.

---

## 5. Work-item list

### Phase 1 — Status-line prompt plumbing
- [ ] Add a yes/no (or general text) status-line input prompt mechanism to the Editor's internal API
- [ ] Expose the prompt mechanism to JS via dukglue (e.g. `Editor.promptYesNo(msg, callback)`)
- [ ] Confirm callback-based (async) signature fits the existing UI event loop, vs. a blocking call

### Phase 2 — Native filesystem/bundle bindings
- [ ] Add `App.bundlePath()` binding (macOS `NSBundle.mainBundle`), with defined behavior on non-macOS backends
- [ ] Add `Fs.exists(path)` binding
- [ ] Add `Fs.isWritableDir(path)` binding, distinguishing "doesn't exist" vs. "exists but not writable"
- [ ] Add `Fs.writeFile(path, content)` binding
- [ ] Add `Fs.chmod(path, mode)` binding
- [ ] Settle on a consistent error model (return value vs. JS exception) matching existing dukglue binding conventions
- [ ] Unit test all new bindings against the Headless backend with a mocked `$HOME`/path set

### Phase 3 — JS install logic
- [ ] Implement `Cli.candidatePaths()` fallback chain (`~/bin` → `~/.local/bin` → `/usr/local/bin`)
- [ ] Implement `Cli.isInstalled()`
- [ ] Implement `Cli.findInstallTarget()`
- [ ] Implement `Cli.install()`, writing the wrapper script and chmod'ing it `0755`
- [ ] Handle the "no writable location found" case by printing the manual `sudo` command rather than attempting elevation

### Phase 4 — Command + boot-script integration
- [ ] Add `:install-cli` internal command (interactive)
- [ ] Add `:install-cli --check` flag (status only, no prompt)
- [ ] Wire `Cli.isInstalled()` check into the default boot script with the yes/no prompt
- [ ] Verify boot-script auto-check doesn't fire repeatedly/annoyingly once declined once (e.g. respect a "don't ask again" choice)

### Phase 5 — Packaging follow-through (lower priority / later)
- [ ] Add CMake `install()` rule placing the wrapper script inside `Contents/Resources/bin/goat` in the bundle
- [ ] Document the manual symlink step (`ln -s .../Contents/Resources/bin/goat /usr/local/bin/goat`) as the supported non-scripted path
- [ ] Revisit `.pkg` + postflight automation only if/when distributing GoatEdit beyond personal use
