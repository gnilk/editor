//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_EDITCONTROLLER_H
#define EDITOR_EDITCONTROLLER_H

#include "Core/TextBuffer.h"
#include "BaseController.h"
#include "logger.h"
#include <memory>

namespace gedit {
    // FIXME: Reconsider this class, most stuff moved to EditorModel instead...
    class EditController : public BaseController {
    public:
        using Ref = std::shared_ptr<EditController>;
        using TextBufferChangedDelegate = std::function<void()>;

    public:
        EditController() = default;
        virtual ~EditController() = default;

        void Begin() override;
        void SetTextBuffer(TextBuffer::Ref newTextBuffer);

        const TextBuffer::Ref GetTextBuffer() {
            return textBuffer;
        }
        void SetTextBufferChangedHandler(TextBufferChangedDelegate newOnTextBufferChanged) {
            onTextBufferChanged = newOnTextBufferChanged;
        }
        bool HandleKeyPress(Cursor &cursor, size_t idxActiveLine, const KeyPress &keyPress) override;

        // Returns index to the new active line
        size_t NewLine(size_t idxCurrentLine, Cursor &cursor);

        // Proxy for buffer
        std::vector<Line::Ref> &Lines() {
            return textBuffer->Lines();
        }
        // Const accessor...
        const std::vector<Line::Ref> &Lines() const {
            return textBuffer->Lines();
        }
        Line::Ref LineAt(size_t idxLine) {
            return textBuffer->LineAt(idxLine);
        }

        void Paste(size_t idxActiveLine, const char *buffer);

        void UpdateSyntaxForBuffer();

    private:
        gnilk::ILogger *logger = nullptr;
        TextBuffer::Ref textBuffer = nullptr;
        TextBufferChangedDelegate onTextBufferChanged = nullptr;
    };
}


#endif //EDITOR_EDITCONTROLLER_H
