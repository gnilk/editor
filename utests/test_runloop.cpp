//
// Runloop action dispatch. DispatchAction is the single chokepoint that routes an already-resolved
// action to the focused handler; the gansi backend uses it to turn a bracketed-paste event into a
// normal PasteFromClipboard action (so a TTY paste inserts exactly like a GUI Cmd+V).
//
#include <testinterface.h>

#include "Core/Runloop.h"
#include "Core/Editor.h"
#include "Core/KeypressAndActionHandler.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_runloop(ITesting *t);
DLL_EXPORT int test_runloop_dispatchaction_to_focused(ITesting *t);
}

namespace {
    // Records the action it was handed and claims to have handled it (so DispatchAction does not fall
    // through to the global handler).
    struct RecordingHandler : KeypressAndActionHandler {
        bool gotAction = false;
        kAction lastAction = kAction::kActionNone;
        bool HandleAction(const EditorAction &kpAction) override {
            gotAction = true;
            lastAction = kpAction.action;
            return true;
        }
    };
}

DLL_EXPORT int test_runloop(ITesting *t) {
    return kTR_Pass;
}

// A resolved action is routed to the focused (top-of-stack) handler.
DLL_EXPORT int test_runloop_dispatchaction_to_focused(ITesting *t) {
    RecordingHandler recorder;
    Runloop::SetKeypressAndActionHook(&recorder);   // push as focused handler

    EditorAction action;
    action.action = kAction::kActionPasteFromClipboard;
    bool dispatched = Runloop::DispatchAction(action);

    TR_ASSERT(t, dispatched);
    TR_ASSERT(t, recorder.gotAction);
    TR_ASSERT(t, recorder.lastAction == kAction::kActionPasteFromClipboard);

    Runloop::SetKeypressAndActionHook(nullptr);     // pop
    return kTR_Pass;
}
