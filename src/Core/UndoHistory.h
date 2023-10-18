//
// Created by gnilk on 14.07.23.
//

#ifndef EDITOR_UNDOHISTORY_H
#define EDITOR_UNDOHISTORY_H

#include <memory>
#include <deque>
#include <string>
#include <utility>

#include "logger.h"
#include "TextBuffer.h"
namespace gedit {
    // Extremely simplistic undo buffer - works for regular edits but wastes an extreme amount of memory..
    class UndoHistory {
    public:
        enum class kRestoreAction {
            kInsertAsNew,               // When you have delete something...
            kClearAndAppend,            // Paste of regular editing or mult-line actions (like comment line), we just clear the contents and restore
            kDeleteBeforeInsert,        // When you undo a NewLine or a Paste action..
            kDeleteFirstBeforeInsert,   // Paste for when backspace removes one line up or delete-forward takes next line up and merge, we will delete the first line and replace and
        };
        // Base class for any item which can be un-done
        // holds common data
        class UndoItem {
            friend UndoHistory;
        public:
            using Ref = std::shared_ptr<UndoItem>;
        public:
            UndoItem() = default;
            virtual ~UndoItem() = default;
            virtual int32_t Restore(TextBuffer::Ref textBuffer) { return 0; }
            bool IsValid() { return isValid; }
            void SetRestoreAction(kRestoreAction newRestoreAction) {
                action = newRestoreAction;
            }
        protected:
            virtual void Initialize();
        protected:
            bool isValid = false;
            LineCursor lineCursor;
//            int idxLine = {};
//            int offset = {};
            // The action tell's us if we should replace lines or insert lines..
            // This depends if the undo item was created during a delete operation or a modify operation
            kRestoreAction action = kRestoreAction::kInsertAsNew;
        };

        // Specialization for a single item (line) which can be un-done, this one holds the actual data
        class UndoItemSingle : public UndoItem {
            friend UndoHistory;
        public:
            using Ref = std::shared_ptr<UndoItemSingle>;
        public:
            UndoItemSingle() = default;
            virtual ~UndoItemSingle() = default;
            static UndoItemSingle::Ref Create();
            int32_t Restore(TextBuffer::Ref buffer) override;
        protected:
            void Initialize() override;
        private:
            std::u32string data = {};
        };

        class UndoItemRange : public UndoItem {
            friend UndoHistory;
        public:
            using Ref = std::shared_ptr<UndoItemRange>;
        public:
            UndoItemRange() = default;
            virtual ~UndoItemRange() = default;

            static UndoItemRange::Ref Create();
            int32_t Restore(TextBuffer::Ref textBuffer) override;
         protected:
            void InitRange(const Point &ptStart, const Point &ptEnd);
        private:
            Point start = {};
            Point end = {};
            std::vector<std::u32string> data;
        };

    public:
        UndoHistory() = default;
        virtual ~UndoHistory() = default;
        UndoItem::Ref NewUndoItem();
        UndoItem::Ref NewUndoFromSelection();
        UndoItem::Ref NewUndoFromLineRange(size_t idxStartLine, size_t idxEndLine);
        void PushUndoItem(UndoItem::Ref undoItem) {
            // Nopes, we don't allow this...
            if (undoItem == nullptr) {
                return;
            }
            historystack.push_front(undoItem);
        }
        UndoItem::Ref PopItem() {
            auto top = historystack.front();
            historystack.pop_front();
            return top;
        }
        // Returns number of lines that was restored
        int32_t RestoreOneItem(Cursor &cursor, size_t &idxActiveLine, TextBuffer::Ref textBuffer);

        bool HaveHistory() {
            return !historystack.empty();
        }
        void Dump() {
            auto logger = gnilk::Logger::GetLogger("History");
            int count = 0;
            for(auto &undoItem : historystack) {
                logger->Debug("%d: cursor=(%d:%d), idxActiveLine=%d", count, undoItem->lineCursor.cursor.position.x, undoItem->lineCursor.cursor.position.y, undoItem->lineCursor.idxActiveLine);
                count ++;
            }
        }
    private:
        std::deque<UndoItem::Ref> historystack;
    };

}

#endif //EDITOR_UNDOHISTORY_H
