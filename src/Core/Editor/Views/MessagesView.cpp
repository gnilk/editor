//
// See MessagesView.h
//

#include <algorithm>
#include <filesystem>

#include "MessagesView.h"
#include "Core/ActionHelper.h"
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/UI/UIHost.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

std::u32string MessagesView::Label(const std::string &name) {
    Refresh();
    auto label = rows.empty() ? name : name + " (" + std::to_string(rows.size()) + ")";
    return UnicodeHelper::utf8to32(label);
}

size_t MessagesView::NumRows() {
    Refresh();
    return rows.size();
}

std::optional<size_t> MessagesView::SelectedRow() {
    Refresh();
    return rows.empty() ? std::nullopt : std::optional(idxSelected);
}

std::string MessagesView::Caption() {
    Refresh();
    return caption;
}

void MessagesView::InitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("Messages");
}

void MessagesView::ReInitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
}

void MessagesView::OnActivate(bool isActivated) {
    if (isActivated) {
        Editor::Instance().SetActiveKeyMapping(settings.GetStr("keymap", "messages_keymap"));
    }
}

void MessagesView::DrawViewContents() {
    Refresh();
    auto &dc = window->GetContentDC();
    dc.ResetDrawColors();
    auto &uiColors = UIHost::Instance().GetUIColors();
    auto fg = uiColors["foreground"];
    auto bg = uiColors["background"];
    // Between the two colours - the folder steps back from the row
    auto dimmed = ColorRGBA::FromRGB((fg.RedAsInt() + bg.RedAsInt()) / 2, (fg.GreenAsInt() + bg.GreenAsInt()) / 2,
                                     (fg.BlueAsInt() + bg.BlueAsInt()) / 2);
    dc.SetColor(fg, bg);
    auto width = dc.GetRect().Width();
    for (int y = 0; y < dc.GetRect().Height(); y++) {
        dc.FillLine(y, kTextAttributes::kNormal, ' ');
    }
    dc.SetColor(dimmed, bg);
    dc.DrawStringWithAttributesAt(1, 0, kTextAttributes::kNormal, UnicodeHelper::utf8to32(caption).substr(0, std::max(0, width - 1)));
    dc.SetColor(fg, bg);

    for (int i = 0; i < ListHeight(); i++) {
        auto idxRow = (size_t)(idxTopRow + i);
        if (idxRow >= rows.size()) {
            break;
        }
        auto &message = messages[rows[idxRow]];
        auto y = LIST_TOP_ROW + i;
        // The folder on the right, as printed - which of two 'util.h' it is; a long one shows its end
        auto folder = UnicodeHelper::utf8to32(std::filesystem::path(message.path).parent_path().string());
        auto maxFolder = (size_t)std::max(0, width / 3);
        if (folder.size() > maxFolder) {
            auto numKept = (maxFolder > 3) ? (maxFolder - 3) : 0;
            folder = U"..." + folder.substr(folder.size() - numKept);
        }
        auto widthText = folder.empty() ? width - 1 : (width - 2 - (int)folder.size());
        auto text = UnicodeHelper::utf8to32(BuildMessages::RowText(message)).substr(0, (size_t)std::max(0, widthText));
        auto xFolder = width - 1 - (int)folder.size();
        if (idxRow == idxSelected) {
            dc.FillLine(y, kTextAttributes::kInverted, ' ');
            dc.DrawStringWithAttributesAt(1, y, kTextAttributes::kInverted, text);
            dc.DrawStringWithAttributesAt(xFolder, y, kTextAttributes::kInverted, folder);
        } else {
            dc.DrawStringWithAttributesAt(1, y, kTextAttributes::kNormal, text);
            dc.SetColor(dimmed, bg);
            dc.DrawStringWithAttributesAt(xFolder, y, kTextAttributes::kNormal, folder);
            dc.SetColor(fg, bg);
        }
    }
}

bool MessagesView::OnAction(const EditorAction &action) {
    Refresh();
    switch (action.uiAction) {
        case kUIAction::kUIActionLineUp:
            MoveSelection(-1);
            return true;
        case kUIAction::kUIActionLineDown:
            MoveSelection(1);
            return true;
        case kUIAction::kUIActionPageUp:
            MoveSelection(-std::max(1, ListHeight() - 1));
            return true;
        case kUIAction::kUIActionPageDown:
            MoveSelection(std::max(1, ListHeight() - 1));
            return true;
        case kUIAction::kUIActionBufferStart:
            MoveSelection(-(int)rows.size());
            return true;
        case kUIAction::kUIActionBufferEnd:
            MoveSelection((int)rows.size());
            return true;
        case kUIAction::kUIActionCommitLine:
            GoToSelected();
            return true;
        default:
            break;
    }
    return ViewBase::OnAction(action);
}

// A click selects a row, a double-click goes there; the wheel scrolls
bool MessagesView::OnMouseEvent(const MouseEvent &mouseEvent) {
    Refresh();
    if (mouseEvent.kind == MouseEvent::kMouseEventKind_Wheel) {
        MoveSelection(-mouseEvent.wheelDelta);
        return true;
    }
    if (mouseEvent.kind != MouseEvent::kMouseEventKind_Press) {
        return false;
    }
    auto y = mouseEvent.y - GetContentRect().TopLeft().y - LIST_TOP_ROW;
    if ((y < 0) || ((size_t)(idxTopRow + y) >= rows.size())) {
        return false;
    }
    SelectRow((size_t)(idxTopRow + y));
    if (mouseEvent.clicks >= 2) {
        GoToSelected();
    }
    Editor::Instance().TriggerUIRedraw();
    return true;
}

void MessagesView::Refresh() {
    if (!buildMessages.HasRun()) {
        messages.clear();
        rows.clear();
        caption = "No build yet - run make / cmake / ninja in a terminal";
        return;
    }
    messages = buildMessages.Messages();
    rows = BuildMessages::Order(messages);
    caption = buildMessages.Command() + " - " + buildMessages.Summary() + (buildMessages.IsRunning() ? " (running)" : "");
    // A new run: from the top
    auto runNumber = buildMessages.RunNumber();
    if (runNumber != seenRun) {
        seenRun = runNumber;
        seenCurrent.reset();
        idxSelected = 0;
        idxTopRow = 0;
    }
    // F8 / Shift+F8 (or a pick in the modal) moved: the selection goes with it
    auto current = buildMessages.Current();
    if (current != seenCurrent) {
        seenCurrent = current;
        if (current.has_value()) {
            auto it = std::find(rows.begin(), rows.end(), current.value());
            if (it != rows.end()) {
                SelectRow((size_t)(it - rows.begin()));
            }
        }
    }
    if (!rows.empty() && (idxSelected >= rows.size())) {
        SelectRow(rows.size() - 1);
    }
}

void MessagesView::MoveSelection(int delta) {
    if (rows.empty()) {
        return;
    }
    auto target = std::clamp((int)idxSelected + delta, 0, (int)rows.size() - 1);
    SelectRow((size_t)target);
}

// Kept in sight
void MessagesView::SelectRow(size_t idxRow) {
    idxSelected = idxRow;
    auto height = std::max(1, ListHeight());
    if ((int)idxSelected < idxTopRow) {
        idxTopRow = (int)idxSelected;
    } else if ((int)idxSelected >= idxTopRow + height) {
        idxTopRow = (int)idxSelected - height + 1;
    }
}

void MessagesView::GoToSelected() {
    if (idxSelected >= rows.size()) {
        return;
    }
    auto position = buildMessages.Select(rows[idxSelected]);
    // Select makes it the current one - that is where the selection is already
    seenCurrent = buildMessages.Current();
    if (position.has_value()) {
        ActionHelper::GoToBuildMessage(position.value());
    }
}

int MessagesView::ListHeight() const {
    if (window == nullptr) {
        return std::max(0, viewRect.Height() - LIST_TOP_ROW);
    }
    return std::max(0, window->GetContentDC().GetRect().Height() - LIST_TOP_ROW);
}
