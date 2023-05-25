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
        typedef enum {
            CommandState,
            SearchState,
        } State;
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
        const std::string &GetPrompt() const {
            return prompt;
        }
        const Cursor &GetCursor() const {
            return cursor;
        }
    protected:
        bool HandleActionInSearch(const KeyPressAction &kpAction);
        bool HandleActionInCommandState(const KeyPressAction &kpAction);
        bool ParseAndExecute();
        void DoLeaveOnSuccess();
        void SearchInActiveEditorModel(const std::string &searchItem);
        void NextSearchResult();
        void PrevSearchResult();
    private:
        void ChangeState(State newState);
    private:
        gnilk::ILogger *logger = nullptr;
        BaseController cmdInputBaseController;
        Cursor cursor = {};
        Line::Ref cmdInput = nullptr;
        std::string prompt = "C";
        State state = CommandState;
        //bool isSearchMode = false;

    };
}

#endif //EDITOR_QUICKCOMMANDCONTROLLER_H
