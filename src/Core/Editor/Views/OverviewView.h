//
// OverviewView - the whole file down the editor's right edge (docs/usability-round.md §7): the minimap - the text drawn
// small, the change marks, the search hits and the active line in a strip at its left edge, and a translucent box
// over the part the editor shows. A click centres the view there, a drag moves it.
//
// Pixels: drawn only where the screen draws them (ScreenBase::CellSizeInPixels). The ruler, in cells, for the screens
// that don't is §7 step 3.
//
// Three layers, each rebuilt only when it changed: the text (Minimap::Render - on a buffer revision, a pause between
// builds that grows with what a build costs), the frame (the text, the marks and the box - on any of them changing)
// and the backend's texture (on the frame's revision).
//
#ifndef GEDIT_OVERVIEWVIEW_H
#define GEDIT_OVERVIEWVIEW_H

#include <chrono>
#include <cstdint>
#include <vector>

#include "Core/Document.h"
#include "Core/OverviewMap.h"
#include "Core/UI/Graphics/PixelImage.h"
#include "Core/UI/Views/ViewBase.h"

namespace gedit {

    class OverviewView : public ViewBase {
    public:
        OverviewView();
        virtual ~OverviewView() = default;

        void InitView() override;
        void ReInitView() override;
        void DrawViewContents() override;

        bool OnMouseDragStart(const MouseEvent &press) override;
        void OnMouseDrag(const MouseEvent &mouseEvent) override;
        bool IsSubCellDrag() const override {
            return true;
        }

    private:
        // What the frame was composed from - composed again when any of it differs
        struct FrameKey {
            uint64_t textRevision = 0;
            int boxTop = -1;
            int boxBottom = -1;
            std::vector<uint8_t> marks;

            bool operator==(const FrameKey &other) const = default;
        };

        bool UpdateGeometry();
        void UpdateText(const Document::Ref &document);
        void ComposeFrame(const Document::Ref &document);
        void DrawMarks(const std::vector<uint8_t> &marks);
        void DrawBox(const OverviewMap::Box &box);
        std::vector<uint8_t> MarksOf(const Document::Ref &document) const;
        int PointerRow(const MouseEvent &mouseEvent) const;
        void ScrollTo(const MouseEvent &mouseEvent);

    private:
        // The column in the screen's pixels
        int imageWidth = 0;
        int imageHeight = 0;
        int pixelsPerChar = 1;
        int stripWidth = 2;
        float cellHeight = 1.0f;
        OverviewMap map;

        // The text, as Minimap::Render left it, and what it was built from
        PixelImage textImage;
        const Document *textDocument = nullptr;
        uint64_t textBufferRevision = UINT64_MAX;
        size_t textNumLines = 0;
        std::chrono::steady_clock::time_point timeTextBuilt = {};
        std::chrono::steady_clock::duration textBuildCost = {};

        // What is drawn: the text with the marks and the box on it
        PixelImage frameImage;
        FrameKey frameKey;

        int grabOffset = 0;
    };

}

#endif //GEDIT_OVERVIEWVIEW_H
