//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_VSPLITVIEW_H
#define NCWIN_VSPLITVIEW_H

#include "ViewBase.h"
#include "Core/RuntimeConfig.h"

namespace gedit {
    // Vertical splitter — left | right panels separated by a vertical divider
    class VSplitView : public ViewBase {
    public:
        VSplitView() = default;
        explicit VSplitView(const Rect &rect) : ViewBase(rect) {}
        virtual ~VSplitView() = default;

        void InitView() override {
            auto screen = RuntimeConfig::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            if (splitterPos == 0) {
                splitterPos = viewRect.Width() / 2;
            }
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);
            UpdateLeftViewRect();
            UpdateRightViewRect();
        }

        void ReInitView() override {
            auto screen = RuntimeConfig::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Invisible, WindowBase::kWinDeco_None);

            int contentWidth = viewRect.Width();
            // splitterPos is the AUTHORITY; the child view widths are DERIVED from it - never the
            // reverse. (See HSplitView::ReInitView: adopting a child's live width here misreads a
            // resize-driven relayout as a deliberate splitter move and ratchets the divider into an
            // edge, collapsing a panel.) Keep the user's split across resizes, clamped into the new
            // width; skip while the rect is momentarily unestablished (width 0) during the traversal.
            if (contentWidth > 0) {
                if (splitterPos == 0) {
                    splitterPos = contentWidth / 2;
                } else {
                    splitterPos = ClampSplitterPos(splitterPos);
                }
            }

            UpdateLeftViewRect();
            UpdateRightViewRect();
        }

        void SetSplitterPos(int newSplitterPos) {
            // Clamp at the single chokepoint every resize path uses (increase/decrease-width actions,
            // relative positioning). Without it the divider can be dragged off either edge, giving the
            // left or right panel a negative width.
            splitterPos = ClampSplitterPos(newSplitterPos);
            UpdateLeftViewRect();
            UpdateRightViewRect();
            Initialize();
            InvalidateAll();
        }
        int GetSplitterPos() {
            return splitterPos;
        }
        void SetSplitterPosRelative(float newSplitterPos) {
            SetSplitterPos(viewRect.Width() * newSplitterPos);
        }
        float GetSplitterPosRelative() {
            return splitterPos / (float)viewRect.Width();
        }
        // Layout session (docs/session-cache.md §4.1) — persist this splitter under its session id.
        void ToSession(LayoutSession &layout) override {
            if (GetSessionId().empty()) {
                return;
            }
            SplitterSession s;
            s.id = GetSessionId();
            s.absolute = GetSplitterPos();
            s.relative = GetSplitterPosRelative();
            layout.splitters.push_back(s);
        }
        void FromSession(const LayoutSession &layout) override {
            if (GetSessionId().empty()) {
                return;
            }
            for (const auto &s : layout.splitters) {
                if (s.id == GetSessionId()) {
                    // Restore by ratio so a different window size lands sensibly; SetSplitterPos clamps.
                    SetSplitterPosRelative(s.relative);
                    return;
                }
            }
        }

        void SetViewRect(const Rect &rect) override {
            viewRect = rect;
            if (splitterPos == 0) {
                splitterPos = viewRect.Width() / 2;
            }
            UpdateLeftViewRect();
            UpdateRightViewRect();
        }

        void SetInitialSplitterPos(int pos) {
            splitterPos = pos;
        }

        void SetLeft(ViewBase *newLeft) {
            leftView = newLeft;
            leftView->SetLayoutHandler(this);
            AddView(newLeft);
            UpdateLeftViewRect();
        }

        void SetRight(ViewBase *newRight) {
            rightView = newRight;
            rightView->SetLayoutHandler(this);
            AddView(newRight);
            UpdateRightViewRect();
        }

    protected:
        void OnActionIncreaseWidth() override {
            int delta = (rightView != nullptr && rightView->IsActive()) ? -1 : 1;
            SetSplitterPos(GetSplitterPos() + delta);
            splitterPosBeforeReset = GetSplitterPos();      // record the clamped value
        }
        void OnActionDecreaseWidth() override {
            int delta = (rightView != nullptr && rightView->IsActive()) ? 1 : -1;
            SetSplitterPos(GetSplitterPos() + delta);
            splitterPosBeforeReset = GetSplitterPos();
        }

        // A vertical splitter only manages WIDTH. Height changes belong to an enclosing layout
        // (e.g. an HSplitView), so pass them up the layout-handler chain until something that owns
        // height applies (and clamps) them. Without this the action falls through to
        // ViewBase::SetHeight on the splitter itself - unbounded, and it never reaches the HSplitView
        // that actually moves the divide, so the status bar could be driven off-screen.
        void OnActionIncreaseHeight() override {
            auto handler = GetLayoutHandler();
            if (handler != nullptr && handler != this) { handler->OnActionIncreaseHeight(); }
        }
        void OnActionDecreaseHeight() override {
            auto handler = GetLayoutHandler();
            if (handler != nullptr && handler != this) { handler->OnActionDecreaseHeight(); }
        }

        void UpdateRightViewRect() {
            if (rightView == nullptr) { return; }
            Rect rightRect = viewRect;
            rightRect.SetWidth(viewRect.Width() - splitterPos);
            rightRect.Move(splitterPos, 0);
            rightView->SetViewRect(rightRect);
        }
        void UpdateLeftViewRect() {
            if (leftView == nullptr) { return; }
            Rect leftRect = viewRect;
            leftRect.SetWidth(splitterPos);
            leftView->SetViewRect(leftRect);
        }

        // Keep at least this many columns for each of the left/right panels (and the divider) when the
        // view is wide enough to allow it.
        static constexpr int kMinViewExtent = 5;

        // Clamp a candidate divider column into the visible width, leaving room for both panels.
        // Falls back to a strict on-screen clamp when the view is too narrow for the normal margin.
        int ClampSplitterPos(int pos) {
            int w = viewRect.Width();
            int lo = kMinViewExtent;
            int hi = w - kMinViewExtent;
            if (hi < lo) {
                lo = 0;
                hi = (w > 0) ? (w - 1) : 0;
            }
            if (pos < lo) { return lo; }
            if (pos > hi) { return hi; }
            return pos;
        }

    protected:
        int splitterPos = 0;
        int splitterPosBeforeReset = -1;
        ViewBase *leftView = nullptr;
        ViewBase *rightView = nullptr;
    };
}

#endif //NCWIN_VSPLITVIEW_H
