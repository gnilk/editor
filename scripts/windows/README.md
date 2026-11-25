# ConPTY probe

Answers the riskiest question of the Windows port (hitlist #81, `docs/windows-port.md` §4.5) before anything is ported:
**do the prompt marks (OSC 133) and the cwd (OSC 7) survive Windows' pseudo console, and does a build's output keep its
lines?** One file, nothing of the editor. Safe to run: it starts its own small programs and the shells that are installed,
runs `echo` and a failing command in them, and writes only to `conpty_out`.

## Build

In a **Developer PowerShell / x64 Native Tools prompt** of Visual Studio, in this folder:

    cl /std:c++20 /EHsc /utf-8 /O1 conpty_probe.cpp

(`clang-cl` takes the same line - Visual Studio's, `%VCINSTALLDIR%Tools\Llvm\x64\bin\clang-cl.exe`: an older LLVM on `PATH`
comes first, and Visual Studio's library refuses it.) From macOS / Linux with MinGW:

    x86_64-w64-mingw32-g++ -std=c++20 -O1 -s -static -D_WIN32_WINNT=0x0A00 conpty_probe.cpp -o conpty_probe.exe -ladvapi32

The analysis alone runs anywhere: `c++ -std=c++20 conpty_probe.cpp -o probe && ./probe --selftest`.

## Run

    .\conpty_probe.exe

About a minute or two. Git Bash, pwsh, PowerShell and cmd are tried when installed, else `[skip]`. Send the whole `conpty_out` folder:
`report.txt` and each test's raw bytes. The raw bytes of the shell tests hold your prompt, so user and folder names. One
test again: `--only marks`.

A Microsoft `conpty.dll` (NuGet `Microsoft.Windows.Console.ConPTY`, with its `OpenConsole.exe` next to it) instead of the
one built into Windows: `--conpty C:\path\conpty.dll`.

## What an answer means

| `[NO]` in | Means for the port |
|---|---|
| `marks` - they don't come through | No prompt marks from the built-in console. Try `--conpty`; if that works, ship that dll. Otherwise the shell must tell us another way |
| `marks` - not in their place | Present but unusable for "where the output starts" and the build summary - as above |
| `burst` - lines missing | Build messages can't be read from the terminal's bytes |
| `burst` - 'done' before the last line | The build summary would come too early |
| `blank`, a shell's 'blank lines' | Empty lines come as a jump to a fixed row, not line breaks: whoever reads lines from the bytes must take a row change as one |
| a shell's 'moves to a fixed row' | The console places text by row on its own screen: text the editor puts in the grid itself (dot-command output, notices) puts the two out of step |
| `longline` | A long compiler message is cut in two: the message reader must join wrapped lines |
| `modes` - ?2004 | A paste into Claude must not rely on the program asking for bracketed paste |
| `modes` - ?2026, OSC 8, OSC 52, 4:3 | Flicker, links, copy, underline styles: less, not a blocker |
| `resize` - sent again | Output is read twice after a resize: build messages and marks would be doubled |
| `exit` - the pipe doesn't end | `[info]` only: #82's "the shell ended" must wait on the process |
| `ctrlc` | Ctrl+C needs another way than the byte. (The probe switches Ctrl+C on for its children - started over SSH it was off, inherited, and 0x03 did nothing) |
| a shell's `133;C` / `133;D` | That shell's bootstrap needs work - if `marks` is `[yes]`, not ConPTY's fault |
