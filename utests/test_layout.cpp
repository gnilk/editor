//
// Created by gnilk on 28.05.2026.
//
#include <testinterface.h>
#include "Core/Views/ViewBase.h"
#include "Core/Views/RootView.h"
#include "Core/Views/HSplitView.h"

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


    auto logger = gnilk::Logger::GetLogger("main");

    auto screen = RuntimeConfig::Instance().GetScreen();
    auto dimensions = screen->Dimensions();


    RootView rootView;

    HSplitView hSplitView;
    rootView.AddView(&hSplitView);

    ViewBase upperView;
    ViewBase lowerView;

    hSplitView.SetUpper(&upperView);
    hSplitView.SetLower(&lowerView);


    printf("---------> Initialize\n");

    rootView.Initialize();

    printf("---------> Layout after initialize\n");
    rootView.DumpLayout(0);


    printf("---------> Change upper height\n");
    upperView.SetHeight(20);
    printf("---------> Reinitialize\n");
    rootView.Initialize();
    printf("---------> Layout after initialize\n");
    rootView.DumpLayout(0);

    return kTR_Pass;
}