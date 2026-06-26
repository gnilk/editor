//
// Created by gnilk on 27.06.26.
//
// Detects multi-click gestures (double-click) on top of raw mouse Press events.
// Pure logic: no SDL, no singletons, no Config dependency — the threshold is injected so the class
// stays deterministic and unit-testable over fabricated timestamps (see docs/mouse-dblclick.md WP2).
//
// This is the single hook point for any future click semantics: triple-click (keep counting past 2
// instead of resetting), or synthesized Click/DoubleClick event kinds, would be a local change here.
//

#ifndef GOATEDIT_MOUSECLICKTRACKER_H
#define GOATEDIT_MOUSECLICKTRACKER_H

#include <chrono>

namespace gedit {
    class MouseClickTracker {
    public:
        using TimePoint = std::chrono::steady_clock::time_point;
        using DurationMS = std::chrono::milliseconds;

        // SDL_BUTTON_LEFT == 1; only the left button participates in double-click detection.
        static constexpr int kLeftButton = 1;
    public:
        MouseClickTracker() = default;
        explicit MouseClickTracker(DurationMS threshold) : threshold(threshold) {}
        virtual ~MouseClickTracker() = default;

        void SetThreshold(DurationMS newThreshold) {
            threshold = newThreshold;
        }
        DurationMS GetThreshold() const {
            return threshold;
        }

        // Register a press and return its click count: 1 = single, 2 = double.
        // A press is the SECOND click (=> 2) when it lands on the same cell, with the same (left)
        // button, within the threshold of the previous press. After emitting a 2 the tracker resets,
        // so a third quick click counts as 1 again (sequence is 1,2,1,2… never a runaway 2,2,2).
        int RegisterPress(TimePoint now, int x, int y, int button);

    private:
        DurationMS threshold = std::chrono::milliseconds(250);
        bool hasLastPress = false;
        TimePoint lastPress = {};
        int lastX = 0;
        int lastY = 0;
        int lastButton = 0;
    };
}

#endif //GOATEDIT_MOUSECLICKTRACKER_H
