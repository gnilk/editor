//
// Created by gnilk on 15.02.24.
//
// Others:
//  mintty; https://github.com/mintty/mintty/blob/master/src/termout.c#L1823
//  kitty; https://github.com/kovidgoyal/kitty/blob/master/kitty/vt-parser.c
//  iterm2: https://github.com/gnachman/iTerm2/blob/7b26eb979b21863b463c43952baed07fb999ba3c/sources/VT100CSIParser.m#L174
//

//
// The purpose of this is to separate the interleaved terminal/ansi control commands from the string.
// As such the control is saved to a separate command-list and the string is cleaned from any escape codes
//
// Also - refactor this to:
// * stream based
// * state machine
//
// Just a minimal set of stuff is supported right now.
// Prio 1:
//  - get colors and basic cursor movements within the same line supported
// Prio 2:
//  - get full cursor movement support (ability to launch 'vim')
//
//

#include <stdio.h>

#include "HexDump.h"
#include "VTermParser.h"
#include "StrUtil.h"
#include "logger.h"

using namespace gedit;

// Detect ANSI ESC codes (this is one of many), see: https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797
// https://en.wikipedia.org/wiki/ANSI_escape_code

static const uint8_t ESC_7BIT=0x1b;
static const uint8_t ESC_8BIT=0x9b;
static const uint8_t CSI_7BIT=0x5b; // Control Sequence Indicator
static const uint8_t CSI_8BIT=0x9b; // Control Sequence Indicator
static const uint8_t OSC_7BIT=0x5d; // Operating System Command
static const uint8_t OSC_8BIT=0x9d; // Operating System Command
static const uint8_t APC_7BIT=(uint8_t)('_');
static const uint8_t PM_7BIT=(uint8_t)('^');
static const uint8_t DCS_7BIT=(uint8_t)('P');
static const uint8_t DCS_8BIT=0x90;
// static const uint8_t ST=0x5c;   // See: https://xtermjs.org/docs/api/vtfeatures/#c1

static const uint8_t ST=0x9c;   // See: https://xtermjs.org/docs/api/vtfeatures/#c1

std::string VTermParser::Parse(const uint8_t *ptrBuffer, const size_t szBuffer) {
    auto logger = gnilk::Logger::GetLogger("AnsiParser");

    // Log raw bytes before any parsing so we can see exactly what arrives
    std::string hex;
    for (size_t i = 0; i < szBuffer && ptrBuffer[i] != 0; i++) {
        char tmp[8];
        uint8_t b = ptrBuffer[i];
        if (b == 0x1b) {
            snprintf(tmp, sizeof(tmp), " ESC");
        } else if (b < 0x20 || b == 0x7f) {
            snprintf(tmp, sizeof(tmp), " ^%02x", b);
        } else {
            snprintf(tmp, sizeof(tmp), " %c", (char)b);
        }
        hex += tmp;
    }
    logger->Debug("RAW[%zu]:%s", szBuffer, hex.c_str());

    buffer = ptrBuffer;
    idx = 0;
    max = szBuffer;
    strParsed = {};
    cmdBuffer = {};

    return ParseInternal();
}


std::string VTermParser::ParseInternal() {
    auto logger = gnilk::Logger::GetLogger("AnsiParser");
    logger->Dbg("Start");

    while(At() && (idx < max)) {
        // There are multiple ways to get to this point...
        // see: https://vt100.net/emu/dec_ansi_parser
        if((At() == ESC_7BIT) || (At() == ESC_8BIT)) {
            if (!Next()) return strParsed;
            auto clsCode = At();

            if ((clsCode >= 0x30) && (clsCode <= 0x3f)) {
                // ESC Fp — private two-character sequences
                switch (clsCode) {
                    case '7': EmitCmd(kAnsiCmd::kSaveCursor);    break;
                    case '8': EmitCmd(kAnsiCmd::kRestoreCursor); break;
                    case '=': break;  // DECKPAM — application keypad mode (ignore)
                    case '>': break;  // DECKPNM — normal keypad mode (ignore)
                    default: {
                        auto logger = gnilk::Logger::GetLogger("AnsiParser");
                        logger->Debug("ESC Fp unhandled: 0x%02x ('%c')",
                                      (unsigned)clsCode, isprint(clsCode) ? clsCode : '?');
                        break;
                    }
                }
                Next();
            } else if ((clsCode>=0x40) && (clsCode<=0x5f)) {
                switch(clsCode) {
                    case CSI_7BIT :
                    case CSI_8BIT :
                        logger->Dbg("CSI found");
                        ParseCSI();
                        break;
                    case OSC_7BIT :
                    case OSC_8BIT :
                        ParseOSC();
                        break;
                    case DCS_8BIT :
                    case PM_7BIT :  // Privacy sequence
                    case APC_7BIT : // APC Sequence
                    case DCS_7BIT :
                        while(Next() && At()!=ST);
                        Next();
                        break;
                    case 'M':  // Reverse Index — scroll down or move cursor up
                        EmitCmd(kAnsiCmd::kReverseIndex);
                        Next();
                        break;
                    default:
                        Next();
                        break;
                }
            }
        } else {
            strParsed += At();
            if (!Next()) break;
        }
    }
    return strParsed;
}


bool VTermParser::InRange(const std::pair<int,int> &range) {
    if (At() < range.first) {
        return false;
    }
    if (At() > range.second) {
        return false;
    }
    return true;
}
void VTermParser::ParseCSI() {
    // see: https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_sequences
    static std::pair<int, int> CSI_PARAM_RANGE = {0x30, 0x3f};
    static std::pair<int, int> CSI_INTERM_RANGE = {0x20, 0x2f};
    static std::pair<int, int> CSI_CMD_RANGE = {0x40, 0x7e};

    std::string csiParamString;
    std::vector<std::string> params;
    bool isPrivate = false;

    // Collect parameters: semicolon-separated integers.
    // 0x3c-0x3f are parameter modifier/private bytes (<=>?): skip them as
    // numeric content but track '?' for the private-mode dispatch.
    while (Next() && (At() != 0) && InRange(CSI_PARAM_RANGE)) {
        switch (At()) {
            case ';' :
                params.push_back(csiParamString.empty() ? "0" : csiParamString);
                csiParamString = "";
                break;
            case '?' : isPrivate = true; break;
            case '<' : break;  // parameter modifier — skip
            case '=' : break;  // parameter modifier — skip
            case '>' : break;  // parameter modifier — skip
            default :
                csiParamString += At();
        }
    }
    if (!csiParamString.empty()) {
        params.push_back(csiParamString);
    }

    // Swallow intermediate bytes (rare)
    while (InRange(CSI_INTERM_RANGE)) {
        Next();
    }

    if (!InRange(CSI_CMD_RANGE)) {
        return;
    }

    // Helper: extract param by index with a default
    auto P = [&](int i, int def = 1) -> int {
        if (i >= (int)params.size() || params[i].empty()) {
            return def;
        }
        return std::stoi(params[i]);
    };

    switch (At()) {
        // --- Cursor movement ---
        case 'A': EmitCmd(kAnsiCmd::kCursorUp,      P(0)); break;
        case 'B': EmitCmd(kAnsiCmd::kCursorDown,    P(0)); break;
        case 'C': EmitCmd(kAnsiCmd::kCursorForward, P(0)); break;
        case 'D': EmitCmd(kAnsiCmd::kCursorBack,    P(0)); break;

        case 'H':  // CUP — cursor position (ESC[row;colH, default 1;1)
        case 'f':  // HVP — same semantics
            EmitCmd(kAnsiCmd::kCursorPos, P(0, 1), P(1, 1));
            break;

        // --- Erase ---
        case 'J': EmitCmd(kAnsiCmd::kEraseInDisplay, P(0, 0)); break;
        case 'K': EmitCmd(kAnsiCmd::kEraseInLine,    P(0, 0)); break;

        // --- Scroll region ---
        case 'r': EmitCmd(kAnsiCmd::kSetScrollRegion, P(0, 1), P(1, 0)); break;

        // --- Cursor save / restore ---
        case 's': EmitCmd(kAnsiCmd::kSaveCursor);    break;
        case 'u': EmitCmd(kAnsiCmd::kRestoreCursor); break;

        // --- Insert / delete ---
        case 'L': EmitCmd(kAnsiCmd::kInsertLine, P(0, 1)); break;
        case 'M': EmitCmd(kAnsiCmd::kDeleteLine, P(0, 1)); break;
        case '@': EmitCmd(kAnsiCmd::kInsertChar, P(0, 1)); break;
        case 'P': EmitCmd(kAnsiCmd::kDeleteChar, P(0, 1)); break;

        // --- Terminal queries ---
        case 'n':
            if (!isPrivate && P(0, 0) == 6) {
                EmitCmd(kAnsiCmd::kDeviceStatusReport);
            }
            break;
        case 'c':
            if (!isPrivate) {
                EmitCmd(kAnsiCmd::kPrimaryDA);
            }
            break;

        // --- Private mode set/reset ---
        case 'h':
            if (isPrivate) {
                switch (P(0, 0)) {
                    case 1:    EmitCmd(kAnsiCmd::kCursorKeyModeApp); break;
                    case 25:   EmitCmd(kAnsiCmd::kCursorShow);       break;
                    case 1049: EmitCmd(kAnsiCmd::kEnterAltScreen);   break;
                }
            }
            break;
        case 'l':
            if (isPrivate) {
                switch (P(0, 0)) {
                    case 1:    EmitCmd(kAnsiCmd::kCursorKeyModeNormal); break;
                    case 25:   EmitCmd(kAnsiCmd::kCursorHide);          break;
                    case 1049: EmitCmd(kAnsiCmd::kLeaveAltScreen);      break;
                }
            }
            break;

        // --- SGR — Select Graphic Rendition ---
        case 'm': {
            if (params.empty()) {
                params.push_back("0");
            }
            for (int i = 0; i < (int)params.size(); i++) {
                int cmd = std::stoi(params[i]);
                // 256-colour: ESC[38;5;Nm (FG) or ESC[48;5;Nm (BG)
                if (cmd == 38 && i + 2 < (int)params.size() && std::stoi(params[i+1]) == 5) {
                    EmitCmd(kAnsiCmd::kSetForeground256, std::stoi(params[i+2]));
                    i += 2;
                } else if (cmd == 48 && i + 2 < (int)params.size() && std::stoi(params[i+1]) == 5) {
                    EmitCmd(kAnsiCmd::kSetBackground256, std::stoi(params[i+2]));
                    i += 2;
                } else if (cmd >= 30 && cmd <= 37) {
                    EmitCmd(kAnsiCmd::kSetForegroundColor, cmd - 30);
                } else if (cmd >= 40 && cmd <= 47) {
                    EmitCmd(kAnsiCmd::kSetBackgroundColor, cmd - 40);
                } else if (cmd >= 90 && cmd <= 97) {
                    // Bright foreground — indices 8..15 in the 256-colour table
                    EmitCmd(kAnsiCmd::kSetForeground256, cmd - 90 + 8);
                } else if (cmd >= 100 && cmd <= 107) {
                    // Bright background — indices 8..15
                    EmitCmd(kAnsiCmd::kSetBackground256, cmd - 100 + 8);
                } else {
                    switch (cmd) {
                        case 0:  EmitCmd(kAnsiCmd::kSGRReset);                  break;
                        case 1:  EmitCmd(kAnsiCmd::kFontBold);                  break;
                        case 3:  EmitCmd(kAnsiCmd::kFontItalic);                break;
                        case 4:  EmitCmd(kAnsiCmd::kFontUnderline);             break;
                        case 7:  EmitCmd(kAnsiCmd::kInvertColors);              break;
                        case 10: EmitCmd(kAnsiCmd::kFontNormal);                break;
                        case 39: EmitCmd(kAnsiCmd::kSetDefaultForegroundColor); break;
                        case 49: EmitCmd(kAnsiCmd::kSetDefaultBackgroundColor); break;
                    }
                }
            }
            break;
        }
        case 't': break;  // XTWINOPS — window title save/restore stack (ignore)

        default: {
            auto logger = gnilk::Logger::GetLogger("AnsiParser");
            std::string pstr;
            for (auto &p : params) { pstr += p + ";"; }
            logger->Debug("CSI unhandled: cmd=0x%02x ('%c') private=%s params=[%s]",
                          (unsigned)At(), isprint(At()) ? At() : '?',
                          isPrivate ? "yes" : "no",
                          pstr.c_str());
            break;
        }
    }

    Next();
}

void VTermParser::EmitCmd(gedit::VTermParser::kAnsiCmd kCmd) {
    CMD cmd={strParsed.size(), kCmd, {}};
    cmdBuffer.push_back(cmd);
}

void VTermParser::EmitCmd(gedit::VTermParser::kAnsiCmd kCmd, int param) {
    CMD cmd = {strParsed.size(), kCmd, {param}};
    cmdBuffer.push_back(cmd);
}

void VTermParser::EmitCmd(gedit::VTermParser::kAnsiCmd kCmd, int p1, int p2) {
    CMD cmd = {strParsed.size(), kCmd, {p1, p2}};
    cmdBuffer.push_back(cmd);
}

static const int C0_BEL = 0x07;
static const int C0_ST = 0x9c;
static const int C0_CAN = 0x18; // Cancel
static const int C0_SUB = 0x1a; // Substitue
static const int C0_ESP = 0x1b; // Escape

// All commands are terminated by 'BEL'
enum kOscCommands {
    kWindowTitleAndIcon = 0,    // str
    kIconName = 1,              // str
    kWindowTitleOnly = 2,       // str
    kChangeColor = 4,           // '4;<col num>;<spec>'
    kCreateHyperLink = 8,       // '8;params;uri

    kQueryDefaultForegroundColor = 10,  // str
    kQueryDefaultBackgroundColor = 11,  // str
    kQueryDefaultCursorColor = 12,      // str

    kResetColor = 104,                  // nothing
    kRestoreForegroundColor = 110,      // nothing
    kRestoreBackgroundColor = 111,      // nothing
    kRestoreCursorColor = 112,          // nothing
};

// Quite good overview of OSC stuff
// https://xtermjs.org/docs/api/vtfeatures/
void VTermParser::ParseOSC() {
    // see: https://en.wikipedia.org/wiki/ANSI_escape_code#OSC

    static std::pair<int, int> OSC_ESC_Fs = {0x60,0x7e};
    static std::pair<int, int> OSC_ESC_Fp = {0x30,0x3f};
    static std::pair<int, int> OSC_ESC_nF = {0x20,0x2f};

    Next();

    auto oscType = At();
    if (InRange(OSC_ESC_Fs)) {
        // fs type
        while(Next() && At() != C0_ST) {

        }

    } else if (InRange(OSC_ESC_Fp)) {
        // XTerm - Set Window Title - ends with 'BEL'
        std::string oscCommand;
        while(At() != ';') {
            oscCommand += At();
            if (!Next()) {
                return;
            }
        }
        Next();

        switch(std::stoi(oscCommand)) {
            case kWindowTitleAndIcon : // Window Title
                OSC_ParseStringToBel();
                break;
            case kIconName :
                OSC_ParseStringToBel();
                break;
            case kWindowTitleOnly :
                OSC_ParseStringToBel();
                break;
            case kQueryDefaultForegroundColor :
            case kQueryDefaultBackgroundColor :
            case kQueryDefaultCursorColor :
                OSC_ParseStringToBel();
                break;
            default :
                while(At() != C0_BEL) {
                    if (!Next()) return;
                }
        }

    } else if (InRange(OSC_ESC_nF)) {
        // nF
    }


}

std::string VTermParser::OSC_ParseStringToBel() {
    std::string strOut;
    while (Next() && At() != C0_BEL) {
        strOut += At();
    }
    Next(); // Remove 'C0_BEL'

    // printf("Window Title: %s\n", title.c_str());

    return strOut;
}

bool VTermParser::Next() {
    if (idx < (max -1)) {
        idx++;
        return true;
    }
    return false;
}
uint8_t VTermParser::At() {
    return buffer[idx];
}
