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
DLL_EXPORT int test_keymapping_inherit(ITesting *t);
DLL_EXPORT int test_keymapping_inherit_empty_actions(ITesting *t);
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

//
// Inheritance test - relies on the distribution assets.
// 'terminal_keymap' declares 'inherit: default_keymap' and binds Tab to ShellCompletion. We assert:
//  1) the inherited bindings are present (DownArrow -> LineDown is defined ONLY in default_keymap)
//  2) the child overrides the parent on a conflict (default_keymap binds Tab to Indent, the child
//     re-binds Tab to ShellCompletion - and since the child is parsed first, first-match must win)
//
// NOTE: terminal_keymap currently only exists on Linux (config.yml maps it under the 'linux'
// section). This test is deliberately NOT guarded by GEDIT_LINUX - it should FAIL when porting to a
// new platform so the missing keymap asset is caught early rather than silently skipped.
//
DLL_EXPORT int test_keymapping_inherit(ITesting *t) {
    auto keymap = Editor::Instance().GetKeyMapping("terminal_keymap");
    TR_ASSERT(t, keymap != nullptr);

    // Inherited from default_keymap (not declared in terminal_keymap)
    KeyPress down = {};
    down.key = Keyboard::kKeyCode_DownArrow;
    down.specialKey = Keyboard::kKeyCode_DownArrow;
    down.isKeyValid = true;
    down.isSpecialKey = true;

    auto inherited = keymap->ActionFromKeyPress(down);
    TR_ASSERT(t, inherited.has_value());
    TR_ASSERT(t, inherited->action == kAction::kActionLineDown);

    // Child overrides parent: Tab is Indent in default_keymap but ShellCompletion in terminal_keymap
    KeyPress tab = {};
    tab.key = Keyboard::kKeyCode_Tab;
    tab.specialKey = Keyboard::kKeyCode_Tab;
    tab.isKeyValid = true;
    tab.isSpecialKey = true;

    auto overridden = keymap->ActionFromKeyPress(tab);
    TR_ASSERT(t, overridden.has_value());
    TR_ASSERT(t, overridden->action == kAction::kActionShellCompletion);

    return kTR_Pass;
}

//
// A pure-inheritance keymap has NO 'actions' of its own - it only re-uses its parent's bindings.
// This used to be rejected outright ("Keymap must at least have an action section"); after the
// empty-actions relaxation it must initialize cleanly and resolve to exactly the parent's bindings.
// This is what lets a view keymap collapse to just `inherit: default_keymap`.
//
static const std::string strEmptyInherit = "{\n"
                                            "  inherit: default_keymap\n"
                                            "}";
DLL_EXPORT int test_keymapping_inherit_empty_actions(ITesting *t) {
    KeyMapping keyMapping;
    auto cfgNode = ConfigNode::FromString(strEmptyInherit);
    TR_ASSERT(t, cfgNode.has_value());

    // No 'actions' section, but 'inherit' is present -> must initialize without error.
    TR_ASSERT(t, keyMapping.Initialize(cfgNode.value()));

    // Every binding resolves through the inherited parent (default_keymap binds DownArrow -> LineDown)
    KeyPress down = {};
    down.key = Keyboard::kKeyCode_DownArrow;
    down.specialKey = Keyboard::kKeyCode_DownArrow;
    down.isKeyValid = true;
    down.isSpecialKey = true;

    auto inherited = keyMapping.ActionFromKeyPress(down);
    TR_ASSERT(t, inherited.has_value());
    TR_ASSERT(t, inherited->action == kAction::kActionLineDown);

    return kTR_Pass;
}
