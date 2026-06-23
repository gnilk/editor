//
// Created by gnilk on 23.06.2026.
//
#include <testinterface.h>
#include "Core/Editor/Controllers/TerminalController.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_terminalcontroller(ITesting *t);
DLL_EXPORT int test_terminalcontroller_viewport_defaults(ITesting *t);
DLL_EXPORT int test_terminalcontroller_viewport_scroll_clamps(ITesting *t);
DLL_EXPORT int test_terminalcontroller_viewport_anchor_stationary_on_output(ITesting *t);
DLL_EXPORT int test_terminalcontroller_viewport_snap_to_bottom(ITesting *t);
DLL_EXPORT int test_terminalcontroller_viewport_pageup_pagedown(ITesting *t);
DLL_EXPORT int test_terminalcontroller_viewport_bufferstart_bufferend(ITesting *t);
DLL_EXPORT int test_terminalcontroller_viewport_first_pageup_moves_window(ITesting *t);
DLL_EXPORT int test_terminalcontroller_commitline_opens_block(ITesting *t);
DLL_EXPORT int test_terminalcontroller_commitline_empty_opens_no_block(ITesting *t);
DLL_EXPORT int test_terminalcontroller_commitline_closes_prior_block(ITesting *t);
DLL_EXPORT int test_terminalcontroller_jump_prev_next_prompt(ITesting *t);
DLL_EXPORT int test_terminalcontroller_jump_prev_prompt_clamps_at_oldest(ITesting *t);
DLL_EXPORT int test_terminalcontroller_jump_next_prompt_clamps_at_newest(ITesting *t);
}

// Push 'n' lines through the grid into scrollback WITHOUT starting a real shell - WriteLine (the
// IOutputConsole path search-results output already uses today) writes straight into the grid under
// screenLock and never touches Shell, so it is the one TerminalController entry point that is safe to
// drive on a default-constructed (un-Begin()'d) controller.
static void WriteLines(TerminalController &c, int n) {
    for (int i = 0; i < n; i++) {
        c.WriteLine(U"line of output");
    }
}

DLL_EXPORT int test_terminalcontroller(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_terminalcontroller_viewport_defaults(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    TR_ASSERT(t, c.IsFollowingBottom());
    return kTR_Pass;
}

// Viewport clamping (docs/terminal-scrollback.md §5.1/§5.3): scrolling past either end of the
// abs-row range clamps into [scrollbackBase, AbsRowCount()]; clamping at the bottom re-enters
// followBottom.
DLL_EXPORT int test_terminalcontroller_viewport_scroll_clamps(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    WriteLines(c, 20);   // far more than fits in a 5-row grid -> plenty of scrollback history

    auto bottom = c.GetScreen().AbsRowCount();
    auto top    = c.GetScreen().ScrollbackBase();
    TR_ASSERT(t, bottom > top);   // sanity: there IS history to scroll into

    // Scroll up (toward older history) past the top - clamps at scrollbackBase, leaves followBottom.
    c.ScrollViewport(-1000);
    TR_ASSERT(t, !c.IsFollowingBottom());
    TR_ASSERT(t, c.GetAnchorAbsRow() == top);

    // Scroll back down past the bottom - clamps at AbsRowCount(), re-enters followBottom.
    c.ScrollViewport(1000);
    TR_ASSERT(t, c.IsFollowingBottom());
    TR_ASSERT(t, c.GetAnchorAbsRow() == c.GetScreen().AbsRowCount());

    return kTR_Pass;
}

// New output must NOT move the anchor while scrolled up - what you're reading stays put (you can
// watch a long build run without losing your place).
DLL_EXPORT int test_terminalcontroller_viewport_anchor_stationary_on_output(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    WriteLines(c, 20);

    c.ScrollViewport(-3);
    TR_ASSERT(t, !c.IsFollowingBottom());
    auto anchorBefore = c.GetAnchorAbsRow();

    WriteLines(c, 10);   // simulate more output arriving while scrolled up
    TR_ASSERT(t, !c.IsFollowingBottom());
    TR_ASSERT(t, c.GetAnchorAbsRow() == anchorBefore);

    return kTR_Pass;
}

// Explicit ScrollToBottom() (what CommitLine and local text input wire into, §5.3) always re-enters
// followBottom and resyncs the anchor to the live bottom.
DLL_EXPORT int test_terminalcontroller_viewport_snap_to_bottom(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    WriteLines(c, 20);

    c.ScrollViewport(-5);
    TR_ASSERT(t, !c.IsFollowingBottom());

    c.ScrollToBottom();
    TR_ASSERT(t, c.IsFollowingBottom());
    TR_ASSERT(t, c.GetAnchorAbsRow() == c.GetScreen().AbsRowCount());

    return kTR_Pass;
}

// TS-0f: at the prompt (local edit), plain PageUp/PageDown page the backlog by a screenful - the
// shell-prompt handler docs/terminal-scrollback.md §5.3 calls "the one new handler".
DLL_EXPORT int test_terminalcontroller_viewport_pageup_pagedown(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    WriteLines(c, 20);

    auto bottom = c.GetScreen().AbsRowCount();

    EditorAction pageUp;
    pageUp.uiAction = kUIAction::kUIActionPageUp;
    TR_ASSERT(t, c.OnAction(pageUp));
    TR_ASSERT(t, !c.IsFollowingBottom());
    TR_ASSERT(t, c.GetAnchorAbsRow() < bottom);

    auto anchorAfterPageUp = c.GetAnchorAbsRow();
    EditorAction pageDown;
    pageDown.uiAction = kUIAction::kUIActionPageDown;
    TR_ASSERT(t, c.OnAction(pageDown));
    TR_ASSERT(t, c.GetAnchorAbsRow() > anchorAfterPageUp);

    return kTR_Pass;
}

// TS-0f: BufferStart/BufferEnd (Ctrl+Home/End) jump to the oldest retained row / snap back to live.
DLL_EXPORT int test_terminalcontroller_viewport_bufferstart_bufferend(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    WriteLines(c, 20);

    EditorAction bufferStart;
    bufferStart.uiAction = kUIAction::kUIActionBufferStart;
    TR_ASSERT(t, c.OnAction(bufferStart));
    TR_ASSERT(t, !c.IsFollowingBottom());
    TR_ASSERT(t, c.GetAnchorAbsRow() == c.GetScreen().ScrollbackBase());

    EditorAction bufferEnd;
    bufferEnd.uiAction = kUIAction::kUIActionBufferEnd;
    TR_ASSERT(t, c.OnAction(bufferEnd));
    TR_ASSERT(t, c.IsFollowingBottom());
    TR_ASSERT(t, c.GetAnchorAbsRow() == c.GetScreen().AbsRowCount());

    return kTR_Pass;
}

// Regression for the TS-0f flicker: the very first PageUp off followBottom must move the visible
// window by a full page relative to what's ALREADY on screen (bottom - visibleRows), not relative to
// 'bottom' itself - the latter lands on the same window already shown (only the bottom row changes
// from the prompt to the affordance), which looks like a no-op scroll until the second press.
DLL_EXPORT int test_terminalcontroller_viewport_first_pageup_moves_window(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    WriteLines(c, 20);   // plenty of history - well more than a screenful

    auto bottom = c.GetScreen().AbsRowCount();
    int visibleRows = c.GetScreen().Rows() - 1;
    uint64_t windowTopBeforeScroll = bottom - (uint64_t)visibleRows;   // what's on screen right now

    c.ScrollViewport(-visibleRows);   // one PageUp
    TR_ASSERT(t, !c.IsFollowingBottom());
    // Must land a full page ABOVE the already-visible window, not back on top of it.
    TR_ASSERT(t, c.GetAnchorAbsRow() < windowTopBeforeScroll);

    return kTR_Pass;
}

// TS-1b: committing a non-empty line opens exactly one block carrying the command text (docs/
// terminal-scrollback.md §4.1). The controller is never Begin()'d (no real shell), so shell.SendCmd
// silently fails on the unset pty fd - harmless, CommitLine's other side effects (block bookkeeping,
// cmdHistory, ScrollToBottom) still run.
DLL_EXPORT int test_terminalcontroller_commitline_opens_block(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    auto blocksBefore = c.GetScreen().Blocks().size();   // just the initial loose block

    c.GetInputLine()->Append(std::u32string(U"echo hi"));
    c.CommitLine();

    TR_ASSERT(t, c.GetScreen().Blocks().size() == blocksBefore + 1);
    TR_ASSERT(t, c.GetScreen().Blocks().back().command == std::u32string(U"echo hi"));
    TR_ASSERT(t, !c.GetScreen().Blocks().back().endAbsRow.has_value());   // still open
    TR_ASSERT(t, c.GetScreen().Blocks().back().source == TerminalScreen::CommandBlock::Source::kCommitLine);

    return kTR_Pass;
}

// Committing an EMPTY line (just pressing Enter at a blank prompt) must not open a block - there's no
// command to name, and a string of blank Enters shouldn't fragment the backlog into empty blocks.
DLL_EXPORT int test_terminalcontroller_commitline_empty_opens_no_block(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    auto blocksBefore = c.GetScreen().Blocks().size();

    c.CommitLine();   // inputLine is empty by default

    TR_ASSERT(t, c.GetScreen().Blocks().size() == blocksBefore);

    return kTR_Pass;
}

// The NEXT commit closes the PRIOR block at exactly the abs row the new one starts (no gap/overlap) -
// same invariant exercised at the model level in test_terminalscreen_block_open_close, now driven
// through the real controller entry point.
DLL_EXPORT int test_terminalcontroller_commitline_closes_prior_block(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);

    c.GetInputLine()->Append(std::u32string(U"first"));
    c.CommitLine();
    auto firstIdx = c.GetScreen().Blocks().size() - 1;

    c.GetInputLine()->Append(std::u32string(U"second"));
    c.CommitLine();

    TR_ASSERT(t, c.GetScreen().Blocks().size() == firstIdx + 2);
    auto &first  = c.GetScreen().Blocks()[firstIdx];
    auto &second = c.GetScreen().Blocks()[firstIdx + 1];
    TR_ASSERT(t, first.endAbsRow.has_value());
    TR_ASSERT(t, first.endAbsRow.value() == second.startAbsRow);
    TR_ASSERT(t, !second.endAbsRow.has_value());

    return kTR_Pass;
}

// TS-1c (§5.4): PrevPrompt/NextPrompt walk screen.Blocks() relative to whatever's top-visible,
// landing exactly on a block's startAbsRow each time - the iTerm2 "jump to previous/next command"
// behaviour, driven entirely off the abs-id block index built in TS-1a/TS-1b.
DLL_EXPORT int test_terminalcontroller_jump_prev_next_prompt(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);

    auto commit = [&](const std::u32string &cmd) {
        c.GetInputLine()->Append(cmd);
        c.CommitLine();
    };

    WriteLines(c, 2);              // loose-block output before any command
    commit(U"cmd1");
    WriteLines(c, 5);
    commit(U"cmd2");
    WriteLines(c, 5);
    commit(U"cmd3");
    WriteLines(c, 5);

    // loose (closed) + cmd1 + cmd2 + cmd3 (open) - all four boundaries distinct.
    const auto &blocks = c.GetScreen().Blocks();
    TR_ASSERT(t, blocks.size() == 4);
    auto cmd1Start = blocks[1].startAbsRow;
    auto cmd2Start = blocks[2].startAbsRow;
    auto cmd3Start = blocks[3].startAbsRow;
    TR_ASSERT(t, blocks[0].startAbsRow < cmd1Start);
    TR_ASSERT(t, cmd1Start < cmd2Start);
    TR_ASSERT(t, cmd2Start < cmd3Start);

    EditorAction prevPrompt;
    prevPrompt.uiAction = kUIAction::kUIActionPrevPrompt;

    TR_ASSERT(t, c.OnAction(prevPrompt));
    TR_ASSERT(t, !c.IsFollowingBottom());
    auto afterFirstPrev = c.GetAnchorAbsRow();
    // Lands exactly on SOME block's start, not an arbitrary row.
    TR_ASSERT(t, afterFirstPrev == blocks[0].startAbsRow || afterFirstPrev == cmd1Start ||
                 afterFirstPrev == cmd2Start || afterFirstPrev == cmd3Start);

    TR_ASSERT(t, c.OnAction(prevPrompt));
    auto afterSecondPrev = c.GetAnchorAbsRow();
    TR_ASSERT(t, afterSecondPrev < afterFirstPrev);   // walked strictly further back

    // NextPrompt reverses exactly one step - back to where the first Prev landed.
    EditorAction nextPrompt;
    nextPrompt.uiAction = kUIAction::kUIActionNextPrompt;
    TR_ASSERT(t, c.OnAction(nextPrompt));
    TR_ASSERT(t, !c.IsFollowingBottom());
    TR_ASSERT(t, c.GetAnchorAbsRow() == afterFirstPrev);

    return kTR_Pass;
}

// PrevPrompt clamps at the oldest block (the loose block, when no command was ever committed) -
// repeated calls must not walk past it or crash.
DLL_EXPORT int test_terminalcontroller_jump_prev_prompt_clamps_at_oldest(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    WriteLines(c, 3);   // just the loose block's output, no commits

    EditorAction prevPrompt;
    prevPrompt.uiAction = kUIAction::kUIActionPrevPrompt;
    TR_ASSERT(t, c.OnAction(prevPrompt));
    auto firstAnchor = c.GetAnchorAbsRow();
    TR_ASSERT(t, firstAnchor == c.GetScreen().ScrollbackBase());   // only the loose block exists

    TR_ASSERT(t, c.OnAction(prevPrompt));   // calling again must not crash or go further back
    TR_ASSERT(t, c.GetAnchorAbsRow() == firstAnchor);

    return kTR_Pass;
}

// NextPrompt clamps at the newest (open) block - repeated calls must not crash or overshoot it.
DLL_EXPORT int test_terminalcontroller_jump_next_prompt_clamps_at_newest(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);

    WriteLines(c, 3);   // loose-block output before any command
    c.GetInputLine()->Append(std::u32string(U"cmd1"));
    c.CommitLine();
    WriteLines(c, 3);

    EditorAction bufferStart;
    bufferStart.uiAction = kUIAction::kUIActionBufferStart;
    TR_ASSERT(t, c.OnAction(bufferStart));   // scroll all the way up first

    EditorAction nextPrompt;
    nextPrompt.uiAction = kUIAction::kUIActionNextPrompt;
    TR_ASSERT(t, c.OnAction(nextPrompt));
    auto afterFirstNext = c.GetAnchorAbsRow();
    TR_ASSERT(t, afterFirstNext == c.GetScreen().Blocks().back().startAbsRow);   // the open "cmd1" block

    TR_ASSERT(t, c.OnAction(nextPrompt));   // already at the newest - must clamp, not crash
    TR_ASSERT(t, c.GetAnchorAbsRow() == afterFirstNext);

    return kTR_Pass;
}
