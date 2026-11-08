//
// Created by gnilk on 24.02.23.
//

#ifndef NCWIN_VSPLITVIEW_H
#define NCWIN_VSPLITVIEW_H

#include "ViewBase.h"
#include "Core/UI/UIHost.h"

namespace gedit {
    // Vertical splitter — left | right panels separated by a vertical divider
    class VSplitView : public ViewBase {
    public:
        VSplitView() = default;
        explicit VSplitView(const Rect &rect) : ViewBase(rect) {}
        virtual ~VSplitView() = default;

        void InitView() override {
            auto screen = UIHost::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            if (splitterPos == 0) {
                splitterPos = viewRect.Width() / 2;
            }
            // Visible, as HSplitView is: an invisible window's context draws nowhere, and the divider is drawn (DoDraw)
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
            UpdateLeftViewRect();
            UpdateRightViewRect();
        }

        void ReInitView() override {
            auto screen = UIHost::Instance().GetScreen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);

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
            NotifySessionChanged();
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
        void ToSession(ILayoutSink &sink) override {
            if (GetSessionId().empty()) {
                return;
            }
            sink.PutSplitter(GetSessionId(), GetSplitterPos(), GetSplitterPosRelative());
        }
        void FromSession(const ILayoutSink &sink) override {
            if (GetSessionId().empty()) {
                return;
            }
            int absolutePos = 0;
            float relativePos = 0.0f;
            if (sink.GetSplitter(GetSessionId(), absolutePos, relativePos)) {
                // Restore by ratio so a different window size lands sensibly; SetSplitterPos clamps.
                SetSplitterPosRelative(relativePos);
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

        // No column of its own: the divide is the left panel's last column, drawn as a line over it (DoDraw). A press on
        // the line, or on the column right of it, drags with the left button instead of reaching a panel - one cell is
        // easy to miss.
        bool OnMouseDragStart(const MouseEvent &press) override {
            bool isOnDivider = (press.x == DividerColumn()) || (press.x == DividerColumn() + 1);
            if ((press.button != kLeftButton) || !isOnDivider || !viewRect.PointInRect(press.x, press.y)) {
                return false;
            }
            dragOffset = press.x - DividerColumn();
            return true;
        }

        void OnMouseDrag(const MouseEvent &mouseEvent) override {
            if ((mouseEvent.kind != MouseEvent::kMouseEventKind_Move) && (mouseEvent.kind != MouseEvent::kMouseEventKind_Release)) {
                return;
            }
            // The divider column follows the pointer; SetSplitterPos clamps
            int newPos = mouseEvent.x - dragOffset - viewRect.TopLeft().x + 1;
            if (newPos != splitterPos) {
                SetSplitterPos(newPos);
            }
            splitterPosBeforeReset = GetSplitterPos();
        }

        // The panels first, then the line over the left one's last column - drawn before them, they would paint over it
        void DoDraw() override {
            ViewBase::DoDraw();
            DrawDivider();
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

        // Shows where the divide is, and where to take hold of it. In the panel's background, as the gutter's numbers are
        // drawn: a line, not a panel of its own - no column is spent on it.
        void DrawDivider() {
            if ((window == nullptr) || (leftView == nullptr) || (rightView == nullptr) || !leftView->IsVisible() || !rightView->IsVisible()) {
                return;
            }
            auto &dc = window->GetContentDC();
            auto &uiColors = UIHost::Instance().GetUIColors();
            dc.SetColor(uiColors["gutter_foreground"], uiColors["background"]);
            int x = DividerColumn() - viewRect.TopLeft().x;
            for (int y = 0; y < viewRect.Height(); y++) {
                dc.DrawStringWithAttributesAt(x, y, kTextAttributes::kNormal, U"|");
            }
            dc.ResetDrawColors();
        }

        // The left panel's last column, on screen (UpdateLeftViewRect)
        int DividerColumn() {
            return viewRect.TopLeft().x + splitterPos - 1;
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
        static constexpr int kLeftButton = 1;      // as the backends number them (SDL; gansi maps onto it)

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
        int dragOffset = 0;         // where on the divider the drag took hold
        ViewBase *leftView = nullptr;
        ViewBase *rightView = nullptr;
    };
}

#endif //NCWIN_VSPLITVIEW_H
