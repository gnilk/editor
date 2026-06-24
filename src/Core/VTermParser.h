//
// Created by gnilk on 15.02.24.
//

#ifndef SHELL_VTERMPARSER_H
#define SHELL_VTERMPARSER_H

#include <stdint.h>
#include <string>
#include <vector>

namespace gedit {
    class VTermParser {
    public:
        enum class kAnsiCmd {
            // SGR — Select Graphic Rendition
            kSGRReset,
            kFontBold,
            kFontItalic,
            kFontUnderline,
            kFontNormal,
            kInvertColors,
            kSetForegroundColor,           // param[0]: 0..7 index (SGR 30-37)
            kSetBackgroundColor,           // param[0]: 0..7 index (SGR 40-47)
            kSetForeground256,             // param[0]: 0..255 (SGR 38;5;N or 90-97)
            kSetBackground256,             // param[0]: 0..255 (SGR 48;5;N or 100-107)
            kSetDefaultForegroundColor,
            kSetDefaultBackgroundColor,

            // Cursor movement  (param[0] = count, default 1)
            kCursorUp,
            kCursorDown,
            kCursorForward,
            kCursorBack,
            kCursorPos,                    // param[0]=row, param[1]=col (1-indexed)

            // Erase
            kEraseInLine,                  // param[0]: 0=to end, 1=from start, 2=entire
            kEraseInDisplay,               // param[0]: 0=to end, 1=from start, 2=entire

            // Cursor save/restore (cursor position only, not full grid)
            kSaveCursor,
            kRestoreCursor,

            // Scroll region  (param[0]=top, param[1]=bottom, 1-indexed)
            kSetScrollRegion,

            // Alternate screen buffer (Step 3)
            kEnterAltScreen,               // ESC[?1049h
            kLeaveAltScreen,               // ESC[?1049l

            // Terminal responses (we send these back to the pty)
            kDeviceStatusReport,           // CSI 6 n — report cursor position
            kPrimaryDA,                    // CSI c   — primary device attributes

            // Full-screen app operations
            kReverseIndex,                 // ESC M   — scroll down / cursor up
            kInsertLine,                   // CSI L   — insert blank lines
            kDeleteLine,                   // CSI M   — delete lines
            kInsertChar,                   // CSI @   — insert blank chars
            kDeleteChar,                   // CSI P   — delete chars

            // Cursor key mode
            kCursorKeyModeApp,             // ESC[?1h — application cursor keys
            kCursorKeyModeNormal,          // ESC[?1l — normal cursor keys

            // Cursor visibility
            kCursorShow,                   // ESC[?25h
            kCursorHide,                   // ESC[?25l

            // Shell integration — OSC 133 semantic prompts (FinalTerm/iTerm2) + OSC 7 cwd.
            // These carry no grid effect; the controller maps them onto the command-block index.
            kPromptStart,                  // OSC 133;A  — prompt start
            kCommandStart,                 // OSC 133;B  — command start (end of prompt)
            kOutputStart,                  // OSC 133;C  — command output start
            kCommandEnd,                   // OSC 133;D[;<exit>] — command end; param[0]=exit (-1=unknown)
            kSetCwd,                       // OSC 7      — working dir; strParam = the file:// path
        };

        struct CMD {
            size_t idxString;
            kAnsiCmd cmd;
            std::vector<int> param;
            std::string strParam;          // string payload (OSC 7 path); empty for everything else
        };

    public:
        VTermParser() = default;
        virtual ~VTermParser() = default;

        std::string Parse(const uint8_t *ptrBuffer, const size_t size);

        __inline const std::vector<CMD> &LastCmdBuffer() const {
            return cmdBuffer;
        }

    protected:
        std::string ParseInternal();
        void ParseCSI();
        void ParseOSC();
        bool InRange(const std::pair<int,int> &range);

        std::string OSC_ParseStringToBel();
        std::string OSC_ReadPayloadToTerminator();
        void ParseOSC133();
        void EmitCmd(kAnsiCmd kCmd);
        void EmitCmd(kAnsiCmd kCmd, int param);
        void EmitCmd(kAnsiCmd kCmd, int p1, int p2);
        void EmitCmd(kAnsiCmd kCmd, const std::string &strParam);

        bool Next();
        uint8_t At();

    private:
        size_t idx = 0;
        size_t max = 0;
        const uint8_t *buffer = nullptr;
        std::string strParsed;
        std::vector<CMD> cmdBuffer;
    };
}

#endif //SHELL_VTERMPARSER_H
