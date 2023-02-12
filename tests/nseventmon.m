#import <Foundation/Foundation.h>
#import <AppKit/NSEvent.h>
#import <Cocoa/Cocoa.h>

int main(int argc, char **argv) {
    id monitor = [NSEvent addGlobalMonitorForEventsMatchingMask:NSEventMaskAny handler:^(NSEvent *event) {
        //VuoMouse_fireScrollDeltaIfNeeded(event, window, modifierKey, ^(VuoPoint2d point){ scrolled(point); });
        printf("Key down\n");
        return event;
    }];

    [[NSApplication sharedApplication] run];




//    return NSApplicationMain(argc, (const char **)argv);
//    CFRunLoopSourceRef _runLoopSource = CFMachPortCreateRunLoopSource( kCFAllocatorDefault,
//                                                    monitor, 0);
//    // Add to the current run loop.
//    CFRunLoopAddSource( CFRunLoopGetCurrent(), _runLoopSource,
//                        kCFRunLoopCommonModes);
//
//    NSLog(@"Registering ns event tap as run loop source.");
//    CFRunLoopRun();

}