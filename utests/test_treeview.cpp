//
// Created by gnilk on 18.06.26.
//
// Unit tests for the generic TreeView<T> data-manipulation layer (FS-6 W4).
// We test over TreeView<int> without a display or Editor singleton — Flatten(),
// AddItem(), and Expand() all operate on in-memory data structures only, so no
// window/UIHost initialisation is required. Flatten() is protected; TestTree
// exposes it for assertion purposes.
//
#include <testinterface.h>
#include <string>
#include "Core/UI/Views/TreeView.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_treeview(ITesting *t);
DLL_EXPORT int test_treeview_unfetched_glyph(ITesting *t);
DLL_EXPORT int test_treeview_fetch_fires_once(ITesting *t);
DLL_EXPORT int test_treeview_fetch_reentrant_additem(ITesting *t);
DLL_EXPORT int test_treeview_fetch_noop_reexpand(ITesting *t);
DLL_EXPORT int test_treeview_additem_no_flatten(ITesting *t);
}

namespace {
    // Thin subclass that exposes the protected Flatten() for test assertions.
    struct TestTree : TreeView<int> {
        void Reflatten() { Flatten(); }
    };
}

DLL_EXPORT int test_treeview(ITesting *t) {
    return kTR_Pass;
}

// A node with hasUnfetchedChildren==true shows '+'; ==false and no children shows ' '.
DLL_EXPORT int test_treeview_unfetched_glyph(ITesting *t) {
    TestTree tree;
    tree.SetToStringDelegate([](int i) { return std::to_string(i); });

    auto node = tree.AddItem(42);

    // Default: no children, no unfetched → leaf glyph ' '.
    TR_ASSERT(t, !node->drawString.empty());
    TR_ASSERT(t, node->drawString[0] == ' ');

    // Set the flag and re-flatten → glyph switches to '+'.
    node->hasUnfetchedChildren = true;
    tree.Reflatten();
    TR_ASSERT(t, node->drawString[0] == '+');

    // Clear the flag again → back to leaf.
    node->hasUnfetchedChildren = false;
    tree.Reflatten();
    TR_ASSERT(t, node->drawString[0] == ' ');

    return kTR_Pass;
}

// Expand() on a node with hasUnfetchedChildren fires cbFetchChildrenForNode exactly
// once, clears the flag, and marks the node expanded.
DLL_EXPORT int test_treeview_fetch_fires_once(ITesting *t) {
    TestTree tree;
    tree.SetToStringDelegate([](int i) { return std::to_string(i); });

    auto node = tree.AddItem(42);
    node->hasUnfetchedChildren = true;
    tree.Reflatten();

    int callCount = 0;
    tree.cbFetchChildrenForNode = [&](TestTree::TreeNode::Ref /*n*/) {
        callCount++;
    };

    tree.GetLineCursor().idxActiveLine = 0;
    tree.Expand();

    TR_ASSERT(t, callCount == 1);
    TR_ASSERT(t, !node->hasUnfetchedChildren);   // cleared by Expand before calling
    TR_ASSERT(t, node->isExpanded);

    return kTR_Pass;
}

// The callback may call AddItem(node, child), which triggers Flatten() internally.
// Expand() holds the node by value so the re-entrant Flatten doesn't dangle.
DLL_EXPORT int test_treeview_fetch_reentrant_additem(ITesting *t) {
    TestTree tree;
    tree.SetToStringDelegate([](int i) { return std::to_string(i); });

    auto node = tree.AddItem(1);
    node->hasUnfetchedChildren = true;
    tree.Reflatten();

    tree.cbFetchChildrenForNode = [&](TestTree::TreeNode::Ref n) {
        // AddItem calls Flatten internally — this is the re-entrancy the by-value
        // copy in Expand() is there to guard against.
        tree.AddItem(n, 2);
        tree.AddItem(n, 3);
    };

    tree.GetLineCursor().idxActiveLine = 0;
    tree.Expand();   // must not crash

    TR_ASSERT(t, node->children.size() == 2);
    TR_ASSERT(t, node->isExpanded);
    TR_ASSERT(t, node->drawString[0] == '-');   // expanded + has children

    return kTR_Pass;
}

// Re-expanding a node whose hasUnfetchedChildren is already false does NOT fire the
// callback again. The already-populated children remain.
DLL_EXPORT int test_treeview_fetch_noop_reexpand(ITesting *t) {
    TestTree tree;
    tree.SetToStringDelegate([](int i) { return std::to_string(i); });

    auto node = tree.AddItem(10);
    node->hasUnfetchedChildren = true;
    tree.Reflatten();

    int callCount = 0;
    tree.cbFetchChildrenForNode = [&](TestTree::TreeNode::Ref n) {
        callCount++;
        tree.AddItemNoFlatten(n, 20);
    };

    tree.GetLineCursor().idxActiveLine = 0;
    tree.Expand();
    TR_ASSERT(t, callCount == 1);

    // Collapse and re-expand — flag is already false; callback must NOT fire.
    tree.Collapse();
    tree.GetLineCursor().idxActiveLine = 0;
    tree.Expand();
    TR_ASSERT(t, callCount == 1);   // still 1
    TR_ASSERT(t, node->children.size() == 1);

    return kTR_Pass;
}

// AddItemNoFlatten adds a child without triggering Flatten, so it's safe to call
// many times inside cbFetchChildrenForNode; Expand() calls Flatten once at the end.
DLL_EXPORT int test_treeview_additem_no_flatten(ITesting *t) {
    TestTree tree;
    tree.SetToStringDelegate([](int i) { return std::to_string(i); });

    auto parent = tree.AddItem(0);

    // Add three children without intermediate Flatten calls.
    tree.AddItemNoFlatten(parent, 1);
    tree.AddItemNoFlatten(parent, 2);
    tree.AddItemNoFlatten(parent, 3);

    // flattenNodeList is stale here (only contains the parent); trigger a Flatten.
    tree.Reflatten();

    TR_ASSERT(t, parent->children.size() == 3);
    // Parent has children → glyph '+' (collapsed).
    TR_ASSERT(t, parent->drawString[0] == '+');

    return kTR_Pass;
}
