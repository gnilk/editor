//
// Created by gnilk on 14.01.23.
// HID hooking based on: https://github.com/theevilbit/macos
//
//
// Reading of low-level keyboard data in macOS. All keyboard data emitting from here are lowlevel scan-code's and not
// human readable. There is a lame attempt to translate to various categories.
//
// The main purpose is to use it to drive keyboard shortcut handling.
// Features:
// - Can handle modifier keys, distinguish between LEFT/RIGHT shift/ctrl/alt/cmd
// - Can handle all non-printable (ESC, F1..F12,HOME,INSERT,ARROWS,PageUp,PageDown, etc..)
//   These are translated to KeyBoard::kKeyCode_XYZ (raw value is also provided)
// - Do a rough translation if scancode is a well-known ASCII char..
//
// The monitor runs in its own thread, you should attach to it through the event delegate/callback function
//  see: SetEventNotificationsForKeyUpDown
//
// You also need to synchronize reading data off from your "normal" input (getchar, ncruses::getch, or whatever).
// For NCurses the event might pop-up before the actual getch() returns data.
// There are several ways of dealing with this.
// 1) read from 'getch' in a non-blocking fashion - timeout(1) - and if return is ERR and you have an event, do a
//    quick spin around 'getch' until you get something. Need to take care about modifiers!!!
//
// 2) Read only from 'getch' when you get an event - risk of getting out of sync...
//
// etc..
//
// See kbdmon.cpp for an example on how this can be done!
//
// You should NOT use this as a generic keyboard input - better use
//
#include <IOKit/hid/IOHIDValue.h>
#include <IOKit/hid/IOHIDManager.h>
#include <Carbon/Carbon.h>
#include <CoreServices/CoreServices.h>

#include <pthread.h>

#include <map>
#include <unordered_map>

#include <ncurses.h>

#include "Core/KeyCodes.h"
#include "Core/HexDump.h"
#include "MacOSKeyboardMonitor.h"
static void *CaptureThread(void *arg);
static pthread_t captureThread;


// See: IOHIDUsageTables.h (look for table containing: kHIDUsage_KeyboardEscape)
// FIXME: Replace numbers in mapping table with enums from IOHIDUsageTables.h
// Translation table from original source
static std::map<uint32_t, Keyboard::kModifierKeys> scancodeToModifierMap = {
        {0xE0, Keyboard::kMod_LeftCtrl},    // Left Control
        {0xE1, Keyboard::kMod_LeftShift},
        {0xE2, Keyboard::kMod_LeftAlt},
        {0xE3, Keyboard::kMod_LeftCommand},   // called 'Left GUI' in origial code...
        {0xE4, Keyboard::kMod_RightCtrl},
        {0xE5, Keyboard::kMod_RightShift},
        {0xE6, Keyboard::kMod_RightAlt},
        {0xE7, Keyboard::kMod_RightCommand},
};

//
// These are all non-printable editing keys, we can translate from scan-code to these
// So tactics:
// ONLY translate what we know, this table defines the scan-code we translate (need to add KEYPAD keys to it)
// modifierMask and keyCode will be 0,0 respectively if we don't know about it...
// modifierMask can be set (in cast we need to trap CTRL+<char>
//
//
static std::unordered_map<int, Keyboard::kKeyCode> scancodeToKeycodeMap = {
        {0x28, Keyboard::kKeyCode_Return},
        {0x29, Keyboard::kKeyCode_Escape},
        {0x2a, Keyboard::kKeyCode_Backspace},
        {0x2b, Keyboard::kKeyCode_Tab},
        {0x2c, Keyboard::kKeyCode_Space},
        {0x3a, Keyboard::kKeyCode_F1},
        {0x3b, Keyboard::kKeyCode_F2},
        {0x3c, Keyboard::kKeyCode_F3},
        {0x3d, Keyboard::kKeyCode_F4},
        {0x3e, Keyboard::kKeyCode_F5},
        {0x3f, Keyboard::kKeyCode_F6},
        {0x40, Keyboard::kKeyCode_F7},
        {0x41, Keyboard::kKeyCode_F8},
        {0x42, Keyboard::kKeyCode_F9},
        {0x43, Keyboard::kKeyCode_F10},
        {0x44, Keyboard::kKeyCode_F11},
        {0x45, Keyboard::kKeyCode_F12},
        {0x46, Keyboard::kKeyCode_PrintScreen},
        {0x47, Keyboard::kKeyCode_ScrollLock},
        {0x48, Keyboard::kKeyCode_Pause},
        {0x49, Keyboard::kKeyCode_Insert},
        {0x4a, Keyboard::kKeyCode_Home},
        {0x4b, Keyboard::kKeyCode_PageUp},
        {0x4c, Keyboard::kKeyCode_DeleteForward},
        {0x4d, Keyboard::kKeyCode_End},
        {0x4e, Keyboard::kKeyCode_PageDown},
        {0x4f, Keyboard::kKeyCode_RightArraow},
        {0x50, Keyboard::kKeyCode_LeftArrow},
        {0x51, Keyboard::kKeyCode_DownArrow},
        {0x52, Keyboard::kKeyCode_UpArrow},
        {0x53, Keyboard::kKeyCode_NumLock},
};

static std::unordered_map<int, char> asciiTranslationMap;
static std::unordered_map<int, char> asciiShiftTranslationMap;

//
// This is based on inspection...
// Not exactly sure which 'driver' I used to do this...
//
static int createTranslationTable() {
    int scanCode = 0x04;
    for(int i='a';i<='z';i++) {
        asciiTranslationMap[scanCode] = i;
        asciiShiftTranslationMap[scanCode] = std::toupper(i);
        scanCode++;
    }

    static std::string numbers="1234567890";
    static std::string numbersShift="!@#$%^&*()";
    for(int i=0;i<numbers.size();i++) {
        asciiTranslationMap[scanCode] = numbers[i];
        asciiShiftTranslationMap[scanCode] = numbersShift[i];
        scanCode++;
    }

    // These are next to the enter key on my keyboard...
    asciiTranslationMap[0x2f] = '[';
    asciiTranslationMap[0x30] = ']';
    asciiTranslationMap[0x32] = '\\';
    asciiTranslationMap[0x33] = ';';
    asciiTranslationMap[0x34] = '\'';
    asciiTranslationMap[0x35] = 0x60; //'`';
    asciiTranslationMap[0x36] = ',';
    asciiTranslationMap[0x37] = '.';
    asciiTranslationMap[0x38] = '/';
    // 0x64 - key left of 'Z' (Y - physically on a german layout)

    auto testMe = asciiTranslationMap[0x35];


    return scanCode;
}



bool MacOSKeyboardMonitor::Start() {
    createTranslationTable();
    auto err = pthread_create(&captureThread, NULL, CaptureThread, this);
    if (!err) {
        return true;
    }
    if (err == EAGAIN) {
        printf("EAGAIN\n");
    }
    if (err == EPERM) {
        printf("EPERM\n");
    }
    if (err == EINVAL) {
        printf("EINVAL\n");
    }
    return false;
}

bool MacOSKeyboardMonitor::IsModifierPressed(Keyboard::kModifierKeys ctrlKey) {
    if (modifierKeyStatus.find(ctrlKey) != modifierKeyStatus.end()) {
        return modifierKeyStatus[ctrlKey];
    }
    return false;
}


uint8_t MacOSKeyboardMonitor::GetModifiersCurrentlyPressed() const {
    int32_t pressedMask = 0;
    for(auto const &kvp : modifierKeyStatus) {
        //printf(" %d - %s\n", kvp.first, kvp.second?"d":"u");
        if (kvp.second) {
            pressedMask |= kvp.first;
        }
    }
    return pressedMask;
}



Keyboard::kKeyCode MacOSKeyboardMonitor::TranslateScanCodeToKeyCode(uint32_t scancode) {
    if (scancodeToKeycodeMap.find(scancode) == scancodeToKeycodeMap.end()) {
        return Keyboard::kKeyCode_None;
    }
    return scancodeToKeycodeMap[scancode];
}

uint8_t MacOSKeyboardMonitor::TranslateScanCodeToASCII(uint32_t scancode) {
    if (asciiTranslationMap.find(scancode) == asciiTranslationMap.end()) {
        //printw("not found for 0x%.2x", scancode);
        return 0;
    }
    //printw("found for 0x%.2x", scancode);
    return asciiTranslationMap[scancode];
}

void MacOSKeyboardMonitor::OnKeyEvent(uint32_t scancode, long pressed, int32_t pid) {
    if (bDebug) {
        printf("scancode: 0x%.2x (%d), pressed: %ld, keyboardId=%d\n", scancode, scancode, pressed, pid);
    }
    bool scanCodeIsModifier = false;
    if (scancodeToModifierMap.find(scancode) != scancodeToModifierMap.end()) {
        modifierKeyStatus[scancodeToModifierMap[scancode]] = pressed?true:false;
        scanCodeIsModifier = true;
    }

    // Don't emit modifier events unless explicitly enabled...
    if (scanCodeIsModifier && !bEmitEventsForModifiers) {
        return;
    }
    if (pressed && !bNotifyKeyDown) {
        return;
    }
    if (!pressed && !bNotifyKeyUp) {
        return;
    }
    if (cbKeyPress == nullptr) {
        return;
    }

    // Create and dispatch event..
    auto modifiers = GetModifiersCurrentlyPressed();
    auto keyCode = TranslateScanCodeToKeyCode(scancode);

    uint8_t translatedScanCode = 0;

    // The scan code was not a modifier nor a 'special' key - let's try to translate it to something human readable
    if ((keyCode == Keyboard::kKeyCode_None) && !scanCodeIsModifier) {
        translatedScanCode = TranslateScanCodeToASCII(scancode);
    }

    struct Keyboard::HWKeyEvent event {
        .modifiers = modifiers,
        .keyCode = keyCode,
        .scanCode = scancode,
        .translatedScanCode = translatedScanCode,
        .isPressedDown = pressed?true:false
    };
    // Dispatch to listener
    cbKeyPress(event);
}



void myHIDKeyboardCallback( void* context,  IOReturn result,  void* sender,  IOHIDValueRef value )
{
    MacOSKeyboardMonitor *kbdMonitor = (MacOSKeyboardMonitor *)context;

    IOHIDElementRef elem = IOHIDValueGetElement(value);
    if (IOHIDElementGetUsagePage(elem) != 0x07) {
        return;
    }

    IOHIDDeviceRef device = static_cast<IOHIDDeviceRef>(sender);

    //get keyborad product id
    int32_t pid = 1;
    CFNumberGetValue((CFNumberRef)IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey)), kCFNumberSInt32Type, &pid);

    uint32_t scancode = IOHIDElementGetUsage(elem);
    if (scancode < 4 || scancode > 231) {
        // See: kHIDUsage_GD_X
        // long pressed = IOHIDValueGetIntegerValue(value);
        // printw("scdiscard: %d, pressed: %d, pid: %d\n", scancode, (int)pressed, (int)pid);
        return;
    }

    long pressed = IOHIDValueGetIntegerValue(value);
    // Dispatch to above
    kbdMonitor->OnKeyEvent(scancode, pressed, pid);

    //result = kIOReturnUnsupported;
}


CFMutableDictionaryRef myCreateDeviceMatchingDictionary( UInt32 usagePage,  UInt32 usage )
{
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0
            , & kCFTypeDictionaryKeyCallBacks
            , & kCFTypeDictionaryValueCallBacks );
    if ( ! dict ) {
        printf("Failed dict\n");
        return NULL;
    }

    CFNumberRef pageNumberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, & usagePage );
    if ( ! pageNumberRef ) {
        printf("Failed pageNumberRef\n");
        CFRelease( dict );
        return NULL;
    }

    CFDictionarySetValue( dict, CFSTR(kIOHIDDeviceUsagePageKey), pageNumberRef );
    CFRelease( pageNumberRef );

    CFNumberRef usageNumberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, & usage );

    if ( ! usageNumberRef ) {
        printf("Failed usageNumberRef\n");
        CFRelease( dict );
        return NULL;
    }

    CFDictionarySetValue( dict, CFSTR(kIOHIDDeviceUsageKey), usageNumberRef );
    CFRelease( usageNumberRef );

    return dict;
}

void *CaptureThread(void *arg) {
    IOHIDManagerRef hidManager = IOHIDManagerCreate( kCFAllocatorDefault, kIOHIDOptionsTypeNone );

    if (CFGetTypeID(hidManager) != IOHIDManagerGetTypeID()) {
        // this is a HID Manager reference!
        printf("Not HID Manager ref!\n");
        return NULL;
    }

    CFMutableArrayRef matches = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    {
//        CFMutableDictionaryRef keyboard = myCreateDeviceMatchingDictionary( kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard ); //usage page: 0x01, usage: 0x06
//        CFMutableDictionaryRef keypad   = myCreateDeviceMatchingDictionary( kHIDPage_GenericDesktop, kHIDUsage_GD_Keypad ); //usage page: 0x01, usage: 0x07
        CFMutableDictionaryRef keyboard = myCreateDeviceMatchingDictionary( kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard ); //usage page: 0x01, usage: 0x06
        CFMutableDictionaryRef keypad   = myCreateDeviceMatchingDictionary( kHIDPage_GenericDesktop, kHIDUsage_GD_Keypad ); //usage page: 0x01, usage: 0x07

        CFArrayAppendValue(matches, keyboard);
        CFArrayAppendValue(matches, keypad);
    }

    IOHIDManagerSetDeviceMatchingMultiple( hidManager, matches );
    IOHIDManagerRegisterInputValueCallback( hidManager, myHIDKeyboardCallback, arg );
    IOHIDManagerScheduleWithRunLoop( hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode );
    IOReturn res = IOHIDManagerOpen( hidManager, kIOHIDOptionsTypeNone );
    if (res != kIOReturnSuccess) {
        printf("Failed IOHIDManagerOpen: %d\n", res);
        return NULL;
    }
    //printf("[MacOSKeyboardMonitor] Driver - Running!\n");
    CFRunLoopRun();

    return NULL;
}
