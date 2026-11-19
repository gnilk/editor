//
// The menu SDL gives a macOS app (Window > Close is Cmd+W). Cmd+W is the editor's key - close a terminal tab - so
// the menu must not also take it: it closed the window, the last one, and SDL made that a quit.
//
#ifndef GEDIT_MACOSAPPMENU_H
#define GEDIT_MACOSAPPMENU_H

namespace gedit::MacOSAppMenu {
    // Window > Close keeps its place in the menu (and the window's close button still works), without its key.
    // After the backend has made its app and menu (SDL: after SDL_CreateWindow).
    void DropCloseKey();
}

#endif //GEDIT_MACOSAPPMENU_H
