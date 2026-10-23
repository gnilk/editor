//
// Modal host (docs/modal_fixes.md §3.1 / step 1): RootView's modal stack, routing, deferred close,
// nesting and the modal keymap.
//
#include <functional>
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "Core/UI/Views/ViewBase.h"
#include "Core/UI/Views/RootView.h"
#include "Core/UI/Views/ModalView.h"
#include "Core/UI/Views/ConfirmModal.h"
#include "Core/Keyboard.h"
#include "Core/UI/UIHost.h"
#include "Core/MouseEvent.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_modal(ITesting *t);
    DLL_EXPORT int test_modal_routing(ITesting *t);
    DLL_EXPORT int test_modal_closeinaction(ITesting *t);
    DLL_EXPORT int test_modal_cancel(ITesting *t);
    DLL_EXPORT int test_modal_nested(ITesting *t);
    DLL_EXPORT int test_modal_showfromclose(ITesting *t);
    DLL_EXPORT int test_modal_closeoutside(ITesting *t);
    DLL_EXPORT int test_modal_keymap(ITesting *t);
    DLL_EXPORT int test_modal_confirmhotkey(ITesting *t);
    DLL_EXPORT int test_modal_confirmselect(ITesting *t);
    DLL_EXPORT int test_modal_confirmcancel(ITesting *t);
    DLL_EXPORT int test_modal_confirmsize(ITesting *t);
}

namespace {
    // Stands in for the editor underneath - counts what reaches it
    class RecordingView : public ViewBase {
    public:
        void InitView() override {
            // The tree is drawn in 'closeoutside'
            auto screen = UIHost::Instance().GetScreen();
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
        }
        bool OnAction(const EditorAction &action) override {
            nActions++;
            return true;
        }
        void OnKeyPress(const KeyPress &keyPress) override {
            nKeys++;
        }
        bool OnMouseEvent(const MouseEvent &mouseEvent) override {
            nMouse++;
            return true;
        }
    public:
        int nActions = 0;
        int nKeys = 0;
        int nMouse = 0;
    };

    // Outlives the modal (the host destroys it after OnClosed)
    struct ModalLog {
        int nActions = 0;
        int nKeys = 0;
        int nMouse = 0;
        int nClosed = 0;
        bool wasCancelled = false;
    };

    // CommitLine closes; LineDown runs 'onLineDown' (e.g. show another modal from inside the dispatch)
    class TestModal : public ModalView {
    public:
        explicit TestModal(ModalLog &log) : log(log) {
        }
        bool OnAction(const EditorAction &action) override {
            log.nActions++;
            if (action.uiAction == kUIAction::kUIActionCommitLine) {
                Close();
                return true;
            }
            if ((action.uiAction == kUIAction::kUIActionLineDown) && onLineDown) {
                onLineDown();
                return true;
            }
            return ModalView::OnAction(action);
        }
        void OnKeyPress(const KeyPress &keyPress) override {
            log.nKeys++;
        }
        bool OnMouseEvent(const MouseEvent &mouseEvent) override {
            log.nMouse++;
            return true;
        }
    protected:
        void OnClosed(bool wasCancelled) override {
            log.nClosed++;
            log.wasCancelled = wasCancelled;
            if (onClosed) {
                onClosed();
            }
        }
    public:
        std::function<void()> onLineDown = nullptr;
        std::function<void()> onClosed = nullptr;
    private:
        ModalLog &log;
    };

    EditorAction UIAction(kUIAction uiAction) {
        EditorAction action;
        action.uiAction = uiAction;
        return action;
    }

    KeyPress Key(char32_t key) {
        KeyPress keyPress;
        keyPress.isKeyValid = true;
        keyPress.key = key;
        return keyPress;
    }

    MouseEvent Click(int x, int y) {
        MouseEvent mouseEvent;
        mouseEvent.kind = MouseEvent::kMouseEventKind_Press;
        mouseEvent.x = x;
        mouseEvent.y = y;
        return mouseEvent;
    }

    // A root with one top view filling it - the headless screen is 100x100, a modal sits at 25..75
    struct Fixture {
        Fixture() {
            rootView.AddView(&underneath);
            rootView.AddTopView(&underneath, "underneath");
            rootView.Initialize();
        }
        RootView rootView;
        RecordingView underneath;
    };
}

DLL_EXPORT int test_modal(ITesting *t) {
    // Headless screen (test_main) - views need it to initialize
    Editor::Instance().OpenScreen();
    return kTR_Pass;
}

// Everything goes to the top modal - actions (handled or not), keys, mouse - and nothing reaches the view
// underneath; a click outside the dialog is swallowed
DLL_EXPORT int test_modal_routing(ITesting *t) {
    Fixture fixture;
    auto &rootView = fixture.rootView;
    auto &underneath = fixture.underneath;

    // Before: the view gets it (the counters discriminate)
    rootView.OnAction(UIAction(kUIAction::kUIActionLineUp));
    TR_ASSERT(t, underneath.nActions == 1);

    ModalLog log;
    rootView.ShowModal(std::make_unique<TestModal>(log));
    TR_ASSERT(t, rootView.HasModal());

    TR_ASSERT(t, rootView.OnAction(UIAction(kUIAction::kUIActionLineUp)));
    // Not handled by the modal - still not passed on
    TR_ASSERT(t, rootView.OnAction(UIAction(kUIAction::kUIActionPageDown)));
    TR_ASSERT(t, log.nActions == 2);
    TR_ASSERT(t, underneath.nActions == 1);

    rootView.HandleKeyPress(Key('x'));
    TR_ASSERT(t, log.nKeys == 1);
    TR_ASSERT(t, underneath.nKeys == 0);

    TR_ASSERT(t, rootView.DispatchMouse(Click(50, 50)));
    TR_ASSERT(t, log.nMouse == 1);
    TR_ASSERT(t, rootView.DispatchMouse(Click(1, 1)));
    TR_ASSERT(t, log.nMouse == 1);
    TR_ASSERT(t, underneath.nMouse == 0);

    TR_ASSERT(t, log.nClosed == 0);
    return kTR_Pass;
}

// Closing from inside the modal's own OnAction: popped after the dispatch, the callback fires once, the
// view underneath gets input again
DLL_EXPORT int test_modal_closeinaction(ITesting *t) {
    Fixture fixture;
    auto &rootView = fixture.rootView;

    ModalLog log;
    rootView.ShowModal(std::make_unique<TestModal>(log));
    rootView.OnAction(UIAction(kUIAction::kUIActionCommitLine));

    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, log.nClosed == 1);
    TR_ASSERT(t, !log.wasCancelled);

    rootView.OnAction(UIAction(kUIAction::kUIActionLineUp));
    TR_ASSERT(t, fixture.underneath.nActions == 1);
    TR_ASSERT(t, log.nClosed == 1);
    return kTR_Pass;
}

// Esc (CloseModal) cancels
DLL_EXPORT int test_modal_cancel(ITesting *t) {
    Fixture fixture;
    auto &rootView = fixture.rootView;

    ModalLog log;
    rootView.ShowModal(std::make_unique<TestModal>(log));
    rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));

    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, log.nClosed == 1);
    TR_ASSERT(t, log.wasCancelled);
    return kTR_Pass;
}

// A modal opens another from inside its dispatch; closing the inner one gives the input back to the outer
DLL_EXPORT int test_modal_nested(ITesting *t) {
    Fixture fixture;
    auto &rootView = fixture.rootView;

    ModalLog outerLog;
    ModalLog innerLog;
    auto outer = std::make_unique<TestModal>(outerLog);
    auto outerPtr = outer.get();
    outer->onLineDown = [&rootView, &innerLog]() {
        rootView.ShowModal(std::make_unique<TestModal>(innerLog));
    };
    rootView.ShowModal(std::move(outer));

    rootView.OnAction(UIAction(kUIAction::kUIActionLineDown));
    TR_ASSERT(t, rootView.TopModal() != outerPtr);
    TR_ASSERT(t, !outerPtr->IsActive());

    rootView.OnAction(UIAction(kUIAction::kUIActionLineUp));
    TR_ASSERT(t, innerLog.nActions == 1);
    TR_ASSERT(t, outerLog.nActions == 1);

    // Close the inner one - the outer is on top and active again
    rootView.OnAction(UIAction(kUIAction::kUIActionCommitLine));
    TR_ASSERT(t, innerLog.nClosed == 1);
    TR_ASSERT(t, rootView.TopModal() == outerPtr);
    TR_ASSERT(t, outerPtr->IsActive());

    rootView.OnAction(UIAction(kUIAction::kUIActionLineUp));
    TR_ASSERT(t, outerLog.nActions == 2);
    TR_ASSERT(t, innerLog.nActions == 2);

    rootView.OnAction(UIAction(kUIAction::kUIActionCommitLine));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, outerLog.nClosed == 1);
    TR_ASSERT(t, innerLog.nClosed == 1);
    TR_ASSERT(t, fixture.underneath.nActions == 0);
    return kTR_Pass;
}

// The close callback shows the next modal (a picker -> a confirm): it is up, and was not reaped with
// the one that closed
DLL_EXPORT int test_modal_showfromclose(ITesting *t) {
    Fixture fixture;
    auto &rootView = fixture.rootView;

    ModalLog firstLog;
    ModalLog nextLog;
    auto first = std::make_unique<TestModal>(firstLog);
    first->onClosed = [&rootView, &nextLog]() {
        rootView.ShowModal(std::make_unique<TestModal>(nextLog));
    };
    rootView.ShowModal(std::move(first));
    rootView.OnAction(UIAction(kUIAction::kUIActionCommitLine));

    TR_ASSERT(t, firstLog.nClosed == 1);
    TR_ASSERT(t, rootView.HasModal());
    TR_ASSERT(t, nextLog.nClosed == 0);

    rootView.OnAction(UIAction(kUIAction::kUIActionLineUp));
    TR_ASSERT(t, nextLog.nActions == 1);
    TR_ASSERT(t, firstLog.nActions == 1);
    return kTR_Pass;
}

// Closed from outside an input dispatch (a posted message): the close invalidates, the next draw reaps
DLL_EXPORT int test_modal_closeoutside(ITesting *t) {
    Fixture fixture;
    auto &rootView = fixture.rootView;

    ModalLog log;
    rootView.ShowModal(std::make_unique<TestModal>(log));
    rootView.Draw();
    TR_ASSERT(t, !rootView.IsInvalid());

    rootView.TopModal()->Close();
    TR_ASSERT(t, rootView.IsInvalid());
    TR_ASSERT(t, log.nClosed == 0);

    rootView.Draw();
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, log.nClosed == 1);
    return kTR_Pass;
}

// 'modal_keymap' while a modal is up (also across a nested one); a keymap change underneath is held back
// and applies when the last modal closes
DLL_EXPORT int test_modal_keymap(ITesting *t) {
    auto &editor = Editor::Instance();
    auto modalKeyMap = editor.GetKeyMapping("modal_keymap");
    TR_ASSERT(t, modalKeyMap != nullptr);

    auto before = editor.GetActiveKeyMap();
    TR_ASSERT(t, before != modalKeyMap);

    // What the run loop would be switched to (it installs this delegate; no run loop in the tests)
    KeyMapping::Ref pushed = nullptr;
    editor.SetKeymapUpdateDelegate([&pushed](KeyMapping::Ref newKeymap) {
        pushed = newKeymap;
    });

    Fixture fixture;
    auto &rootView = fixture.rootView;

    ModalLog outerLog;
    ModalLog innerLog;
    auto outer = std::make_unique<TestModal>(outerLog);
    outer->onLineDown = [&rootView, &innerLog]() {
        rootView.ShowModal(std::make_unique<TestModal>(innerLog));
    };
    rootView.ShowModal(std::move(outer));
    TR_ASSERT(t, editor.GetActiveKeyMap() == modalKeyMap);
    TR_ASSERT(t, pushed == modalKeyMap);

    rootView.OnAction(UIAction(kUIAction::kUIActionLineDown));
    rootView.OnAction(UIAction(kUIAction::kUIActionCommitLine));
    // Inner closed, outer still up
    TR_ASSERT(t, editor.GetActiveKeyMap() == modalKeyMap);

    // A view activating underneath sets its keymap - held back
    editor.SetActiveKeyMapping("workspace_keymap");
    TR_ASSERT(t, editor.GetActiveKeyMap() == modalKeyMap);
    TR_ASSERT(t, pushed == modalKeyMap);

    rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, editor.GetActiveKeyMap() == editor.GetKeyMapping("workspace_keymap"));
    TR_ASSERT(t, pushed == editor.GetKeyMapping("workspace_keymap"));

    // Leave the default behind for the next test
    editor.SetKeymapUpdateDelegate(nullptr);
    editor.SetActiveKeyMapping("default_keymap");
    return kTR_Pass;
}

namespace {
    // What a ConfirmModal reported - outlives it
    struct ConfirmLog {
        int nChoice = 0;
        int nCancel = 0;
        int idxChoice = -1;
    };

    ConfirmModal::Ptr SaveDiscardCancel(ConfirmLog &log, const std::u32string &message = U"'x.cpp' has changes not yet saved") {
        auto modal = ConfirmModal::Create("Confirm", message, {U"Save", U"Discard", U"Cancel"});
        modal->SetOnChoice([&log](int idxChoice) {
            log.nChoice++;
            log.idxChoice = idxChoice;
        });
        modal->SetOnCancel([&log]() {
            log.nCancel++;
        });
        return modal;
    }
}

// A letter picks its choice - either case, shift is fine - and closes; one with ctrl/cmd or no match does nothing
DLL_EXPORT int test_modal_confirmhotkey(ITesting *t) {
    Fixture fixture;
    auto &rootView = fixture.rootView;

    ConfirmLog log;
    rootView.ShowModal(SaveDiscardCancel(log));

    rootView.HandleKeyPress(Key('x'));
    auto ctrlD = Key('d');
    ctrlD.modifiers = Keyboard::kMod_LeftCtrl;
    rootView.HandleKeyPress(ctrlD);
    TR_ASSERT(t, rootView.HasModal());
    TR_ASSERT(t, log.nChoice == 0);

    rootView.HandleKeyPress(Key('d'));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, log.nChoice == 1);
    TR_ASSERT(t, log.idxChoice == 1);
    TR_ASSERT(t, log.nCancel == 0);

    ConfirmLog logUpper;
    rootView.ShowModal(SaveDiscardCancel(logUpper));
    auto shiftS = Key('S');
    shiftS.modifiers = Keyboard::kMod_LeftShift;
    rootView.HandleKeyPress(shiftS);
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, logUpper.idxChoice == 0);
    return kTR_Pass;
}

// Left/Right move the selection (stopping at the ends), Return picks the selected one
DLL_EXPORT int test_modal_confirmselect(ITesting *t) {
    Fixture fixture;
    auto &rootView = fixture.rootView;

    ConfirmLog log;
    auto modal = SaveDiscardCancel(log);
    auto modalPtr = modal.get();
    rootView.ShowModal(std::move(modal));
    TR_ASSERT(t, modalPtr->GetSelectedChoice() == 0);

    rootView.OnAction(UIAction(kUIAction::kUIActionLineLeft));
    TR_ASSERT(t, modalPtr->GetSelectedChoice() == 0);
    for (int i = 0; i < 5; i++) {
        rootView.OnAction(UIAction(kUIAction::kUIActionLineRight));
    }
    TR_ASSERT(t, modalPtr->GetSelectedChoice() == 2);
    rootView.OnAction(UIAction(kUIAction::kUIActionLineLeft));
    TR_ASSERT(t, modalPtr->GetSelectedChoice() == 1);

    rootView.OnAction(UIAction(kUIAction::kUIActionCommitLine));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, log.nChoice == 1);
    TR_ASSERT(t, log.idxChoice == 1);
    return kTR_Pass;
}

// Esc: the cancel callback, never the choice one
DLL_EXPORT int test_modal_confirmcancel(ITesting *t) {
    Fixture fixture;
    auto &rootView = fixture.rootView;

    ConfirmLog log;
    rootView.ShowModal(SaveDiscardCancel(log));
    rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));
    TR_ASSERT(t, !rootView.HasModal());
    TR_ASSERT(t, log.nCancel == 1);
    TR_ASSERT(t, log.nChoice == 0);
    return kTR_Pass;
}

// Sized from the content (not half the screen), centred; a longer message makes it wider and taller,
// and it never outgrows the screen
DLL_EXPORT int test_modal_confirmsize(ITesting *t) {
    Fixture fixture;
    auto &rootView = fixture.rootView;
    auto screenRect = UIHost::Instance().GetScreen()->Dimensions();

    ConfirmLog log;
    rootView.ShowModal(SaveDiscardCancel(log, U"short"));
    auto small = rootView.TopModal()->GetViewRect();
    // Wide enough for the choices row, a caption, blank, one line, blank, choices, blank
    TR_ASSERT(t, small.Width() >= (int)std::u32string(U"[S]ave   [D]iscard   [C]ancel").size());
    TR_ASSERT(t, small.Width() < screenRect.Width() / 2);
    TR_ASSERT(t, small.Height() == 6);
    // Centred
    TR_ASSERT(t, std::abs(small.TopLeft().x - (screenRect.Width() - small.Width() - small.TopLeft().x)) <= 1);
    TR_ASSERT(t, std::abs(small.TopLeft().y - (screenRect.Height() - small.Height() - small.TopLeft().y)) <= 1);
    rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));

    rootView.ShowModal(SaveDiscardCancel(log, U"a much longer first line than the row of choices is wide\nline two\nline three"));
    auto big = rootView.TopModal()->GetViewRect();
    TR_ASSERT(t, big.Width() > small.Width());
    TR_ASSERT(t, big.Height() == small.Height() + 2);
    rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));

    rootView.ShowModal(SaveDiscardCancel(log, std::u32string(500, U'x')));
    auto huge = rootView.TopModal()->GetViewRect();
    TR_ASSERT(t, huge.Width() <= screenRect.Width());
    TR_ASSERT(t, huge.TopLeft().x >= 0);
    rootView.OnAction(UIAction(kUIAction::kUIActionCloseModal));
    return kTR_Pass;
}
