# Editor Project

Someone (Peter Norton?) once said that in order to really become a programmer one has to write
an editor.

This is my playground....

![screenshot](screenshots/main_edit_mode.png?raw=true)

Currently only macOS....

Goals
- Mimic Amiga AsmOne with a dedicated cmd-line mode (like a shell) for editor commands
- Pass through to underlying shell in case the command is not an editor command
- Support for different backends
  - NCurses (terminal)
  - IMGUI (?) for Portable UI
  - etc...

Noteworthy:
A terminal application can't properly trap SHIFT + <certain keys> (at least not through ncurses).
Therefore, the terminal backend (NCurses for now) will spawn a special thread very similar
to a keylogger. You will have to allow the terminal "Input Monitoring" capabilities in 
the macOS settings IF you want SHIFT+Arrow keys to work properly.

The code for this "keylogger" is also part of the repository.

Got q quite simple but nice stack-based language tokenizer running. Use it to drive syntax highlighting.


Dependencies:
- yaml-cpp, https://github.com/jbeder/yaml-cpp
- ncurses, on *nix it is generally available, otherwise: https://invisible-island.net/ncurses/announce.html
- nlohmann/json, https://github.com/nlohmann/json


Playing around with embedding a script language to drive command-mode cmd-let's.
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

