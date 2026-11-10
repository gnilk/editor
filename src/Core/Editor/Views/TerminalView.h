//
// Created by gnilk on 22.02.2024.
//

#ifndef GOATEDIT_TERMINALVIEW_H
#define GOATEDIT_TERMINALVIEW_H

#include <string>

#include "Core/Editor/Controllers/TerminalController.h"
#include "Core/RuntimeConfig.h"
#include "Core/TerminalScreen.h"
#include "Core/UI/Graphics/DrawContext.h"
#include "Core/UI/Views/ViewBase.h"
#include "logger.h"

namespace gedit {
    class TerminalView : public ViewBase {
    public:
        TerminalView() = default;
        explicit TerminalView(const Rect &viewArea) : ViewBase(viewArea) {
        }
        virtual ~TerminalView() = default;

        void InitView() override;
        void ReInitView() override;
        void OnKeyPress(const KeyPress &keyPress) override;
        void DrawViewContents() override;

        bool OnAction(const EditorAction &kpAction) override;
        bool OnMouseEvent(const MouseEvent &mouseEvent) override;
        // The I-beam: what is under the pointer is text
        std::optional<kPointerShape> PointerShapeAt(int x, int y) override {
            return viewRect.PointInRect(x, y) ? std::optional(kPointerShape::kText) : std::nullopt;
        }

        const std::u32string &GetStatusBarAbbreviation() override {
            static std::u32string defaultAbbr = U"TRM";
            return defaultAbbr;
        }

        // The embedded controller - exposed so the session orchestrator (Editor::SaveSession) can drive
        // scrollback persistence (terminal-scrollback.md §8) through it.
        TerminalController &GetController() { return controller; }

    protected:
        void OnActivate(bool isActive) override;
        bool OnActionCommitLine();

    private:
        void PasteFromClipboard();
        bool ForwardMouseToApp(const MouseEvent &mouseEvent);
        void DrawAppFrame(DrawContext &dc, const TerminalScreen &screen, const TerminalScreen::Frame &frame,
                          int viewHeight);
        void DrawAppScreen(DrawContext &dc, const TerminalScreen &screen, const TerminalScreen::Frame &frame,
                           int viewHeight);
        void DrawScreenRow(DrawContext &dc, const TerminalScreen::Row &row, int y);
        static Cursor::Shape ToCursorShape(gnilk::vt::CursorShape shape);

    private:
        TerminalController controller;
        gnilk::ILogger *logger = nullptr;
        // Cached from config ("terminal.lines_per_scroll_wheel_notch") in InitView/ReInitView.
        int linesPerScrollWheelNotch = 3;
        // Cached from config ("terminal.show_block_markers"): draw a separator rule at the end of
        // each closed command block (§5.5 of docs/terminal-scrollback.md). Off by default.
        bool showBlockMarkers = false;
        // Grid row drawn on view row 0 while an app owns the terminal (set by the draw, read by mouse reporting):
        // grid y = view y + this. 0 on the alternate screen; a foreground job is drawn bottom-anchored.
        int gridRowAtViewTop = 0;
    };
}

#endif //GOATEDIT_TERMINALVIEW_H
