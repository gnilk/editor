//
// gvt tests — reading a Terminal back as text, and a host that records what it is told.
//
#ifndef GVT_TESTS_SCREENDUMP_H
#define GVT_TESTS_SCREENDUMP_H

#include <string>
#include <vector>

#include "gvt/Terminal.h"
#include "EventRecorder.h"

namespace gvttest {

    // UTF-8, trailing blanks trimmed. A wide character's continuation cell contributes nothing.
    inline std::string CellsText(std::span<const gnilk::vt::Cell> cells) {
        std::string out;
        for (auto &cell : cells) {
            if (cell.width == 0) {
                continue;
            }
            AppendUtf8(out, cell.codepoint);
        }
        while (!out.empty() && (out.back() == ' ')) {
            out.pop_back();
        }
        return out;
    }

    inline std::string RowText(const gnilk::vt::Terminal &terminal, int y) {
        std::vector<gnilk::vt::Cell> cells;
        for (int x = 0; x < terminal.Cols(); x++) {
            cells.push_back(terminal.At(x, y));
        }
        return CellsText(cells);
    }

    // All rows joined with '\n', trailing empty rows dropped - compact for assertions
    inline std::string ScreenText(const gnilk::vt::Terminal &terminal) {
        std::vector<std::string> lines;
        for (int y = 0; y < terminal.Rows(); y++) {
            lines.push_back(RowText(terminal, y));
        }
        while (!lines.empty() && lines.back().empty()) {
            lines.pop_back();
        }
        std::string out;
        for (size_t i = 0; i < lines.size(); i++) {
            if (i > 0) {
                out.push_back('\n');
            }
            out += lines[i];
        }
        return out;
    }

    class RecordingHost : public gnilk::vt::ITerminalHost {
    public:
        void WriteToPty(std::string_view bytes) override {
            replies += bytes;
        }
        void OnRowScrolledOut(std::span<const gnilk::vt::Cell> row, bool isWrapped) override {
            scrolledOut.push_back(CellsText(row));
            scrolledOutWrapped.push_back(isWrapped);
        }
        void OnBell() override {
            bells++;
        }
        void OnUnhandled(std::string_view description) override {
            unhandled.emplace_back(description);
        }

    public:
        std::string replies;
        std::vector<std::string> scrolledOut;
        std::vector<bool> scrolledOutWrapped;
        std::vector<std::string> unhandled;
        int bells = 0;
    };

}

#endif // GVT_TESTS_SCREENDUMP_H
