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

        const std::u32string &GetStatusBarAbbreviation() override {
            static std::u32string defaultAbbr = U"TRM";
            return defaultAbbr;
        }

    protected:
        void OnActivate(bool isActive) override;
        bool OnActionCommitLine();

    private:
        void DrawScreenRow(DrawContext &dc, const TerminalScreen::Row &row, int y);

    private:
        TerminalController controller;
        gnilk::ILogger *logger = nullptr;
        // Cached from config ("commandview.lines_per_scroll_wheel_notch") in InitView/ReInitView.
        int linesPerScrollWheelNotch = 3;
    };
}

#endif //GOATEDIT_TERMINALVIEW_H
