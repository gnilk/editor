//
// Created by gnilk on 14.02.23.
//
#include <unordered_map>

#include "Core/Action.h"
#include "Core/Config/Config.h"
#include "Core/Graphics/DrawContext.h"
#include "Core/Document.h"
#include "Core/KeyMapping.h"
#include "Core/RuntimeConfig.h"
#include "Core/LineRender.h"
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

    document = Editor::Instance().GetActiveDocument();
    if (document == nullptr) {
        logger->Error("Document is null - no active textbuffer");
        return;
    }

    auto workspaceNode = Editor::Instance().GetWorkspaceNodeForDocument(document);
    if (workspaceNode == nullptr) {
        logger->Error("No workspace node for model!!");
        return;
    }

    editController = workspaceNode->GetController();
    if (editController == nullptr) {
        logger->Error("No controller for model!");
        return;
    }


    editController->SetTextBufferChangedHandler([this]()->void {
        auto node = Editor::Instance().GetWorkspace()->GetNodeFromDocument(document);
        if (node == nullptr) {
            return;
        }
        window->SetCaption(node->GetDisplayName());
    });

    editController->OnViewInit(rect);
}

void EditorView::ReInitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    auto &rect = window->GetContentDC().GetRect();

    bUseCLionPageNav = Config::Instance()[cfgSectionName].GetBool("pgupdown_content_first", true);

    document = Editor::Instance().GetActiveDocument();
    if (document == nullptr) {
        logger->Error("Document is null - no active textbuffer");
        return;
    }
    auto node = Editor::Instance().GetWorkspaceNodeForDocument(document);
    editController = node->GetController();

    // Fetch and update the view-model information
//    lineCursor = document->GetLineCursorRef();

    editController->OnViewInit(viewRect);
}

// FIXME: This is never used!!!
void EditorView::OnResized() {
    // Update the view Bottom line - as this affects how many lines we draw...
    auto &lineCursor = document->GetLineCursor();
    lineCursor.viewBottomLine = GetContentRect().Height();

    editController->OnViewInit(GetContentRect());
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
    lineRender.DrawLines(editController->Lines(),
                         lineCursor.viewTopLine,
                         lineCursor.viewBottomLine,
                         document->GetSelection());
}

void EditorView::DrawSearchResultOverlays(DrawContext &dc, int tabSize) {
    if (document->searchResults.empty()) {
        return;
    }
    auto &lineCursor = document->GetLineCursor();
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
        overlay.isActive = true;
        dc.AddOverlay(overlay);
    }
}

void EditorView::DrawSelectionOverlay(DrawContext &dc, int tabSize) {
    auto &selection = document->GetSelection();
    if (!selection.IsActive()) {
        return;
    }
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
    if (editController == nullptr) {
        return;
    }

    auto res = editController->OnKeyPress(keyPress);
    if (res) {
        InvalidateView();
        return;
    }



    // It was not to us..
    ViewBase::OnKeyPress(keyPress);
}


//
// Add actions here - all except human-readable inserting of text
//
bool EditorView::OnAction(const KeyPressAction &kpAction) {
    // fully possible - if the editor has no open files..
    if (!editController) {
        return false;
    }
    if (editController->OnAction(kpAction)) {
        return true;
    }

    if (DispatchAction(kpAction)) {
        return true;
    }

    return ViewBase::OnAction(kpAction);
}

bool EditorView::DispatchAction(const KeyPressAction &kpAction) {
    switch(kpAction.action) {
        case kAction::kActionCycleActiveBufferNext :
            OnActionNextBuffer();
            break;
        case kAction::kActionCycleActiveBufferPrev :
            OnActionPreviousBuffer();
            break;
        default :
            return false;
    }
    return true;
}


bool EditorView::OnActionNextBuffer() {
    ActionHelper::SwitchToNextBuffer();
    InvalidateAll();
    return true;
}

bool EditorView::OnActionPreviousBuffer() {
    ActionHelper::SwitchToPreviousBuffer();
    InvalidateAll();
    return true;
}

void EditorView::SetWindowCursor(const Cursor &cursor) {
    if ((Editor::Instance().GetState() == Editor::ViewState) && (document != nullptr)) {
        // The model cursor's position.x is a character index. Tabs render wider than one cell, so
        // translate it to a visual column before drawing the caret - otherwise the caret sits left
        // of where the character actually renders and inserts appear misplaced. The model cursor is
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
    // If we have a model - draw details...
    auto node = Editor::Instance().GetWorkspaceNodeForActiveDocument();
    if (node == nullptr) {
        return {statusCenter, statusRight};
    }
    // Hmm - why can't I use the document-> here????
    auto model = node->GetDocument();
    if (model == nullptr) {
        return {statusCenter, statusRight};
    }

    // Resolve center information
    if (model->GetTextBuffer()->GetBufferState() == TextBuffer::kBuffer_Changed) {
        statusCenter += U"* ";
    }
    if (model->GetTextBuffer()->IsReadOnly()) {
        statusCenter += U"R/O ";
    }

    statusCenter += node->GetDisplayNameU32();
    statusCenter += U" | ";
    if (!model->GetTextBuffer()->CanEdit()) {
        statusCenter += U"[locked] | ";
    }
    statusCenter += model->GetTextBuffer()->HaveLanguage() ? model->GetTextBuffer()->GetLanguage().Identifier() : U"none";

    // resolve right status
    auto &lineCursor = model->GetLineCursor();

    int indent = -1;
    auto line = model->LineAt(lineCursor.idxActiveLine);
    if (line != nullptr) {
        indent = line->GetIndent();
    }

    // Show Line:Row or more 'x/y' -> Configureation!
    // Column is the visual (tab-expanded) column, matching what the caret shows.
    int visualCol = (line != nullptr)
        ? line->CharToVisualColumn(lineCursor.cursor.position.x, model->GetTabSize())
        : lineCursor.cursor.position.x;
    auto strtmp = fmt::format(U"id: {} l: {}, c({}:{})",
                              indent,
                              strutil::itou32(lineCursor.idxActiveLine),
                              strutil::itou32(lineCursor.cursor.position.y),
                              strutil::itou32(visualCol));

    statusRight += strtmp;

    return {statusCenter, statusRight};
}
