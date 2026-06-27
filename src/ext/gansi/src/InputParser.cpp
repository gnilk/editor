//
// gansi input parser — byte-stream state machine -> neutral Events. Split-read tolerant.
//
#include "gansi/InputParser.h"

#include <string>

using namespace gnilk::ansi;

namespace {
    constexpr uint8_t kEsc = 0x1b;
    const std::string kPasteEnd = "\x1b[201~";

    // xterm modifier parameter (1 + bitmask) -> Mod flags.
    Mod ModFromXterm(int param) {
        Mod m = Mod::None;
        if (param < 1) {
            return m;
        }
        int mask = param - 1;
        if (mask & 1) m |= Mod::Shift;
        if (mask & 2) m |= Mod::Alt;
        if (mask & 4) m |= Mod::Ctrl;
        if (mask & 8) m |= Mod::Super;
        return m;
    }

    // UTF-8 byte count from the lead byte (0 = not a valid lead byte).
    int Utf8Len(uint8_t c) {
        if (c < 0x80) return 1;
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        if ((c & 0xF8) == 0xF0) return 4;
        return 0;
    }

    char32_t Utf8Decode(const std::string &s, size_t pos, int len) {
        uint8_t c = static_cast<uint8_t>(s[pos]);
        char32_t cp;
        if (len == 1) cp = c;
        else if (len == 2) cp = c & 0x1F;
        else if (len == 3) cp = c & 0x0F;
        else cp = c & 0x07;
        for (int i = 1; i < len; ++i) {
            cp = (cp << 6) | (static_cast<uint8_t>(s[pos + i]) & 0x3F);
        }
        return cp;
    }

    std::u32string Utf8ToU32(const std::string &s) {
        std::u32string out;
        size_t i = 0;
        while (i < s.size()) {
            uint8_t c = static_cast<uint8_t>(s[i]);
            int len = Utf8Len(c);
            if (len == 0 || i + static_cast<size_t>(len) > s.size()) {
                out.push_back(c);
                ++i;
                continue;
            }
            out.push_back(Utf8Decode(s, i, len));
            i += len;
        }
        return out;
    }
}

void InputParser::Feed(const std::string &bytes, std::vector<Event> &out) {
    Feed(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size(), out);
}

void InputParser::Feed(const uint8_t *bytes, size_t n, std::vector<Event> &out) {
    pending.append(reinterpret_cast<const char *>(bytes), n);

    size_t pos = 0;
    while (pos < pending.size()) {
        if (inPaste) {
            size_t idx = pending.find(kPasteEnd, pos);
            if (idx == std::string::npos) {
                // Terminator not seen yet — flush all but a possible partial terminator at the tail.
                size_t keep = std::min(kPasteEnd.size() - 1, pending.size() - pos);
                size_t end = pending.size() - keep;
                if (end > pos) {
                    pasteBuf.append(pending, pos, end - pos);
                    pos = end;
                }
                break;
            }
            pasteBuf.append(pending, pos, idx - pos);
            PasteEvent pe;
            pe.text = Utf8ToU32(pasteBuf);
            out.push_back(pe);
            pasteBuf.clear();
            inPaste = false;
            pos = idx + kPasteEnd.size();
            continue;
        }

        size_t consumed = ParseToken(pos, out);
        if (consumed == 0) {
            break;      // incomplete sequence — wait for more bytes
        }
        pos += consumed;
    }
    pending.erase(0, pos);
}

void InputParser::Flush(std::vector<Event> &out) {
    // A lone pending ESC resolves to the Escape key (no continuation arrived).
    if (pending.size() == 1 && static_cast<uint8_t>(pending[0]) == kEsc) {
        KeyEvent k;
        k.key = Key::Escape;
        out.push_back(k);
        pending.clear();
    }
}

size_t InputParser::ParseToken(size_t pos, std::vector<Event> &out) {
    uint8_t c = static_cast<uint8_t>(pending[pos]);
    size_t avail = pending.size() - pos;

    if (c == kEsc) {
        if (avail < 2) {
            return 0;       // ESC alone — could begin a sequence; hold pending (Flush resolves it)
        }
        uint8_t n1 = static_cast<uint8_t>(pending[pos + 1]);
        if (n1 == '[') {
            return ParseCSI(pos, out);
        }
        if (n1 == 'O') {
            return ParseSS3(pos, out);
        }
        // ESC + char -> Alt + char (legacy modifier encoding).
        int len = Utf8Len(n1);
        if (len == 0) {
            KeyEvent k;
            k.isChar = true;
            k.ch = n1;
            k.mods = Mod::Alt;
            out.push_back(k);
            return 2;
        }
        if (avail < static_cast<size_t>(1 + len)) {
            return 0;
        }
        KeyEvent k;
        k.isChar = true;
        k.ch = Utf8Decode(pending, pos + 1, len);
        k.mods = Mod::Alt;
        out.push_back(k);
        return static_cast<size_t>(1 + len);
    }

    // C0 controls with a named key.
    if (c == '\r' || c == '\n') {
        KeyEvent k; k.key = Key::Enter; out.push_back(k); return 1;
    }
    if (c == '\t') {
        KeyEvent k; k.key = Key::Tab; out.push_back(k); return 1;
    }
    if (c == 0x7f || c == 0x08) {
        KeyEvent k; k.key = Key::Backspace; out.push_back(k); return 1;
    }
    // Remaining C0 in 0x01..0x1a -> Ctrl + letter.
    if (c >= 0x01 && c <= 0x1a) {
        KeyEvent k;
        k.isChar = true;
        k.ch = U'a' + (c - 1);
        k.mods = Mod::Ctrl;
        out.push_back(k);
        return 1;
    }
    if (c < 0x20) {
        return 1;       // other C0 control — ignore
    }

    // Printable / UTF-8 text.
    int len = Utf8Len(c);
    if (len == 0) {
        return 1;       // stray continuation byte — skip
    }
    if (avail < static_cast<size_t>(len)) {
        return 0;       // incomplete UTF-8 sequence
    }
    KeyEvent k;
    k.isChar = true;
    k.ch = Utf8Decode(pending, pos, len);
    out.push_back(k);
    return static_cast<size_t>(len);
}

size_t InputParser::ParseCSI(size_t pos, std::vector<Event> &out) {
    const size_t size = pending.size();
    size_t i = pos + 2;     // past ESC [

    char marker = 0;
    if (i < size) {
        char c = pending[i];
        if (c == '<' || c == '?' || c == '>' || c == '=') {
            marker = c;
            ++i;
        }
    }

    // Parameter bytes: digits, ';', ':'.
    size_t paramStart = i;
    while (i < size) {
        char c = pending[i];
        if ((c >= '0' && c <= '9') || c == ';' || c == ':') {
            ++i;
        } else {
            break;
        }
    }
    std::string params = pending.substr(paramStart, i - paramStart);

    // Intermediate bytes 0x20..0x2f.
    while (i < size) {
        unsigned char c = static_cast<unsigned char>(pending[i]);
        if (c >= 0x20 && c <= 0x2f) {
            ++i;
        } else {
            break;
        }
    }
    if (i >= size) {
        return 0;       // final byte not yet received
    }
    char finalByte = pending[i];
    size_t consumed = (i - pos) + 1;

    // Split params by ';' into ints (each field: value before any ':' subparam).
    std::vector<int> ps;
    if (!params.empty()) {
        size_t p = 0;
        while (true) {
            size_t semi = params.find(';', p);
            std::string field = params.substr(p, semi == std::string::npos ? std::string::npos : semi - p);
            size_t colon = field.find(':');
            std::string main = (colon == std::string::npos) ? field : field.substr(0, colon);
            ps.push_back(main.empty() ? 0 : std::stoi(main));
            if (semi == std::string::npos) {
                break;
            }
            p = semi + 1;
        }
    }
    auto paramOr = [&](size_t idx, int def) -> int {
        return idx < ps.size() ? ps[idx] : def;
    };

    // --- SGR mouse (1006) ---
    if (marker == '<') {
        int b = paramOr(0, 0);
        int col = paramOr(1, 1);
        int row = paramOr(2, 1);
        MouseEvent m;
        m.col = col - 1;
        m.row = row - 1;
        if (b & 4) m.mods |= Mod::Shift;
        if (b & 8) m.mods |= Mod::Alt;
        if (b & 16) m.mods |= Mod::Ctrl;
        int low = b & 0x3;
        if (b & 64) {
            m.kind = MouseEvent::Kind::Wheel;
            m.wheel = (low == 0) ? 1 : -1;
            m.button = low;
        } else if (finalByte == 'm') {
            m.kind = MouseEvent::Kind::Release;
            m.button = low;
        } else if (b & 32) {
            m.kind = MouseEvent::Kind::Move;
            m.button = low;
        } else {
            m.kind = MouseEvent::Kind::Press;
            m.button = low;
        }
        out.push_back(m);
        return consumed;
    }

    // Modifier param (2nd param) for keyboard sequences.
    Mod mods = Mod::None;
    if (ps.size() >= 2 && ps[1] >= 1) {
        mods = ModFromXterm(ps[1]);
    }

    auto pushKey = [&](Key key) {
        KeyEvent k;
        k.key = key;
        k.mods = mods;
        out.push_back(k);
    };

    switch (finalByte) {
        case 'A': pushKey(Key::Up); return consumed;
        case 'B': pushKey(Key::Down); return consumed;
        case 'C': pushKey(Key::Right); return consumed;
        case 'D': pushKey(Key::Left); return consumed;
        case 'H': pushKey(Key::Home); return consumed;
        case 'F': pushKey(Key::End); return consumed;
        case 'P': pushKey(Key::F1); return consumed;
        case 'Q': pushKey(Key::F2); return consumed;
        case 'S': pushKey(Key::F4); return consumed;
        case 'I': out.push_back(FocusEvent{true}); return consumed;
        case 'O': out.push_back(FocusEvent{false}); return consumed;
        case 'u': {
            // Kitty keyboard: CSI codepoint ; mods u
            int cp = paramOr(0, 0);
            switch (cp) {
                case 13: pushKey(Key::Enter); return consumed;
                case 9:  pushKey(Key::Tab); return consumed;
                case 27: pushKey(Key::Escape); return consumed;
                case 8:
                case 127: pushKey(Key::Backspace); return consumed;
                default:
                    if (cp >= 0x20) {
                        KeyEvent k;
                        k.isChar = true;
                        k.ch = static_cast<char32_t>(cp);
                        k.mods = mods;
                        out.push_back(k);
                    }
                    return consumed;
            }
        }
        case '~': {
            int keyNum = paramOr(0, 0);
            switch (keyNum) {
                case 1: case 7: pushKey(Key::Home); break;
                case 2: pushKey(Key::Insert); break;
                case 3: pushKey(Key::Delete); break;
                case 4: case 8: pushKey(Key::End); break;
                case 5: pushKey(Key::PageUp); break;
                case 6: pushKey(Key::PageDown); break;
                case 11: pushKey(Key::F1); break;
                case 12: pushKey(Key::F2); break;
                case 13: pushKey(Key::F3); break;
                case 14: pushKey(Key::F4); break;
                case 15: pushKey(Key::F5); break;
                case 17: pushKey(Key::F6); break;
                case 18: pushKey(Key::F7); break;
                case 19: pushKey(Key::F8); break;
                case 20: pushKey(Key::F9); break;
                case 21: pushKey(Key::F10); break;
                case 23: pushKey(Key::F11); break;
                case 24: pushKey(Key::F12); break;
                case 200: inPaste = true; break;    // bracketed-paste start
                default: break;                      // unknown — ignore
            }
            return consumed;
        }
        case 'R':
            // Modified F3 (CSI 1;mod R). Bare CPR (row;col R) is never requested, so this is safe.
            pushKey(Key::F3);
            return consumed;
        default:
            return consumed;        // unknown CSI — consume and ignore
    }
}

size_t InputParser::ParseSS3(size_t pos, std::vector<Event> &out) {
    if (pending.size() - pos < 3) {
        return 0;
    }
    char f = pending[pos + 2];
    KeyEvent k;
    switch (f) {
        case 'A': k.key = Key::Up; break;
        case 'B': k.key = Key::Down; break;
        case 'C': k.key = Key::Right; break;
        case 'D': k.key = Key::Left; break;
        case 'H': k.key = Key::Home; break;
        case 'F': k.key = Key::End; break;
        case 'P': k.key = Key::F1; break;
        case 'Q': k.key = Key::F2; break;
        case 'R': k.key = Key::F3; break;
        case 'S': k.key = Key::F4; break;
        default: return 3;      // consume, ignore
    }
    out.push_back(k);
    return 3;
}
