//
// OverviewView - see OverviewView.h
//
#include "OverviewView.h"

#include <algorithm>
#include <cmath>

#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/LineDiff.h"
#include "Core/Minimap.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

namespace {
    // A build that took this long waits this many times as long before the next - typing in a huge file doesn't
    // rebuild the whole picture at every key
    constexpr int kTextBuildBackoff = 4;
    // Between the marks strip and the text, in characters
    constexpr int kStripGap = 1;
    // The marks strip, in logical pixels: its width, and the least height of a mark - a changed line in a long file is
    // otherwise a single screen pixel
    constexpr int kStripWidth = 3;
    constexpr int kMinMarkHeight = 2;
}

OverviewView::OverviewView() : ViewBase() {
    SetWidth(Config::Instance()["minimap"].GetInt("width", 10));
}

void OverviewView::InitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("OverviewView");
}

void OverviewView::ReInitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
}

void OverviewView::DrawViewContents() {
    auto &dc = window->GetContentDC();
    auto theme = Editor::Instance().GetTheme();
    auto &contentColors = theme->GetContentColors();
    dc.SetColor(contentColors["foreground"], contentColors["background"]);
    dc.Clear();

    // As the gutter: nothing for the hex view, its rows aren't lines
    auto document = Editor::Instance().GetActiveDocument();
    if ((document == nullptr) || (document->GetViewMode() == DocumentViewMode::kHex)) {
        return;
    }
    if (!UpdateGeometry()) {
        return;
    }
    auto maxPixelsPerLine = Config::Instance()["minimap"].GetInt("max_pixels_per_line", 2);
    auto density = RuntimeConfig::Instance().GetScreen()->PixelDensity();
    map = OverviewMap(document->Lines().size(), imageHeight, (double)maxPixelsPerLine * density);

    UpdateText(document);
    ComposeFrame(document);
    dc.DrawImage(Rect(Point(0, 0), viewRect.Width(), viewRect.Height()), frameImage);
}

// The column's size in the screen's pixels; false when the screen draws cells only
bool OverviewView::UpdateGeometry() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    float cellWidth = 0;
    if (!screen->CellSizeInPixels(cellWidth, cellHeight) || (viewRect.Width() <= 0) || (viewRect.Height() <= 0)) {
        imageWidth = 0;
        imageHeight = 0;
        return false;
    }
    imageWidth = (int)std::lround((float)viewRect.Width() * cellWidth);
    imageHeight = (int)std::lround((float)viewRect.Height() * cellHeight);
    // A character a logical pixel wide
    pixelsPerChar = std::max(1, (int)std::lround(screen->PixelDensity()));
    stripWidth = kStripWidth * pixelsPerChar;
    return (imageWidth > 0) && (imageHeight > 0);
}

// The text again when the buffer changed (an edit, a parse, a recolour), the document or the size did - but not sooner
// after the last build than a few times what it cost
void OverviewView::UpdateText(const Document::Ref &document) {
    auto textLeft = stripWidth + kStripGap * pixelsPerChar;
    auto textWidth = std::max(imageWidth - textLeft, 0);
    auto buffer = document->GetTextBuffer();
    auto revision = buffer->Revision();
    auto numLines = document->Lines().size();
    bool isSameDocument = (textDocument == document.get());
    bool isSameSize = (textImage.Width() == textWidth) && (textImage.Height() == imageHeight);
    if (isSameDocument && isSameSize && (revision == textBufferRevision) && (numLines == textNumLines)) {
        return;
    }
    auto now = std::chrono::steady_clock::now();
    if (isSameDocument && isSameSize && ((now - timeTextBuilt) < (textBuildCost * kTextBuildBackoff))) {
        return;
    }

    auto theme = Editor::Instance().GetTheme();
    auto &contentColors = theme->GetContentColors();
    Minimap::Style style;
    style.pixelsPerChar = pixelsPerChar;
    style.tabSize = document->GetTabSize();
    style.background = contentColors["background"];
    style.foreground = contentColors["foreground"];
    Minimap::Render(textImage, textWidth, map, document->Lines(), style);

    textDocument = document.get();
    textBufferRevision = revision;
    textNumLines = numLines;
    timeTextBuilt = std::chrono::steady_clock::now();
    textBuildCost = timeTextBuilt - now;
}

// The text with the marks strip and the box on it - only when one of them changed, so the backend uploads it only then
void OverviewView::ComposeFrame(const Document::Ref &document) {
    auto &lineCursor = document->GetLineCursor();
    auto idxTopLine = (size_t)std::max(lineCursor.viewTopLine, (int32_t)0);
    auto numVisible = (size_t)std::max(lineCursor.Height(), (int32_t)1);
    auto box = map.BoxOf(idxTopLine, numVisible);

    FrameKey key;
    key.textRevision = textImage.Revision();
    key.boxTop = box.top;
    key.boxBottom = box.bottom;
    key.marks = MarksOf(document);
    if ((key == frameKey) && (frameImage.Width() == imageWidth) && (frameImage.Height() == imageHeight)) {
        return;
    }

    auto &contentColors = Editor::Instance().GetTheme()->GetContentColors();
    frameImage.Reset(imageWidth, imageHeight, contentColors["background"]);
    frameImage.CopyFrom(textImage, stripWidth + kStripGap * pixelsPerChar, 0);
    DrawMarks(key.marks);
    DrawBox(box);
    frameKey = std::move(key);
}

std::vector<uint8_t> OverviewView::MarksOf(const Document::Ref &document) const {
    std::vector<size_t> searchLines;
    for (auto &result : document->GetSearchResults()) {
        if (searchLines.empty() || (searchLines.back() != result.idxLine)) {
            searchLines.push_back(result.idxLine);
        }
    }
    auto changes = document->GetLineChanges();
    static const std::vector<uint8_t> noChanges;
    auto &lineChanges = (changes != nullptr) ? changes->marks : noChanges;
    auto marks = map.MarksPerRow(lineChanges, searchLines, document->GetLineCursor().idxActiveLine);
    return OverviewMap::Thicken(marks, kMinMarkHeight * pixelsPerChar);
}

// The gutter's colours for the changes (the tree's in a theme without them), the editor's for a search hit and the caret
void OverviewView::DrawMarks(const std::vector<uint8_t> &marks) {
    auto theme = Editor::Instance().GetTheme();
    auto &uiColors = theme->GetUIColors();
    auto &contentColors = theme->GetContentColors();
    auto uiColour = [&uiColors](const char *name) {
        auto gutterKey = std::string("gutter_") + name;
        return uiColors.HasColor(gutterKey) ? uiColors[gutterKey] : uiColors[std::string("tree_") + name];
    };
    ColorRGBA colours[OverviewMap::kActiveLine + 1];
    colours[OverviewMap::kAdded] = uiColour("added");
    colours[OverviewMap::kModified] = uiColour("modified");
    colours[OverviewMap::kRemoved] = uiColour("deleted");
    colours[OverviewMap::kSearch] = contentColors.HasColor("search") ? contentColors["search"] : contentColors["selection"];
    colours[OverviewMap::kActiveLine] = theme->GetGlobalColors()["caret"];
    for (auto &colour : colours) {
        colour.SetAlpha(1.0f);
    }
    for (size_t row = 0; row < marks.size(); row++) {
        if (marks[row] != OverviewMap::kNone) {
            frameImage.BlendRect(0, (int)row, stripWidth, 1, colours[marks[row]]);
        }
    }
}

void OverviewView::DrawBox(const OverviewMap::Box &box) {
    auto &uiColors = Editor::Instance().GetTheme()->GetUIColors();
    auto colour = uiColors.HasColor("minimap_viewport") ? uiColors["minimap_viewport"]
                                                        : ColorRGBA::FromRGBA(0.6f, 0.6f, 0.6f, 0.2f);
    frameImage.BlendRect(0, box.top, imageWidth, box.Height(), colour);
}

// A press anywhere on the minimap: the view goes there and the box follows the pointer until the release. The editor
// keeps the focus - a drag is taken before focus follows the click.
bool OverviewView::OnMouseDragStart(const MouseEvent &press) {
    if ((press.button != 1) || (imageHeight <= 0) || !viewRect.PointInRect(press.x, press.y)) {
        return false;
    }
    auto document = Editor::Instance().GetActiveDocument();
    if ((document == nullptr) || (document->GetViewMode() == DocumentViewMode::kHex)) {
        return false;
    }
    auto &lineCursor = document->GetLineCursor();
    auto box = map.BoxOf((size_t)std::max(lineCursor.viewTopLine, (int32_t)0),
                         (size_t)std::max(lineCursor.Height(), (int32_t)1));
    grabOffset = OverviewMap::GrabOffset(box, PointerRow(press));
    ScrollTo(press);
    return true;
}

void OverviewView::OnMouseDrag(const MouseEvent &mouseEvent) {
    if (mouseEvent.kind == MouseEvent::kMouseEventKind_Move) {
        ScrollTo(mouseEvent);
    }
}

// The pointer's row in the image's pixels - the cell and where in it
int OverviewView::PointerRow(const MouseEvent &mouseEvent) const {
    auto cellsDown = (float)(mouseEvent.y - viewRect.TopLeft().y) + mouseEvent.subCellY;
    return (int)std::floor(cellsDown * cellHeight);
}

void OverviewView::ScrollTo(const MouseEvent &mouseEvent) {
    auto document = Editor::Instance().GetActiveDocument();
    if (document == nullptr) {
        return;
    }
    auto numVisible = (size_t)std::max(document->GetLineCursor().Height(), (int32_t)1);
    document->ScrollViewTo(map.TopLineFor(PointerRow(mouseEvent), grabOffset, numVisible));
    RuntimeConfig::Instance().GetRootView().InvalidateAll();
}
