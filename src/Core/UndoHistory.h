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
            kInsertAsNew,
            kClearAndAppend,
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
            virtual void Restore(TextBuffer::Ref textBuffer) {}
            bool IsValid() { return isValid; }
            void SetRestoreAction(kRestoreAction newRestoreAction) {
                action = newRestoreAction;
            }
        protected:
            virtual void Initialize();
        protected:
            bool isValid = false;
            int idxLine = {};
            int offset = {};
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
            void Restore(TextBuffer::Ref buffer) override;
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
            void Restore(TextBuffer::Ref textBuffer) override;
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
            historystack.push_front(undoItem);
        }
        UndoItem::Ref PopItem() {
            auto top = historystack.front();
            historystack.pop_front();
            return top;
        }
        void RestoreOneItem(Cursor &cursor, TextBuffer::Ref textBuffer);

        bool HaveHistory() {
            return !historystack.empty();
        }
        void Dump() {
            auto logger = gnilk::Logger::GetLogger("History");
            int count = 0;
            for(auto &undoItem : historystack) {
                logger->Debug("%d: line=%d, offset=%d", count, undoItem->idxLine, undoItem->offset);
                count ++;
            }
        }
    private:
        std::deque<UndoItem::Ref> historystack;
    };

}

#endif //EDITOR_UNDOHISTORY_H
