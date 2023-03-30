# Editor Project

Someone (Peter Norton?) once said that in order to really become a programmer one has to write
an editor.

This is my playground....

![screenshot](screenshots/main_edit_230318.png?raw=true)

Currently only macOS....

Goals
- Mimic Amiga AsmOne with a dedicated cmd-line mode (like a shell) for editor commands
- Pass through to underlying shell in case the command is not an editor command
- Support for different backends
  - NCurses (terminal)
  - IMGUI (?) for Portable UI
  - etc...

Pressing ESC enters command mode, which is an embedded terminal (bash/sh/zsh or whatever you fancy)
![screenshot](screenshots/cmd_view_230318.png?raw=true)

The idea with the shell/command is similar to a game-console or if you want 
a multi-line VI/VIM command mode. Allows us to execute editor functionality through a command-line interface.
If the given command is not an editor command it is sent to the terminal. In order to avoid actual terminal commands
being blocked by editor commands or vice-verse a special prefix (configurable) must be present before any editor command.

Noteworthy:
A terminal application can't properly trap SHIFT + <certain keys> (at least not through ncurses).
Therefore, the terminal backend (NCurses for now) will spawn a special thread very similar
to a keylogger. You will have to allow the terminal "Input Monitoring" capabilities in 
the macOS settings IF you want SHIFT+Arrow keys to work properly.

The code for this "keylogger" is also part of the repository.

Got a quite simple but nice stack-based language tokenizer running. Use it to drive syntax highlighting.


Dependencies:
- yaml-cpp, https://github.com/jbeder/yaml-cpp
- ncurses, on *nix it is generally available, otherwise: https://invisible-island.net/ncurses/announce.html
- nlohmann/json, https://github.com/nlohmann/json
- libSDL, https://github.com/libsdl-org/SDL currently using master branch (SDL3)
- stb, https://github.com/nothings/stb, ttf and rect_pack (added to my repo)

libSDL, ncurses you can download and install the packages. Stb is added to the project under 'ext'.
yaml-cpp, nlohmann you should clone and link - see CMakeLists.txt

## SDL3 Backend
![screenshot](screenshots/sdlbackend_only_editor.png?raw=true)
Took a stab at testing if multiple backends where actually possible. Decided to try libSDL - haven't used it before.
Worked fine, using stb_ttf for true type fond rendering.
Only rendering right now - keyboard driver up next..



Playing around with embedding a script language to drive command-mode cmd-let's.
## JavaSript
Played around with DukTape and QuickJS.
QuickJS is probably good but it is completely lacking in documentation (or I can't find it). In the end
I had ChatGPT generating a simple stub with a hello world kind of example for me. 

Had better luck with Duktape. In the end I got a structure where you can write plugins nicely and with
support for `require` and such. Will start integrating it as a proof-of-concept. Still not sure what I want
to use it for except one little thing...

- People know it
- I don't like it
- Small programs, so it doesn't really matter

## Python
- You can do most and it is familiar to many
- Quite a lot of boilerplate for integration
- Tricky to "pre-compile and cache" (or it least it looks like it from documentation)
  - as we don't want to compile the cmd-let's every time we open the editor (defeats the purpose of beeing fast)

## Lua
- Way more simple to integrate
- Can expose LUA objects but must be backed by c-style interface, see: https://www.lua.org/pil/28.1.html 
- Pre-compile??
- Use LuaCpp library (C++ wrapper) instead of directly dealing with low-level C-api?
  - https://github.com/jordanvrtanoski/luacpp
- 

