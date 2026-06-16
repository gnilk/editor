//
// Toolkit-owned action set (AI-4). The generic UI (ViewBase/RootView, list/tree
// selection widgets) matches only against these values; everything else is
// app/editor-owned (see Core/Action.h's kAction) and opaque to the toolkit.
//
#ifndef EDITOR_UIACTION_H
#define EDITOR_UIACTION_H

namespace gedit {
    enum class kUIAction {
        kUIActionNone,

        // Shared navigation set - consumed by both editor cursor movement and
        // the list/tree selection widgets.
        kUIActionPageUp,
        kUIActionPageDown,
        kUIActionLineUp,
        kUIActionLineDown,
        kUIActionLineHome,
        kUIActionLineEnd,
        kUIActionLineLeft,
        kUIActionLineRight,
        kUIActionLineWordLeft,
        kUIActionLineWordRight,
        kUIActionBufferStart,
        kUIActionBufferEnd,
        kUIActionGotoTopLine,
        kUIActionGotoBottomLine,
        kUIActionCommitLine,

        // View management - handled by the toolkit itself.
        kUIActionCycleActiveView,
        kUIActionCycleActiveViewNext,
        kUIActionCycleActiveViewPrev,
        kUIActionCloseModal,
        kUIActionIncreaseViewWidth,
        kUIActionDecreaseViewWidth,
        kUIActionIncreaseViewHeight,
        kUIActionDecreaseViewHeight,
        kUIActionMaximizeViewHeight,
    };
}

#endif //EDITOR_UIACTION_H
