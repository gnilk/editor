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

            const std::vector<std::u32string> &GetData() {
                return data;
            }
            const Point &GetStart() {
                return start;
            }
            const Point &GetEnd() {
                return end;
            }

            // Splice the stored data into dstBuffer at ptWhere. Returns the buffer-coordinate Point
            // where the caret should land (end of the pasted text).
            Point PasteToBuffer(TextBuffer::Ref dstBuffer, const Point &ptWhere);

            // Number of lines the paste will OCCUPY (one per resolved segment). The number of NEW
            // lines added to the buffer is this minus one. Independent of the paste destination, so
            // it can be queried before pasting (e.g. to size an undo range).
            size_t GetPasteLineCount();

            // The item flattened to text for the OS clipboard: the resolved segments joined by '\n'.
            // A whole-line (linewise) copy carries a trailing empty segment, so its text ends in '\n'
            // (the trailing newline survives a round-trip back through CopyFromExternal); a charwise
            // copy has none. No extra trailing newline is appended. The single serialization point used
            // by every backend's clipboard OnUpdate hook.
            std::u32string AsText();

        protected:
            std::vector<std::u32string> ResolveSegments();
            void CopyFromExternal(const char *srcData);
            void CopyFromBuffer(TextBuffer::Ref srcBuffer);
            void Dump();

        protected:
            Point start = {};
            Point end = {};
            bool isExternal = false;
            std::vector<std::u32string> data;
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
        // Put literal text onto the clipboard as a new top item AND forward it to the OS pasteboard
        // (fires the OnUpdate hook). The app-internal way to copy arbitrary, non-buffer text - e.g. a
        // terminal command-block's output - so it can be pasted here or in another application. Unlike
        // CopyFromExternal (data arriving FROM the OS, which must NOT notify back), this is data going
        // TO the OS, so it notifies. Serialization stays in AsText() (E.18).
        bool CopyText(const std::u32string &text);
        // Splice the top clipboard item into dstBuffer at ptWhere; returns the caret end Point
        // (or ptWhere unchanged if the clipboard is empty).
        Point PasteToBuffer(TextBuffer::Ref dstBuffer, const Point &ptWhere);

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
