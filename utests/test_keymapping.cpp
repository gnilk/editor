//
// Created by gnilk on 08.07.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/API/EditorAPI.h"
#include "Core/KeyMapping.h"
#include "Core/KeyMappingCache.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_keymapping(ITesting *t);
DLL_EXPORT int test_keymapping_parse(ITesting *t);
DLL_EXPORT int test_keymapping_kpaction(ITesting *t);
DLL_EXPORT int test_keymapping_load(ITesting *t);
DLL_EXPORT int test_keymapping_inherit(ITesting *t);
DLL_EXPORT int test_keymapping_inherit_empty_actions(ITesting *t);
DLL_EXPORT int test_keymapping_inherit_multilevel(ITesting *t);
DLL_EXPORT int test_keymapping_workspace_inherit(ITesting *t);
DLL_EXPORT int test_keymapping_cache(ITesting *t);
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

//
// Multi-level inheritance: child -> terminal_keymap -> default_keymap (a 3-level chain, so the
// ResolveInheritance walk runs twice). The child is loaded from MEMORY (ConfigNode::FromString) and
// inherits the real terminal_keymap asset, which itself inherits default_keymap. We assert across
// all three levels:
//   L3 (default, deepest): DownArrow -> LineDown   (only default_keymap defines it -> walk reached bottom)
//   L2 (terminal beats default): Tab -> ShellCompletion (terminal overrides default's Indent across a hop)
//   L1 (child beats all): Escape -> CommitLine      (child overrides the ancestors' EnterCommandMode)
//
// NOTE: only the CHILD can come from memory - inherited parents are resolved by name via the asset
// loader (LoadKeymapConfig), so the ancestors must be real loadable keymaps.
//
static const std::string strMultiLevelChild = "{\n"
                                              "  inherit: terminal_keymap,\n"
                                              "  actions: {\n"
                                              "    CommitLine: KeyCode_Escape,\n"
                                              "  }\n"
                                              "}";
DLL_EXPORT int test_keymapping_inherit_multilevel(ITesting *t) {
    KeyMapping keyMapping;
    auto cfgNode = ConfigNode::FromString(strMultiLevelChild);
    TR_ASSERT(t, cfgNode.has_value());
    TR_ASSERT(t, keyMapping.Initialize(cfgNode.value()));

    // L3 - reaches the deepest ancestor (default_keymap): DownArrow -> LineDown
    KeyPress down = {};
    down.key = Keyboard::kKeyCode_DownArrow;
    down.specialKey = Keyboard::kKeyCode_DownArrow;
    down.isKeyValid = true;
    down.isSpecialKey = true;
    auto lineDown = keyMapping.ActionFromKeyPress(down);
    TR_ASSERT(t, lineDown.has_value());
    TR_ASSERT(t, lineDown->action == kAction::kActionLineDown);

    // L2 - nearer ancestor wins over a more distant one: terminal_keymap's Tab -> ShellCompletion
    // overrides default_keymap's Tab -> Indent (the child does not bind Tab).
    KeyPress tab = {};
    tab.key = Keyboard::kKeyCode_Tab;
    tab.specialKey = Keyboard::kKeyCode_Tab;
    tab.isKeyValid = true;
    tab.isSpecialKey = true;
    auto tabAction = keyMapping.ActionFromKeyPress(tab);
    TR_ASSERT(t, tabAction.has_value());
    TR_ASSERT(t, tabAction->action == kAction::kActionShellCompletion);

    // L1 - the child overrides every ancestor: Escape -> CommitLine (ancestors bind EnterCommandMode)
    KeyPress esc = {};
    esc.key = Keyboard::kKeyCode_Escape;
    esc.specialKey = Keyboard::kKeyCode_Escape;
    esc.isKeyValid = true;
    esc.isSpecialKey = true;
    auto escAction = keyMapping.ActionFromKeyPress(esc);
    TR_ASSERT(t, escAction.has_value());
    TR_ASSERT(t, escAction->action == kAction::kActionCommitLine);

    return kTR_Pass;
}

//
// Loads the REAL workspace_keymap asset (OS-resolved). It is a (mostly) pure-inheritance keymap:
// on Linux it declares no bindings of its own and inherits default_keymap wholesale; on macOS it
// adds a single CycleActiveViewNext override. Either way it must load and resolve inherited
// bindings. Like test_keymapping_inherit this is deliberately NOT platform-guarded - it should fail
// loudly if the workspace keymap is missing/broken on the build platform.
//
DLL_EXPORT int test_keymapping_workspace_inherit(ITesting *t) {
    auto keymap = Editor::Instance().GetKeyMapping("workspace_keymap");
    TR_ASSERT(t, keymap != nullptr);

    // Inherited from default_keymap (the workspace map declares no nav bindings of its own)
    KeyPress down = {};
    down.key = Keyboard::kKeyCode_DownArrow;
    down.specialKey = Keyboard::kKeyCode_DownArrow;
    down.isKeyValid = true;
    down.isSpecialKey = true;

    auto inherited = keymap->ActionFromKeyPress(down);
    TR_ASSERT(t, inherited.has_value());
    TR_ASSERT(t, inherited->action == kAction::kActionLineDown);

    return kTR_Pass;
}

//
// KeyMappingCache: the single source of truth for keymap loading. Asserts (1) a name resolves to the
// SAME instance on repeated calls (built once, cached), (2) inheritance is resolved THROUGH the cache
// so a child shares the very same parent instance, and (3) the resolved-parent path produces the same
// bindings as before (terminal overrides default's Tab; default's DownArrow is inherited).
//
DLL_EXPORT int test_keymapping_cache(ITesting *t) {
    auto &cache = KeyMappingCache::Instance();

    // (1) built once - same instance returned
    auto a = cache.GetOrLoad("default_keymap");
    auto b = cache.GetOrLoad("default_keymap");
    TR_ASSERT(t, a != nullptr);
    TR_ASSERT(t, a.get() == b.get());
    TR_ASSERT(t, cache.Has("default_keymap"));

    // (2) the parent a child inherits is the very same cached default_keymap instance
    auto term = cache.GetOrLoad("terminal_keymap");
    TR_ASSERT(t, term != nullptr);
    TR_ASSERT(t, term.get() != a.get());
    TR_ASSERT(t, cache.GetOrLoad("default_keymap").get() == a.get());

    // (3) bindings resolve correctly through the cache: child overrides parent...
    KeyPress tab = {};
    tab.key = Keyboard::kKeyCode_Tab;
    tab.specialKey = Keyboard::kKeyCode_Tab;
    tab.isKeyValid = true;
    tab.isSpecialKey = true;
    auto tabAction = term->ActionFromKeyPress(tab);
    TR_ASSERT(t, tabAction.has_value());
    TR_ASSERT(t, tabAction->action == kAction::kActionShellCompletion);

    // ...and the inherited binding is present
    KeyPress down = {};
    down.key = Keyboard::kKeyCode_DownArrow;
    down.specialKey = Keyboard::kKeyCode_DownArrow;
    down.isKeyValid = true;
    down.isSpecialKey = true;
    auto lineDown = term->ActionFromKeyPress(down);
    TR_ASSERT(t, lineDown.has_value());
    TR_ASSERT(t, lineDown->action == kAction::kActionLineDown);

    return kTR_Pass;
}
