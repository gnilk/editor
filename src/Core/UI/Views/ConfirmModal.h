//
// A message and 2-4 choices (docs/modal_fixes.md §3.4 item 1). A choice's hotkey is its first letter,
// shown as '[S]ave'. Left/Right move the selection, Return picks it, a hotkey picks its choice, Esc cancels.
//

#ifndef GOATEDIT_CONFIRMMODAL_H
#define GOATEDIT_CONFIRMMODAL_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ModalView.h"

namespace gedit {
    class ConfirmModal : public ModalView {
    public:
        using Ptr = std::unique_ptr<ConfirmModal>;
        using ChoiceDelegate = std::function<void(int idxChoice)>;
        using CancelDelegate = std::function<void()>;
    public:
        // 'message' may have several lines ('\n')
        ConfirmModal(const std::string &caption, const std::u32string &message, const std::vector<std::u32string> &choices);
        virtual ~ConfirmModal() = default;

        static Ptr Create(const std::string &caption, const std::u32string &message, const std::vector<std::u32string> &choices) {
            return std::make_unique<ConfirmModal>(caption, message, choices);
        }

        void SetOnChoice(ChoiceDelegate newOnChoice) {
            onChoice = std::move(newOnChoice);
        }
        void SetOnCancel(CancelDelegate newOnCancel) {
            onCancel = std::move(newOnCancel);
        }

        // Which choice Return would pick
        void SetSelectedChoice(int idxNewSelected);
        int GetSelectedChoice() const {
            return idxSelected;
        }

        bool OnAction(const EditorAction &kpAction) override;
        void OnKeyPress(const KeyPress &keyPress) override;
    protected:
        void OnClosed(bool wasCancelled) override;
        void DrawViewContents() override;
        Rect ComputeRect(const Rect &screenRect) override;
    private:
        void Choose(int idxChoice);
        std::u32string ChoiceLabel(size_t idxChoice) const;
        size_t ChoiceRowWidth() const;
    private:
        std::vector<std::u32string> messageLines;
        std::vector<std::u32string> choices;
        int idxSelected = 0;
        int idxChosen = -1;
        ChoiceDelegate onChoice = nullptr;
        CancelDelegate onCancel = nullptr;
    };
}

#endif //GOATEDIT_CONFIRMMODAL_H
