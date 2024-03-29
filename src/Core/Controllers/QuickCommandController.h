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
            QuickCmdState,
            SearchState,
            CmdLetState,    // the cmd-let (plugin) prefix was entered and we
        } State;
    public:
        QuickCommandController() = default;
        virtual ~QuickCommandController() = default;

        void Enter();
        void Leave();

        bool HandleAction(const KeyPressAction &kpAction) override;
        void HandleKeyPress(const KeyPress &keyPress) override;

        const std::u32string_view GetCmdLine() const {
            return cmdInput->Buffer();
        }
        const std::u32string &GetPrompt() const {
            return prompt;
        }
        const Cursor &GetCursor() const {
            return cursor;
        }
    protected:
        bool HandleActionInQuickCmdState(const KeyPressAction &kpAction);
        bool HandleActionInSearch(const KeyPressAction &kpAction);
        bool HandleActionInCmdLetState(const KeyPressAction &kpAction);
        void DoLeaveOnSuccess();
        void SearchInActiveEditorModel(const std::u32string &searchItem);
        void NextSearchResult();
        void PrevSearchResult();
    private:
        void ChangeState(State newState);
    private:
        gnilk::ILogger *logger = nullptr;
        BaseController cmdInputBaseController;
        Cursor cursor = {};
        Line::Ref cmdInput = nullptr;
        std::vector<std::u32string> searchHistory;
        std::u32string prompt = U"C";
        State state = QuickCmdState;
    };
}

#endif //EDITOR_QUICKCOMMANDCONTROLLER_H
