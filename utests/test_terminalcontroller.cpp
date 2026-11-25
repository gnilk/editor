//
// Created by gnilk on 23.06.2026.
//
#include <testinterface.h>
#include "Core/Editor/Controllers/TerminalController.h"
#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/ClipBoard.h"
#include "Core/Plugins/PluginExecutor.h"
#include "Core/Editor/Views/PanelView.h"

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
DLL_EXPORT int test_terminalcontroller_commitline_marks_output(ITesting *t);
DLL_EXPORT int test_terminalcontroller_viewport_scroll_stops_at_newest_page(ITesting *t);
DLL_EXPORT int test_terminalcontroller_commitline_empty_marks_nothing(ITesting *t);
DLL_EXPORT int test_terminalcontroller_dotcommand_keeps_output(ITesting *t);
DLL_EXPORT int test_terminalcontroller_not_output_console(ITesting *t);
DLL_EXPORT int test_terminalcontroller_settled(ITesting *t);
DLL_EXPORT int test_terminalcontroller_dotcommand_output_below(ITesting *t);
DLL_EXPORT int test_terminalcontroller_osc133_output(ITesting *t);
DLL_EXPORT int test_terminalcontroller_osc133_commit_stands_down(ITesting *t);
DLL_EXPORT int test_terminalcontroller_raw_tab(ITesting *t);
DLL_EXPORT int test_terminalcontroller_config_consolidated(ITesting *t);
DLL_EXPORT int test_terminalcontroller_passthrough_keys(ITesting *t);
DLL_EXPORT int test_terminalcontroller_passthrough_actions(ITesting *t);
DLL_EXPORT int test_terminalcontroller_local_mode_keeps_keys(ITesting *t);
DLL_EXPORT int test_terminalcontroller_ctrlc_local(ITesting *t);
DLL_EXPORT int test_terminalcontroller_paste(ITesting *t);
DLL_EXPORT int test_terminalcontroller_promote(ITesting *t);
DLL_EXPORT int test_terminalcontroller_mouse_focus(ITesting *t);
DLL_EXPORT int test_terminalcontroller_replies_to_pty(ITesting *t);
DLL_EXPORT int test_terminalcontroller_tab_commit_history(ITesting *t);
DLL_EXPORT int test_terminalcontroller_tab_dotcommand(ITesting *t);
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_wrapped(ITesting *t);
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_fullrow(ITesting *t);
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_rprompt(ITesting *t);
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_retyped(ITesting *t);
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_scrolled(ITesting *t);
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_listed(ITesting *t);
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_listed_newprompt(ITesting *t);
DLL_EXPORT int test_terminalcontroller_sync_output(ITesting *t);
DLL_EXPORT int test_terminalcontroller_sync_output_frame(ITesting *t);
DLL_EXPORT int test_terminalcontroller_panel_names(ITesting *t);
DLL_EXPORT int test_terminalcontroller_title(ITesting *t);
DLL_EXPORT int test_terminalcontroller_build_fastend(ITesting *t);
DLL_EXPORT int test_terminalcontroller_build_quietend(ITesting *t);
DLL_EXPORT int test_terminalcontroller_ctrl_to_shell(ITesting *t);
DLL_EXPORT int test_terminalcontroller_ctrl_d_shell_owned(ITesting *t);
DLL_EXPORT int test_terminalcontroller_settings(ITesting *t);
DLL_EXPORT int test_terminalcontroller_panel_configured(ITesting *t);
}

#include <cstdint>   // UINT64_MAX
#include <atomic>
#include <thread>
#include <chrono>

// Feed a scripted byte stream into the controller as if it came off the pty (HandleTerminalData is
// the Shell output-delegate target). Lets a test drive OSC 133 markers without a live shell.
static void FeedPty(TerminalController &c, const std::string &bytes) {
    c.HandleTerminalData(reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
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

// A committed command's output starts below the line it was typed on, and runs to the next prompt. The controller
// is never Begin()'d (no real shell): the commit's pty write goes nowhere, the bookkeeping still runs.
DLL_EXPORT int test_terminalcontroller_commitline_marks_output(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    FeedPty(c, "$ ");

    c.GetInputLine()->Append(std::u32string(U"echo hi"));
    c.CommitLine();
    FeedPty(c, "echo hi\r\nhi\r\nthere\r\n$ ");      // the shell's echo, the output, the next prompt

    auto text = c.GetLastOutputText();
    TR_ASSERT(t, text.has_value());
    TR_ASSERT(t, text.value() == std::u32string(U"hi\nthere"));
    return kTR_Pass;
}

// Enter at a blank prompt is no command: the last output stays what it was
DLL_EXPORT int test_terminalcontroller_commitline_empty_marks_nothing(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    c.CommitLine();   // inputLine is empty by default
    TR_ASSERT(t, !c.GetLastOutputText().has_value());
    return kTR_Pass;
}

// A dot-command runs in the editor, not the shell: `.co` right after a build copies the BUILD's output, so it must
// not move the last output onto its own (empty) one
DLL_EXPORT int test_terminalcontroller_dotcommand_keeps_output(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    TR_ASSERT(t, RuntimeConfig::Instance().HasPluginCommand("co"));
    FeedPty(c, "$ ");
    c.GetInputLine()->Append(std::u32string(U"make"));
    c.CommitLine();
    FeedPty(c, "make\r\nerror: x\r\n$ ");

    auto *prevConsole = RuntimeConfig::Instance().OutputConsole();
    c.RegisterAsOutputConsole();
    c.GetInputLine()->Append(std::u32string(U".co"));
    c.CommitLine();
    RuntimeConfig::Instance().SetOutputConsole(prevConsole);

    TR_ASSERT(t, c.GetLastOutputText() == std::optional<std::u32string>(U"error: x"));
    auto top = Editor::Instance().GetClipBoard().Top();
    TR_ASSERT(t, top != nullptr);
    TR_ASSERT(t, top->AsText() == std::u32string(U"error: x"));
    return kTR_Pass;
}

static std::u32string RowText(const TerminalScreen &screen, int y) {
    std::u32string text;
    for (const auto &cell : screen.GetRow(y)) {
        text.push_back(cell.ch == 0 ? U' ' : cell.ch);
    }
    while (!text.empty() && (text.back() == U' ')) {
        text.pop_back();
    }
    return text;
}

// A dot-command typed at the prompt reads like a shell command: the command on its prompt line, the output right
// below, the shell's next prompt right below that (it was: output above the prompt it was typed at, the command gone)
DLL_EXPORT int test_terminalcontroller_dotcommand_output_below(ITesting *t) {
    TerminalController c;
    c.Resize(100, 20);
    auto *prevConsole = RuntimeConfig::Instance().OutputConsole();
    c.RegisterAsOutputConsole();
    FeedPty(c, "\x1b[32m$\x1b[0m ");
    c.GetInputLine()->Append(std::u32string(U".theme bogus"));
    c.CommitLine();
    FeedPty(c, "\r\n\x1b[32m$\x1b[0m ");      // the shell's answer to the empty line: down, the next prompt
    RuntimeConfig::Instance().SetOutputConsole(prevConsole);

    const auto &screen = c.GetScreen();
    TR_ASSERT(t, RowText(screen, 0) == U"$ .theme bogus");
    TR_ASSERT(t, RowText(screen, 1).find(U"use: .theme list") == 0);
    int cursorRow = screen.GetCursorPos().y;
    TR_ASSERT(t, cursorRow > 2);
    TR_ASSERT(t, RowText(screen, cursorRow) == U"$");
    // No blank row between the output and the prompt, nothing below
    TR_ASSERT(t, !RowText(screen, cursorRow - 1).empty());
    TR_ASSERT(t, RowText(screen, cursorRow + 1).empty());
    TR_ASSERT(t, screen.GetCursorPos().x == 2);
    TR_ASSERT(t, c.GetInputLine()->Buffer().empty());
    return kTR_Pass;
}

// Scrolling down stops at the newest full page and follows the bottom again - it never shows a part-empty page
// that then snaps back (the jump seen when wheel-scrolling to the end)
DLL_EXPORT int test_terminalcontroller_viewport_scroll_stops_at_newest_page(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    WriteLines(c, 20);

    c.ScrollViewport(-6);
    TR_ASSERT(t, !c.IsFollowingBottom());
    auto scrolledTop = c.GetAnchorAbsRow();
    c.ScrollViewport(3);
    TR_ASSERT(t, !c.IsFollowingBottom());
    TR_ASSERT(t, c.GetAnchorAbsRow() == scrolledTop + 3);
    // 3 more reach the newest page (6 up, 6 down) - that is the bottom, not a page with blank rows under it
    c.ScrollViewport(3);
    TR_ASSERT(t, c.IsFollowingBottom());
    c.ScrollViewport(-1);
    TR_ASSERT(t, c.GetAnchorAbsRow() == scrolledTop + 5);
    return kTR_Pass;
}

// OSC 133 (§4.2): ;C marks exactly where the output starts
DLL_EXPORT int test_terminalcontroller_osc133_output(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    FeedPty(c,
        "\x1b]133;A\x07" "$ "
        "\x1b]133;B\x07" "ls -la"
        "\r\n\x1b]133;C\x07" "total 0\r\n"
        "\x1b]133;D;0\x07" "\x1b]133;A\x07" "$ ");
    auto text = c.GetLastOutputText();
    TR_ASSERT(t, text.has_value());
    TR_ASSERT(t, text.value() == std::u32string(U"total 0"));
    return kTR_Pass;
}

// With OSC 133 active the commit's guess stands down: the shell's ;C is where the output starts
DLL_EXPORT int test_terminalcontroller_osc133_commit_stands_down(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    FeedPty(c, "\x1b]133;A\x07" "$ " "\x1b]133;B\x07");

    c.GetInputLine()->Append(std::u32string(U"echo hi"));
    c.CommitLine();
    TR_ASSERT(t, !c.GetLastOutputText().has_value());       // no guess

    // The shell echoes the command, then brackets the output
    FeedPty(c, "echo hi\r\nbanner\r\n" "\x1b]133;C\x07" "hi\r\n" "\x1b]133;D;0\x07" "$ ");
    auto text = c.GetLastOutputText();
    TR_ASSERT(t, text.has_value());
    TR_ASSERT(t, text.value() == std::u32string(U"hi"));
    return kTR_Pass;
}

// open-bugs.md #11: a raw TAB (0x09) off the pty must advance the cursor to the next tab stop
// (multiple of 8) instead of being silently dropped. macOS BSD `ls`'s multi-column listing pads
// BETWEEN columns with tabs; with the tab eaten the entries fuse ("foobar" with no gap). This
// drives the EXACT dispatch chain in HandleTerminalData where the byte was being discarded.
DLL_EXPORT int test_terminalcontroller_raw_tab(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);

    // 'a' at col 0, a TAB, then 'b'. With the tab honoured 'b' lands at the next tab stop (col 8);
    // with it dropped (the bug) 'b' would land at col 1, fused against the 'a'.
    FeedPty(c, "a\tb");

    const auto &screen = c.GetScreen();
    TR_ASSERT(t, screen.GetRow(0)[0].ch == U'a');
    TR_ASSERT(t, screen.GetRow(0)[8].ch == U'b');       // advanced to the next multiple of 8
    TR_ASSERT(t, screen.GetCursorPos().x == 9);         // cursor sits just past 'b'
    // The skipped cells are real blanks, not stale/garbage content.
    for (int x = 1; x < 8; x++) {
        TR_ASSERT(t, screen.GetRow(0)[x].ch == U' ');
    }

    return kTR_Pass;
}

// open-bugs #12: the terminal's settings used to be split across two config sections - the view read
// 'commandview' (keymap, scroll notch) while the shell/process read 'terminal'. They
// are now consolidated under 'terminal' so the terminal exposes ONE section to the user. This runs on
// the real shipped config (loaded headless by test_main) and guards against a re-split: it asserts the
// PROPERTY that the view keys and the process keys now live in the same section, and that the legacy
// 'commandview' section no longer carries them. (TerminalView::InitView spawns a real shell, so we
// verify the consolidation at the config layer rather than driving the view - the view reads from the
// same section constant asserted here.)
DLL_EXPORT int test_terminalcontroller_config_consolidated(ITesting *t) {
    auto terminal    = Config::Instance()["terminal"];
    auto commandview = Config::Instance()["commandview"];

    // The former view keys now resolve under 'terminal'...
    TR_ASSERT(t, terminal.HasKey("keymap"));
    TR_ASSERT(t, terminal.HasKey("lines_per_scroll_wheel_notch"));
    // ...alongside the shell/process keys - proving it's one shared section, not just a rename. ('shell' is
    // unset by default - the login shell - so 'init', the shell's arguments, stands for them.)
    TR_ASSERT(t, terminal.HasKey("init"));

    // The load-bearing contract: the terminal binds the terminal keymap (NOT the default editor one).
    TR_ASSERT(t, terminal.GetStr("keymap", "") == "terminal_keymap");

    // The legacy section is gone (or at least no longer owns the terminal-view settings).
    TR_ASSERT(t, !commandview.HasKey("keymap"));
    TR_ASSERT(t, !commandview.HasKey("lines_per_scroll_wheel_notch"));

    return kTR_Pass;
}

//
// VT-8: input to the pty. A capturing pty writer and an injected foreground-job probe stand in for the shell.
//
static KeyPress TextKey(char32_t ch, uint8_t modifiers = 0) {
    KeyPress keyPress;
    keyPress.isKeyValid = true;
    keyPress.key = ch;
    keyPress.modifiers = modifiers;
    return keyPress;
}

static KeyPress SpecialKey(int code, uint8_t modifiers = 0) {
    KeyPress keyPress;
    keyPress.isSpecialKey = true;
    keyPress.specialKey = code;
    keyPress.modifiers = modifiers;
    return keyPress;
}

static void CaptureInput(TerminalController &c, std::string &sink, bool isForegroundJob) {
    c.SetPtyWriter([&sink](std::string_view bytes) { sink += bytes; });
    c.SetForegroundJobProbe([isForegroundJob]() { return isForegroundJob; });
}

// A foreground job owns the keyboard: every key reaches it, encoded as xterm does (TE-6, TE-7, TE-12)
DLL_EXPORT int test_terminalcontroller_passthrough_keys(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    std::string sent;
    CaptureInput(c, sent, true);
    TR_ASSERT(t, c.IsAppActive());

    Cursor cursor;
    size_t line = 0;
    auto press = [&](const KeyPress &keyPress) {
        sent.clear();
        c.HandleKeyPress(cursor, line, keyPress);
        return sent;
    };
    TR_ASSERT(t, press(TextKey(U'a')) == "a");
    TR_ASSERT(t, press(TextKey(U'å')) == "\xC3\xA5");
    TR_ASSERT(t, press(TextKey(U'c', Keyboard::kMod_LeftCtrl)) == "\x03");
    TR_ASSERT(t, press(TextKey(U'b', Keyboard::kMod_LeftAlt)) == "\x1b" "b");
    TR_ASSERT(t, press(SpecialKey(Keyboard::kKeyCode_Return)) == "\r");
    TR_ASSERT(t, press(SpecialKey(Keyboard::kKeyCode_Return, Keyboard::kMod_LeftAlt)) == "\x1b\r");
    TR_ASSERT(t, press(SpecialKey(Keyboard::kKeyCode_Tab, Keyboard::kMod_LeftShift)) == "\x1b[Z");
    TR_ASSERT(t, press(SpecialKey(Keyboard::kKeyCode_Escape)) == "\x1b");
    TR_ASSERT(t, press(SpecialKey(Keyboard::kKeyCode_UpArrow)) == "\x1b[A");
    TR_ASSERT(t, press(SpecialKey(Keyboard::kKeyCode_RightArrow, Keyboard::kMod_LeftCtrl)) == "\x1b[1;5C");
    // DECCKM from the app switches the arrows to SS3
    FeedPty(c, "\x1b[?1h");
    TR_ASSERT(t, press(SpecialKey(Keyboard::kKeyCode_UpArrow)) == "\x1bOA");
    // Cmd stays with the editor (macOS)
    TR_ASSERT(t, press(TextKey(U's', Keyboard::kMod_LeftCommand)).empty());
    return kTR_Pass;
}

// Actions bound in the editor still send their key to the app; the escape hatch is left to the editor
DLL_EXPORT int test_terminalcontroller_passthrough_actions(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    c.SetAltArrowsAsWords(false);
    std::string sent;
    CaptureInput(c, sent, true);

    // Esc is EnterCommandMode, Ctrl+C is CopyToClipboard - vim and Claude must still get them
    EditorAction esc;
    esc.action = kAction::kActionEnterCommandMode;
    esc.keyPress = SpecialKey(Keyboard::kKeyCode_Escape);
    TR_ASSERT(t, c.ForwardActionToShell(esc));
    EditorAction copy;
    copy.action = kAction::kActionCopyToClipboard;
    copy.keyPress = TextKey(U'c', Keyboard::kMod_LeftCtrl);
    TR_ASSERT(t, c.ForwardActionToShell(copy));
    EditorAction pageUp;
    pageUp.uiAction = kUIAction::kUIActionPageUp;
    pageUp.keyPress = SpecialKey(Keyboard::kKeyCode_PageUp);
    TR_ASSERT(t, c.ForwardActionToShell(pageUp));
    TR_ASSERT(t, sent == "\x1b\x03\x1b[5~");

    // Buffer switching keys belong to the app: Alt+Right is readline's word motion, Alt+1 is tmux/irssi's
    sent.clear();
    EditorAction nextBuffer;
    nextBuffer.action = kAction::kActionCycleActiveBufferNext;
    nextBuffer.keyPress = SpecialKey(Keyboard::kKeyCode_RightArrow, Keyboard::kMod_LeftAlt);
    TR_ASSERT(t, c.ForwardActionToShell(nextBuffer));
    EditorAction buffer1;
    buffer1.action = kAction::kActionSwitchToBuffer;
    buffer1.keyPress = TextKey(U'1', Keyboard::kMod_LeftAlt);
    TR_ASSERT(t, c.ForwardActionToShell(buffer1));
    TR_ASSERT(t, sent == "\x1b[1;3C\x1b" "1");

    // The macOS convention (terminal.alt_arrows: words): Alt+Left/Right is ESC b / ESC f; other modifiers keep xterm's
    sent.clear();
    c.SetAltArrowsAsWords(true);
    TR_ASSERT(t, c.ForwardActionToShell(nextBuffer));
    EditorAction prevBuffer;
    prevBuffer.action = kAction::kActionCycleActiveBufferPrev;
    prevBuffer.keyPress = SpecialKey(Keyboard::kKeyCode_LeftArrow, Keyboard::kMod_LeftAlt);
    TR_ASSERT(t, c.ForwardActionToShell(prevBuffer));
    EditorAction wordRight;
    wordRight.keyPress = SpecialKey(Keyboard::kKeyCode_RightArrow, Keyboard::kMod_LeftCtrl);
    TR_ASSERT(t, c.ForwardActionToShell(wordRight));
    TR_ASSERT(t, sent == "\x1b" "f" "\x1b" "b" "\x1b[1;5C");
    c.SetAltArrowsAsWords(false);

    // View switching and resizing are not sent; the view hands them on
    sent.clear();
    EditorAction toEditor;
    toEditor.action = kAction::kActionSwitchToEditor;
    toEditor.keyPress = TextKey(U'e', Keyboard::kMod_LeftAlt);
    TR_ASSERT(t, !c.ForwardActionToShell(toEditor));
    EditorAction wider;
    wider.uiAction = kUIAction::kUIActionIncreaseViewWidth;
    wider.keyPress = TextKey(U'd', Keyboard::kMod_LeftAlt);
    TR_ASSERT(t, !c.ForwardActionToShell(wider));
    TR_ASSERT(t, sent.empty());
    TR_ASSERT(t, TerminalController::IsTerminalEscapeHatch(toEditor));
    TR_ASSERT(t, !TerminalController::IsTerminalEscapeHatch(esc));
    return kTR_Pass;
}

// At the shell prompt (no job, no alt screen) keys stay with the local line editor - nothing reaches the pty
DLL_EXPORT int test_terminalcontroller_local_mode_keeps_keys(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    std::string sent;
    CaptureInput(c, sent, false);
    TR_ASSERT(t, !c.IsAppActive());
    Cursor cursor;
    size_t line = 0;
    c.HandleKeyPress(cursor, line, TextKey(U'l'));
    c.HandleKeyPress(cursor, line, TextKey(U's'));
    TR_ASSERT(t, sent.empty());
    TR_ASSERT(t, c.GetInputLine()->Buffer() == std::u32string(U"ls"));
    // The alternate screen makes the app active without a job probe
    FeedPty(c, "\x1b[?1049h");
    TR_ASSERT(t, c.IsAppActive());
    c.HandleKeyPress(cursor, line, TextKey(U'q'));
    TR_ASSERT(t, sent == "q");
    return kTR_Pass;
}

// Ctrl+C at the local prompt: the shell gets the interrupt, the line stays visible in the grid - not run, nothing inserted. It comes as
// a key (macOS: unbound) or as CopyToClipboard (Linux binds Ctrl+C to it)
DLL_EXPORT int test_terminalcontroller_ctrlc_local(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    std::string sent;
    CaptureInput(c, sent, false);
    Cursor cursor;
    size_t line = 0;
    c.HandleKeyPress(cursor, line, TextKey(U'l'));
    c.HandleKeyPress(cursor, line, TextKey(U's'));
    TR_ASSERT(t, c.HandleKeyPress(cursor, line, TextKey(U'c', Keyboard::kMod_LeftCtrl)));
    TR_ASSERT(t, sent == "\x03");
    TR_ASSERT(t, c.GetInputLine()->Length() == 0);
    TR_ASSERT(t, c.GetScreen().GetRow(0)[0].ch == U'l');
    TR_ASSERT(t, c.GetScreen().GetRow(0)[1].ch == U's');

    sent.clear();
    c.HandleKeyPress(cursor, line, TextKey(U'x'));
    EditorAction copy;
    copy.action = kAction::kActionCopyToClipboard;
    copy.keyPress = TextKey(U'c', Keyboard::kMod_LeftCtrl);
    TR_ASSERT(t, c.OnAction(copy));
    TR_ASSERT(t, sent == "\x03");
    TR_ASSERT(t, c.GetInputLine()->Length() == 0);

    // Another Ctrl chord nothing is bound to is not text - it goes to the shell (Ctrl+Y, readline's yank)
    sent.clear();
    TR_ASSERT(t, c.HandleKeyPress(cursor, line, TextKey(U'y', Keyboard::kMod_LeftCtrl)));
    TR_ASSERT(t, sent == "\x19");
    TR_ASSERT(t, c.GetInputLine()->Length() == 0);
    return kTR_Pass;
}

// TE-9: paste - bracketed for an app that asked; a line at the local prompt; several lines to readline, not run
DLL_EXPORT int test_terminalcontroller_paste(ITesting *t) {
    {
        TerminalController c;
        c.Resize(40, 10);
        std::string sent;
        CaptureInput(c, sent, true);
        c.Paste(U"echo hi\n");
        TR_ASSERT(t, sent == "echo hi\r");
        sent.clear();
        FeedPty(c, "\x1b[?2004h");
        c.Paste(U"a\nb");
        TR_ASSERT(t, sent == "\x1b[200~a\rb\x1b[201~");
    }
    {
        TerminalController c;
        c.Resize(40, 10);
        std::string sent;
        CaptureInput(c, sent, false);
        Cursor cursor;
        size_t line = 0;
        c.HandleKeyPress(cursor, line, TextKey(U'x'));
        c.Paste(U"yz");
        TR_ASSERT(t, sent.empty());
        TR_ASSERT(t, c.GetInputLine()->Buffer() == std::u32string(U"xyz"));
        TR_ASSERT(t, c.GetCursorXPos() >= 3);

        FeedPty(c, "\x1b[?2004h");      // readline asked for bracketed paste (bash 5.1+, zsh)
        c.Paste(U"one\ntwo\n");
        TR_ASSERT(t, sent == "xyz\x1b[200~one\rtwo\r\x1b[201~");
        TR_ASSERT(t, c.DoesShellOwnLineEditing());
    }
    {   // A shell that never asked (macOS' bash 3.2): the lines as typed - brackets showed as '00~' in its line
        TerminalController c;
        c.Resize(40, 10);
        std::string sent;
        CaptureInput(c, sent, false);
        c.Paste(U"one\ntwo");
        TR_ASSERT(t, sent == "one\rtwo");
        TR_ASSERT(t, c.DoesShellOwnLineEditing());
    }
    return kTR_Pass;
}

// Editor text into the terminal (hitlist #47): a paste, then Enter where it lands - the app, the local prompt, readline
DLL_EXPORT int test_terminalcontroller_promote(ITesting *t) {
    {   // An app (a chat in the terminal) that asked for bracketed paste: one paste, then Enter
        TerminalController c;
        c.Resize(40, 10);
        std::string sent;
        CaptureInput(c, sent, true);
        FeedPty(c, "\x1b[?2004h");
        c.Promote(U"explain this\nplease\n", true);
        TR_ASSERT(t, sent == "\x1b[200~explain this\rplease\x1b[201~\r");
        sent.clear();
        c.Promote(U"wait", false);
        TR_ASSERT(t, sent == "\x1b[200~wait\x1b[201~");
        sent.clear();
        c.Promote(U"\n", true);
        TR_ASSERT(t, sent.empty());
    }
    {   // One line at the local prompt: run as typed, after what was already typed there
        TerminalController c;
        c.Resize(40, 10);
        std::string sent;
        CaptureInput(c, sent, false);
        FeedPty(c, "$ ");
        c.Promote(U"echo hi", false);
        TR_ASSERT(t, c.GetInputLine()->Buffer() == std::u32string(U"echo hi"));
        c.GetInputLine()->Clear();
        c.Promote(U"echo hi", true);
        TR_ASSERT(t, c.GetInputLine()->Buffer().empty());
        EditorAction up;
        up.uiAction = kUIAction::kUIActionLineUp;
        c.OnAction(up);
        TR_ASSERT(t, c.GetInputLine()->Buffer() == std::u32string(U"echo hi"));    // it ran: in the history
    }
    {   // Several lines at the local prompt: readline gets them, Enter runs them, back to local editing
        TerminalController c;
        c.Resize(40, 10);
        std::string sent;
        CaptureInput(c, sent, false);
        FeedPty(c, "\x1b[?2004h$ ");
        c.Promote(U"cd /tmp\nls", true);
        TR_ASSERT(t, sent == "\x1b[200~cd /tmp\rls\x1b[201~\r");
        TR_ASSERT(t, !c.DoesShellOwnLineEditing());
    }
    {   // The same in a shell without bracketed paste: each line runs as typed
        TerminalController c;
        c.Resize(40, 10);
        std::string sent;
        CaptureInput(c, sent, false);
        FeedPty(c, "$ ");
        c.Promote(U"cd /tmp\nls", true);
        TR_ASSERT(t, sent == "cd /tmp\rls\r");
        TR_ASSERT(t, !c.DoesShellOwnLineEditing());
    }
    return kTR_Pass;
}

// TE-11 + focus events: only when the app asked for them
DLL_EXPORT int test_terminalcontroller_mouse_focus(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    std::string sent;
    CaptureInput(c, sent, true);
    using gnilk::vt::MouseButton;
    using gnilk::vt::MouseAction;
    TR_ASSERT(t, !c.ForwardMouse(MouseButton::kLeft, MouseAction::kPress, 3, 4, 0));
    c.SetFocus(true);
    TR_ASSERT(t, sent.empty());

    FeedPty(c, "\x1b[?1000;1006h\x1b[?1004h");
    TR_ASSERT(t, c.ForwardMouse(MouseButton::kLeft, MouseAction::kPress, 3, 4, 0));
    TR_ASSERT(t, c.ForwardMouse(MouseButton::kWheelDown, MouseAction::kPress, 0, 0, 0));
    c.SetFocus(false);
    TR_ASSERT(t, sent == "\x1b[<0;4;5M\x1b[<65;1;1M\x1b[O");
    return kTR_Pass;
}

// The terminal's own replies (DA, CPR) take the same way out as keys
DLL_EXPORT int test_terminalcontroller_replies_to_pty(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    std::string sent;
    CaptureInput(c, sent, false);
    FeedPty(c, "\x1b[c\x1b[2;5H\x1b[6n");
    TR_ASSERT(t, sent == "\x1b[?62;22c\x1b[2;5R");
    return kTR_Pass;
}

// A line finished in readline after Tab is committed through ForwardActionToShell, not CommitLine - it must still
// reach the history (Up recalls it) and mark its output, with the text readline shows, not what was typed
DLL_EXPORT int test_terminalcontroller_tab_commit_history(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    std::string sent;
    CaptureInput(c, sent, false);
    FeedPty(c, "$ ");

    Cursor cursor;
    size_t line = 0;
    for (auto ch : std::u32string(U"vim a")) {
        c.HandleKeyPress(cursor, line, TextKey(ch));
    }
    EditorAction complete;
    complete.action = kAction::kActionShellCompletion;
    TR_ASSERT(t, c.OnAction(complete));
    TR_ASSERT(t, c.DoesShellOwnLineEditing());
    TR_ASSERT(t, sent == "vim a\t");

    FeedPty(c, "vim apa.cpp");                  // readline's echo of the completed line
    sent.clear();
    EditorAction commit;
    commit.uiAction = kUIAction::kUIActionCommitLine;
    commit.keyPress = SpecialKey(Keyboard::kKeyCode_Return);
    TR_ASSERT(t, c.ForwardActionToShell(commit));
    TR_ASSERT(t, sent == "\r");
    TR_ASSERT(t, !c.DoesShellOwnLineEditing());

    FeedPty(c, "\r\nopened\r\n$ ");
    TR_ASSERT(t, c.GetLastOutputText() == std::optional<std::u32string>(U"opened"));
    EditorAction up;
    up.uiAction = kUIAction::kUIActionLineUp;
    TR_ASSERT(t, c.OnAction(up));
    TR_ASSERT(t, c.GetInputLine()->Buffer() == std::u32string(U"vim apa.cpp"));
    return kTR_Pass;
}

// ?2026: output inside a synchronized update is taken in without a redraw; the end of the update redraws
DLL_EXPORT int test_terminalcontroller_sync_output(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    FeedPty(c, "plain");
    TR_ASSERT(t, !c.IsHoldingRedraw());
    FeedPty(c, "\x1b[?2026h\x1b[2J\x1b[Hhalf");
    TR_ASSERT(t, c.IsHoldingRedraw());
    FeedPty(c, " a frame");
    TR_ASSERT(t, c.IsHoldingRedraw());
    FeedPty(c, "\x1b[?2026l");
    TR_ASSERT(t, !c.IsHoldingRedraw());
    TR_ASSERT(t, c.GetScreen().GetRow(0)[0].ch == U'h');
    TR_ASSERT(t, c.GetScreen().GetHeldFrame() == nullptr);
    return kTR_Pass;
}

// What the view draws during an update is the frame from before it, and an update the app never ends is let go at
// the cap with no further output (the Mac repro: `printf '\e[?2026h'; echo held; sleep 3` showed "held" at once)
DLL_EXPORT int test_terminalcontroller_sync_output_frame(ITesting *t) {
    TerminalController c;
    c.Resize(20, 5);
    std::atomic<int> redraws = 0;
    c.SetRedrawTrigger([&redraws]() { redraws++; });
    c.SetMaxSyncHold(std::chrono::milliseconds(100));
    FeedPty(c, "before");
    TR_ASSERT(t, redraws == 1);

    // The update's start and its first output in one read: the held frame is cut at the mode change
    FeedPty(c, "\x1b[?2026h\r\nheld");
    TR_ASSERT(t, redraws == 1);
    {
        std::lock_guard<std::mutex> guard(c.GetScreenLock());
        TR_ASSERT(t, c.IsHoldingRedraw());
        auto frame = c.GetScreen().GetHeldFrame();
        TR_ASSERT(t, frame != nullptr);
        TR_ASSERT(t, frame->GetRow(0)[0].ch == U'b');
        TR_ASSERT(t, frame->GetRow(1)[0].ch == U' ');
        TR_ASSERT(t, frame->cursor.y == 0);
        TR_ASSERT(t, c.GetScreen().GetRow(1)[0].ch == U'h');     // the grid itself has it
    }

    // No more output: the timer asks for the redraw once the cap is past, and the hold is over
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((redraws == 1) && (std::chrono::steady_clock::now() < deadline)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TR_ASSERT(t, redraws == 2);
    {
        std::lock_guard<std::mutex> guard(c.GetScreenLock());
        TR_ASSERT(t, !c.IsHoldingRedraw());
    }
    return kTR_Pass;
}

namespace {
    // A prompt with a command's output above it ('make' -> 'error: x'), so '.co' has something to copy. The
    // controller is the output console for the test's lifetime.
    struct DotCommandFixture {
        TerminalController c;
        std::string sent;
        IOutputConsole *prevConsole = nullptr;

        explicit DotCommandFixture(int cols, int rows = 10, const std::string &prompt = "$ ") {
            c.Resize(cols, rows);
            CaptureInput(c, sent, false);
            prevConsole = RuntimeConfig::Instance().OutputConsole();
            c.RegisterAsOutputConsole();
            FeedPty(c, prompt);
            c.GetInputLine()->Append(std::u32string(U"make"));
            c.CommitLine();
            FeedPty(c, "make\r\nerror: x\r\n" + prompt);
            Editor::Instance().GetClipBoard().CopyText(U"before");
        }
        ~DotCommandFixture() {
            RuntimeConfig::Instance().SetOutputConsole(prevConsole);
        }
        // Type 'typed' locally, Tab (readline takes the line), then readline's echo arrives
        void TabWithEcho(const std::u32string &typed, const std::string &echo) {
            Cursor cursor;
            size_t line = 0;
            for (auto ch : typed) {
                c.HandleKeyPress(cursor, line, TextKey(ch));
            }
            EditorAction complete;
            complete.action = kAction::kActionShellCompletion;
            c.OnAction(complete);
            FeedPty(c, echo);
            sent.clear();
        }
        bool Commit() {
            EditorAction commit;
            commit.uiAction = kUIAction::kUIActionCommitLine;
            commit.keyPress = SpecialKey(Keyboard::kKeyCode_Return);
            return c.ForwardActionToShell(commit);
        }
        std::u32string ClipboardText() {
            auto top = Editor::Instance().GetClipBoard().Top();
            return (top == nullptr) ? std::u32string() : top->AsText();
        }
    };
}

// Bug #17: a dot-command finished in readline after Tab runs in the editor - readline's line is cleared (end of
// line, kill line) and an empty line gives a fresh prompt; the shell never sees '.co'. Like CommitLine, the
// dot-command goes into the history and leaves the last output alone.
DLL_EXPORT int test_terminalcontroller_tab_dotcommand(ITesting *t) {
    DotCommandFixture f(40);
    f.TabWithEcho(U".c", ".co");
    TR_ASSERT(t, f.c.DoesShellOwnLineEditing());

    TR_ASSERT(t, f.Commit());
    TR_ASSERT(t, f.sent == "\x05\x15\r");
    TR_ASSERT(t, !f.c.DoesShellOwnLineEditing());
    TR_ASSERT(t, f.ClipboardText() == U"error: x");     // '.co' ran
    TR_ASSERT(t, f.c.GetLastOutputText() == std::optional<std::u32string>(U"error: x"));   // not marked as a command

    FeedPty(f.c, "\r\x1b[K$ \r\n$ ");                   // the shell erases its line, an empty line, a fresh prompt
    EditorAction up;
    up.uiAction = kUIAction::kUIActionLineUp;
    TR_ASSERT(t, f.c.OnAction(up));
    TR_ASSERT(t, f.c.GetInputLine()->Buffer() == std::u32string(U".co"));
    return kTR_Pass;
}

// The line wrapped (the cursor left the prompt's row): what we read back is no longer the whole line - don't run it,
// let the shell have its own line
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_wrapped(ITesting *t) {
    DotCommandFixture f(12);
    f.TabWithEcho(U".c", ".co aaaa");                  // still on the prompt's row: a good mirror, a '.co' line
    FeedPty(f.c, "aaaaaaaaaa");                         // ...then it wraps
    f.sent.clear();
    TR_ASSERT(t, f.Commit());
    TR_ASSERT(t, f.sent == "\r");
    TR_ASSERT(t, f.ClipboardText() == U"before");       // nothing ran
    return kTR_Pass;
}

// The line fills the prompt's row to the last column: the cursor is still on the row (pending wrap) but the line
// may go on below - not trusted either
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_fullrow(ITesting *t) {
    DotCommandFixture f(12);
    f.TabWithEcho(U".c", ".co aaaaaa");                // '$ ' + 10 = 12 columns
    TR_ASSERT(t, f.Commit());
    TR_ASSERT(t, f.sent == "\r");
    TR_ASSERT(t, f.ClipboardText() == U"before");
    return kTR_Pass;
}

// What the shell draws after the cursor - zsh's RPROMPT, an autosuggestion - is not part of the command
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_rprompt(ITesting *t) {
    DotCommandFixture f(40);
    f.TabWithEcho(U".c", ".co\x1b" "7\x1b[34G12:30\x1b" "8");   // save cursor, RPROMPT at column 34, restore
    TR_ASSERT(t, f.Commit());
    TR_ASSERT(t, f.sent == "\x05\x15\r");
    TR_ASSERT(t, f.ClipboardText() == U"error: x");
    EditorAction up;
    up.uiAction = kUIAction::kUIActionLineUp;
    TR_ASSERT(t, f.c.OnAction(up));
    TR_ASSERT(t, f.c.GetInputLine()->Buffer() == std::u32string(U".co"));
    return kTR_Pass;
}

// The line is erased after Tab and a dot-command typed in its place - still readline's line, still a dot-command.
// zsh's own echo, captured from a pty (80x24): the Tab completion, 40 backspaces, then '.q!'.
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_retyped(ITesting *t) {
    TerminalController c;
    c.Resize(80, 24);
    std::string sent;
    CaptureInput(c, sent, false);
    FeedPty(c, "\x1b[1m\x1b[7m%\x1b[27m\x1b[1m\x1b[0m                                                                               \r \r"
               "\r\x1b[0m\x1b[27m\x1b[24m\x1b[Jgnilk@MacBook-M5 /tmp % \x1b[K\x1b[?2004h");
    Cursor cursor;
    size_t line = 0;
    for (auto ch : std::u32string(U".theme --import ~/Down")) {
        c.HandleKeyPress(cursor, line, TextKey(ch));
    }
    EditorAction complete;
    complete.action = kAction::kActionShellCompletion;
    c.OnAction(complete);
    FeedPty(c, ".\x08.theme --import ~/Downloads\x1b[1m/\x1b[0m");
    TR_ASSERT(t, c.GetInputLine()->Buffer() == std::u32string(U".theme --import ~/Downloads/"));

    for (int i = 0; i < 40; i++) {
        c.HandleKeyPress(cursor, line, SpecialKey(Keyboard::kKeyCode_Backspace));
    }
    FeedPty(c, "\x08\x1b[0m \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 "
               "\x08\x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08 \x08\x08\x08 \x08\x08 \x08\x08 \x08\x08 "
               "\x08\x08\x08. \x08\x08 \x08");
    TR_ASSERT(t, c.GetInputLine()->Buffer().empty());
    for (auto ch : std::u32string(U".q!")) {
        c.HandleKeyPress(cursor, line, TextKey(ch));
    }
    FeedPty(c, ".\x08.q!");
    TR_ASSERT(t, c.GetInputLine()->Buffer() == std::u32string(U".q!"));
    TR_ASSERT(t, PluginExecutor::IsPluginCommandLine(U".q!"));
    return kTR_Pass;
}

// The prompt on the bottom row, the line wraps: the screen scrolls and the prompt moves up a row. Erased back to a
// dot-command that fits, the line is whole again - the anchor follows the prompt (an abs row), not a grid row.
// Found in the GUI: a narrow terminal panel, '.theme --import ~/Down' + Tab, erased, '.q!' -> zsh: command not found.
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_scrolled(ITesting *t) {
    TerminalController c;
    c.Resize(12, 4);
    std::string sent;
    CaptureInput(c, sent, false);
    auto *prevConsole = RuntimeConfig::Instance().OutputConsole();
    c.RegisterAsOutputConsole();
    FeedPty(c, "$ ");
    c.GetInputLine()->Append(std::u32string(U"make"));
    c.CommitLine();
    FeedPty(c, "make\r\nerror: x\r\nb\r\n$ ");          // the prompt on the bottom row (3)
    Editor::Instance().GetClipBoard().CopyText(U"before");

    Cursor cursor;
    size_t line = 0;
    c.HandleKeyPress(cursor, line, TextKey(U'.'));
    EditorAction complete;
    complete.action = kAction::kActionShellCompletion;
    c.OnAction(complete);
    FeedPty(c, ".co aaaaaaaa");                         // 14 columns: wraps, the screen scrolls, the prompt is row 2
    TR_ASSERT(t, c.GetScreen().GetCursorPos().y == 3);
    FeedPty(c, "\x1b[A\x1b[6G\x1b[J");                  // erased back to '.co': up to the prompt's row, clear below
    sent.clear();

    EditorAction commit;
    commit.uiAction = kUIAction::kUIActionCommitLine;
    commit.keyPress = SpecialKey(Keyboard::kKeyCode_Return);
    TR_ASSERT(t, c.ForwardActionToShell(commit));
    RuntimeConfig::Instance().SetOutputConsole(prevConsole);
    TR_ASSERT(t, sent == "\x05\x15\r");
    auto top = Editor::Instance().GetClipBoard().Top();
    TR_ASSERT(t, (top != nullptr) && (top->AsText() == U"error: x\nb"));
    return kTR_Pass;
}

namespace {
    // Tab twice on an ambiguous path: zsh lists the candidates below the line and redraws the prompt + line under
    // the list (its output shape, captured from a pty - here with 'zsh %' as the prompt). 'redrawnPrompt' is what
    // the prompt reads when it is redrawn. On a 6-row screen the list scrolls it.
    void ListAndRetype(DotCommandFixture &f, const std::string &redrawnPrompt) {
        f.TabWithEcho(U".theme --import ~/Downloads/", ".\x08.theme --import ~/Downloads/");
        FeedPty(f.c, "\a\r\r\na.pdf\r\nb.pdf\r\nc.pdf\r\nd.pdf\r\n\x1b[0m\x1b[27m\x1b[24m\x1b[J" + redrawnPrompt +
                     ".theme --import ~/Downloads/\x1b[K");
        Cursor cursor;
        size_t line = 0;
        for (int i = 0; i < 28; i++) {
            f.c.HandleKeyPress(cursor, line, SpecialKey(Keyboard::kKeyCode_Backspace));
        }
        std::string erase;
        for (int i = 0; i < 28; i++) {
            erase += "\x08 \x08";
        }
        FeedPty(f.c, erase);
        for (auto ch : std::u32string(U".co")) {
            f.c.HandleKeyPress(cursor, line, TextKey(ch));
        }
        FeedPty(f.c, ".\x08.co");
        f.sent.clear();
    }
}

// Bug #17, found in the GUI: '.theme --import ~/Downloads/' + Tab Tab lists the files and redraws the prompt below
// them; erased and retyped as a dot-command, it is still one - the anchor moves to the redrawn prompt
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_listed(ITesting *t) {
    DotCommandFixture f(60, 6, "zsh % ");
    ListAndRetype(f, "zsh % ");
    TR_ASSERT(t, f.c.GetInputLine()->Buffer() == std::u32string(U".co"));
    TR_ASSERT(t, f.Commit());
    TR_ASSERT(t, f.sent == "\x05\x15\r");
    // '.co' ran - the output since 'make' runs to the cursor, the listing included
    TR_ASSERT(t, f.ClipboardText().starts_with(U"error: x"));
    return kTR_Pass;
}

// A prompt redrawn with other text (a clock, a git status that changed) is not recognised: the line can't be read
// back - the shell gets its own line
DLL_EXPORT int test_terminalcontroller_tab_dotcommand_listed_newprompt(ITesting *t) {
    DotCommandFixture f(60, 6, "zsh % ");
    ListAndRetype(f, "zsh # ");
    TR_ASSERT(t, f.Commit());
    TR_ASSERT(t, f.sent == "\r");
    TR_ASSERT(t, f.ClipboardText() == U"before");
    return kTR_Pass;
}

// Terminal tabs (hitlist #53): ids name a tab's files - the first terminal "" (its files as before tabs), then the
// first free number; the label is the title the shell / an app set, cut, else the name
DLL_EXPORT int test_terminalcontroller_panel_names(ITesting *t) {
    TR_ASSERT(t, PanelView::NextId({""}) == "2");
    TR_ASSERT(t, PanelView::NextId({"", "2"}) == "3");
    TR_ASSERT(t, PanelView::NextId({"", "3"}) == "2");
    TR_ASSERT(t, PanelView::NextId({}) == "2");
    TR_ASSERT(t, PanelView::NameFor("") == "Shell");
    TR_ASSERT(t, PanelView::NameFor("3") == "Shell 3");
    TR_ASSERT(t, PanelView::LabelFor("Shell 2", "") == U"Shell 2");
    TR_ASSERT(t, PanelView::LabelFor("Shell", "vim a.cpp") == U"vim a.cpp");
    auto cut = PanelView::LabelFor("Shell", "user@host: ~/src/github.com/editor/src");
    TR_ASSERT(t, cut.size() == PanelView::MAX_LABEL);
    TR_ASSERT(t, cut.back() == U'…');
    return kTR_Pass;
}

// OSC 0 / 2 set the title; the handler is told once per change
DLL_EXPORT int test_terminalcontroller_title(ITesting *t) {
    TerminalController controller;
    controller.SetPtyWriter([](std::string_view) {});
    controller.SetRedrawTrigger([]() {});
    controller.Resize(40, 10);
    int numChanges = 0;
    controller.SetTitleChangedHandler([&numChanges]() { numChanges++; });
    TR_ASSERT(t, controller.GetTitle().empty());
    std::string osc = "\x1b]0;vim a.cpp\x07";
    controller.HandleTerminalData((const uint8_t *)osc.data(), osc.size());
    TR_ASSERT(t, controller.GetTitle() == "vim a.cpp");
    TR_ASSERT(t, numChanges == 1);
    controller.HandleTerminalData((const uint8_t *)osc.data(), osc.size());
    TR_ASSERT(t, numChanges == 1);
    std::string osc2 = "\x1b]2;other\x1b\\";
    controller.HandleTerminalData((const uint8_t *)osc2.data(), osc2.size());
    TR_ASSERT(t, controller.GetTitle() == "other");
    TR_ASSERT(t, numChanges == 2);
    return kTR_Pass;
}

// Hitlist #52 (Linux report 2026-11-19): a build that starts, prints and ends between two reads was never seen running,
// so the run never ended and "build: ..." never showed. The shell's ;D in the bytes ends it - after those bytes are read.
DLL_EXPORT int test_terminalcontroller_build_fastend(ITesting *t) {
    TerminalController c;
    c.Resize(60, 10);
    FeedPty(c, "\x1b]133;A\x07" "$ " "\x1b]133;B\x07");
    c.GetInputLine()->Append(std::u32string(U"cc -c bad.c"));
    c.CommitLine();
    auto &build = Editor::Instance().GetBuildMessages();
    TR_ASSERT(t, build.IsRunning());
    // One read: the echo, the error, the end, the next prompt
    FeedPty(c, "cc -c bad.c\r\n" "\x1b]133;C\x07" "bad.c:2:16: error: use of undeclared identifier 'nope'\r\n"
               "\x1b]133;D;1\x07" "\x1b]133;A\x07" "$ ");
    TR_ASSERT(t, !build.IsRunning());
    TR_ASSERT(t, build.NumErrors() == 1);
    return kTR_Pass;
}

// The same fast build with shell integration off (the default): no ;D, no read sees the job - the quiet check ends it.
// A job still running when the terminal goes quiet keeps the run going.
DLL_EXPORT int test_terminalcontroller_build_quietend(ITesting *t) {
    TerminalController c;
    c.Resize(60, 10);
    bool isJobRunning = false;
    c.SetForegroundJobProbe([&isJobRunning]() { return isJobRunning; });
    FeedPty(c, "$ ");
    c.GetInputLine()->Append(std::u32string(U"cc -c bad.c"));
    c.CommitLine();
    auto &build = Editor::Instance().GetBuildMessages();
    TR_ASSERT(t, build.IsRunning());
    FeedPty(c, "cc -c bad.c\r\nbad.c:2:16: error: use of undeclared identifier 'nope'\r\n$ ");
    TR_ASSERT(t, build.IsRunning());
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    TR_ASSERT(t, !build.IsRunning());
    TR_ASSERT(t, build.NumErrors() == 1);

    // A slow link: quiet, but the job runs - the run goes on until the job is gone
    c.GetInputLine()->Append(std::u32string(U"make"));
    c.CommitLine();
    isJobRunning = true;
    FeedPty(c, "make\r\ncc -o app main.o\r\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    TR_ASSERT(t, build.IsRunning());
    isJobRunning = false;
    FeedPty(c, "$ ");
    TR_ASSERT(t, !build.IsRunning());
    return kTR_Pass;
}

// An unbound Ctrl chord at the local prompt is the shell's: readline takes the line with its cursor at the caret, then
// the key (Ctrl+W deletes the word before the caret). Ctrl+D stays out - on an empty line it would end the shell.
DLL_EXPORT int test_terminalcontroller_ctrl_to_shell(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    std::string sent;
    CaptureInput(c, sent, false);
    FeedPty(c, "$ ");

    Cursor cursor;
    size_t line = 0;
    for (auto ch : std::u32string(U"echo one two")) {
        c.HandleKeyPress(cursor, line, TextKey(ch));
    }
    TR_ASSERT(t, sent.empty());
    // Ctrl+D: not handed over
    TR_ASSERT(t, !c.HandleKeyPress(cursor, line, TextKey(U'd', Keyboard::kMod_LeftCtrl)));
    TR_ASSERT(t, !c.DoesShellOwnLineEditing());
    TR_ASSERT(t, sent.empty());

    // The caret after 'one' (four left): the line, readline's cursor four back, then Ctrl+W
    EditorAction lineLeft;
    lineLeft.uiAction = kUIAction::kUIActionLineLeft;
    for (int i = 0; i < 4; i++) {
        c.OnAction(lineLeft);
    }
    TR_ASSERT(t, sent.empty());
    TR_ASSERT(t, c.HandleKeyPress(cursor, line, TextKey(U'w', Keyboard::kMod_LeftCtrl)));
    TR_ASSERT(t, c.DoesShellOwnLineEditing());
    std::string left = "\x1b[D";
    TR_ASSERT(t, sent == "echo one two" + left + left + left + left + "\x17");

    // From here readline has it: the next one goes straight through
    sent.clear();
    TR_ASSERT(t, c.HandleKeyPress(cursor, line, TextKey(U't', Keyboard::kMod_LeftCtrl)));
    TR_ASSERT(t, sent == "\x14");
    return kTR_Pass;
}

// Once a Ctrl chord has handed the line to readline, Ctrl+D on an empty line must still not reach it - it ends the shell
// and the panel is dead. With text on the line it is readline's (delete), and a program's own always.
DLL_EXPORT int test_terminalcontroller_ctrl_d_shell_owned(ITesting *t) {
    TerminalController c;
    c.Resize(40, 10);
    std::string sent;
    bool isForegroundJob = false;
    c.SetPtyWriter([&sent](std::string_view bytes) { sent += bytes; });
    c.SetForegroundJobProbe([&isForegroundJob]() { return isForegroundJob; });
    FeedPty(c, "$ ");

    Cursor cursor;
    size_t line = 0;
    auto ctrlD = TextKey(U'd', Keyboard::kMod_LeftCtrl);
    TR_ASSERT(t, c.HandleKeyPress(cursor, line, TextKey(U'u', Keyboard::kMod_LeftCtrl)));
    TR_ASSERT(t, c.DoesShellOwnLineEditing());

    // Empty: held back, and readline keeps the line
    sent.clear();
    TR_ASSERT(t, c.HandleKeyPress(cursor, line, ctrlD));
    TR_ASSERT(t, sent.empty());
    TR_ASSERT(t, c.DoesShellOwnLineEditing());

    // Text on it: readline's
    FeedPty(c, "abc");
    TR_ASSERT(t, c.HandleKeyPress(cursor, line, ctrlD));
    TR_ASSERT(t, sent == "\x04");

    // Erased again
    FeedPty(c, "\r$ \x1b[K");
    sent.clear();
    TR_ASSERT(t, c.HandleKeyPress(cursor, line, ctrlD));
    TR_ASSERT(t, sent.empty());

    // A program reading its input gets it
    isForegroundJob = true;
    TR_ASSERT(t, c.HandleKeyPress(cursor, line, ctrlD));
    TR_ASSERT(t, sent == "\x04");
    return kTR_Pass;
}

// #53 step 4 b: the terminal uses the settings it is handed (a resolved section), not the global 'terminal' section -
// an agent's tab can keep less scrollback than the shell's
DLL_EXPORT int test_terminalcontroller_settings(ITesting *t) {
    auto cfg = ConfigNode::FromString(
        "terminal:\n"
        "  scrollback_lines: 1000\n"
        "  promote_sends_enter: yes\n"
        "small:\n"
        "  inherit: terminal\n"
        "  scrollback_lines: 2\n");
    TR_ASSERT(t, cfg.has_value());
    TerminalController c;
    c.SetSettings(cfg->ResolveSection("small"));
    c.Resize(20, 2);
    for (int i = 0; i < 8; i++) {
        FeedPty(c, "line\r\n");
    }
    TR_ASSERT(t, c.GetScreen().GetScrollback()->NumLines() == 2);
    TR_ASSERT(t, c.Settings().GetBool("promote_sends_enter", false));

    // Without settings, the code's defaults
    TerminalController plain;
    TR_ASSERT(t, plain.Settings().GetInt("scrollback_lines", -1) == -1);
    return kTR_Pass;
}

// #53 step 4 d: 'panel.tabs' - a tab is a view kind and a config section (default: the view's name); a kind the panel
// doesn't have ('cmake', not yet) is left out; the first tab keeps the files from before tabs ("")
DLL_EXPORT int test_terminalcontroller_panel_configured(ITesting *t) {
    auto cfg = ConfigNode::FromString(
        "panel:\n"
        "  tabs:\n"
        "    - name: Build\n"
        "    - name: Later\n"
        "      view: cmake\n"
        "    - view: messages\n"
        "    - name: Claude\n"
        "      view: terminal\n"
        "      config: ai_agent\n"
        "    - {}\n");
    TR_ASSERT(t, cfg.has_value());
    auto specs = PanelView::ConfiguredTabs(cfg.value());
    TR_ASSERT(t, specs.size() == 4);
    TR_ASSERT(t, specs[0].id.empty() && (specs[0].name == "Build"));
    TR_ASSERT(t, (specs[0].view == "terminal") && (specs[0].config == "terminal"));
    // Step 5: a Messages tab - its name and section default to 'Messages' / 'messages'
    TR_ASSERT(t, (specs[1].view == "messages") && (specs[1].name == "Messages") && (specs[1].config == "messages"));
    TR_ASSERT(t, (specs[2].id == "3") && (specs[2].name == "Claude") && (specs[2].config == "ai_agent"));
    TR_ASSERT(t, (specs[3].id == "4") && (specs[3].name == "Shell 4"));

    // None configured: none (the panel makes one 'Shell')
    auto empty = ConfigNode::FromString("panel:\n  other: 1\n");
    TR_ASSERT(t, PanelView::ConfiguredTabs(empty.value()).empty());
    return kTR_Pass;
}


// 'output_console: no' (the Editor Agent's tab): registering does nothing - dot-command output never goes into an agent
DLL_EXPORT int test_terminalcontroller_not_output_console(ITesting *t) {
    auto *prevConsole = RuntimeConfig::Instance().OutputConsole();
    TerminalController agent;
    auto settings = ConfigNode::FromString("output_console: no\n");
    TR_ASSERT(t, settings.has_value());
    agent.SetSettings(settings.value());
    agent.RegisterAsOutputConsole();
    TR_ASSERT(t, RuntimeConfig::Instance().OutputConsole() == prevConsole);

    TerminalController shell;
    shell.RegisterAsOutputConsole();
    TR_ASSERT(t, RuntimeConfig::Instance().OutputConsole() != prevConsole);
    RuntimeConfig::Instance().SetOutputConsole(prevConsole);
    return kTR_Pass;
}

// An app has settled (agent-integration.md §6.3) when it reads a paste (?2004) and the pty has been quiet for the time
// asked - never before it turned bracketed paste on; new output starts the quiet time over. Focus is remembered.
DLL_EXPORT int test_terminalcontroller_settled(ITesting *t) {
    using namespace std::chrono_literals;
    TerminalController c;
    c.Resize(40, 10);
    std::string sent;
    CaptureInput(c, sent, true);
    FeedPty(c, "starting...");
    TR_ASSERT(t, !c.IsSettled(0ms));
    FeedPty(c, "\x1b[?2004h> ");
    TR_ASSERT(t, c.IsSettled(0ms));
    TR_ASSERT(t, !c.IsSettled(10s));
    std::this_thread::sleep_for(60ms);
    TR_ASSERT(t, c.IsSettled(50ms));
    FeedPty(c, "x");
    TR_ASSERT(t, !c.IsSettled(50ms));
    FeedPty(c, "\x1b[?2004l");
    TR_ASSERT(t, !c.IsSettled(0ms));

    TR_ASSERT(t, !c.HasFocus());
    c.SetFocus(true);
    TR_ASSERT(t, c.HasFocus());
    c.SetFocus(false);
    TR_ASSERT(t, !c.HasFocus());
    return kTR_Pass;
}
