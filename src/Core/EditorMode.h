//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_EDITORMODE_H
#define EDITOR_EDITORMODE_H

#include "Core/ScreenBase.h"
#include "Core/ModeBase.h"
#include "Core/KeyboardDriverBase.h"

struct Selection {

    bool IsActive() {
        return active;
    }
    void SetActive(bool bActive) {
        active = bActive;
    }

    void Begin(int startLine, int startColumn = 0) {
        active = true;

        idxStartLine = startLine;
        idxStartColumn = startColumn;

        idxEndLine = startLine;
        idxEndColumn = startColumn;
    }

    void Continue(int newEndLine, int newEndColumn = 0) {
        if (!active) {
            return;
        }
        idxEndLine = newEndLine;
        idxEndColumn = newEndColumn;
    }

    bool active = false;
    int idxStartLine = 0;
    int idxStartColumn = 0;
    int idxEndLine = 0;
    int idxEndColumn = 0;
};

class EditorMode : public ModeBase {
public:
        EditorMode();
        virtual ~EditorMode() = default;

        void Update() override;
        void DrawLines() override;

        // Navigate
        void OnNavigateUp(int rows);
        void OnNavigateDown(int rows);

        void SetBuffer(Buffer &newBuffer);

        const std::vector<Line *> &Lines() const override { return lines; }
protected:
        bool UpdateNavigation(KeyPress &keyPress, bool isShiftPressed);
        void ClearSelectedLines();
        void NewLine();
private:
        KeyPress lastChar {};
        int idxActiveLine = 0;
        Line *currentLine = nullptr;
//    std::vector<Line *>::iterator it;
        Buffer lines;

        // Selection stuff
        struct Selection selection;
        bool bClearSelectionNextUpdate = false;
};

#endif //EDITOR_EDITORMODE_H
