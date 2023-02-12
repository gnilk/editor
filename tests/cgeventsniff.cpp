//
// Created by gnilk on 11.02.23.
//
#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>
#include <ApplicationServices/ApplicationServices.h>

#define MY_DEBUGGED_KEYBOARD 44
static int keyboard = 0;

CGEventRef
myCGEventCallback(CGEventTapProxy proxy, CGEventType type,
                  CGEventRef event, void *refcon) {
// Paranoid sanity check.
//    if ((type != kCGEventKeyDown) && (type != kCGEventKeyUp))
//        return event;



    keyboard = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);


// if you found your keyboard-value...
//    if (keyboard != MY_DEBUGGED_KEYBOARD) {
//        return event;
//    }


// ... you can proceed with your stuff... i.e. remap input, etc.

    printf("%d\n", keyboard);


// Set the modified keycode field in the event.
//    CGEventSetIntegerValueField(event, kCGKeyboardEventKeycode, (int64_t) keycode);

// We must return the event for it to be useful.
    return event;
}

int main(int argc, char* argv[]) {
    CFMachPortRef      eventTap;
    CGEventMask        eventMask;
    CFRunLoopSourceRef runLoopSource;

// Create an event tap. We are interested in key presses.
//kCGEventFlagMaskShift
//    kCGEventFlagMaskShift
//    eventMask = ((1 << kCGEventKeyDown) | (1 << kCGEventKeyUp) | (1<<kCGEventFlagsChanged));
//    eventMask = ((1 << kCGEventKeyDown) | (1 << kCGEventKeyUp));

    eventMask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) | CGEventMaskBit(kCGEventFlagsChanged);
    //eventMask = kCGEventMaskForAllEvents;


    ProcessSerialNumber psn;
    OSErr err = GetCurrentProcess(&psn);
    if (err) {
        printf("GetCurrentProcess, failed!\n");
        exit(1);
    }

//    printf("PID: %d\n", pidSelf);
//    eventTap = CGEventTapCreateForPSN((void *)&psn, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
//                           eventMask, myCGEventCallback, NULL);



//    GetProcessForPID();
//    ProcessSerialNumber psn = {.highLongOfPSN = 0,.lowLongOfPSN = kCurrentProcess};
//
//    GetCurrentProcess(&psn);
//
    eventTap = CGEventTapCreateForPSN(&psn, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                           eventMask, myCGEventCallback, NULL);


// This works but monitors the whole system..
//    eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
//                                eventMask, myCGEventCallback, NULL);

    if (!eventTap) {
        fprintf(stderr, "failed to create event tap\n");
        exit(1);
    }

// Create a run loop source.
    runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);

// Add to the current run loop.
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);

// Enable the event tap.
    CGEventTapEnable(eventTap, true);

// Set it all running.
    CFRunLoopRun();
    return 0;
}