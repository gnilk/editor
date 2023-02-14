//
// Created by gnilk on 14.02.23.
//

#ifndef EDITOR_EDITORVIEW_H
#define EDITOR_EDITORVIEW_H

#include "Core/ViewBase.h"
#include "Core/EditorMode.h"

namespace gedit {
    class EditorView : public ViewBase {
    public:
        EditorView() = default;
        explicit EditorView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        virtual ~EditorView() = default;

        void Begin() override;
        void OnKeyPress(gedit::NCursesKeyboardDriverNew::KeyPress keyPress) override;
        void DrawViewContents() override;
    private:
        void DrawLines();

    private:
        EditorMode editorMode;
    };
}


#endif //EDITOR_EDITORVIEW_H
