//
// Created by gnilk on 22.02.2024.
//

#ifndef GOATEDIT_TERMINALVIEW_H
#define GOATEDIT_TERMINALVIEW_H

#include <string>

#include "Core/Controllers/TerminalController.h"
#include "Core/RuntimeConfig.h"
#include "ViewBase.h"
#include "logger.h"

namespace gedit {
    class TerminalView : public ViewBase,  IOutputConsole {
    public:
        TerminalView() = default;
        explicit TerminalView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        virtual ~TerminalView() = default;

        void InitView() override;
        void ReInitView() override;
        void OnKeyPress(const KeyPress &keyPress) override;
        void DrawViewContents() override;

        bool OnAction(const KeyPressAction &kpAction) override;

        const std::u32string &GetStatusBarAbbreviation() override {
            static std::u32string defaultAbbr = U"TRM";
            return defaultAbbr;
        }

    public: // IOutputConsole
        void WriteLine(const std::u32string &str) override;

    protected:
        void OnActivate(bool isActive) override;
        bool OnActionCommitLine();
    private:
        TerminalController controller;
        gnilk::ILogger *logger = nullptr;
    };
}


#endif //GOATEDIT_TERMINALVIEW_H
