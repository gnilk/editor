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

namespace gedit {
    // Extremely simplistic undo buffer - works for regular edits but wastes an extreme amount of memory..
    class UndoHistory {
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
        UndoHistory() = default;
        virtual ~UndoHistory() = default;
        UndoItem::Ref NewUndoItem();
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

}

#endif //EDITOR_UNDOHISTORY_H
