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
            kSetForegroundColor,           // param[0]: 0..7 index
            kSetBackgroundColor,           // param[0]: 0..7 index
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
        };

        struct CMD {
            size_t idxString;
            kAnsiCmd cmd;
            std::vector<int> param;
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
        void EmitCmd(kAnsiCmd kCmd);
        void EmitCmd(kAnsiCmd kCmd, int param);
        void EmitCmd(kAnsiCmd kCmd, int p1, int p2);

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
