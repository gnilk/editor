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

    logger->Debug("---------> Initialize");

    rootView.Initialize();

    logger->Debug("---------> Layout after initialize");
    rootView.DumpLayout(0);


    logger->Debug("---------> Change upper height");
    upperView.SetHeight(20);
    logger->Debug("---------> Reinitialize");
    rootView.Initialize();
    logger->Debug("---------> Layout after initialize");
    rootView.DumpLayout(0);

    return kTR_Pass;
}