//
// Created by gnilk on 10.06.26.
//
// EditorAction — the resolved-action bundle the keymap hands to the UI. Pulled out of KeyMapping.h
// (Phase 2, the buffer/window split): it does not belong to the keymap. Every action consumer
// (Document, views, controllers) needs this type, but most of them do not need KeyMapping, so it
// lives in its own lightweight header. It depends only on Action.h (kAction / kActionModifier) and
// KeyPress.h (the originating keypress), both of which KeyMapping.h already pulls in — so no new
// include cycle. Naming: 'EditorAction' over 'DocumentAction' (kAction spans view resize/cycle,
// terminal, search and modals — not just documents) and over the abstract 'SemanticAction'. It makes
// the Phase-2 rule self-evident: Document::OnAction(EditorAction) is semantic-action handling, not
// keypress awareness.
//

#ifndef EDITOR_EDITORACTION_H
#define EDITOR_EDITORACTION_H

#include <optional>

#include "Action.h"
#include "KeyPress.h"

namespace gedit {

    //
    // When a keypress is given this is given to the UI from ActionFromKeyPress.
    //
    struct EditorAction {
        kAction action = kAction::kActionNone;     // Key press was mapped to this action
        kUIAction uiAction = kUIAction::kActionNone;     // ...or this one, if it resolved into the toolkit's action space
        std::optional <kActionModifier> actionModifier = {};
        int modifierMask = {};
        KeyPress keyPress = {};  // Underlying keypress (the action is already resolved by dispatch time)
    };

}

#endif //EDITOR_EDITORACTION_H
