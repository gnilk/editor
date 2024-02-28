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
#include "AnsiParser.h"
#include "StrUtil.h"

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

std::string AnsiParser::Parse(const uint8_t *ptrBuffer, const size_t szBuffer) {

    buffer = ptrBuffer;
    idx = 0;
    max = szBuffer;
    strParsed = {};
    cmdBuffer = {};

    return ParseInternal();

}


std::string AnsiParser::ParseInternal() {

    while(At() && (idx < max)) {
        // There are multiple ways to get to this point...
        // see: https://vt100.net/emu/dec_ansi_parser
        if((At() == ESC_7BIT) || (At() == ESC_8BIT)) {
            if (!Next()) return strParsed;
            auto clsCode = At();

            if ((clsCode>=0x40) && (clsCode<=0x5f)) {
                switch(clsCode) {
                    case CSI_7BIT :
                    case CSI_8BIT :
                        ParseCSI();
                        break;
                    case OSC_7BIT :
                    case OSC_8BIT :
                        ParseOSC();
                        // printf("After OSC\n");
                        // HexDump::ToConsole(&buffer[idx],max-idx);
                        break;
                    case DCS_8BIT :
                    case PM_7BIT :  // Privacy sequence (?) - takes one string argument
                    case APC_7BIT : // APC Sequence (?) - takes one string argument
                    case DCS_7BIT :
                        // this is a sequence - we need to get rid of it
                        while(Next() && At()!=ST);
                        Next();
                        break;
                    default:
                        Next(); // just swallow the class code
                        break;
                }
            }
        } else {
            strParsed += At();
            Next();
        }
    }
    return strParsed;
}


bool AnsiParser::InRange(const std::pair<int,int> &range) {
    if (At() < range.first) {
        return false;
    }
    if (At() > range.second) {
        return false;
    }
    return true;
}
void AnsiParser::ParseCSI() {
    // see: https://en.wikipedia.org/wiki/ANSI_escape_code#CSIsection

    // printf("CSI\n");

    static std::pair<int, int> CSI_PARAM_RANGE = {0x30,0x3f};
    static std::pair<int, int> CSI_INTERM_RANGE = {0x20,0x2f};
    static std::pair<int, int> CSI_CMD_RANGE = {0x40,0x7e};

    static std::pair<int, int> CSI_END_RANGE = {0x40,0x7e};

    // If this would be a state machine;

    // FIXME: I need a parser here!
    //
    // This  is how it works (I think).
    // <cmd>;<value>;...
    //
    // The cmd controls the rest
    // Example:
    //   ESC[01;32m
    // Dissected:
    //   ESC    - 0x1b (or any other)
    //   [      - 0x5b, CSI escape (Control Sequence Indicator)
    //   <value>;<value>  <- as long as the string is within 'CSI_PARAM_RANGE' (all numerical value plus ';' and a few others
    //   <cmd>  - Must be outside 'CSI_PARAM_RANGE' but within CSI_END_RANGE
    //
    // If a command has a value is determined by the command...
    // It is not possible to skip a command unless you parse all of the commands...
    //

    std::string csiParamString;
    std::vector<std::string> params;

    // Get Parameters; this is a ';' list of numbers
    while(Next() && (At() != 0) && InRange(CSI_PARAM_RANGE)) {
        // Do nothing..
        switch(At()) {
                break;
            case ';' :
                // printf("CSI Cmd: %s\n", csiParamString.c_str());
                params.push_back(csiParamString);
                csiParamString = "";
                break;
                // FIXME: Support for ':' as seen in some (xterm/Konsole)
                // see: iterm2, VT100CSIParser.m @ 250
            default :
                csiParamString += At();
        }
    }
    if (!csiParamString.empty()) {
        // 0x40-0x7e => dispatch
        // printf("Last CSI Cmd: %s\n", csiParamString.c_str());
        params.push_back(csiParamString);
    }
    // This should be possible - but I haven't seend it
    if (InRange(CSI_INTERM_RANGE)) {
        // Just swallow of these for the time being
        while(Next() && (InRange(CSI_INTERM_RANGE)));
    }

    // NOTE: See kitty (https://github.com/kovidgoyal/kitty)
    //       vt-parser.c, line 1100 and onwards...
    //       Basically all parsers I've seen parse the string in to some structure and then dispatches from that structure
    //       Specifically cursor-movements and similar..

    // Now figure out which command we have...
    if (InRange(CSI_CMD_RANGE)) {
        // dispatch
        if (At() == 'm') {
            // printf("SGR - CSI Select Graphics Rendition\n");
            for(auto &s : params) {
                // printf("  %s\n", s.c_str());
                auto cmd = std::stoi(s);
                if ((cmd>=30) && (cmd<=37)) {
                    EmitCmd(kAnsiCmd::kSetForegroundColor, cmd - 30);
                } else if ((cmd>=40) && (cmd<=47)) {
                    EmitCmd(kAnsiCmd::kSetBackgroundColor, cmd - 40);
                } else {
                    // FIXME: Ok, this needs better handling
                    switch (cmd) {
                        case 0 :
                            // printf("  %d - Reset to Normal\n", cmd);
                            EmitCmd(kAnsiCmd::kSGRReset);
                            break;
                        case 1 : // bold or increased intensity
                            EmitCmd(kAnsiCmd::kFontBold);
                            break;
                        case 2 : // Faint, decreased intensity - not supported
                            break;
                        case 3 : // Italic - not supported
                            EmitCmd(kAnsiCmd::kFontItalic);
                            break;
                        case 4 : // Underline
                            EmitCmd(kAnsiCmd::kFontUnderline);
                            break;
                        case 7 : // invert (swap fg/bg)
                            EmitCmd(kAnsiCmd::kInvertColors);
                            break;
                        case 10 : // Normal font
                            EmitCmd(kAnsiCmd::kFontNormal);
                            break;
                        case 38 :
                            // printf("  %d - Set foreground color - with arguments (not supported)\n", cmd);
                            break;
                        case 39 :
                            // printf("  %d - Set default foreground color\n");
                            EmitCmd(kAnsiCmd::kSetDefaultForegroundColor);
                            break;
                        case 48 :
                            // printf("  %d - Set background color - with arguments (not supported)\n", cmd);
                            break;
                        case 49 :
                            // printf("  %d - Set default foreground color\n");
                            EmitCmd(kAnsiCmd::kSetDefaultForegroundColor);
                            break;
                    }
                }
            }
            // do something useful...
        }
        Next();
        return;
    }

    // 0x20-0x2f => collect
    // Get Intermediate, if any...
    if ((At() >= 0x20) && (At()<=0x2f)) {
        while ((At() != 0) && InRange(CSI_INTERM_RANGE)) {
            // Do nothing..
            Next();
        }
    }
    // Now one final byte in range 0x40-0x7e
    if(InRange(CSI_END_RANGE)) {
        Next();
    }
}

void AnsiParser::EmitCmd(gedit::AnsiParser::kAnsiCmd kCmd) {
    CMD cmd={strParsed.size(), kCmd, {}};
    cmdBuffer.push_back(cmd);
}

void AnsiParser::EmitCmd(gedit::AnsiParser::kAnsiCmd kCmd, int param) {
    CMD cmd={strParsed.size(), kCmd, {param}};
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
void AnsiParser::ParseOSC() {
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

std::string AnsiParser::OSC_ParseStringToBel() {
    std::string strOut;
    while (Next() && At() != C0_BEL) {
        strOut += At();
    }
    Next(); // Remove 'C0_BEL'

    // printf("Window Title: %s\n", title.c_str());

    return strOut;
}

bool AnsiParser::Next() {
    if (idx < (max -1)) {
        idx++;
        return true;
    }
    return false;
}
uint8_t AnsiParser::At() {
    return buffer[idx];
}
