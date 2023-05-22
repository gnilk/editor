//
// Created by gnilk on 15.05.23.
//

#ifndef EDITOR_QUICKCOMMANDCONTROLLER_H
#define EDITOR_QUICKCOMMANDCONTROLLER_H

#include "logger.h"
#include "Core/KeypressAndActionHandler.h"
#include "BaseController.h"
#include "Core/Line.h"
namespace gedit {
    class QuickCommandController : public KeypressAndActionHandler {
    public:
        QuickCommandController() = default;
        virtual ~QuickCommandController() = default;

        void Enter();
        void Leave();

        bool HandleAction(const KeyPressAction &kpAction) override;
        void HandleKeyPress(const KeyPress &keyPress) override;

        const std::string_view GetCmdLine() const {
            return cmdInput->Buffer();
        }
    protected:
        bool ParseAndExecute();
        void DoLeaveOnSuccess();
        void SearchInActiveEditorModel(const std::string &searchItem);
        void NextSearchResult();
        void PrevSearchResult();
    private:
        gnilk::ILogger *logger = nullptr;
        BaseController cmdInputBaseController;
        Cursor cursor = {};
        Line::Ref cmdInput = nullptr;
        size_t idxHit = 0;
        bool isSearchMode = false;

    };
}

#endif //EDITOR_QUICKCOMMANDCONTROLLER_H
