//
// Created by gnilk on 14.02.23.
//
#include <unordered_map>

#include "Core/Action.h"
#include "Core/Config/Config.h"
#include "Core/UI/Graphics/DrawContext.h"
#include "Core/Document.h"
#include "Core/KeyMapping.h"
#include "Core/RuntimeConfig.h"
#include "Core/Editor/LineRender.h"
#include "EditorView.h"
#include "Core/Editor.h"
#include "Core/ActionHelper.h"
#include "fmt/chrono.h"


#include "fmt/xchar.h"

using namespace gedit;

// This is the global section in the config.yml for this view
static const std::string cfgSectionName = "editorview";

// "editorview.wrap": none | char | word - anything else is no wrap.
static WrapMode WrapModeFromString(const std::string &str) {
    if (str == "char") {
        return WrapMode::kChar;
    }
    if (str == "word") {
        return WrapMode::kWord;
    }
    return WrapMode::kNone;
}

void EditorView::InitView()  {
    logger = gnilk::Logger::GetLogger("EditorView");
    logger->Debug("InitView!");

    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("EditorView");

    auto &rect = window->GetContentDC().GetRect();

    bUseCLionPageNav = Config::Instance()[cfgSectionName].GetBool("pgupdown_content_first", true);
    linesPerScrollWheelNotch = Config::Instance()[cfgSectionName].GetInt("lines_per_scroll_wheel_notch", 5);
    wrapMode = WrapModeFromString(Config::Instance()[cfgSectionName].GetStr("wrap", "none"));

    // Bind our controller to the active document - null detaches it, so key presses are no-ops when no
    // buffer is open (it holds a raw pointer, it must never keep a closed document).
    document = Editor::Instance().GetActiveDocument();
    editController.Attach(document);
    if (document == nullptr) {
        logger->Error("Document is null - no active textbuffer");
        return;
    }

    // Seed the view geometry from the document.
    document->OnViewInit(rect);
    document->SetWrap(wrapMode, rect.Width());
}

void EditorView::ReInitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    auto &rect = window->GetContentDC().GetRect();

    bUseCLionPageNav = Config::Instance()[cfgSectionName].GetBool("pgupdown_content_first", true);
    linesPerScrollWheelNotch = Config::Instance()[cfgSectionName].GetInt("lines_per_scroll_wheel_notch", 5);
    wrapMode = WrapModeFromString(Config::Instance()[cfgSectionName].GetStr("wrap", "none"));

    // Re-point the controller at whatever document is now active (the document switch / attach). Null
    // after the last buffer was closed - that detaches it; the controller must never keep a closed document.
    document = Editor::Instance().GetActiveDocument();
    editController.Attach(document);
    if (document == nullptr) {
        logger->Error("Document is null - no active textbuffer");
        return;
    }
    document->OnViewInit(viewRect);
    // Every resize and document switch funnels through here - re-push the wrap width (content width).
    document->SetWrap(wrapMode, rect.Width());
}

// FIXME: This is never used!!!
void EditorView::OnResized() {
    // Update the view Bottom line - as this affects how many lines we draw...
    auto &lineCursor = document->GetLineCursor();
    lineCursor.viewBottomLine = GetContentRect().Height();

    document->OnViewInit(GetContentRect());
    ViewBase::OnResized();
}

void EditorView::DrawViewContents() {
    if (document == nullptr) {
        return;
    }

    auto &dc = window->GetContentDC();
    auto &lineCursor = document->GetLineCursor();

    logger->Debug("DrawViewContents, dc Height=%d, topLine=%d, bottomLine=%d",
                  dc.GetRect().Height(), lineCursor.viewTopLine, lineCursor.viewBottomLine);

    dc.ResetDrawColors();
    dc.ClearOverlays();

    // One row projection per frame: the text, the overlays (and the caret, in SetWindowCursor) all map text
    // positions through the same rows. Tab size must be the one the rows were projected with.
    int tabSize = document->GetTabSize();
    auto rows = document->GetVisibleRows();
    DrawSearchResultOverlays(dc, rows, tabSize);
    DrawSelectionOverlay(dc, rows, tabSize);

    LineRender lineRender(dc, tabSize);
    lineRender.DrawRows(document->Lines(), rows);
}

void EditorView::DrawSearchResultOverlays(DrawContext &dc, const std::vector<WrapRow> &rows, int tabSize) {
    if (document->searchResults.empty()) {
        return;
    }
    // Search matches are NOT selections — paint them with the theme's 'search' color so they read
    // distinctly. Fall back to 'selection' for themes that don't define one.
    const auto &contentColors = Editor::Instance().GetTheme()->GetContentColors();
    const ColorRGBA searchColor = contentColors.HasColor("search")
                                      ? contentColors["search"]
                                      : contentColors["selection"];
    logger->Debug("DrawSearchResultOverlays, count=%zu", document->searchResults.size());
    auto &lines = document->Lines();
    for (auto &result : document->searchResults) {
        // Overlay ends are screen (row, visual column); a match on a wrapped line may span rows.
        Point textStart(static_cast<int>(result.cursor_x), static_cast<int>(result.idxLine));
        Point textEnd(static_cast<int>(result.cursor_x + result.length), static_cast<int>(result.idxLine));
        auto start = WrapProjection::TextToScreenClamped(rows, lines, textStart, tabSize);
        auto end = WrapProjection::TextToScreenClamped(rows, lines, textEnd, tabSize);
        if ((end.y < 0) || (start.y >= static_cast<int>(rows.size()))) {
            continue;   // wholly above or below the view
        }
        DrawContext::Overlay overlay;
        overlay.Set(start, end);
        overlay.color = searchColor;
        overlay.isActive = true;
        dc.AddOverlay(overlay);
    }
}

void EditorView::DrawSelectionOverlay(DrawContext &dc, const std::vector<WrapRow> &rows, int tabSize) {
    auto &selection = document->GetSelection();
    if (!selection.IsActive()) {
        return;
    }
    const ColorRGBA selectionColor = Editor::Instance().GetTheme()->GetContentColors()["selection"];
    // Selection is in text coordinates (x=charIdx, y=line); a part running out of the view clamps to the row
    // just outside it, so every visible row it spans still paints.
    auto &lines = document->Lines();
    auto start = WrapProjection::TextToScreenClamped(rows, lines, selection.GetStart(), tabSize);
    auto end = WrapProjection::TextToScreenClamped(rows, lines, selection.GetEnd(), tabSize);
    if ((end.y < 0) || (start.y >= static_cast<int>(rows.size()))) {
        return;
    }

    DrawContext::Overlay overlay;
    overlay.Set(start, end);
    overlay.color = selectionColor;
    overlay.attributes = 0;
    overlay.isActive = true;
    dc.AddOverlay(overlay);
}

void EditorView::OnActivate(bool isActive) {
    logger->Debug("OnActive, isActive: %s", isActive?"yes":"no");
    if (isActive) {
        Editor::Instance().SetActiveKeyMapping(Config::Instance()[cfgSectionName].GetStr("keymap", "default_keymap"));
        // Maximize editor content view...
        //MaximizeContentHeight();
    }
}

void EditorView::OnKeyPress(const KeyPress &keyPress) {
    if (editController.OnKeyPress(keyPress)) {
        InvalidateView();
        return;
    }

    // It was not to us..
    ViewBase::OnKeyPress(keyPress);
}


//
// Add actions here - all except human-readable inserting of text
//
bool EditorView::OnAction(const EditorAction &kpAction) {
    // Resolved actions go straight to the document (fully possible to have none, if no file is open).
    if (document == nullptr) {
        return false;
    }
    if (document->OnAction(kpAction)) {
        return true;
    }

    return ViewBase::OnAction(kpAction);
}


bool EditorView::OnMouseEvent(const MouseEvent &mouseEvent) {
    if (document == nullptr) {
        return false;
    }

    switch (mouseEvent.kind) {
        case MouseEvent::kMouseEventKind_Press:
            return OnMousePressedEvent(mouseEvent);
        case MouseEvent::kMouseEventKind_Wheel:
            document->MoveCursorByLines(-mouseEvent.wheelDelta * linesPerScrollWheelNotch);
            InvalidateView();
            return true;
        default:
            return false;
    }
}

bool EditorView::OnMousePressedEvent(const MouseEvent &mouseEvent) {
    auto contentOrigin = GetContentRect().TopLeft();
    int row = mouseEvent.y - contentOrigin.y;
    int col = mouseEvent.x - contentOrigin.x;

    auto rows = document->GetVisibleRows();
    if ((row < 0) || (row >= static_cast<int>(rows.size()))) {
        return true;    // below the last line - nothing there
    }
    auto textPos = WrapProjection::ScreenToText(rows, document->Lines(), Point(col, row), document->GetTabSize());
    document->SetCursorPosition(textPos.y, textPos.x);
    InvalidateView();
    return true;
}

void EditorView::SetWindowCursor(const Cursor &cursor) {
    if ((Editor::Instance().GetState() == Editor::ViewState) && (document != nullptr)) {
        // The document cursor's position.x is a character index. Tabs render wider than one cell, so
        // translate it to a visual column before drawing the caret - otherwise the caret sits left
        // of where the character actually renders and inserts appear misplaced. The document cursor is
        // left untouched (char index stays the source of truth for editing); only the draw copy moves.
        // With soft-wrap the screen row is derived the same way: the caret's (line, char) is mapped through the
        // visible rows, so it lands on the wrapped row actually holding that char.
        Cursor screenCursor = document->GetCursor();
        auto &lineCursor = document->GetLineCursor();
        int tabSize = document->GetTabSize();
        Point textPos(screenCursor.position.x, static_cast<int>(lineCursor.idxActiveLine));
        Point screenPos;
        if (WrapProjection::TextToScreen(document->GetVisibleRows(), document->Lines(), textPos, tabSize, screenPos)) {
            screenCursor.position = screenPos;
        } else if (auto activeLine = document->LineAt(lineCursor.idxActiveLine)) {
            // Active line not on a visible row - keep the model's screen row (pre-wrap behaviour).
            screenCursor.position.x = activeLine->CharToVisualColumn(screenCursor.position.x, tabSize);
        }
        window->SetCursor(screenCursor);
    } else {
        // The editor view is NOT in 'command' but rather the 'owner' of the quick-cmd input view..
        ViewBase::SetWindowCursor(cursor);
    }
}

// returns the center and right side information for the status bar
std::pair<std::u32string, std::u32string> EditorView::GetStatusBarInfo() {
    std::u32string statusCenter = U"";
    std::u32string statusRight = U"";
    // If we have a document - draw details...
    auto node = Editor::Instance().GetWorkspaceNodeForActiveDocument();
    if (node == nullptr) {
        return {statusCenter, statusRight};
    }
    // Hmm - why can't I use the document-> here????
    auto document = node->GetDocument();
    if (document == nullptr) {
        return {statusCenter, statusRight};
    }

    // Resolve center information
    if (document->GetTextBuffer()->GetBufferState() == TextBuffer::kBuffer_Changed) {
        statusCenter += U"* ";
    }
    if (document->GetTextBuffer()->IsReadOnly()) {
        statusCenter += U"R/O ";
    }
    if (node->IsMissingOnDisk()) {
        // Gone from disk (kept open) - cleared by the save that re-creates it
        statusCenter += U"[DELETED] ";
    }

    statusCenter += node->GetDisplayNameU32();
    statusCenter += U" | ";
    if (!document->GetTextBuffer()->CanEdit()) {
        statusCenter += U"[locked] | ";
    }
    statusCenter += document->GetTextBuffer()->HaveLanguage() ? document->GetTextBuffer()->GetLanguage().Identifier() : U"none";

    // resolve right status
    auto &lineCursor = document->GetLineCursor();

    int indent = -1;
    auto line = document->LineAt(lineCursor.idxActiveLine);
    if (line != nullptr) {
        indent = line->GetIndent();
    }

    // Show Line:Row or more 'x/y' -> Configureation!
    // Column is the visual (tab-expanded) column, matching what the caret shows.
    int visualCol = (line != nullptr)
        ? line->CharToVisualColumn(lineCursor.cursor.position.x, document->GetTabSize())
        : lineCursor.cursor.position.x;
    auto strtmp = fmt::format(U"id: {} l: {}, c({}:{})",
                              indent,
                              strutil::itou32(lineCursor.idxActiveLine),
                              strutil::itou32(lineCursor.cursor.position.y),
                              strutil::itou32(visualCol));

    statusRight += strtmp;

    return {statusCenter, statusRight};
}
