//
// Created by gnilk on 14.02.23.
//

#include "Core/Editor/Controllers/EditController.h"
#include "Core/RuntimeConfig.h"
#include "Core/Document.h"
#include "GutterView.h"
#include "logger.h"
#include "Core/Editor.h"
#include "Core/LineDiff.h"
using namespace gedit;

GutterView::GutterView() : ViewBase() {
    // Width is important...
    //layout.SetNewRect(Rect(6,0));
    SetWidth(10);
    //SetViewAnchoring(ViewLayout::kViewAnchor_FixedWidth);
}
GutterView::GutterView(const Rect &viewArea) : ViewBase(viewArea) {
        //SetViewAnchoring(ViewLayout::kViewAnchor_FixedWidth);
}
void GutterView::InitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("GutterView");


}
void GutterView::ReInitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
}



void GutterView::DrawViewContents() {

    // Can't draw if no parent.. (as it will hold the shared data
    if (GetParentView() == nullptr) {
        return;
    }
    auto document = Editor::Instance().GetActiveDocument();
    if (document == nullptr) {
        return;
    }

//    auto logger = gnilk::Logger::GetLogger("GutterView");
//    logger->Debug("Active Line: %d", document->idxActiveLine);

    auto &dc = window->GetContentDC();

    auto theme = Editor::Instance().GetTheme();
    auto &uiColors = theme->GetUIColors();
    dc.SetColor(uiColors["gutter_foreground"], uiColors["gutter_background"]);

    dc.Clear();

    // The gutter is text-editor chrome: line numbers + the active-line '*', paged by lineCursor.viewTopLine.
    // In hex mode the rows are 16-byte rows (the hexdump carries its own offset column) and the caret moves
    // in byte space, so a text-line gutter can neither align with the hex caret row nor track its scroll —
    // leave it blank rather than drawing a stray, mis-placed '*'.
    if (document->GetViewMode() == DocumentViewMode::kHex) {
        return;
    }

    // Number the same rows the editor draws (DocumentViewState::ProjectVisibleRows): a line's number goes
    // on its first row only; the continuation rows of a soft-wrapped line stay blank.
    char str[64];
    auto &lineCursor = document->GetLineCursor();
    auto rows = document->GetVisibleRows();
    auto changes = document->GetLineChanges();
    for (size_t idxRow = 0; idxRow < rows.size(); idxRow++) {
        auto &row = rows[idxRow];
        if (row.idxSegment != 0) {
            continue;
        }
        snprintf(str, 64, "%6zu", row.idxLine);
        if (row.idxLine == lineCursor.idxActiveLine) {
            snprintf(str, 64, "%6zu  *", row.idxLine);
        }
        dc.DrawStringAt(0, static_cast<int>(idxRow), str);
        if (changes != nullptr) {
            DrawChangeMark(dc, static_cast<int>(idxRow), changes->MarkOf(row.idxLine));
        }
    }
}

// The change against the last commit (docs/usability-round.md §0c), in the free column between the number and the
// active line's '*': a bar for a line added or changed, '‾' where lines were removed above, '_' below the last line.
// A line's own change wins over a removal next to it.
void GutterView::DrawChangeMark(DrawContext &dc, int row, uint8_t mark) {
    const char32_t *glyph = nullptr;
    const char *colourKey = nullptr;
    if (mark & LineDiff::kAdded) {
        glyph = U"\u2502";
        colourKey = "added";
    } else if (mark & LineDiff::kModified) {
        glyph = U"\u2502";
        colourKey = "modified";
    } else if (mark & LineDiff::kRemovedAbove) {
        glyph = U"\u203e";
        colourKey = "deleted";
    } else if (mark & LineDiff::kRemovedBelow) {
        glyph = U"_";
        colourKey = "deleted";
    }
    if (glyph == nullptr) {
        return;
    }
    auto &uiColors = Editor::Instance().GetTheme()->GetUIColors();
    // A theme without the gutter's keys uses the tree's; without those, the mark in the gutter's own colour
    auto gutterKey = std::string("gutter_") + colourKey;
    auto treeKey = std::string("tree_") + colourKey;
    auto &key = uiColors.HasColor(gutterKey) ? gutterKey : treeKey;
    if (uiColors.HasColor(key)) {
        dc.SetColor(uiColors[key], uiColors["gutter_background"]);
    }
    dc.DrawStringAt(CHANGE_MARK_COLUMN, row, std::u32string(glyph));
    dc.SetColor(uiColors["gutter_foreground"], uiColors["gutter_background"]);
}
