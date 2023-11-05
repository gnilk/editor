//
// Created by gnilk on 14.02.23.
//
#include <unordered_map>

#include "Core/Action.h"
#include "Core/Config/Config.h"
#include "Core/DrawContext.h"
#include "Core/EditorModel.h"
#include "Core/KeyMapping.h"
#include "Core/RuntimeConfig.h"
#include "Core/LineRender.h"
#include "EditorView.h"
#include "Core/Editor.h"
#include "Core/ActionHelper.h"


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

    editorModel = Editor::Instance().GetActiveModel();
    if (editorModel == nullptr) {
        logger->Error("EditorModel is null - no active textbuffer");
        return;
    }

    editController->SetTextBufferChangedHandler([this]()->void {
        auto node = Editor::Instance().GetWorkspace()->GetNodeFromModel(editorModel);
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

    // FIXME: Replace model with controller
    editorModel = Editor::Instance().GetActiveModel();
    if (editorModel == nullptr) {
        logger->Error("EditorModel is null - no active textbuffer");
        return;
    }
    auto node = Editor::Instance().GetWorkspaceNodeForModel(editorModel);
    editController = node->GetController();

    // Fetch and update the view-model information
//    lineCursor = editorModel->GetLineCursorRef();

    editController->OnViewInit(viewRect);
}

// FIXME: This is never used!!!
void EditorView::OnResized() {
    // Update the view Bottom line - as this affects how many lines we draw...
    auto &lineCursor = editorModel->GetLineCursor();
    lineCursor.viewBottomLine = GetContentRect().Height();

    editController->OnViewInit(GetContentRect());
    ViewBase::OnResized();
}

void EditorView::DrawViewContents() {
    // Nothing to draw we don't have a model...
    if (editorModel == nullptr) {
        return;
    }

    auto &dc = window->GetContentDC(); //ViewBase::ContentAreaDrawContext();
    auto &selection = editorModel->GetSelection();
    auto &lineCursor = editorModel->GetLineCursor();

    logger->Debug("DrawViewContents, dc Height=%d, topLine=%d, bottomLine=%d",
                  dc.GetRect().Height(), lineCursor.viewTopLine, lineCursor.viewBottomLine);

    dc.ClearOverlays();


    // Add in the result from search if any...
    if (editorModel->searchResults.size() > 0) {
        logger->Debug("Overlays from search results");
        logger->Debug("  SR0: x=%d, len=%d line=%d", editorModel->searchResults[0].cursor_x, editorModel->searchResults[0].length, editorModel->searchResults[0].idxLine);
        for(auto &result : editorModel->searchResults) {
            // Perhaps a have a function to translate this - surprised I don't have one..

//            if ((result.idxLine >= lineCursor.viewTopLine) && (result.idxLine <lineCursor.viewBottomLine)) {
            if (lineCursor.IsInside(result.idxLine)) {
                DrawContext::Overlay overlay;
                auto yPos = result.idxLine - lineCursor.viewTopLine;
                overlay.Set(Point(result.cursor_x, yPos),
                            Point(result.cursor_x + result.length, yPos));
                overlay.isActive = true;
                dc.AddOverlay(overlay);
            }
        }
    }

    if (selection.IsActive()) {
        DrawContext::Overlay overlay;
        overlay.Set(selection.GetStart(), selection.GetEnd());
        overlay.attributes = 0;     // ??
        overlay.isActive = true;

        int dy = selection.GetEnd().y - selection.GetStart().y;
        overlay.start.y -= lineCursor.viewTopLine;
        overlay.end.y = overlay.start.y + dy;

        dc.AddOverlay(overlay);

        logger->Debug("Transform overlay to:");
        logger->Debug("  (%d:%d) - (%d:%d)", overlay.start.x, overlay.start.y, overlay.end.x, overlay.end.y);
        // ---- End test

    }

    LineRender lineRender(dc);
    // Consider refactoring this function call...
    lineRender.DrawLines(editController->Lines(),
                         lineCursor.viewTopLine,
                         lineCursor.viewBottomLine,
                         editorModel->GetSelection());
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
    if ((Editor::Instance().GetState() == Editor::ViewState) && (editorModel != nullptr)) {
        window->SetCursor(editorModel->GetCursor());
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
    auto node = Editor::Instance().GetWorkspaceNodeForActiveModel();
    if (node == nullptr) {
        return {statusCenter, statusRight};
    }
    // Hmm - why can't I use the editorModel-> here????
    auto model = node->GetModel();
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
    auto strtmp = fmt::format(U"id: {} l: {}, c({}:{})",
                              indent,
                              strutil::itou32(lineCursor.idxActiveLine),
                              strutil::itou32(lineCursor.cursor.position.y),
                              strutil::itou32(lineCursor.cursor.position.x));

    statusRight += strtmp;

    return {statusCenter, statusRight};
}
