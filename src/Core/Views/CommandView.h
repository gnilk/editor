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

        void InitView() override;
        void OnKeyPress(const KeyPress &keyPress) override;
        void DrawViewContents() override;
    private:
        CommandController commandController;
        gnilk::ILogger *logger = nullptr;
    };
}


#endif //EDITOR_COMMANDVIEW_H
