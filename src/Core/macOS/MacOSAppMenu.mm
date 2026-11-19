//
// See MacOSAppMenu.h
//
#import <AppKit/AppKit.h>

#include "MacOSAppMenu.h"

void gedit::MacOSAppMenu::DropCloseKey() {
    @autoreleasepool {
        for (NSMenuItem *topItem in [[NSApp mainMenu] itemArray]) {
            for (NSMenuItem *item in [[topItem submenu] itemArray]) {
                if ([item action] == @selector(performClose:)) {
                    [item setKeyEquivalent:@""];
                }
            }
        }
    }
}
