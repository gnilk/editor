//
// Toolkit-owned action set (AI-4). The generic UI (ViewBase/RootView, list/tree
// selection widgets) matches only against these values; everything else is
// app/editor-owned (see Core/Action.h's kAction) and opaque to the toolkit.
//
#ifndef EDITOR_UIACTION_H
#define EDITOR_UIACTION_H

namespace gedit {
    enum class kUIAction {
        kActionNone,

        // Shared navigation set - consumed by both editor cursor movement and
        // the list/tree selection widgets.
        kActionPageUp,
        kActionPageDown,
        kActionLineUp,
        kActionLineDown,
        kActionLineHome,
        kActionLineEnd,
        kActionLineLeft,
        kActionLineRight,
        kActionLineWordLeft,
        kActionLineWordRight,
        kActionBufferStart,
        kActionBufferEnd,
        kActionGotoTopLine,
        kActionGotoBottomLine,
        kActionCommitLine,

        // View management - handled by the toolkit itself.
        kActionCycleActiveView,
        kActionCycleActiveViewNext,
        kActionCycleActiveViewPrev,
        kActionCloseModal,
        kActionIncreaseViewWidth,
        kActionDecreaseViewWidth,
        kActionIncreaseViewHeight,
        kActionDecreaseViewHeight,
        kActionMaximizeViewHeight,
    };
}

#endif //EDITOR_UIACTION_H
