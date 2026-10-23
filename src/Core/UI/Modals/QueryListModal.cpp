//
// An edit line with a result list - see QueryListModal.h
//

#include <algorithm>

#include "QueryListModal.h"
#include "Core/UI/UIHost.h"
#include "Core/Keyboard.h"

using namespace gedit;

// Content rows above the list: the edit line and a blank row
static constexpr int kListTopRow = 2;
static constexpr int kMarginX = 1;

static std::u32string UnicodeOf(int value) {
    auto str = std::to_string(value);
    return std::u32string(str.begin(), str.end());
}

QueryListModal::QueryListModal(const std::string &caption, const std::u32string &prompt) : prompt(prompt) {
    SetCaption(caption);
    input = Line::Create(U"");
}

void QueryListModal::SetQuery(const std::u32string &query) {
    input = Line::Create(query);
    inputCursor.position.x = (int)query.length();
    RunQuery();
}

bool QueryListModal::OnAction(const EditorAction &kpAction) {
    switch (kpAction.uiAction) {
        case kUIAction::kUIActionLineUp :
            Select(idxSelected - 1);
            return true;
        case kUIAction::kUIActionLineDown :
            Select(idxSelected + 1);
            return true;
        case kUIAction::kUIActionPageUp :
            Select(idxSelected - ListHeight());
            return true;
        case kUIAction::kUIActionPageDown :
            Select(idxSelected + ListHeight());
            return true;
        case kUIAction::kUIActionLineLeft :
            inputCursor.position.x = std::max(0, inputCursor.position.x - 1);
            InvalidateView();
            return true;
        case kUIAction::kUIActionLineRight :
            inputCursor.position.x = std::min((int)input->Length(), inputCursor.position.x + 1);
            InvalidateView();
            return true;
        case kUIAction::kUIActionLineHome :
            inputCursor.position.x = 0;
            InvalidateView();
            return true;
        case kUIAction::kUIActionLineEnd :
            inputCursor.position.x = (int)input->Length();
            InvalidateView();
            return true;
        case kUIAction::kUIActionCommitLine :
            // Nothing to go to - stay open
            if (idxSelected >= 0) {
                idxCommitted = idxSelected;
                Close();
            }
            return true;
        default:
            break;
    }
    return ModalView::OnAction(kpAction);
}

// Typing edits the line (shift is fine, ctrl/alt/cmd combos are not text)
void QueryListModal::OnKeyPress(const KeyPress &keyPress) {
    if ((keyPress.modifiers & ~Keyboard::ShiftMask()) != 0) {
        return;
    }
    auto before = input->Buffer();
    inputController.DefaultEditLine(inputCursor, input, keyPress, true);
    if (input->Buffer() != before) {
        RunQuery();
    }
    InvalidateView();
}

void QueryListModal::RunQuery() {
    rows = OnQuery(input->Buffer());
    idxTopRow = 0;
    idxSelected = -1;
    if (rows.empty()) {
        OnPreview(-1);
    } else {
        Select(0);
    }
    InvalidateView();
}

// Clamped to the list; scrolls it so the selection is visible
void QueryListModal::Select(int idxRow) {
    if (rows.empty()) {
        return;
    }
    idxRow = std::clamp(idxRow, 0, (int)rows.size() - 1);
    if (idxRow == idxSelected) {
        return;
    }
    idxSelected = idxRow;
    auto height = std::max(1, ListHeight());
    if (idxSelected < idxTopRow) {
        idxTopRow = idxSelected;
    } else if (idxSelected >= idxTopRow + height) {
        idxTopRow = idxSelected - height + 1;
    }
    OnPreview(idxSelected);
    InvalidateView();
}

// The rect minus the caption row and the rows above the list
int QueryListModal::ListHeight() {
    return viewRect.Height() - 1 - kListTopRow;
}

void QueryListModal::OnClosed(bool wasCancelled) {
    if (wasCancelled || (idxCommitted < 0)) {
        OnCancel();
        return;
    }
    OnCommit(idxCommitted);
}

// Upper part of the screen, so what is previewed below it stays visible
Rect QueryListModal::ComputeRect(const Rect &screenRect) {
    auto width = std::max(std::min(40, screenRect.Width()), (screenRect.Width() * 6) / 10);
    auto height = std::max(std::min(8, screenRect.Height()), (screenRect.Height() * 35) / 100);
    auto rect = CenterIn(screenRect, width, height);
    rect.Move(0, screenRect.Height() / 20 - rect.TopLeft().y);
    return rect;
}

void QueryListModal::DrawViewContents() {
    auto &dc = window->GetContentDC();
    dc.ResetDrawColors();
    auto &uiColors = UIHost::Instance().GetUIColors();
    auto fg = uiColors.HasColor("modal_foreground") ? uiColors["modal_foreground"] : uiColors["foreground"];
    auto bg = uiColors.HasColor("modal_background") ? uiColors["modal_background"] : uiColors["background"];
    dc.SetColor(fg, bg);

    auto rect = dc.GetRect();
    auto width = rect.Width() - 2 * kMarginX;
    for (int y = 0; y < rect.Height(); y++) {
        dc.FillLine(y, kTextAttributes::kNormal, ' ');
    }

    // The edit line, and how many results on the right
    std::u32string counter = rows.empty() ? (input->Length() > 0 ? U"no hits" : U"")
                                          : (UnicodeOf(idxSelected + 1) + U"/" + UnicodeOf((int)rows.size()));
    dc.DrawStringWithAttributesAt(kMarginX, 0, kTextAttributes::kNormal, prompt + input->Buffer());
    if (!counter.empty()) {
        dc.DrawStringWithAttributesAt(std::max(kMarginX, rect.Width() - kMarginX - (int)counter.size()), 0, kTextAttributes::kNormal, counter);
    }
    cursor.position.x = kMarginX + (int)prompt.size() + inputCursor.position.x;
    cursor.position.y = 0;

    // The list - the selected row inverted across the whole width
    for (int i = 0; i < ListHeight(); i++) {
        auto idxRow = idxTopRow + i;
        if (idxRow >= (int)rows.size()) {
            break;
        }
        auto y = kListTopRow + i;
        auto text = rows[idxRow].substr(0, std::max(0, width));
        if (idxRow == idxSelected) {
            dc.FillLine(y, kTextAttributes::kInverted, ' ');
            dc.DrawStringWithAttributesAt(kMarginX, y, kTextAttributes::kInverted, text);
        } else {
            dc.DrawStringWithAttributesAt(kMarginX, y, kTextAttributes::kNormal, text);
        }
    }
}
