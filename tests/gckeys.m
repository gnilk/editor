// This method can hook event _AND_ disable them (like SHIFT+PageUp)
// compile and run from the commandline with:
//    clang -fobjc-arc -framework Cocoa  ./foo.m  -o foo
//    sudo ./foo

#import <Foundation/Foundation.h>
#import <AppKit/NSEvent.h>

typedef CFMachPortRef EventTap;

// - - - - - - - - - - - - - - - - - - - - -

@interface KeyChanger : NSObject
{
@private
    EventTap            _eventTap;
    CFRunLoopSourceRef  _runLoopSource;
    CGEventRef          _lastEvent;
}
@end

// - - - - - - - - - - - - - - - - - - - - -

CGEventRef _tapCallback(
        CGEventTapProxy proxy,
        CGEventType     type,
        CGEventRef      event,
        KeyChanger*     listener
);

// - - - - - - - - - - - - - - - - - - - - -

@implementation KeyChanger

- (BOOL)tapEvents
{
    if (!_eventTap) {
        NSLog(@"Initializing an event tap.");

            ProcessSerialNumber psn;
            OSErr err = MacGetCurrentProcess(&psn);
            if (err) {
                printf("GetCurrentProcess, failed!\n");
                exit(1);
            }
        CGEventMask eventMask = kCGEventMaskForAllEvents;
        //eventMask = CGEventMaskBit( kCGEventKeyDown ) | CGEventMaskBit( kCGEventFlagsChanged ) | CGEventMaskBit( NSEventTypeSystemDefined );

        // kCGHeadInsertEventTap -- new event tap should be inserted before any pre-existing event taps at the same location,
        _eventTap = CGEventTapCreateForPSN(&psn,   //kCGHIDEventTap, kCGSessionEventTap, kCGAnnotatedSessionEventTap
                                      kCGHeadInsertEventTap,
                                      kCGEventTapOptionDefault,
                                      eventMask,
                                      (CGEventTapCallBack)_tapCallback,
                                      (__bridge void *)(self));
        if (!_eventTap) {
            NSLog(@"unable to create event tap. must run as root or "
                  "add privlidges for assistive devices to this app.");
            return NO;
        }
    }
    CGEventTapEnable(_eventTap, TRUE);

    return [self isTapActive];
}

- (BOOL)isTapActive
{
    return CGEventTapIsEnabled(_eventTap);
}

- (void)listen
{
    if( ! _runLoopSource ) {
        if( _eventTap ) { //dont use [self tapActive]
            _runLoopSource = CFMachPortCreateRunLoopSource( kCFAllocatorDefault,
                                                            _eventTap, 0);
            // Add to the current run loop.
            CFRunLoopAddSource( CFRunLoopGetCurrent(), _runLoopSource,
                                kCFRunLoopCommonModes);

            NSLog(@"Registering event tap as run loop source.");
            CFRunLoopRun();
        }else{
            NSLog(@"No Event tap in place! You will need to call "
                  "listen after tapEvents to get events.");
        }
    }
}

- (CGEventRef)processEvent:(CGEventRef)cgEvent
{
    NSEvent* event = [NSEvent eventWithCGEvent:cgEvent];


    NSUInteger modifiers = [event modifierFlags] &
                           (NSEventModifierFlagCommand | NSEventModifierFlagOption | NSEventModifierFlagShift | NSEventModifierFlagControl);

    enum {
        kVK_ANSI_3 = 0x14,
        kVK_PageUp = 116,
    };


    switch( event.type ) {
        case NSEventTypeFlagsChanged:
            NSLog(@"NSFlagsChanged: %d", event.keyCode);
            if (!([event modifierFlags] & NSEventModifierFlagShift)) {
                NSLog(@"  KeyUp");
            } else {
                NSLog(@"  KeyDown");
            }
            break;

        case NSEventTypeSystemDefined:
            NSLog(@"NSSystemDefined: %lx", event.data1);
            return NULL;

        case kCGEventKeyDown:
            NSLog(@"KeyDown: %d, %s", event.keyCode, [event.characters UTF8String]);
            break;

        default:
            NSLog(@"WTF");
    }

    // Retruning NIL will explicitly prohibt any key-strokes in the system...
    return nil;
    if (
            event.keyCode == kVK_PageUp
            && modifiers == NSEventModifierFlagShift
            )
    {
        NSLog(@"Got SHIFT+PageUp (disabling)");
        return nil;

        event = [NSEvent keyEventWithType: event.type
                                 location: NSZeroPoint
                            modifierFlags: event.modifierFlags & ! NSEventModifierFlagShift
                                timestamp: event.timestamp
                             windowNumber: event.windowNumber
                                  context: nil
                               characters: @"#"
              charactersIgnoringModifiers: @"#"
                                isARepeat: event.isARepeat
                                  keyCode: event.keyCode];
    }

//
//    // TODO: add other cases and do proper handling of case
//    if (
//            event.keyCode == kVK_ANSI_3
//            && modifiers == NSEventModifierFlagShift
//            )
//    {
//        NSLog(@"Got SHIFT+3");
//
//        event = [NSEvent keyEventWithType: event.type
//                                 location: NSZeroPoint
//                            modifierFlags: event.modifierFlags & ! NSEventModifierFlagShift
//                                timestamp: event.timestamp
//                             windowNumber: event.windowNumber
//                                  context: nil
//                               characters: @"#"
//              charactersIgnoringModifiers: @"#"
//                                isARepeat: event.isARepeat
//                                  keyCode: event.keyCode];
//    }
    _lastEvent = [event CGEvent];
    CFRetain(_lastEvent); // must retain the event. will be released by the system
    return _lastEvent;
}

- (void)dealloc
{
    if( _runLoopSource ) {
        CFRunLoopRemoveSource( CFRunLoopGetCurrent(), _runLoopSource, kCFRunLoopCommonModes );
        CFRelease( _runLoopSource );
    }
    if( _eventTap ) {
        //kill the event tap
        CGEventTapEnable( _eventTap, FALSE );
        CFRelease( _eventTap );
    }
    [super dealloc];
}

@end

// - - - - - - - - - - - - - - - - - - - - -

CGEventRef _tapCallback(
        CGEventTapProxy proxy,
        CGEventType     type,
        CGEventRef      event,
        KeyChanger*     listener
)
{
    printf("WEFWEFW\n");
    //Do not make the NSEvent here.
    //NSEvent will throw an exception if we try to make an event from the tap timout type
    @autoreleasepool {
        if( type == kCGEventTapDisabledByTimeout ) {
            NSLog(@"event tap has timed out, re-enabling tap");
            [listener tapEvents];
            return nil;
        }
        if( type != kCGEventTapDisabledByUserInput ) {
            return [listener processEvent:event];
        }
    }
    return event;
}

// - - - - - - - - - - - - - - - - - - - - -

int main(int argc, const char * argv[])
{
    @autoreleasepool {
        KeyChanger* keyChanger = [KeyChanger new];
        [keyChanger tapEvents];
        [keyChanger listen];//blocking call.
    }
    return 0;
}