//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_EDITORMODE_H
#define EDITOR_EDITORMODE_H

#include "Core/ScreenBase.h"
#include "Core/ModeBase.h"

class EditorMode : public ModeBase {
        public:
        EditorMode();
        virtual ~EditorMode() = default;

        void Update(ScreenBase &screen) override;
        void DrawLines(ScreenBase &screen) override;

        // Navigate
        void OnNavigateUp(int rows);
        void OnNavigateDown(int rows);

        void SetBuffer(Buffer &newBuffer);

        const std::vector<Line *> &Lines() const override { return lines; }
        protected:
        void NewLine();
        private:
        int lastChar = 0;
        int idxActiveLine = 0;
        Line *currentLine = nullptr;
//    std::vector<Line *>::iterator it;
        Buffer lines;
};

#endif //EDITOR_EDITORMODE_H
