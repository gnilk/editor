//
// Created by gnilk on 15.02.24.
//
#include <stdint.h>
#include <testinterface.h>

#include "Core/VTermParser.h"
#include "Core/HexDump.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_vtermparser(ITesting *t);
DLL_EXPORT int test_vtermparser_strip(ITesting *t);
DLL_EXPORT int test_vtermparser_cursor_movement(ITesting *t);
DLL_EXPORT int test_vtermparser_cursor_pos(ITesting *t);
DLL_EXPORT int test_vtermparser_erase(ITesting *t);
DLL_EXPORT int test_vtermparser_terminator_at_boundary(ITesting *t);
DLL_EXPORT int test_vtermparser_scroll_region(ITesting *t);
DLL_EXPORT int test_vtermparser_private(ITesting *t);
DLL_EXPORT int test_vtermparser_decsc(ITesting *t);
}

DLL_EXPORT int test_vtermparser(ITesting *t) {
    return kTR_Pass;
}

static uint8_t raw_ansi_prompt[]= {
        0x1b, 0x5d, 0x30, 0x3b, 0x67, 0x6e, 0x69, 0x6c, 0x6b, 0x40, 0x67, 0x6e, 0x69, 0x6c, 0x6b, 0x2d,
        0x64, 0x65, 0x73, 0x6b, 0x74, 0x6f, 0x70, 0x3a, 0x20, 0x7e, 0x2f, 0x73, 0x72, 0x63, 0x2f, 0x70,
        0x72, 0x69, 0x76, 0x2f, 0x74, 0x65, 0x73, 0x74, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c, 0x6c, 0x2f,
        0x63, 0x6d, 0x61, 0x6b, 0x65, 0x2d, 0x62, 0x75, 0x69, 0x6c, 0x64, 0x2d, 0x64, 0x65, 0x62, 0x75,
        0x67, 0x07, 0x1b, 0x5b, 0x30, 0x31, 0x3b, 0x33, 0x32, 0x6d, 0x67, 0x6e, 0x69, 0x6c, 0x6b, 0x40,
        0x67, 0x6e, 0x69, 0x6c, 0x6b, 0x2d, 0x64, 0x65, 0x73, 0x6b, 0x74, 0x6f, 0x70, 0x1b, 0x5b, 0x30,
        0x30, 0x6d, 0x3a, 0x1b, 0x5b, 0x30, 0x31, 0x3b, 0x33, 0x34, 0x6d, 0x7e, 0x2f, 0x73, 0x72, 0x63,
        0x2f, 0x70, 0x72, 0x69, 0x76, 0x2f, 0x74, 0x65, 0x73, 0x74, 0x73, 0x2f, 0x73, 0x68, 0x65, 0x6c,
        0x6c, 0x2f, 0x63, 0x6d, 0x61, 0x6b, 0x65, 0x2d, 0x62, 0x75, 0x69, 0x6c, 0x64, 0x2d, 0x64, 0x65,
        0x62, 0x75, 0x67, 0x1b, 0x5b, 0x30, 0x30, 0x6d, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

DLL_EXPORT int test_vtermparser_strip(ITesting *t) {
    VTermParser ansiParser;
    auto stripped = ansiParser.Parse(raw_ansi_prompt, sizeof(raw_ansi_prompt));
    auto expected = std::string("gnilk@gnilk-desktop:~/src/priv/tests/shell/cmake-build-debug$ ");
    TR_ASSERT(t, stripped == expected);
    return kTR_Pass;
}

// Helper: parse a raw byte sequence and find the first command matching kCmd
static bool FindCmd(const std::vector<uint8_t> &seq, VTermParser::kAnsiCmd kCmd,
                    VTermParser::CMD *out = nullptr) {
    VTermParser p;
    p.Parse(seq.data(), seq.size());
    for (auto &c : p.LastCmdBuffer()) {
        if (c.cmd == kCmd) {
            if (out) { *out = c; }
            return true;
        }
    }
    return false;
}

DLL_EXPORT int test_vtermparser_cursor_movement(ITesting *t) {
    VTermParser::CMD cmd;

    // ESC[A  — cursor up 1 (default)
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x41}, VTermParser::kAnsiCmd::kCursorUp, &cmd));
    TR_ASSERT(t, cmd.param[0] == 1);

    // ESC[3A — cursor up 3
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x33, 0x41}, VTermParser::kAnsiCmd::kCursorUp, &cmd));
    TR_ASSERT(t, cmd.param[0] == 3);

    // ESC[B  — cursor down
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x42}, VTermParser::kAnsiCmd::kCursorDown, &cmd));
    TR_ASSERT(t, cmd.param[0] == 1);

    // ESC[C  — cursor forward (right)
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x43}, VTermParser::kAnsiCmd::kCursorForward, &cmd));
    TR_ASSERT(t, cmd.param[0] == 1);

    // ESC[5D — cursor back (left) 5
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x35, 0x44}, VTermParser::kAnsiCmd::kCursorBack, &cmd));
    TR_ASSERT(t, cmd.param[0] == 5);

    return kTR_Pass;
}

DLL_EXPORT int test_vtermparser_cursor_pos(ITesting *t) {
    VTermParser::CMD cmd;

    // ESC[H  — cursor home (no params → defaults 1,1)
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x48}, VTermParser::kAnsiCmd::kCursorPos, &cmd));
    TR_ASSERT(t, cmd.param[0] == 1);
    TR_ASSERT(t, cmd.param[1] == 1);

    // ESC[5;10H — row 5, col 10
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x35, 0x3b, 0x31, 0x30, 0x48},
                         VTermParser::kAnsiCmd::kCursorPos, &cmd));
    TR_ASSERT(t, cmd.param[0] == 5);
    TR_ASSERT(t, cmd.param[1] == 10);

    return kTR_Pass;
}

DLL_EXPORT int test_vtermparser_erase(ITesting *t) {
    VTermParser::CMD cmd;

    // ESC[K   — erase to end of line (mode 0 default)
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x4b}, VTermParser::kAnsiCmd::kEraseInLine, &cmd));
    TR_ASSERT(t, cmd.param[0] == 0);

    // ESC[1K  — erase from start of line
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x31, 0x4b}, VTermParser::kAnsiCmd::kEraseInLine, &cmd));
    TR_ASSERT(t, cmd.param[0] == 1);

    // ESC[2J  — erase entire display
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x32, 0x4a}, VTermParser::kAnsiCmd::kEraseInDisplay, &cmd));
    TR_ASSERT(t, cmd.param[0] == 2);

    // ESC[J   — erase to end of display (mode 0 default)
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x4a}, VTermParser::kAnsiCmd::kEraseInDisplay, &cmd));
    TR_ASSERT(t, cmd.param[0] == 0);

    return kTR_Pass;
}

DLL_EXPORT int test_vtermparser_terminator_at_boundary(ITesting *t) {
    // A CSI sequence whose terminator byte is the LAST byte of the buffer must not
    // leak that terminator into the stripped text. Regression: the cursor stopped at
    // max-1, leaving the terminator on the cursor for the outer loop to re-emit.
    VTermParser p;

    // ESC[K at end of buffer — strips to nothing, emits EraseInLine.
    std::vector<uint8_t> eraseOnly = {0x1b, 0x5b, 0x4b};
    auto strippedErase = p.Parse(eraseOnly.data(), eraseOnly.size());
    TR_ASSERT(t, strippedErase.empty());

    // zsh ZLE backspace echo verbatim: BS ESC[K — strips to just the backspace (0x08),
    // emits EraseInLine, and crucially does NOT contain a stray 'K'.
    std::vector<uint8_t> bsErase = {0x08, 0x1b, 0x5b, 0x4b};
    auto strippedBs = p.Parse(bsErase.data(), bsErase.size());
    TR_ASSERT(t, strippedBs.size() == 1);
    TR_ASSERT(t, (uint8_t)strippedBs[0] == 0x08);
    TR_ASSERT(t, strippedBs.find('K') == std::string::npos);

    bool foundErase = false;
    for (auto &c : p.LastCmdBuffer()) {
        if (c.cmd == VTermParser::kAnsiCmd::kEraseInLine) { foundErase = true; }
    }
    TR_ASSERT(t, foundErase);

    return kTR_Pass;
}

DLL_EXPORT int test_vtermparser_scroll_region(ITesting *t) {
    VTermParser::CMD cmd;

    // ESC[1;24r — set scroll region rows 1..24
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x31, 0x3b, 0x32, 0x34, 0x72},
                         VTermParser::kAnsiCmd::kSetScrollRegion, &cmd));
    TR_ASSERT(t, cmd.param[0] == 1);
    TR_ASSERT(t, cmd.param[1] == 24);

    return kTR_Pass;
}

DLL_EXPORT int test_vtermparser_private(ITesting *t) {
    // ESC[?1049h — enter alternate screen
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x3f, 0x31, 0x30, 0x34, 0x39, 0x68},
                         VTermParser::kAnsiCmd::kEnterAltScreen));

    // ESC[?1049l — leave alternate screen
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x3f, 0x31, 0x30, 0x34, 0x39, 0x6c},
                         VTermParser::kAnsiCmd::kLeaveAltScreen));

    return kTR_Pass;
}

DLL_EXPORT int test_vtermparser_decsc(ITesting *t) {
    // ESC 7 — DECSC (save cursor)
    TR_ASSERT(t, FindCmd({0x1b, 0x37}, VTermParser::kAnsiCmd::kSaveCursor));

    // ESC 8 — DECRC (restore cursor)
    TR_ASSERT(t, FindCmd({0x1b, 0x38}, VTermParser::kAnsiCmd::kRestoreCursor));

    // ESC[s — CSI save cursor
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x73}, VTermParser::kAnsiCmd::kSaveCursor));

    // ESC[u — CSI restore cursor
    TR_ASSERT(t, FindCmd({0x1b, 0x5b, 0x75}, VTermParser::kAnsiCmd::kRestoreCursor));

    return kTR_Pass;
}
