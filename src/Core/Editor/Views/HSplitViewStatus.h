//
// Created by gnilk on 18.03.23.
//

#ifndef EDITOR_HSPLITVIEWSTATUS_H
#define EDITOR_HSPLITVIEWSTATUS_H
#include "Core/UI/Views/HSplitView.h"
#include "Core/RuntimeConfig.h"
#include "Core/Git/GitStatus.h"

namespace gedit {
    class HSplitViewStatus : public HSplitView {
    public:
        HSplitViewStatus() = default;
        explicit HSplitViewStatus(const Rect &rect) : HSplitView(rect) {

        }
        virtual ~HSplitViewStatus() = default;

        //
        // Temporary, this set's the cursor in the status bar...
        // Is being used when the Editor goes into 'QuickCmdMode'
        // The main reason here is that the 'View'-system (i.e. UI) don't have
        // widgets (buttons, editboxes, etc..) just windows..
        //
        void SetWindowCursor(const Cursor &cursor) override;

        // '⎇ main', '⎇ (a1b2c3d)' when detached, a long name cut in the middle to 'maxNameChars'; empty without a name
        static std::u32string BranchLabel(const GitStatus::Branch &branch, size_t maxNameChars = MAX_BRANCH_CHARS);
    public:
        static constexpr size_t MAX_BRANCH_CHARS = 32;

    protected:
        std::pair<std::u32string,std::u32string> GetStatusLineForTopview();

        void DrawSplitter(int row) override;
    private:
        // holds the x offset for the cursor relative the status bar position
        int quickModeStatusLineCursorOfs = 0;
    };
}

#endif //EDITOR_HSPLITVIEWSTATUS_H
