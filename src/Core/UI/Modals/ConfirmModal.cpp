//
// A message and 2-4 choices - see ConfirmModal.h
//

#include <algorithm>
#include <cwctype>

#include "ConfirmModal.h"
#include "Core/UI/UIHost.h"
#include "Core/Keyboard.h"

using namespace gedit;

// Columns of space left and right of the text, and between two choices
static constexpr int kMarginX = 2;
static constexpr int kChoiceGap = 3;

ConfirmModal::ConfirmModal(const std::string &caption, const std::u32string &message, const std::vector<std::u32string> &choices) : choices(choices) {
    SetCaption(caption);
    size_t start = 0;
    for (;;) {
        auto end = message.find(U'\n', start);
        messageLines.push_back(message.substr(start, end - start));
        if (end == std::u32string::npos) {
            break;
        }
        start = end + 1;
    }
}

void ConfirmModal::SetSelectedChoice(int idxNewSelected) {
    if ((idxNewSelected < 0) || (idxNewSelected >= (int)choices.size())) {
        return;
    }
    idxSelected = idxNewSelected;
    InvalidateView();
}

bool ConfirmModal::OnAction(const EditorAction &kpAction) {
    switch (kpAction.uiAction) {
        case kUIAction::kUIActionLineLeft :
            SetSelectedChoice(idxSelected - 1);
            return true;
        case kUIAction::kUIActionLineRight :
            SetSelectedChoice(idxSelected + 1);
            return true;
        case kUIAction::kUIActionCommitLine :
            Choose(idxSelected);
            return true;
        default:
            break;
    }
    return ModalView::OnAction(kpAction);
}

// A letter (shift is fine) picks the choice it is the hotkey for - case doesn't matter, the first match wins
void ConfirmModal::OnKeyPress(const KeyPress &keyPress) {
    if (!keyPress.IsHumanReadable() || ((keyPress.modifiers & ~Keyboard::ShiftMask()) != 0)) {
        return;
    }
    auto key = std::towlower((wint_t)keyPress.key);
    for (size_t i = 0; i < choices.size(); i++) {
        if (!choices[i].empty() && (std::towlower((wint_t)choices[i][0]) == key)) {
            Choose((int)i);
            return;
        }
    }
}

void ConfirmModal::Choose(int idxChoice) {
    if ((idxChoice < 0) || (idxChoice >= (int)choices.size())) {
        return;
    }
    idxChosen = idxChoice;
    Close();
}

void ConfirmModal::OnClosed(bool wasCancelled) {
    if (wasCancelled || (idxChosen < 0)) {
        if (onCancel) {
            onCancel();
        }
        return;
    }
    if (onChoice) {
        onChoice(idxChosen);
    }
}

// '[S]ave'
std::u32string ConfirmModal::ChoiceLabel(size_t idxChoice) const {
    auto &choice = choices[idxChoice];
    if (choice.empty()) {
        return {};
    }
    return U"[" + choice.substr(0, 1) + U"]" + choice.substr(1);
}

size_t ConfirmModal::ChoiceRowWidth() const {
    size_t width = 0;
    for (size_t i = 0; i < choices.size(); i++) {
        width += ChoiceLabel(i).size() + ((i > 0) ? kChoiceGap : 0);
    }
    return width;
}

// Sized from the content: the caption row, a blank row, the message, a blank row, the choices, a blank row
Rect ConfirmModal::ComputeRect(const Rect &screenRect) {
    size_t textWidth = ChoiceRowWidth();
    for (auto &line : messageLines) {
        textWidth = std::max(textWidth, line.size());
    }
    auto width = (int)textWidth + 2 * kMarginX;
    auto height = 1 + 1 + (int)messageLines.size() + 1 + 1 + 1;
    return CenterIn(screenRect, width, height);
}

void ConfirmModal::DrawViewContents() {
    auto &dc = window->GetContentDC();
    dc.ResetDrawColors();
    auto &uiColors = UIHost::Instance().GetUIColors();
    auto fg = uiColors.HasColor("modal_foreground") ? uiColors["modal_foreground"] : uiColors["foreground"];
    auto bg = uiColors.HasColor("modal_background") ? uiColors["modal_background"] : uiColors["background"];
    dc.SetColor(fg, bg);

    auto rect = dc.GetRect();
    for (int y = 0; y < rect.Height(); y++) {
        dc.FillLine(y, kTextAttributes::kNormal, ' ');
    }

    int y = 1;
    for (auto &line : messageLines) {
        dc.DrawStringWithAttributesAt(kMarginX, y, kTextAttributes::kNormal, line);
        y++;
    }
    y++;

    // The choices row, centred - the selected one inverted
    auto x = std::max(kMarginX, (rect.Width() - (int)ChoiceRowWidth()) / 2);
    for (size_t i = 0; i < choices.size(); i++) {
        auto label = ChoiceLabel(i);
        auto attrib = ((int)i == idxSelected) ? kTextAttributes::kInverted : kTextAttributes::kNormal;
        dc.DrawStringWithAttributesAt(x, y, attrib, label);
        x += (int)label.size() + kChoiceGap;
    }
}
