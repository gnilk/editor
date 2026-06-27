//
// gansi capability sequences.
//
#include "gansi/Caps.h"

using namespace gnilk::ansi;

std::string Caps::EnableMouse() {
    // Normal tracking + button-event motion + SGR extended coordinates.
    return "\x1b[?1000h\x1b[?1002h\x1b[?1006h";
}

std::string Caps::DisableMouse() {
    return "\x1b[?1006l\x1b[?1002l\x1b[?1000l";
}

std::string Caps::EnablePaste() {
    return "\x1b[?2004h";
}

std::string Caps::DisablePaste() {
    return "\x1b[?2004l";
}

std::string Caps::EnableFocus() {
    return "\x1b[?1004h";
}

std::string Caps::DisableFocus() {
    return "\x1b[?1004l";
}

std::string Caps::EnableKittyKeyboard(int flags) {
    return "\x1b[>" + std::to_string(flags) + "u";
}

std::string Caps::DisableKittyKeyboard() {
    return "\x1b[<u";
}

std::string Caps::EnableModifyOtherKeys() {
    return "\x1b[>4;2m";
}

std::string Caps::DisableModifyOtherKeys() {
    return "\x1b[>4;0m";
}

std::string Caps::Base64(const std::string &data) {
    static const char *kAlphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    const size_t n = data.size();
    while (i + 2 < n) {
        uint32_t triple = (static_cast<uint8_t>(data[i]) << 16) |
                          (static_cast<uint8_t>(data[i + 1]) << 8) |
                          (static_cast<uint8_t>(data[i + 2]));
        out += kAlphabet[(triple >> 18) & 0x3F];
        out += kAlphabet[(triple >> 12) & 0x3F];
        out += kAlphabet[(triple >> 6) & 0x3F];
        out += kAlphabet[triple & 0x3F];
        i += 3;
    }
    size_t rem = n - i;
    if (rem == 1) {
        uint32_t triple = static_cast<uint8_t>(data[i]) << 16;
        out += kAlphabet[(triple >> 18) & 0x3F];
        out += kAlphabet[(triple >> 12) & 0x3F];
        out += "==";
    } else if (rem == 2) {
        uint32_t triple = (static_cast<uint8_t>(data[i]) << 16) |
                          (static_cast<uint8_t>(data[i + 1]) << 8);
        out += kAlphabet[(triple >> 18) & 0x3F];
        out += kAlphabet[(triple >> 12) & 0x3F];
        out += kAlphabet[(triple >> 6) & 0x3F];
        out += "=";
    }
    return out;
}

std::string Caps::SetClipboardOSC52(const std::string &utf8) {
    // OSC 52 ; c ; <base64> ST   (ST = ESC \)
    return "\x1b]52;c;" + Base64(utf8) + "\x1b\\";
}
