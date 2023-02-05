//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_EDITORMODE_H
#define EDITOR_EDITORMODE_H

#include "Core/Buffer.h"
#include "Core/ScreenBase.h"
#include "Core/ModeBase.h"
#include "Core/KeyboardDriverBase.h"
#include "logger.h"
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

        bool Begin() override;

        void Update() override;
        void DrawLines() override;

        const std::vector<Line *> &Lines() const override { return buffer->Lines(); };


    // Navigate
        void OnNavigateUp(int rows);
        void OnNavigateDown(int rows);

        void SetBuffer(Buffer *newBuffer);

protected:
        bool UpdateNavigation(KeyPress &keyPress, bool isShiftPressed);
        void ClearSelectedLines();
        void NewLine();
        void UpdateSyntaxForCurrentLine();
        void UpdateSyntaxForLine(Line *line);
            void UpdateSyntaxForBuffer();
private:
        gnilk::ILogger *logger;
        KeyPress lastChar {};
        int idxActiveLine = 0;
        Line *currentLine = nullptr;
        //Buffer lines;
        Buffer *buffer = nullptr;

        // Selection stuff
        struct Selection selection;
        bool bClearSelectionNextUpdate = false;
};

#endif //EDITOR_EDITORMODE_H
