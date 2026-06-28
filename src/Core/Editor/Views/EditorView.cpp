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

    document = Editor::Instance().GetActiveDocument();
    if (document == nullptr) {
        logger->Error("Document is null - no active textbuffer");
        return;
    }

    // Bind our controller to the active document and seed the view geometry from the document.
    editController.Attach(document);
    document->OnViewInit(rect);
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

    document = Editor::Instance().GetActiveDocument();
    if (document == nullptr) {
        logger->Error("Document is null - no active textbuffer");
        return;
    }
    // Re-point the controller at whatever document is now active (the document switch / attach).
    editController.Attach(document);
    document->OnViewInit(viewRect);
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

    int tabSize = document->GetTextBuffer()->GetLanguage().GetTabSize();
    DrawSearchResultOverlays(dc, tabSize);
    DrawSelectionOverlay(dc, tabSize);

    LineRender lineRender(dc, tabSize);
    lineRender.DrawLines(document->Lines(),
                         lineCursor.viewTopLine,
                         lineCursor.viewBottomLine,
                         document->GetSelection());
}

void EditorView::DrawSearchResultOverlays(DrawContext &dc, int tabSize) {
    if (document->searchResults.empty()) {
        return;
    }
    auto &lineCursor = document->GetLineCursor();
    // Search matches are NOT selections — paint them with the theme's 'search' color so they read
    // distinctly. Fall back to 'selection' for themes that don't define one.
    const auto &contentColors = Editor::Instance().GetTheme()->GetContentColors();
    const ColorRGBA searchColor = contentColors.HasColor("search")
                                      ? contentColors["search"]
                                      : contentColors["selection"];
    logger->Debug("DrawSearchResultOverlays, count=%zu", document->searchResults.size());
    for (auto &result : document->searchResults) {
        if (!lineCursor.IsInside(result.idxLine)) {
            continue;
        }
        int startVisualX = result.cursor_x;
        int endVisualX = result.cursor_x + result.length;
        if (auto line = document->LineAt(result.idxLine)) {
            startVisualX = line->CharToVisualColumn(result.cursor_x, tabSize);
            endVisualX = line->CharToVisualColumn(result.cursor_x + result.length, tabSize);
        }
        DrawContext::Overlay overlay;
        overlay.Set(Point(startVisualX, result.idxLine - lineCursor.viewTopLine),
                    Point(endVisualX,   result.idxLine - lineCursor.viewTopLine));
        overlay.color = searchColor;
        overlay.isActive = true;
        dc.AddOverlay(overlay);
    }
}

void EditorView::DrawSelectionOverlay(DrawContext &dc, int tabSize) {
    auto &selection = document->GetSelection();
    if (!selection.IsActive()) {
        return;
    }
    const ColorRGBA selectionColor = Editor::Instance().GetTheme()->GetContentColors()["selection"];
    auto &lineCursor = document->GetLineCursor();
    auto selStart = selection.GetStart();
    auto selEnd = selection.GetEnd();

    int startVisualX = selStart.x;
    if (auto line = document->LineAt(selStart.y)) {
        startVisualX = line->CharToVisualColumn(selStart.x, tabSize);
    }
    int endVisualX = selEnd.x;
    if (auto line = document->LineAt(selEnd.y)) {
        endVisualX = line->CharToVisualColumn(selEnd.x, tabSize);
    }

    DrawContext::Overlay overlay;
    overlay.Set(Point(startVisualX, selStart.y), Point(endVisualX, selEnd.y));
    overlay.color = selectionColor;
    overlay.attributes = 0;
    overlay.isActive = true;
    int dy = selEnd.y - selStart.y;
    overlay.start.y -= lineCursor.viewTopLine;
    overlay.end.y = overlay.start.y + dy;
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

    auto &lineCursor = document->GetLineCursor();
    size_t idxLine = lineCursor.viewTopLine + row;
    auto line = document->LineAt(idxLine);
    if (line == nullptr) {
        return true;
    }
    int tabSize = document->GetTextBuffer()->GetLanguage().GetTabSize();
    size_t idxChar = line->VisualToCharIndex(col, tabSize);
    document->SetCursorPosition(idxLine, idxChar);
    InvalidateView();
    return true;
}

void EditorView::SetWindowCursor(const Cursor &cursor) {
    if ((Editor::Instance().GetState() == Editor::ViewState) && (document != nullptr)) {
        // The document cursor's position.x is a character index. Tabs render wider than one cell, so
        // translate it to a visual column before drawing the caret - otherwise the caret sits left
        // of where the character actually renders and inserts appear misplaced. The document cursor is
        // left untouched (char index stays the source of truth for editing); only the draw copy moves.
        Cursor screenCursor = document->GetCursor();
        auto &lineCursor = document->GetLineCursor();
        auto activeLine = document->LineAt(lineCursor.idxActiveLine);
        if (activeLine != nullptr) {
            int tabSize = document->GetTextBuffer()->GetLanguage().GetTabSize();
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
