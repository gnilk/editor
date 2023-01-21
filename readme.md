# Editor Project

Someone (Peter Norton?) once said that in order to really become a programmer one has to write
an editor.

This is my playground....

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


Dependencies:
- yaml-cpp, https://github.com/jbeder/yaml-cpp
- ncurses, on *nix it is generally available, otherwise: https://invisible-island.net/ncurses/announce.html

