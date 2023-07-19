//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_EDITCONTROLLER_H
#define EDITOR_EDITCONTROLLER_H

#include "Core/TextBuffer.h"
#include "Core/UndoHistory.h"
#include "BaseController.h"
#include "logger.h"
#include <memory>
#include <deque>

namespace gedit {

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
        bool HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) override;
        bool HandleSpecialKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress);


        void Undo(Cursor &cursor);

        // Returns index to the new active line
        size_t NewLine(size_t idxCurrentLine, Cursor &cursor);
        void MoveLineUp(Cursor &cursor, size_t &idxActiveLine);

        // Proxy for buffer
        const std::vector<Line::Ref> &Lines() {
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

        UndoHistory::UndoItem::Ref BeginUndoItem();
        void EndUndoItem(UndoHistory::UndoItem::Ref undoItem);

        void UpdateSyntaxForBuffer();   // Does a full buffer reparse of the syntax
        void UpdateSyntaxForRegion(size_t idxStartLine, size_t idxEndLine); // Partial reparse (between line index)

        void AddCharToLineNoUndo(Cursor &cursor, Line::Ref line, int ch);
        void RemoveCharFromLineNoUndo(Cursor &cursor, Line::Ref line);

        void AddTab(Cursor &cursor, size_t idxActiveLine);
        void DelTab(Cursor &cursor, size_t idxActiveLine);

        void DeleteRange(const Point &startPos, const Point &endPos);
        void AddLineComment(size_t idxLineStart, size_t idxLineEnd, const std::string_view &lineCommentPrefix);
    protected:
        void DeleteLinesNoSyntaxUpdate(size_t idxLineStart, size_t idxLineEnd);
    private:
        gnilk::ILogger *logger = nullptr;
        TextBuffer::Ref textBuffer = nullptr;
        TextBufferChangedDelegate onTextBufferChanged = nullptr;
        UndoHistory historyBuffer;
    };
}


#endif //EDITOR_EDITCONTROLLER_H
