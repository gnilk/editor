//
// Created by gnilk on 28.05.2026.
//
// The headless backend (`--backend headless`, what every trun case runs on). With the ANSI backend in the build it
// draws into a Gansi cell grid over a terminal IO that keeps nothing it is sent - so a test can read back what a view
// drew (Dump, CellAt) instead of the draw calls being dropped. Without it, the old screen whose draw calls do nothing.
// See docs/usability-round.md §5b.
//

#ifndef GOATEDIT_HLSCREEN_H
#define GOATEDIT_HLSCREEN_H

#include <memory>
#include <string>

#include "Core/RuntimeConfig.h"
#include "Core/UI/Graphics/ScreenBase.h"
#include "Core/UI/Graphics/WindowBase.h"
#include "Core/UI/Graphics/DrawContext.h"
#include "Core/UI/Graphics/KeyboardDriverBase.h"
#include "Core/UnicodeHelper.h"

#ifdef GEDIT_USE_ANSI
#include "Core/Graphics/Gansi/GansiScreen.h"
#include "gansi/ITerminalIO.h"
#include "gansi/Terminal.h"
#endif

namespace gedit {

    // The headless screen's size in cells
    static constexpr int kHeadlessCols = 100;
    static constexpr int kHeadlessRows = 100;

#ifdef GEDIT_USE_ANSI

    // A terminal of a fixed size with no input; what is written to it is dropped, so no ANSI piles up over a test run
    class HeadlessTerminalIO : public gnilk::ansi::ITerminalIO {
    public:
        HeadlessTerminalIO(int cols, int rows) : cols(cols), rows(rows) {
        }
        ~HeadlessTerminalIO() override = default;

        bool EnterRawMode() override {
            return true;
        }
        void RestoreMode() override {
        }
        bool GetSize(int &outCols, int &outRows) override {
            outCols = cols;
            outRows = rows;
            return true;
        }
        long Read(uint8_t *buf, size_t n, int timeoutMs) override {
            return 0;
        }
        void Write(const uint8_t *buf, size_t n) override {
        }
        bool TakePendingResize() override {
            return false;
        }
    private:
        int cols;
        int rows;
    };

    class HLScreen : public Gansi::GansiScreen {
    public:
        HLScreen() : GansiScreen(std::make_unique<gnilk::ansi::Terminal>(std::make_unique<HeadlessTerminalIO>(kHeadlessCols, kHeadlessRows))) {
        }
        ~HLScreen() override = default;

        static ScreenBase::Ref Create() {
            return std::make_shared<HLScreen>();
        }

        // The running headless screen, or nullptr when the screen is another backend
        static HLScreen *FromRuntime() {
            return dynamic_cast<HLScreen *>(RuntimeConfig::Instance().GetScreen().get());
        }

        // The characters of the whole grid, one line per row (UTF-8)
        std::string Dump() {
            return Dump(Dimensions());
        }

        // The characters within `rect` (screen cells), one line per row (UTF-8)
        std::string Dump(const Rect &rect) {
            auto *terminal = GetTerminal();
            std::u32string out;
            for (int y = rect.TopLeft().y; y < rect.BottomRight().y; y++) {
                for (int x = rect.TopLeft().x; x < rect.BottomRight().x; x++) {
                    out += terminal->At(x, y).ch;
                }
                out += U'\n';
            }
            return UnicodeHelper::utf32to8(out);
        }

        // A cell of the grid - the character, colours and attributes drawn there
        const gnilk::ansi::Cell &CellAt(int x, int y) {
            return GetTerminal()->At(x, y);
        }

        // Clear the grid between draws (a test drawing a view twice compares what the second draw left)
        void ClearGrid() {
            GetTerminal()->Clear();
        }
    };

#else

    class HLWindow : public WindowBase {
    public:
        HLWindow(const gedit::Rect &rect) : WindowBase(rect) {
            dummyDC = DrawContext(rect);
            drawContext = &dummyDC;
        }
        virtual ~HLWindow() = default;
    private:
        DrawContext dummyDC;
    };

    // Without the ANSI backend: every draw call is dropped
    class HLScreen : public ScreenBase {
    public:
        HLScreen() = default;
        ~HLScreen() = default;

        bool Open() override { return true; }

        static ScreenBase::Ref Create() {
            return std::make_shared<HLScreen>();
        }

        static HLScreen *FromRuntime() {
            return nullptr;
        }

        WindowBase *CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override {
            auto win = new HLWindow(rect);
            return win;
        }
        WindowBase *UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override {
            return window;
        }

        Rect Dimensions() override {
            return Rect(kHeadlessCols, kHeadlessRows);
        };

    };

#endif

    class HLKeyboardDriver : public KeyboardDriverBase {
    public:
        virtual ~HLKeyboardDriver() = default;
        virtual bool Initialize() { return true; };

        static KeyboardDriverBase::Ref Create() {
            return std::make_shared<HLKeyboardDriver>();
        }
    };

}

#endif //GOATEDIT_HLSCREEN_H
