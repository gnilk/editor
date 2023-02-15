//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_EDITCONTROLLER_H
#define EDITOR_EDITCONTROLLER_H

#include "Core/TextBuffer.h"
#include "BaseController.h"
#include "logger.h"

namespace gedit {
    class EditController : public BaseController {
    public:
        EditController() = default;
        virtual ~EditController() = default;

        void Begin() override;
        void SetTextBuffer(TextBuffer *newTextBuffer);
        bool HandleKeyPress(size_t idxActiveLine, const gedit::NCursesKeyboardDriverNew::KeyPress &keyPress) override;

        // Returns index to the new active line
        size_t NewLine(size_t idxCurrentLine, Cursor &cursor);

        // Proxy for buffer
        std::vector<Line *> &Lines() {
            return textBuffer->Lines();
        }
        Line *LineAt(size_t idxLine) {
            return textBuffer->LineAt(idxLine);
        }

    protected:
        // This can probably be moved to the view
        bool UpdateNavigation(gedit::NCursesKeyboardDriverNew::KeyPress &keyPress);
        void UpdateSyntaxForBuffer();

    private:
        gnilk::ILogger *logger = nullptr;
        TextBuffer *textBuffer = nullptr;
    };
}


#endif //EDITOR_EDITCONTROLLER_H
