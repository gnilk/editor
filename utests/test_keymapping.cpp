//
// Created by gnilk on 08.07.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/API/EditorAPI.h"
#include "Core/KeyMapping.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_keymapping(ITesting *t);
DLL_EXPORT int test_keymapping_parse(ITesting *t);
DLL_EXPORT int test_keymapping_kpaction(ITesting *t);
DLL_EXPORT int test_keymapping_load(ITesting *t);
}

DLL_EXPORT int test_keymapping(ITesting *t) {
    return kTR_Pass;
}

static const std::string strKeymap="{\n"
                                   "  modifiers: {\n"
                                   "    SelectionModifier: KeyCode_Shift,\n"
                                   "    CopyPasteModifier: KeyCode_Cmd,\n"
                                   "    UINavigationModifier : KeyCode_Alt,\n"
                                   "  },\n"
                                   "  actions: {\n"
                                   "    GotoFirstLine : KeyCode_Command + KeyCode_Home + @SelectionModifier,\n"
                                   "  }\n"
                                   "}";
DLL_EXPORT int test_keymapping_parse(ITesting *t) {
    KeyMapping keyMapping;
    auto cfgNode = ConfigNode::FromString(strKeymap);
    TR_ASSERT(t, cfgNode.has_value());
    TR_ASSERT(t, keyMapping.RebuildActionMapping(cfgNode.value()));

    return kTR_Pass;
}

DLL_EXPORT int test_keymapping_kpaction(ITesting *t) {
    KeyMapping keyMapping;
    auto cfgNode = ConfigNode::FromString(strKeymap);
    TR_ASSERT(t, cfgNode.has_value());
    TR_ASSERT(t, keyMapping.RebuildActionMapping(cfgNode.value()));

    KeyPress keyPress = {};
    keyPress.key = Keyboard::kKeyCode_Home;
    keyPress.specialKey = Keyboard::kKeyCode_Home;
    keyPress.modifiers = Keyboard::kMod_LeftCommand;
    keyPress.isKeyValid = true;
    keyPress.isSpecialKey = true;

    auto action = keyMapping.ActionFromKeyPress(keyPress);
    TR_ASSERT(t, action.has_value());
    TR_ASSERT(t, !action->actionModifier.has_value());

    keyPress.modifiers |= Keyboard::kMod_LeftShift;
    action = keyMapping.ActionFromKeyPress(keyPress);
    TR_ASSERT(t, action.has_value());
    TR_ASSERT(t, action.value().actionModifier.has_value());
    TR_ASSERT(t, action->actionModifier.value() == kActionModifier::kActionModifierSelection);

    return kTR_Pass;
}

DLL_EXPORT int test_keymapping_load(ITesting *t) {
    auto keymap = Editor::Instance().GetKeyMapping("default_keymap");
    TR_ASSERT(t, keymap != nullptr);
    return kTR_Pass;
}
