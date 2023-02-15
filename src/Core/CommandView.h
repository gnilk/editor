//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_COMMANDVIEW_H
#define EDITOR_COMMANDVIEW_H

#include "Core/ViewBase.h"

namespace gedit {

    class CommandView : public ViewBase {
    public:
        CommandView() = default;
        explicit CommandView(const Rect &viewArea) : ViewBase(viewArea) {

        }
        virtual ~CommandView() = default;
        void Begin() override;
        void OnKeyPress(const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) override;
        void DrawViewContents() override;
    private:

    };
}


#endif //EDITOR_COMMANDVIEW_H
