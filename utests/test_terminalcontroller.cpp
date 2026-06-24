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
DLL_EXPORT int test_terminalcontroller_selected_block_index(ITesting *t);
DLL_EXPORT int test_terminalcontroller_selected_block_text(ITesting *t);
DLL_EXPORT int test_terminalcontroller_selection_survives_output(ITesting *t);
DLL_EXPORT int test_terminalcontroller_block_index_surface(ITesting *t);
}

#include <cstdint>   // UINT64_MAX

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

// TS-1.5b (§5.5.2): SelectedBlockIndex resolves the block containing the top-visible anchor while
// scrolled, and is empty while following the bottom (you're at the live prompt - nothing selected).
// A JumpToPrevPrompt parks the anchor on a block's startAbsRow, so the selected block is exactly the
// one whose half-open [startAbsRow, endAbsRow) range contains it (the contiguous half-open ranges make
// the block boundary unambiguous - the anchor belongs to the block it STARTS, not the prior one's end).
DLL_EXPORT int test_terminalcontroller_selected_block_index(ITesting *t) {
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

    // Following the bottom: nothing is selected.
    TR_ASSERT(t, c.IsFollowingBottom());
    TR_ASSERT(t, !c.SelectedBlockIndex().has_value());

    // Jump back one prompt -> scrolled, anchor on a block start -> that block is selected and its
    // range really does contain the anchor.
    c.JumpToPrevPrompt();
    TR_ASSERT(t, !c.IsFollowingBottom());
    auto sel = c.SelectedBlockIndex();
    TR_ASSERT(t, sel.has_value());

    const auto &blocks = c.GetScreen().Blocks();
    const auto &block = blocks[*sel];
    uint64_t end = block.endAbsRow.value_or(c.GetScreen().AbsRowCount());
    TR_ASSERT(t, c.GetAnchorAbsRow() >= block.startAbsRow);
    TR_ASSERT(t, c.GetAnchorAbsRow() < end);
    TR_ASSERT(t, block.startAbsRow == c.GetAnchorAbsRow());   // landed exactly on the block start

    // Snapping back to the live bottom clears the selection again.
    c.ScrollToBottom();
    TR_ASSERT(t, c.IsFollowingBottom());
    TR_ASSERT(t, !c.SelectedBlockIndex().has_value());

    return kTR_Pass;
}

// TS-2a (§5.5.3): GetSelectedBlockText is the JS-facing seam (Console.GetSelectedBlock) - it returns
// the selected block's GetBlockOutputText joined with '\n', or nullopt while following the bottom
// (nothing selected). The text it returns must match exactly the model's text for whatever block the
// viewport currently selects (no off-by-one, no foreign block bleeding in).
DLL_EXPORT int test_terminalcontroller_selected_block_text(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);

    auto commit = [&](const std::u32string &cmd) {
        c.GetInputLine()->Append(cmd);
        c.CommitLine();
    };

    WriteLines(c, 2);              // loose-block output before any command
    commit(U"cmd1");
    c.WriteLine(U"out-a");
    c.WriteLine(U"out-b");
    commit(U"cmd2");
    c.WriteLine(U"out-c");

    // Following the bottom: nothing selected -> no text.
    TR_ASSERT(t, c.IsFollowingBottom());
    TR_ASSERT(t, !c.GetSelectedBlockText().has_value());

    // Jump back -> a block is selected; the controller's joined text must equal the model's text for
    // exactly that block (self-consistent, independent of which block the jump lands on / grid size).
    c.JumpToPrevPrompt();
    auto sel = c.SelectedBlockIndex();
    TR_ASSERT(t, sel.has_value());

    const auto &block = c.GetScreen().Blocks()[*sel];
    auto modelLines = c.GetScreen().GetBlockOutputText(block.id);
    std::u32string expected;
    for (size_t i = 0; i < modelLines.size(); i++) {
        if (i > 0) { expected += U"\n"; }
        expected += modelLines[i];
    }

    auto got = c.GetSelectedBlockText();
    TR_ASSERT(t, got.has_value());
    TR_ASSERT(t, got.value() == expected);

    // Back at the bottom: nothing selected again.
    c.ScrollToBottom();
    TR_ASSERT(t, !c.GetSelectedBlockText().has_value());

    return kTR_Pass;
}

// TS-1.5c / §5.5.3 item 4: a selected block must stay the active target while background output keeps
// streaming in - that is what makes quick-command-mode a reliable action surface (you select a block,
// ESC to the status bar, type a command while a build keeps printing, and your selection is still
// there). Only ScrollToBottom (commit / local edit) clears the selection; new output never does. This
// is the regression guard for "preserve the selection across focus loss" (today it holds by
// construction - nothing in the redraw/output path resets the viewport).
DLL_EXPORT int test_terminalcontroller_selection_survives_output(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);

    auto commit = [&](const std::u32string &cmd) {
        c.GetInputLine()->Append(cmd);
        c.CommitLine();
    };

    WriteLines(c, 2);              // loose-block output
    commit(U"cmd1");
    WriteLines(c, 3);
    commit(U"cmd2");
    WriteLines(c, 3);

    // Walk all the way back to the oldest block - guaranteed CLOSED (only the tail block is open), so
    // its [start,end) range and resolved text are fixed regardless of later output.
    for (int i = 0; i < 8; i++) {
        c.JumpToPrevPrompt();
    }
    auto sel = c.SelectedBlockIndex();
    TR_ASSERT(t, sel.has_value());
    auto selectedId  = c.GetScreen().Blocks()[*sel].id;
    auto textBefore  = c.GetSelectedBlockText();
    auto anchorBefore = c.GetAnchorAbsRow();
    TR_ASSERT(t, textBefore.has_value());
    TR_ASSERT(t, c.GetScreen().Blocks()[*sel].endAbsRow.has_value());   // a closed block

    // Background output streams in (a running build). No commit, no local edit -> must NOT snap.
    WriteLines(c, 15);

    TR_ASSERT(t, !c.IsFollowingBottom());                 // still scrolled
    TR_ASSERT(t, c.GetAnchorAbsRow() == anchorBefore);    // anchor stationary
    auto selAfter = c.SelectedBlockIndex();
    TR_ASSERT(t, selAfter.has_value());
    TR_ASSERT(t, c.GetScreen().Blocks()[*selAfter].id == selectedId);   // SAME block selected
    TR_ASSERT(t, c.GetSelectedBlockText() == textBefore);              // SAME text

    return kTR_Pass;
}

// TS-2c: the by-id IOutputConsole surface (GetBlocks / GetBlockOutputText / GetLastBlock) that the
// engine-agnostic TerminalAPI reads. These do NOT depend on the viewport (unlike GetSelectedBlockText)
// - they address blocks by id. Assert the seam mirrors the model exactly: same blocks/order/ids, last
// == the tail, by-id text == the model's joined text, and an unknown id resolves to nullopt.
DLL_EXPORT int test_terminalcontroller_block_index_surface(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);

    auto commit = [&](const std::u32string &cmd) {
        c.GetInputLine()->Append(cmd);
        c.CommitLine();
    };

    WriteLines(c, 2);              // loose-block output before any command
    commit(U"cmd1");
    c.WriteLine(U"out-a");
    c.WriteLine(U"out-b");
    commit(U"cmd2");               // closes cmd1's block; cmd2's block is now the open tail
    c.WriteLine(U"out-c");

    const auto &modelBlocks = c.GetScreen().Blocks();
    auto blocks = c.GetBlocks();

    // Enumeration matches the model: same count, same order, same ids + commands.
    TR_ASSERT(t, blocks.size() == modelBlocks.size());
    TR_ASSERT(t, blocks.size() >= 3);   // loose + cmd1 + cmd2
    for (size_t i = 0; i < blocks.size(); i++) {
        TR_ASSERT(t, blocks[i].id == modelBlocks[i].id);
        TR_ASSERT(t, blocks[i].command == modelBlocks[i].command);
    }

    // The last block is the tail (cmd2, still open).
    auto last = c.GetLastBlock();
    TR_ASSERT(t, last.has_value());
    TR_ASSERT(t, last->id == modelBlocks.back().id);
    TR_ASSERT(t, last->command == U"cmd2");

    // cmd1's block (index 1) is CLOSED - its line count + by-id text are fixed.
    TR_ASSERT(t, modelBlocks[1].endAbsRow.has_value());
    TR_ASSERT(t, blocks[1].lineCount == modelBlocks[1].endAbsRow.value() - modelBlocks[1].startAbsRow);

    auto byId = c.GetBlockOutputText(modelBlocks[1].id);
    TR_ASSERT(t, byId.has_value());
    auto modelLines = c.GetScreen().GetBlockOutputText(modelBlocks[1].id);
    std::u32string expected;
    for (size_t i = 0; i < modelLines.size(); i++) {
        if (i > 0) { expected += U"\n"; }
        expected += modelLines[i];
    }
    TR_ASSERT(t, byId.value() == expected);

    // An id that matches no block resolves to nullopt (ids are monotonic from small values).
    TR_ASSERT(t, !c.GetBlockOutputText(UINT64_MAX).has_value());

    return kTR_Pass;
}
