//
// TabbedView - N views in one place, one shown at a time, with a row of their labels on top (docs/panel-tabs.md §2,
// hitlist #53). Generic: it knows nothing about terminals; the owner adds the views, names them and decides what a
// new tab is. The tab row is hidden while there is only one tab, so a single view looks as it would on its own.
//
// Like EditorViewContainer it is the *top view* for its place: focus, keys, actions and the status line go to the
// active tab, so the rest of the app talks to one stable view whichever tab shows. It owns its tab views: a removed
// tab's view is deleted.
//

#ifndef GEDIT_TABBEDVIEW_H
#define GEDIT_TABBEDVIEW_H

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ViewBase.h"

namespace gedit {
    class TabbedView : public ViewBase {
    public:
        using TabChangedHandler = std::function<void(size_t idxTab)>;
        // Where a label sits in the tab row, content-local columns [xStart, xEnd)
        struct TabSpan {
            size_t idxTab = 0;
            int xStart = 0;
            int xEnd = 0;
        };
    public:
        TabbedView() = default;
        explicit TabbedView(const Rect &rect) : ViewBase(rect) {
        }
        virtual ~TabbedView() = default;

        // The first tab added is the active one; a later one is added behind the active one. Returns the view.
        ViewBase *AddTab(std::unique_ptr<ViewBase> view, const std::u32string &label);
        // The view is deleted; the tab after it becomes active when it was, else the one before
        void RemoveTab(size_t idxTab);
        // As RemoveTab, but the view is handed over, not deleted - to another TabbedView (a running shell survives)
        std::unique_ptr<ViewBase> TakeTab(size_t idxTab);

        size_t NumTabs() const {
            return tabs.size();
        }
        std::optional<size_t> ActiveTab() const {
            return activeTab;
        }
        ViewBase *TabView(size_t idxTab) const;
        ViewBase *ActiveView() const;
        std::optional<size_t> IndexOf(const ViewBase *view) const;

        // Shows the tab; focus moves with it when the container has focus
        void SetActiveTab(size_t idxTab);
        // Wrap around
        void NextTab();
        void PreviousTab();

        void SetTabLabel(size_t idxTab, const std::u32string &label);
        const std::u32string &TabLabel(size_t idxTab) const;

        bool IsTabRowVisible() const {
            return tabs.size() > 1;
        }
        // Called when another tab is shown (not for the first one added)
        void SetTabChangedHandler(TabChangedHandler handler) {
            tabChangedHandler = std::move(handler);
        }

        // The labels laid out from column 0: ' a | b | c ' - each label's clickable span
        static std::vector<TabSpan> LayoutTabRow(const std::vector<std::u32string> &labels);

        // --- Top-view proxy: input, status and focus go to the active tab ---
        void HandleKeyPress(const KeyPress &keyPress) override;
        bool OnAction(const EditorAction &action) override;
        const std::u32string &GetStatusBarAbbreviation() override;
        std::pair<std::u32string, std::u32string> GetStatusBarInfo() override;
        bool OnMouseEvent(const MouseEvent &mouseEvent) override;

        // No splitter of its own - the resize actions go up the layout chain, as the stack views do
        void OnActionIncreaseWidth() override;
        void OnActionDecreaseWidth() override;
        void OnActionIncreaseHeight() override;
        void OnActionDecreaseHeight() override;
        void MaximizeContentHeight() override;
        void RestoreContentHeight() override;
        void ResetContentHeight() override;

    public:
        inline static const std::u32string SEPARATOR = U" | ";

    protected:
        void InitView() override;
        void ReInitView() override;
        void DrawViewContents() override;
        void OnActivate(bool isNowActive) override;
        void SetWindowCursor(const Cursor &) override {
            // The active tab draws its own cursor
        }

    private:
        // Every tab fills the place below the tab row (only the active one is visible)
        void LayoutTabs();
        Rect TabContentRect();
        std::vector<std::u32string> Labels() const;

    private:
        struct Tab {
            std::unique_ptr<ViewBase> view;
            std::u32string label;
        };
        std::vector<Tab> tabs;
        std::optional<size_t> activeTab;
        TabChangedHandler tabChangedHandler;
    };
}

#endif //GEDIT_TABBEDVIEW_H
