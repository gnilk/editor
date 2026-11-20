//
// A place written in text - 'a.cpp:3:16', 'src/main.rs:4', Python's 'File "x.py", line 5' - picked out at a column of
// a line: what Cmd/Ctrl+click in the terminal goes to (docs/build-messages.md §2, hitlist #52). Only the text is read
// here; whether the file is there is the caller's to find out.
//

#ifndef GEDIT_TEXTPLACE_H
#define GEDIT_TEXTPLACE_H

#include <cstdint>
#include <optional>
#include <string>

namespace gedit {
    class TextPlace {
    public:
        // The place the text at 'column' (from 0) is part of, nothing when there is none. 'line' and 'column' count from
        // 1, as the tools print them; 0 when the text has none ('a.cpp' alone).
        static std::optional<TextPlace> At(const std::u32string &text, size_t column);

    public:
        std::string path;
        uint32_t line = 0;
        uint32_t column = 0;

    private:
        static std::optional<TextPlace> PythonFrameAt(const std::u32string &text, size_t column);
        static std::optional<TextPlace> PathAt(const std::u32string &text, size_t column);
    };
}

#endif //GEDIT_TEXTPLACE_H
