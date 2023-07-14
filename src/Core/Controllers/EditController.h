//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_EDITCONTROLLER_H
#define EDITOR_EDITCONTROLLER_H

#include "Core/TextBuffer.h"
#include "BaseController.h"
#include "logger.h"
#include <memory>
#include <deque>

namespace gedit {
    // Extremely simplistic undo buffer - works for regular edits but wastes an extreme amount of memory..
    class History {
    public:
        class UndoItem {
        public:
            using Ref = std::shared_ptr<UndoItem>;
        public:
            int idxLine = {};
            int offset = {};
            int action = {};
            std::string data = {};
        };
    public:
        History() = default;
        virtual ~History() = default;
        UndoItem::Ref NewUndoItem() {
            return std::make_shared<UndoItem>();
        }
        void PushUndoItem(UndoItem::Ref undoItem) {
            historystack.push_front(undoItem);
        }
        UndoItem::Ref PopItem() {
            auto top = historystack.front();
            historystack.pop_front();
            return top;
        }
        bool HaveHistory() {
            return !historystack.empty();
        }
        void Dump() {
            auto logger = gnilk::Logger::GetLogger("History");
            int count = 0;
            for(auto &undoItem : historystack) {
                logger->Debug("%d: line=%d, offset=%d, data=%s", count, undoItem->idxLine, undoItem->offset, undoItem->data.c_str());
                count ++;
            }
        }
    private:
        std::deque<UndoItem::Ref> historystack;
    };

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

        History::UndoItem::Ref BeginUndoItem(const Cursor &cursor, size_t idxActiveLine);
        void EndUndoItem(History::UndoItem::Ref undoItem);

        void UpdateSyntaxForBuffer();
        void AddCharToLineNoUndo(Cursor &cursor, Line::Ref line, int ch);
        void RemoveCharFromLineNoUndo(Cursor &cursor, Line::Ref line);

        void AddTab(Cursor &cursor, size_t idxActiveLine);
        void DelTab(Cursor &cursor, size_t idxActiveLine);

        void DeleteLines(Cursor &cursor, size_t idxLineStart, size_t idxLineEnd);
        void AddLineComment(Cursor &cursor, size_t idxLineStart, size_t idxLineEnd, const std::string_view &lineCommentPrefix);


    private:
        gnilk::ILogger *logger = nullptr;
        TextBuffer::Ref textBuffer = nullptr;
        TextBufferChangedDelegate onTextBufferChanged = nullptr;
        History historyBuffer;
    };
}


#endif //EDITOR_EDITCONTROLLER_H
