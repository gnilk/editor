//
// Created by gnilk on 28.05.2026.
//
#include <testinterface.h>
#include "Core/Views/ViewBase.h"
#include "Core/Views/RootView.h"
#include "Core/Views/HSplitView.h"
#include "Core/Views/VSplitView.h"
#include "logger.h"

using namespace gedit;

extern "C" int test_layout(ITesting *t) {
    // We need an openscreen for this to work
    Editor::Instance().OpenScreen();
    // Note: This can be implicit
    RuntimeConfig::Instance().SetMainThreadID();
    return kTR_Pass;
}

extern "C" int test_layout_simple(ITesting *t) {

    //TestKeyBoardDriver();


    auto screen = RuntimeConfig::Instance().GetScreen();
    auto dimensions = screen->Dimensions();


    RootView rootView;

    HSplitView hSplitView;
    rootView.AddView(&hSplitView);

    ViewBase upperView;
    ViewBase lowerView;

    hSplitView.SetUpper(&upperView);
    hSplitView.SetLower(&lowerView);

    auto logger = gnilk::Logger::GetLogger("Layout");


    rootView.Initialize();
    auto h = upperView.GetHeight();
    TR_ASSERT(t, upperView.GetHeight() == 50);

    rootView.DumpLayout(0);
    KeyPressAction kpAction;
    kpAction.action = kAction::kActionIncreaseViewHeight;
    upperView.OnAction(kpAction);
    //upperView.SetHeight(20);
    rootView.Initialize();
    rootView.DumpLayout(0);

    TR_ASSERT(t, upperView.GetHeight() == 51);

    return kTR_Pass;
}

// ---------------------------------------------------------------------------------------------------
// Splitter bounds (regression: increase/decrease height/width had no clamp, so the splitter - and the
// status line drawn on it - could be pushed off-screen, or a view given a negative extent).
// We drive the action far past the screen size and assert the splitter stays on-screen with both
// views keeping a positive extent.
// ---------------------------------------------------------------------------------------------------

static KeyPressAction MakeAction(kAction a) {
    KeyPressAction kp;
    kp.action = a;
    return kp;
}

// Increasing height must never push the splitter (status bar) past the bottom of the content area.
extern "C" int test_layout_height_max(ITesting *t) {
    RootView rootView;
    HSplitView hSplitView;
    rootView.AddView(&hSplitView);
    ViewBase upperView;
    ViewBase lowerView;
    hSplitView.SetUpper(&upperView);
    hSplitView.SetLower(&lowerView);
    rootView.Initialize();

    int contentH = hSplitView.GetContentRect().Height();
    TR_ASSERT(t, contentH > 0);

    // Hammer the increase action well past the screen height.
    auto kp = MakeAction(kAction::kActionIncreaseViewHeight);
    for (int i = 0; i < contentH + 50; i++) {
        upperView.OnAction(kp);
    }
    rootView.Initialize();

    int sp = hSplitView.GetSplitterPos();
    fprintf(stderr, "  height_max: contentH=%d splitterPos=%d upper=%d lower=%d\n",
            contentH, sp, upperView.GetHeight(), lowerView.GetHeight());

    // Splitter row (carrying the status line) stays strictly inside the content area...
    TR_ASSERT(t, sp > 0);
    TR_ASSERT(t, sp < contentH);
    // ...and the lower view is never collapsed to <= 0 (which would mean a negative rect).
    TR_ASSERT(t, lowerView.GetHeight() > 0);
    TR_ASSERT(t, upperView.GetHeight() < contentH);

    return kTR_Pass;
}

// Regression: a window resize (corner-drag) ratcheted the HSplit splitter to the bottom, so the
// editor filled the screen and the terminal collapsed. Cause: ReInitView adopted the upper view's
// live height as the new splitterPos, misreading a resize-driven relayout of the upper view as a
// deliberate splitter move. Here we reproduce that exact trigger - force the upper child to fill the
// content height, then re-initialize - and assert the splitter does NOT follow it (splitterPos is the
// authority; child heights are derived from it). We run several cycles to catch any ratchet/drift.
extern "C" int test_layout_hsplit_resize_stability(ITesting *t) {
    RootView rootView;
    HSplitView hSplitView;
    rootView.AddView(&hSplitView);
    ViewBase upperView;
    ViewBase lowerView;
    hSplitView.SetUpper(&upperView);
    hSplitView.SetLower(&lowerView);
    rootView.Initialize();

    int contentH = hSplitView.GetContentRect().Height();
    TR_ASSERT(t, contentH > 0);
    int spInitial = hSplitView.GetSplitterPos();

    for (int cycle = 0; cycle < 5; cycle++) {
        // Mimic the resize traversal independently relaying the upper view out to fill the content.
        upperView.SetHeight(contentH);
        rootView.Initialize();

        int sp = hSplitView.GetSplitterPos();
        fprintf(stderr, "  resize_stability cycle %d: contentH=%d splitterPos=%d upper=%d lower=%d\n",
                cycle, contentH, sp, upperView.GetHeight(), lowerView.GetHeight());

        // The splitter must stay where the user left it - never adopt the upper view's fill height.
        TR_ASSERT(t, sp == spInitial);
        // Both views keep a positive extent; the splitter (status line) stays on-screen.
        TR_ASSERT(t, sp > 0);
        TR_ASSERT(t, sp < contentH);
        TR_ASSERT(t, lowerView.GetHeight() > 0);
    }

    return kTR_Pass;
}

// Same regression as above, on the VSplit (workspace | editor) width axis: a corner-drag must not let
// the vertical divider adopt a child's resize-driven width and ratchet into an edge.
extern "C" int test_layout_vsplit_resize_stability(ITesting *t) {
    RootView rootView;
    VSplitView vSplitView;
    rootView.AddView(&vSplitView);
    ViewBase leftView;
    ViewBase rightView;
    vSplitView.SetLeft(&leftView);
    vSplitView.SetRight(&rightView);
    rootView.Initialize();

    int contentW = vSplitView.GetViewRect().Width();
    TR_ASSERT(t, contentW > 0);
    int spInitial = vSplitView.GetSplitterPos();

    for (int cycle = 0; cycle < 5; cycle++) {
        leftView.SetWidth(contentW);            // mimic the resize traversal relaying the left view to fill
        rootView.Initialize();

        int sp = vSplitView.GetSplitterPos();
        TR_ASSERT(t, sp == spInitial);
        TR_ASSERT(t, sp > 0);
        TR_ASSERT(t, sp < contentW);
        TR_ASSERT(t, rightView.GetWidth() > 0);
    }

    return kTR_Pass;
}

// Decreasing height must never drive the splitter to/below zero.
extern "C" int test_layout_height_min(ITesting *t) {
    RootView rootView;
    HSplitView hSplitView;
    rootView.AddView(&hSplitView);
    ViewBase upperView;
    ViewBase lowerView;
    hSplitView.SetUpper(&upperView);
    hSplitView.SetLower(&lowerView);
    rootView.Initialize();

    int contentH = hSplitView.GetContentRect().Height();

    auto kp = MakeAction(kAction::kActionDecreaseViewHeight);
    for (int i = 0; i < contentH + 50; i++) {
        upperView.OnAction(kp);
    }
    rootView.Initialize();

    int sp = hSplitView.GetSplitterPos();
    fprintf(stderr, "  height_min: contentH=%d splitterPos=%d upper=%d lower=%d\n",
            contentH, sp, upperView.GetHeight(), lowerView.GetHeight());

    TR_ASSERT(t, sp > 0);
    TR_ASSERT(t, upperView.GetHeight() > 0);
    TR_ASSERT(t, lowerView.GetHeight() < contentH);

    return kTR_Pass;
}

// Increasing width must never push the divider past the right edge (right panel negative width).
extern "C" int test_layout_width_max(ITesting *t) {
    RootView rootView;
    VSplitView vSplitView;
    rootView.AddView(&vSplitView);
    ViewBase leftView;
    ViewBase rightView;
    vSplitView.SetLeft(&leftView);
    vSplitView.SetRight(&rightView);
    rootView.Initialize();

    int contentW = vSplitView.GetViewRect().Width();
    TR_ASSERT(t, contentW > 0);

    auto kp = MakeAction(kAction::kActionIncreaseViewWidth);
    for (int i = 0; i < contentW + 50; i++) {
        leftView.OnAction(kp);
    }
    rootView.Initialize();

    int sp = vSplitView.GetSplitterPos();
    fprintf(stderr, "  width_max: contentW=%d splitterPos=%d left=%d right=%d\n",
            contentW, sp, leftView.GetWidth(), rightView.GetWidth());

    TR_ASSERT(t, sp > 0);
    TR_ASSERT(t, sp < contentW);
    TR_ASSERT(t, rightView.GetWidth() > 0);
    TR_ASSERT(t, leftView.GetWidth() < contentW);

    return kTR_Pass;
}

// Decreasing width must never drive the divider to/below zero (left panel negative width).
extern "C" int test_layout_width_min(ITesting *t) {
    RootView rootView;
    VSplitView vSplitView;
    rootView.AddView(&vSplitView);
    ViewBase leftView;
    ViewBase rightView;
    vSplitView.SetLeft(&leftView);
    vSplitView.SetRight(&rightView);
    rootView.Initialize();

    int contentW = vSplitView.GetViewRect().Width();

    auto kp = MakeAction(kAction::kActionDecreaseViewWidth);
    for (int i = 0; i < contentW + 50; i++) {
        leftView.OnAction(kp);
    }
    rootView.Initialize();

    int sp = vSplitView.GetSplitterPos();
    fprintf(stderr, "  width_min: contentW=%d splitterPos=%d left=%d right=%d\n",
            contentW, sp, leftView.GetWidth(), rightView.GetWidth());

    TR_ASSERT(t, sp > 0);
    TR_ASSERT(t, leftView.GetWidth() > 0);
    TR_ASSERT(t, rightView.GetWidth() < contentW);

    return kTR_Pass;
}

// ---------------------------------------------------------------------------------------------------
// Nested delegation (regression: matches the real app layout HSplit[ VSplit[...], terminal ]). A
// height action fired from a view DEEP inside the VSplitView must bubble up to the HSplitView that
// actually owns the divide - a vertical splitter only manages width. Before the fix the action died
// at the VSplitView (ViewBase::SetHeight on itself), so the HSplit divide never moved and the status
// bar could be pushed off-screen by growing the inner view unbounded instead.
// ---------------------------------------------------------------------------------------------------

// Height fired from inside a VSplitView reaches the enclosing HSplitView and clamps there.
extern "C" int test_layout_nested_height(ITesting *t) {
    RootView rootView;
    HSplitView hSplitView;
    rootView.AddView(&hSplitView);

    VSplitView vSplitView;       // upper half is itself a left|right split
    ViewBase leftView;
    ViewBase rightView;
    ViewBase lowerView;
    hSplitView.SetUpper(&vSplitView);
    hSplitView.SetLower(&lowerView);
    vSplitView.SetLeft(&leftView);
    vSplitView.SetRight(&rightView);
    rootView.Initialize();

    int contentH = hSplitView.GetContentRect().Height();
    int initialSp = hSplitView.GetSplitterPos();
    TR_ASSERT(t, contentH > 0);

    // Fire the height action from the deeply-nested right pane.
    auto kp = MakeAction(kAction::kActionIncreaseViewHeight);
    for (int i = 0; i < contentH + 50; i++) {
        rightView.OnAction(kp);
    }
    rootView.Initialize();

    int sp = hSplitView.GetSplitterPos();
    fprintf(stderr, "  nested_height: contentH=%d initialSp=%d splitterPos=%d lower=%d\n",
            contentH, initialSp, sp, lowerView.GetHeight());

    // The action must have reached the HSplitView (divide moved past its start) ...
    TR_ASSERT(t, sp > initialSp);
    // ... travelled to the clamp region near the bottom ...
    TR_ASSERT(t, sp >= contentH - 10);
    // ... but never off-screen, and the lower view (status/terminal) stays positive.
    TR_ASSERT(t, sp < contentH);
    TR_ASSERT(t, lowerView.GetHeight() > 0);

    return kTR_Pass;
}

// Width fired from inside an HSplitView reaches the enclosing VSplitView and clamps there.
extern "C" int test_layout_nested_width(ITesting *t) {
    RootView rootView;
    VSplitView vSplitView;
    rootView.AddView(&vSplitView);

    ViewBase leftView;
    HSplitView hSplitView;       // right half is itself an upper|lower split
    ViewBase upperView;
    ViewBase lowerView;
    vSplitView.SetLeft(&leftView);
    vSplitView.SetRight(&hSplitView);
    hSplitView.SetUpper(&upperView);
    hSplitView.SetLower(&lowerView);
    rootView.Initialize();

    int contentW = vSplitView.GetViewRect().Width();
    int initialSp = vSplitView.GetSplitterPos();
    TR_ASSERT(t, contentW > 0);

    auto kp = MakeAction(kAction::kActionIncreaseViewWidth);
    for (int i = 0; i < contentW + 50; i++) {
        upperView.OnAction(kp);
    }
    rootView.Initialize();

    int sp = vSplitView.GetSplitterPos();
    fprintf(stderr, "  nested_width: contentW=%d initialSp=%d splitterPos=%d left=%d\n",
            contentW, initialSp, sp, leftView.GetWidth());

    TR_ASSERT(t, sp > initialSp);
    TR_ASSERT(t, sp >= contentW - 10);
    TR_ASSERT(t, sp < contentW);
    TR_ASSERT(t, leftView.GetWidth() > 0);

    return kTR_Pass;
}