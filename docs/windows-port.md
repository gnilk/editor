# Windows port

Status: **analysis, not started** (2026-11-24; 2026-11-25: there is a Windows machine - §1, §6, §7; 2026-11-26: the ConPTY
probe's answers - §4.5.1). A survey of the tree (`feature/editor-agent-ask`, 73634422) and the
user's decisions. Hitlist #81.

The short version: the core is portable already - TextBuffer, the languages, the code index, gvt, the views and Duktape
are plain C++ and SDL. What ties the editor to unix is a small, findable set: ~25 `#if` sites, a handful of POSIX calls,
and three subsystems with a unix implementation (the shell / pty, processes, the folder monitor). The plan: first move
those behind platform classes and functions **on Linux and macOS** (no behaviour change, tests green), then add the
Windows implementations.

Scope: **the SDL backend only**, no ANSI backend on Windows.

---

## 1. Decisions (user, 2026-11-24)

| Question | Decision |
|---|---|
| Backend | **SDL3 only.** It is the macOS backend already and its Windows support is mature; SDL2 stays Linux-only. One backend less to keep in sync |
| Toolchain | **clang-cl in Visual Studio, natively, on the Windows laptop** (proposed 2026-11-25, when the laptop came; §6). It replaces "MinGW or clang-cl, cross-compiled - there is no Windows development machine"; cross-compiling stays the fallback |
| Windows machine | Dell XPS 13 9310, Core i7-1185G7, 32 GB RAM; Visual Studio 2022 and 2026 Community. Its Windows version isn't noted yet - the probe prints it |
| Windows version | **Windows 10 or newer**, the exact floor to be asked of the target user. Technically **10 1903** (2019): ConPTY needs 1809, the UTF-8 code page manifest 1903 |
| User config | **`%USERPROFILE%\.goatedit`** - unix is the heritage; it is what Cursor and others do. The primary user dir there, so no "legacy dir" warning |
| Logs | **`%LOCALAPPDATA%\GoatEdit\Logs`** |
| Projects outside the home dir | **A Windows-only setting**, unix keeps its rule (below `$HOME`). Code often lives in `C:\src`, `D:\src` (§3.2) |
| Shell | **bash by default (Git Bash), a setting** - pwsh and cmd work too, with less (§4.5) |

## 2. The platform layer - classes and functions instead of `#ifdef`

**The rule:** a header names no platform. Each platform has its own `.cpp` - `src/Core/unix/` (Linux + macOS),
`src/Core/windows/`, and `Linux/` / `macOS/` where the two unix platforms differ - and CMake picks which to compile. An
`#if` is left only where it is **macOS-only**, or where Linux and macOS share the code and only Windows differs - and
then it sits inside the `unix/` file, so Windows code never sees it.

**The precedent is in the tree:** `Process` (`Process.h`: "this header names no platform class and the build picks the
implementation" - `unix/ProcessUnix.cpp`) and `SystemFonts` (`Graphics/SystemFonts.h`, with `MacOSSystemFonts.cpp` /
`LinuxSystemFonts.cpp`).

### 2.1 What moves

| Today | Becomes |
|---|---|
| `pthread_setname_np`, an `#if` in `TextBuffer.cpp` and `unix/Shell.cpp` | `Platform::SetThreadName(name)` - Windows `SetThreadDescription` |
| `localtime_r` (`AgentQuestion`, `AgentAsk`) | `Platform::LocalTime(time_t)` - Windows `localtime_s` |
| `getenv("HOME")` in six places (`ClaudeAgent` x2, `AgentIntegrations`, `ExclusionRules`, `XDGEnvironment`) | `UserDirs::Home()` - one owner (§3) |
| `getenv` / `unsetenv` | `Platform::GetEnv` / `UnsetEnv`, UTF-8 - Windows `_wgetenv` / `_wputenv_s` |
| `access(X_OK)` + `PATH` split on `':'` (`AgentIntegrations::FindExecutable`, `XDGEnvironment`) | `Platform::FindExecutable(name)` - Windows: `;` and `PATHEXT`, so `claude` finds `claude.exe` / `claude.cmd` |
| `wordexp` for `~` (`Editor.cpp`, the bootstrap script) | `PathUtil::ExpandUser` - our own, `~/` and `~\` |
| `::fsync` + rename (`SessionManager`'s crash-safe save) | `Platform::AtomicReplace(tmp, target)` - unix fsync + rename; Windows `ReplaceFileW` with a short retry (antivirus and the indexer hold files open) |
| `_NSGetExecutablePath` / `/proc/self/exe` (`InstallPaths`) | `Platform::ExecutablePath()` - Windows `GetModuleFileNameW` |
| `#ifdef` picking `MacOSFolderMonitor` / `LinuxFolderMonitor` as a member of `RuntimeConfig` | a `FolderMonitor` interface + `FolderMonitor::Create()`; `RuntimeConfig` holds a `FolderMonitor::Ref` |
| `unix/Shell` (pty, fork, the job / cwd probes), included by `TerminalController.h` | a `PtySession` interface - `Begin`, `Write`, `SetWindowSize`, `IsForegroundJobRunning`, `WorkingDirectory`, `Close` - with `UnixPty` and `ConPty` |
| `"linux"` / `"macos"` by `#if` in `KeyMapping.cpp`, set again in `Editor.cpp` | `Platform::Name()` - `linux` / `macos` / `windows` - one constant: the keymap section, the `config.yml` section |
| `signal(SIGSEGV)` + `execinfo` (`main.cpp`) | `Platform::InstallCrashHandler()` - Windows `SetUnhandledExceptionFilter` (`backward.hpp` does Windows with dbghelp) |
| `XDGEnvironment` (home, config, state, data dirs, the session type) | split: **`UserDirs`** - where things live, a pure function of (an environment map, the platform), unit-tested for every platform on any OS - and `XDGEnvironment`, unix's reader of that map + the session type |

### 2.2 What stays an `#if`

- macOS-only: `MacOSAppMenu`, `SDL_HINT_MAC_OPTION_AS_ALT`, the started-from-UI check in `Editor.cpp`.
- SDL2 (Linux-only) `SDL_RenderReadPixels`.
- `ProcessUnix.cpp`'s `pipe2` vs `pipe`, and the `__APPLE__` pragmas - unix-internal.
- `TerminalController::isAltArrowsAsWords` becomes a per-platform config default instead (a `macos:` key).

## 3. Where things live on Windows

| What | Linux / macOS (unchanged) | Windows |
|---|---|---|
| User config, themes, `questions/`, `tools/` | `~/.config/goatedit` (`~/.goatedit` legacy, with a warning) | `%USERPROFILE%\.goatedit` - primary |
| Logs, user state | `~/.local/state/goatedit`, `~/Library/Logs/GoatEdit` | `%LOCALAPPDATA%\GoatEdit\Logs` (not roamed, may grow) |
| System assets | `<exedir>/resources`, `SharedSupport`, `share/goatedit`, `$XDG_DATA_DIRS` | `<exedir>\resources` - a zip works as is; `XDG_DATA_DIRS` is not read |
| Project state | `<root>/.goatedit/` | the same |

All of it in `UserDirs`, so "which dir is the preferred user dir, which is legacy" is one table per platform, tested.

### 3.1 config.yml

`config.yml` has a section per platform already (`macos:`, `linux:` - the keymap files). A **`windows:` section** names
the **`Linux/` keymap files** directly - Windows follows the Linux convention (Ctrl = functionality, Alt = UI control), so
nothing is copied until something differs. Windows' own settings go there too (§3.2, §4.5).

`sdl.font_fallback` is a list of font files, tried in order, a missing one skipped - Windows' (`C:\Windows\Fonts\
consola.ttf`, `seguisym.ttf`, `msgothic.ttc`, ...) can be added to it from day one, before any DirectWrite code (§4.8).

### 3.2 Projects outside the home dir

Two unix rules use `$HOME` (`Editor.cpp`):
- **A launch outside home moves to home** (Finder starts apps in `/`).
- **A project root must be a real sub-folder of home** (`ResolveProjectDir`, `IsProjectRoot`) - so `/`, `$HOME` itself,
  `/var/log` never become a project.

On Windows code often lives in `C:\src` or `D:\src`. **Decided:** a Windows-only setting, unix keeps its rule as it is:

    windows:
      # Folders that may hold projects, besides your home folder
      project_folders: [ 'C:\src', 'D:\src' ]

A root inside one of them (or inside `%USERPROFILE%`) can be a project. Still never: a drive root (`C:\`), `%USERPROFILE%`
itself, a folder that holds it, `%WINDIR%`, `%ProgramFiles%`. The "move to home" rule becomes "move to home when the cwd
is in none of them". Its natural home is `UserDirs` (or a small `ProjectFolders` next to it) - a pure function, testable on
any OS. Note: this is a user-config key; a project config must not widen it (`ProjectConfigPolicy`).

## 4. Windows' own issues - beyond compiling

By risk, highest first.

### 4.1 AltGr arrives as Ctrl+Alt

On a Swedish (and most European) layout `[ ] { } \ @ $ |` are AltGr combinations, and Windows reports AltGr as
**LCtrl + RAlt**. Unhandled, each of them fires a Ctrl or Alt binding instead of typing. Same class of problem as the
macOS Option-key note in CLAUDE.md. Fix in `SDL3/SDLKeyboardDriver`: recognise AltGr (RAlt with the phantom LCtrl) and let
the `SDL_EVENT_TEXT_INPUT` through. **Needs a real Windows keyboard** - Wine won't tell.

### 4.2 Line endings

The editor drops `\r` when it loads (`TextBuffer.cpp`, "trailing whitespace (and a '\r') dropped") and saves `\n` - so
every CRLF file is converted, silently, on its first save. Only `ConfigFilePatcher` keeps endings. Needed: a document's
line ending found on load, kept on save, shown in the status line (`CRLF`). Worth doing on unix too - before the port.

### 4.3 UTF-8 strings and paths

~490 `.string()` / `.c_str()` calls on paths. On Windows a narrow string is the ANSI code page unless the process opts
into UTF-8. The cheap, complete fix: compile with UTF-8 source/execution charset and ship an **application manifest with
`activeCodePage = UTF-8`** (Windows 10 1903+) - then narrow strings and paths are UTF-8, as on unix, and none of those call
sites change. (The same manifest declares per-monitor DPI awareness.)

### 4.4 Paths

- **Drive letters:** `BuildOutputParser` takes the location with `rfind(':')` - `C:\x.cpp:12:5` still works, but check.
  And MSVC writes `file.cpp(12): error C2065: ...` - a format to add.
- **Case:** `C:\Src\a.cpp` and `c:\src\a.cpp` are one file - path equality for open documents, the workspace, the monitor.
- **`ExclusionRules`:** gitignore patterns use `/` - match on `generic_string()`.
- **`ClaudeAgent`:** the transcript folder name and the trust key in `~/.claude.json` are built from the path - check
  against what Claude Code on Windows writes.
- The tools (`why.py`, `typediff.py`) talk to git, which gives `/` paths - fine, but `{file}` goes in with `\`.

### 4.5 The terminal - ConPTY, bash by default

**ConPTY** (`CreatePseudoConsole`, Windows 10 1809+) behind `PtySession`. It speaks VT, so gvt needs nothing. Two things
differ from a unix pty:

- **ConPTY is a renderer, not a pass-through** - it keeps its own screen and re-emits it, and may drop or reorder
  sequences it doesn't know. Whether OSC 133 (where a command's output starts, the build summary) survives is the first
  thing to test, on the Windows builds that matter - **the probe** (`scripts/windows/conpty_probe.cpp` on
  `explore/windows-conpty`, its README says what each answer means). It also asks whether a build's lines come whole and
  in order, whether a resize sends the screen again, and how a shell's end shows (#82).
- **No process probes.** Unix asks the kernel for the foreground job (`tcgetpgrp`) and the shell's cwd (`proc_pidinfo`,
  `/proc`). Windows can't, so both come from **the shell telling us** - OSC 7 (cwd) and OSC 133 (prompt / command / done)
  from the prompt. That is better on unix too.

#### 4.5.1 What the probe found (2026-11-25 / 26)

The laptop: Windows 11 26200.9974, the pseudo console built into Windows, Git for Windows 2.32 (bash 4.4), Windows
PowerShell 5.1, cmd. Run at the laptop and over SSH; built with clang-cl 22.1.3 and cl. Raw bytes kept by the user.

| What | Answer | For the editor |
|---|---|---|
| OSC 133 A / B / C / D;n, OSC 7 | **Come through, in their place** - from a program, Git Bash, PowerShell (a prompt function + a PSReadLine Enter handler) and cmd (`PROMPT`, no D) | The plan holds: the marks and the cwd from the shell |
| A build's output, 3000 lines at once | All, whole, in order; 'done' after the last line | Build messages can read the bytes |
| A line wider than the console | Stays one line | - |
| **Blank lines** | **Arrive as a jump to a fixed row** (`ESC[27;1H`), not line breaks - from a program, Git Bash and PowerShell, while the screen has room below. (cmd's `echo.` writes a space, so it shows as a line.) | The message reader must take a row change as a line break - or read rows from the grid |
| **Moves to a fixed row** | PowerShell's line editor and cmd use them in ordinary use; Git Bash only for blank lines | See below - the editor's own text |
| Other sequences | Kept, but not in place: they come before the frame's text. Synchronized output (?2026) arrives as on-off before the text it should wrap, so it does nothing | Nothing to build on ?2026 |
| Resize, leaving the alternate screen | The visible screen is sent again | Text read twice: build messages must not count a line twice (they already merge equal ones) |
| The program ends | The output pipe stays open | #82's "the shell ended" waits on the process handle, not the pipe |
| Ctrl+C (0x03) | Works - **if Ctrl+C is on in the process that starts the shell.** Over SSH it is inherited off | `SetConsoleCtrlHandler(nullptr, FALSE)` before starting a shell |
| At start | `ESC[?9001h` (win32 input mode - asked for, ignore it: VT keys work), `ESC[2J ESC[H`, the title = the program's path | The start clears the visible screen: the banner and restored scrollback belong above it; the tab name shouldn't be the title |
| OSC 7 | Git Bash / PowerShell: `file://host/C:/x`; cmd: `file://localhost/C:\x` | Take both slashes |
| Mouse reports | Not shown - the test program didn't ask the Windows way | Unknown |

**The editor's own text in the grid.** On unix the editor writes into the terminal's grid itself: dot-command output above
the prompt, the start-up banner, #82's notice, the echo of an interrupted line. A shell there only moves relative to where
it is, so that is safe. The pseudo console places text by row on a screen of its own, which knows nothing of what the editor
added - after that, its rows are not the grid's rows. **Open, for phase 4:** on Windows keep the grid exactly the console's
(the editor's text goes elsewhere - a line of the panel's own, the status line, the Messages tab), or keep track of the
offset. Git Bash for ordinary commands moved only for blank lines; PowerShell's line editor moves on every key.

**Microsoft's newer `conpty.dll`** (NuGet `Microsoft.Windows.Console.ConPTY` 1.25.261109001, MIT; `conpty.dll` +
`OpenConsole.exe`; the probe's `--conpty`, run over SSH 2026-11-26) fixes nearly all of the above:

| | Built into Windows | The newer `conpty.dll` |
|---|---|---|
| Blank lines | a jump to a fixed row | **line breaks** |
| Git Bash | jumps for blank lines | **no jumps at all, every mark** |
| Other sequences | moved before the text | **in place** (?2026 works) |
| Resize | the screen sent again | **not sent again** |
| Mouse reports | not seen | **come through** |
| At start | clears the screen | **no clear**; asks `ESC[c` (and `ESC[1t`) and waits for the answer - gvt answers DA1 |
| PowerShell, cmd | fixed rows | **still fixed rows - for the line being typed** (their line editors redraw it in place); the output flows |

**Decided (2026-11-26): ship the newer `conpty.dll` and `OpenConsole.exe`** next to `goatedit.exe` (x64, arm64 - about 1.2 MB),
as Windows Terminal does; load it from the editor's own folder, the built-in console as the fallback, with a notice. Its MIT
licence goes in with the others. With Git Bash as the default shell, the editor's own text in the grid is no problem;
**open for phase 4:** PowerShell and cmd, whose typed line is drawn at a fixed row - the editor's own text above it puts
that line out of step.

Still to run: the probe on a machine of the target user (§8).

**Shell - decided: bash by default, a setting** (`windows: terminal: shell`):

| Shell | What works |
|---|---|
| **Git Bash** (default) | Everything, with the same bootstrap as on unix. Found at Git for Windows' install path (registry `HKLM\SOFTWARE\GitForWindows`, else `%ProgramFiles%\Git\bin\bash.exe`) - **never `bash` on `PATH`**: `C:\Windows\System32\bash.exe` is WSL, a different filesystem (`/mnt/c/...`). Git Bash under ConPTY needs a recent Git for Windows. Claude Code on Windows wants Git Bash anyway |
| **pwsh / PowerShell** | Everything, with a pwsh flavour of the bootstrap: a `prompt` function that emits OSC 133 + OSC 7 |
| **cmd** | **Works** - it's a ConPTY program like any other: you type, it runs, the output shows, colours too. Less: `PROMPT` can emit escapes (`$e]133;A$e\`, and the cwd with `$P`), so the prompt marks and the cwd work, but cmd has no hook after a command - so no exit code and no "command done": the build summary, F8 to the errors after a build, and "is a job running" (close-tab question) are best effort. Clink adds those hooks, if someone runs cmd seriously |

Fallback when the configured one isn't there: Git Bash -> pwsh -> powershell -> cmd, with a notice saying which.

### 4.6 Processes

`posix_spawn` -> `CreateProcessW` with pipes, in a **job object** so a kill takes the whole tree (`Process::StopAll`).
Tools: a `.py` runs with `python3` today (`AgentQuestionTemplate::ToolCommand`) - on Windows `python3` is often the Store
stub; use `py -3`, then `python`. `Platform::FindExecutable` for `git`, `claude`.

### 4.7 The folder monitor

`ReadDirectoryChangesW`, recursive - one handle per root, simpler than inotify's watch per folder.

### 4.8 Fonts

The shipped Andale Mono works as is. The fallback for glyphs it lacks: first the `sdl.font_fallback` list (§3.1); then
`WindowsSystemFonts` with DirectWrite (`IDWriteFontFallback::MapCharacters` gives a font file stb_truetype can load) - the
same shape as `MacOSSystemFonts` (CoreText).

### 4.9 Smaller

- **Keys owned by the system:** Alt+F4, Alt+Space, Alt+Tab, Win+anything.
- **Crash handler:** `SetUnhandledExceptionFilter`, a minidump in the logs folder.
- **Atomic save:** `ReplaceFileW` (§2.1) - `rename` over a file another process holds open fails on Windows.
- **The agent:** Claude Code on Windows - `claude.exe`, its transcripts' folder naming, the trust key (§4.4). Last.

## 5. Build

- **CMake:** a `WIN32` branch next to the `APPLE` / Linux ones; presets `windows-debug` / `windows-release` (a
  cross-compile toolchain file only if the fallback is kept, §6). SDL3 and yaml-cpp through **FetchContent** - there is no `apt` / `brew` (today
  yaml-cpp and SDL come from `find_package`). fmt, json, dukglue, gnklog are fetched already; Duktape and stb are vendored.
- **Link statically** (the C++ runtime: `/MT`) - one `goatedit.exe` + `resources\`, nothing to install.
- **Unknown until the first compile:** `pid_t` / `ssize_t` in headers (`ProcessUnix.h`, `Shell.h` - both behind the
  platform layer by then), GCC builtins, `#pragma clang`. The core looked clean; `__inline` (5 uses) is fine on both.
- **Unit tests:** trun (testrunner) has `_WIN32` code; `libutests` becomes `utests.dll`. `test_main` finds its library
  with `dladdr` -> `GetModuleHandleEx` on Windows. The cases that need `/bin/sh` or a pty (`process`, `shell`,
  `terminalcontroller`) need Windows variants or a unix-only mark.
- **The app checks (`checks.py`)** drive the ANSI backend through a POSIX pty - they don't run on Windows. There, the app
  is tested by hand until the SDL backend gets a scripted driver.

## 6. The Windows machine

Until 2026-11-25 the plan was to port blind: cross-compile, copy, look. With the laptop (§1) the build is native.

- **Toolchain: clang-cl first** (VS 2026's, 22.1.3 - an older standalone LLVM 14 is first on the laptop's `PATH`, and VS's
  library refuses it: name VS's `clang-cl` in the preset, or remove that LLVM) - the same clang front end as the Mac build, so fewer compiler surprises, with Microsoft's
  headers, library and debugger. **MSVC (`cl`) second**, for what it warns about. One Visual Studio is enough (2026), with
  *Desktop development with C++* and *C++ Clang tools for Windows*; its CMake and Ninja. Also: Git for Windows, Python with
  the `py` launcher.
- **The loop:** OpenSSH server on the laptop (set up 2026-11-25, key login): a Mac or Linux box copies the source, builds and
  runs there. Build from a `.bat` that calls `vcvars64.bat` - over SSH the shell is cmd, which expands `%VAR%` before
  `vcvars64` has set it. Ctrl+C is off in processes started over SSH (§4.5.1). The GUI by hand, or the session open on the laptop.
- **Unit tests and the probe on real Windows** - Wine is no longer needed for them.
- **AltGr (§4.1), ConPTY (§4.5), fonts and per-monitor DPI** can be tried from the first build. The laptop's screen may well
  run scaled, which would be the DPI test; not checked yet.
- **One machine is one Windows build.** ConPTY differs between builds, so an answer from the laptop holds for its build. The
  target user's machines (§8) still need the probe run on them - it is one `.exe`.
- **The fallback: cross-compiling.** MinGW-w64 GCC (installed on the Mac; it compiles the probe) or llvm-mingw - MinGW's
  headers have ConPTY and DirectWrite. Worth keeping building if it costs little.
- Optional: a CI runner on Windows to compile and run the unit tests on every push.

## 7. Order

| Phase | What | Size |
|---|---|---|
| - | **The ConPTY probe on the laptop** (§4.5) - **done 2026-11-26** (§4.5.1): the marks work; ship the newer `conpty.dll` - then only PowerShell's and cmd's typed line is a phase 4 problem | - |
| 0 | **The platform layer (§2) on Linux + macOS**, behaviour unchanged, tests green. After `feature/editor-agent-ask` is run on Linux and merged to `dev` - it changes the same files (`Shell`, the agent). Line endings kept (§4.2) - useful on unix too | 2-3 days |
| 1 | **The Windows build:** the presets (clang-cl, MSVC), the CMake branch, deps, compile fixes, the UTF-8 / DPI manifest, `UserDirs` for Windows (§3), the `windows:` config section; stubs (no folder monitor, the terminal panel says "not yet"). Goal: a window, editing, the config, the unit tests on the laptop | 2-4 days |
| 2 | **Files and keys:** AltGr (§4.1), path case and drive letters (§4.4), MSVC build messages, the projects setting (§3.2). Tried on the laptop's keyboard | 2-3 days |
| 3 | `ProcessWindows` (§4.6), `WindowsFolderMonitor` (§4.7), the font fallback (§4.8), the crash handler | 2-3 days |
| 4 | **The terminal:** `ConPty` over the shipped `conpty.dll` (§4.5.1), Git Bash / pwsh / cmd (§4.5), shell integration (OSC 133 / 7) | 3-5 days - the most uncertain |
| 5 | The agent and the tools on Windows, a zip (later an installer) | 1-2 days |

Roughly two to three weeks of focused work; phase 4 carries the risk. Phase 0 is worth doing whether or not the rest
follows.

## 8. Open

- The Windows 10 floor - the target user's machines (1903 is the technical minimum).
- PowerShell and cmd with the editor's own text in the grid: their typed line is drawn at a fixed row (§4.5.1).
- clang-cl or MSVC for the shipped build - try clang-cl first.
- Whether OSC 133 survives ConPTY on the target user's Windows (§4.5.1: it does on 11 26200).
- What Claude Code on Windows names its transcript folders, and how `~/.claude.json` spells the trusted paths (§4.4).
- `project_folders` as the key name (§3.2).
