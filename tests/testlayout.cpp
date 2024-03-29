//
// Created by gnilk on 17.02.23.
//
#include "Core/Views/ViewBase.h"
#include "Core/Views/ViewLayout.h"
#include "Core/Views/HSplitView.h"
#include "Core/Views/VSplitView.h"
#include "Core/Views/GutterView.h"
#include "Core/Views/CommandView.h"
#include "Core/Views/EditorView.h"
#include "Core/Views/RootView.h"

using namespace gedit;
int main(int argc, char **argv) {


    RootView rootView;
    rootView.SetCaption("Root");

    HSplitView hSplitView;
    hSplitView.SetCaption("HSplit");
    rootView.AddView(&hSplitView);

    VSplitView vSplitView(Rect(100,70));
    vSplitView.SetCaption("VSplit");
    hSplitView.SetTopView(&vSplitView);

    ViewBase cmdView;
    cmdView.SetCaption("CmdView");
    hSplitView.SetBottomView(&cmdView);


    GutterView gutterView;
    gutterView.SetCaption("Gutter");
    vSplitView.SetLeftView(&gutterView);

    ViewBase editorView;
    editorView.SetCaption("Editor");
    vSplitView.SetRightView(&editorView);

    // Must...
    rootView.Begin();

    // Compute the initial layout based on this rect...
    rootView.ComputeInitialLayout(Rect(100,100));

    rootView.DumpViewTree();

    return -1;
}