//
// Created by gnilk on 15.05.23.
//

#ifndef EDITOR_QUICKCOMMANDCONTROLLER_H
#define EDITOR_QUICKCOMMANDCONTROLLER_H

#include "logger.h"
#include "Core/KeypressAndActionHandler.h"
namespace gedit {
    class QuickCommandController : public KeypressAndActionHandler {
    public:
        QuickCommandController() = default;
        virtual ~QuickCommandController() = default;

        void Enter();
        void Leave();

        bool HandleAction(const KeyPressAction &kpAction) override;

        void HandleKeyPress(const KeyPress &keyPress) override;
    private:
        gnilk::ILogger *logger = nullptr;
    };
}

#endif //EDITOR_QUICKCOMMANDCONTROLLER_H
