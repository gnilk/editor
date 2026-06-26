//
// Created by gnilk on 27.06.26.
//
// Unit tests for MouseClickTracker (docs/mouse-dblclick.md WP2/WP6). The tracker is pure logic
// over injected timestamps — no SDL, no Runloop, no Config — so double-click detection is exercised
// deterministically without real sleeping.
//
#include <testinterface.h>
#include <chrono>
#include "Core/MouseClickTracker.h"

using namespace gedit;
using namespace std::chrono;

extern "C" {
DLL_EXPORT int test_mouseclicktracker(ITesting *t);
DLL_EXPORT int test_mouseclicktracker_double_same_cell(ITesting *t);
DLL_EXPORT int test_mouseclicktracker_too_slow(ITesting *t);
DLL_EXPORT int test_mouseclicktracker_different_cell(ITesting *t);
DLL_EXPORT int test_mouseclicktracker_different_button(ITesting *t);
DLL_EXPORT int test_mouseclicktracker_triple_resets(ITesting *t);
DLL_EXPORT int test_mouseclicktracker_nonleft_button(ITesting *t);
}

namespace {
    // A fixed epoch so each case builds its own deterministic timeline by adding ms offsets.
    static const MouseClickTracker::TimePoint kT0 = MouseClickTracker::TimePoint{};

    static MouseClickTracker::TimePoint At(int ms) {
        return kT0 + milliseconds(ms);
    }
}

DLL_EXPORT int test_mouseclicktracker(ITesting *t) {
    return kTR_Pass;
}

// Two left-button presses on the same cell, within the threshold → second is a double-click.
DLL_EXPORT int test_mouseclicktracker_double_same_cell(ITesting *t) {
    MouseClickTracker tracker(milliseconds(250));
    int first  = tracker.RegisterPress(At(0),   10, 5, MouseClickTracker::kLeftButton);
    int second = tracker.RegisterPress(At(100), 10, 5, MouseClickTracker::kLeftButton);
    TR_ASSERT(t, first == 1);
    TR_ASSERT(t, second == 2);
    return kTR_Pass;
}

// Second press lands AFTER the threshold elapses → counts as a fresh single click.
DLL_EXPORT int test_mouseclicktracker_too_slow(ITesting *t) {
    MouseClickTracker tracker(milliseconds(250));
    int first  = tracker.RegisterPress(At(0),   10, 5, MouseClickTracker::kLeftButton);
    int second = tracker.RegisterPress(At(300), 10, 5, MouseClickTracker::kLeftButton);
    TR_ASSERT(t, first == 1);
    TR_ASSERT(t, second == 1);
    return kTR_Pass;
}

// Within the window but on a different cell → single click (you clicked two distinct rows fast).
DLL_EXPORT int test_mouseclicktracker_different_cell(ITesting *t) {
    MouseClickTracker tracker(milliseconds(250));
    int first  = tracker.RegisterPress(At(0),   10, 5, MouseClickTracker::kLeftButton);
    int second = tracker.RegisterPress(At(50),  10, 6, MouseClickTracker::kLeftButton);
    TR_ASSERT(t, first == 1);
    TR_ASSERT(t, second == 1);
    return kTR_Pass;
}

// Within the window, same cell, but a different button → single click.
DLL_EXPORT int test_mouseclicktracker_different_button(ITesting *t) {
    MouseClickTracker tracker(milliseconds(250));
    int first  = tracker.RegisterPress(At(0),   10, 5, MouseClickTracker::kLeftButton);
    int second = tracker.RegisterPress(At(50),  10, 5, MouseClickTracker::kLeftButton + 1);
    TR_ASSERT(t, first == 1);
    TR_ASSERT(t, second == 1);
    return kTR_Pass;
}

// Three quick clicks on the same cell → 1, 2, 1 (reset after the double — no runaway 2,2,2).
DLL_EXPORT int test_mouseclicktracker_triple_resets(ITesting *t) {
    MouseClickTracker tracker(milliseconds(250));
    int c1 = tracker.RegisterPress(At(0),   10, 5, MouseClickTracker::kLeftButton);
    int c2 = tracker.RegisterPress(At(80),  10, 5, MouseClickTracker::kLeftButton);
    int c3 = tracker.RegisterPress(At(160), 10, 5, MouseClickTracker::kLeftButton);
    TR_ASSERT(t, c1 == 1);
    TR_ASSERT(t, c2 == 2);
    TR_ASSERT(t, c3 == 1);
    return kTR_Pass;
}

// A non-left button never doubles, even on the same cell within the window.
DLL_EXPORT int test_mouseclicktracker_nonleft_button(ITesting *t) {
    MouseClickTracker tracker(milliseconds(250));
    int right = MouseClickTracker::kLeftButton + 2;
    int first  = tracker.RegisterPress(At(0),  10, 5, right);
    int second = tracker.RegisterPress(At(50), 10, 5, right);
    TR_ASSERT(t, first == 1);
    TR_ASSERT(t, second == 1);
    return kTR_Pass;
}
