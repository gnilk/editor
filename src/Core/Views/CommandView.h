//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_COMMANDVIEW_H
#define EDITOR_COMMANDVIEW_H

#include "Core/Controllers/CommandController.h"
#include "ViewBase.h"
#include "logger.h"

namespace gedit {

    class CommandView : public ViewBase {
    public:
        CommandView() = default;
        explicit CommandView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        virtual ~CommandView() = default;

        virtual void DoDraw() override {
                ViewBase::DoDraw();
        }

        void InitView() override;
        void ReInitView() override;
        void OnKeyPress(const KeyPress &keyPress) override;
        void DrawViewContents() override;

        bool OnAction(const KeyPressAction &kpAction) override;
    protected:
        bool OnActionCommitLine();
    protected:
        // events - or sort of
        void OnActivate(bool isActive) override;
        void OnNewLineNotification();
    private:
        CommandController commandController;
        gnilk::ILogger *logger = nullptr;
        std::string prompt;
    };
}


#endif //EDITOR_COMMANDVIEW_H
