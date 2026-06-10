//
// Created by gnilk on 10.06.26.
//
// HexView — a read-only `offset | hex | ASCII` view of the active Document, the second representation
// in the HexView spike (docs/workspace-refactor-plan.md, H.3). It is a *projection* of the canonical
// ViewState: the caret stays in text coords (line + char-index) on the Document; HexView maps that to
// a byte offset to draw the highlight, and maps a hex-space navigation back into text coords (written
// through Document::SetCursorPosition) so switching back to text "just works". The byte stream comes
// from a ByteStreamReader (UTF-32 -> UTF-8 of the live buffer); HexProjection does the coord math.
//

#ifndef EDITOR_HEXVIEW_H
#define EDITOR_HEXVIEW_H

#include "Core/ByteStreamReader.h"
#include "Core/Document.h"
#include "Core/HexProjection.h"
#include "ViewBase.h"

#include "logger.h"

namespace gedit {

    class HexView : public ViewBase {
    public:
        // Classic hex-dump width. Fixed for v1; passed explicitly to ComputeNavTarget so the pure
        // navigation math stays testable independent of the constant.
        static constexpr int kBytesPerRow = 16;

        HexView() = default;
        explicit HexView(const Rect &viewArea) : ViewBase(viewArea) {
        }
        virtual ~HexView() = default;

        void InitView() override;
        void ReInitView() override;

        void SetDocument(Document::Ref newDocument);

        bool OnAction(const EditorAction &kpAction) override;

        const std::u32string &GetStatusBarAbbreviation() override {
            static std::u32string abbr = U"HEX";
            return abbr;
        }
        std::pair<std::u32string, std::u32string> GetStatusBarInfo() override;

        // Pure navigation: given the byte stream, the caret's current text position and a nav action,
        // return the caret's new text position. Horizontal moves step whole chars (so a one-byte step
        // never gets stuck mid-multibyte); vertical/page/home/end/buffer move in byte rows then snap to
        // a char boundary on the way back. Static + pure over the BinBuffer so it is unit-tested without
        // a live view.
        static Point ComputeNavTarget(const BinBuffer &utf8, const Point &cursorPos,
                                      kAction action, int bytesPerRow, int rowsPerPage);

    protected:
        void OnActivate(bool isActive) override;
        void DrawViewContents() override;

    private:
        bool DispatchNavAction(const EditorAction &kpAction);
        void RefocusHexView(size_t caretOffset);
        int RowsPerPage();
        size_t CaretByteOffset();
        const BinBuffer &Bytes();

    private:
        Document::Ref document;
        ByteStreamReader::Ref reader;
        int viewTopRow = 0;
        gnilk::ILogger *logger = nullptr;
    };
}

#endif //EDITOR_HEXVIEW_H
