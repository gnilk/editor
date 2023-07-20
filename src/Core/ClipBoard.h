//
// Created by gnilk on 19.07.23.
//

#ifndef EDITOR_CLIPBOARD_H
#define EDITOR_CLIPBOARD_H

#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <functional>

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
            size_t GetByteSize();

            const std::vector<std::string> &GetData() {
                return data;
            }
            const Point &GetStart() {
                return start;
            }
            const Point &GetEnd() {
                return end;
            }

            void PasteToBuffer(TextBuffer::Ref dstBuffer, const Point &ptWhere);

        protected:
            void CopyFromExternal(const char *srcData);
            void CopyFromBuffer(TextBuffer::Ref srcBuffer);
            void Dump();

        protected:
            Point start = {};
            Point end = {};
            bool isExternal = false;
            std::vector<std::string> data;
        };
    public:
        using OnUpdateDelegate = std::function<void(ClipBoard::ClipBoardItem::Ref item)>;
    public:
        ClipBoard() = default;
        virtual ~ClipBoard() = default;

        // The OS layer should call this when it has new clipboard data allowing copy/paste from other applications
        bool CopyFromExternal(const char *srcBuffer);
        // This is the app-internal routines
        bool CopyFromBuffer(TextBuffer::Ref srcBuffer, const Point &ptStart, const Point &ptEnd);
        void PasteToBuffer(TextBuffer::Ref dstBuffer, const Point &ptWhere);

        // This should be set by the OS layer to forward internal clipboard data back to the OS
        // allowing copy/paste between the editor and other applications
        void SetOnUpdateCallback(OnUpdateDelegate newOnUpdateDelegate) {
            cbOnUpdate = newOnUpdateDelegate;
        }

        ClipBoardItem::Ref Top();

        size_t NumItems() {
            return history.size();
        };

        // Debug
        void Dump();
    protected:
        void NotifyChangeHandler(ClipBoardItem::Ref item);
    protected:
        OnUpdateDelegate cbOnUpdate = nullptr;
        std::deque<ClipBoardItem::Ref> history;

    };
}


#endif //EDITOR_CLIPBOARD_H
