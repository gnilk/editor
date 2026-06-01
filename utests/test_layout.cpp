//
// Created by gnilk on 28.05.2026.
//
#include <testinterface.h>
#include "Core/Views/ViewBase.h"
#include "Core/Views/RootView.h"
#include "Core/Views/HSplitView.h"
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