//
// Created by gnilk on 19.07.23.
//

#ifndef EDITOR_CLIPBOARD_H
#define EDITOR_CLIPBOARD_H

#include <memory>
#include <string>
#include <vector>
#include <deque>

#include "Core/TextBuffer.h"
#include "Core/Point.h"

namespace gedit {
    // This almost follows the UndoHistory implementation
    class ClipBoard {
    public:

        class ClipBoardItem {
            friend ClipBoard;
        public:
            using Ref = std::shared_ptr<ClipBoardItem>;
        public:
            ClipBoardItem() = default;
            virtual ~ClipBoardItem() = default;

            static Ref Create(TextBuffer::Ref srcBuffer, const Point &ptStart, const Point &ptEnd);
            static Ref CreateExternal(const char *srcData);

            size_t GetLineCount() {
                return data.size();
            }

        protected:
            void CopyFromExternal(const char *srcData);
            void CopyFromBuffer(TextBuffer::Ref srcBuffer);
            void PasteToBuffer(TextBuffer::Ref dstBuffer, const Point &ptWhere);
            void Dump();

        protected:
            Point start = {};
            Point end = {};
            bool isExternal = false;
            std::vector<std::string> data;
        };
    public:
        ClipBoard() = default;
        virtual ~ClipBoard() = default;

        bool CopyFromExternal(const char *srcBuffer);
        bool CopyFromBuffer(TextBuffer::Ref srcBuffer, const Point &ptStart, const Point &ptEnd);
        void PasteToBuffer(TextBuffer::Ref dstBuffer, const Point &ptWhere);

        ClipBoardItem::Ref Top();

        size_t NumItems() {
            return history.size();
        };

        // Debug
        void Dump();
    protected:
        std::deque<ClipBoardItem::Ref> history;
    };
}


#endif //EDITOR_CLIPBOARD_H
