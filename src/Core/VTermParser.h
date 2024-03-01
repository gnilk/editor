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
            kSGRReset,
            kFontBold,
            kFontItalic,
            kFontUnderline,
            kFontNormal,
            kInvertColors,
            kSetForegroundColor,    // param is 0..7 for FG color index
            kSetBackgroundColor,    // param is 0..7 for BG color index
            kSetDefaultForegroundColor,
            kSetDefaultBackgroundColor,
        };
        struct CMD {
            size_t idxString;
            kAnsiCmd cmd;
            int param[8];  // need list?
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
