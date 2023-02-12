#include "focus_oc_wrapper.hpp"

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import "focus_oc.h"

namespace FocusDetector {

    struct AppFocusImpl {
        OCAppFocus *wrapped = nullptr;
    };

    AppFocus::AppFocus()
    noexcept: impl(new AppFocusImpl) {
    impl->
    wrapped = [[OCAppFocus alloc] init];
}

AppFocus::~AppFocus() {
    if (impl) {
        [impl->wrapped release];
    }
    delete impl;
}

void AppFocus::run() {
    [NSApplication sharedApplication];
    [NSApp setDelegate:impl->wrapped];
    [NSApp run];
}
}