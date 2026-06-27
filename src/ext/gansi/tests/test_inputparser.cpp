//
// gansi InputParser tests — byte-stream -> Event tables. Covers UTF-8 (incl. split reads), C0
// controls, CSI/SS3 special keys with modifier params, Kitty CSI u, SGR mouse, focus, bracketed
// paste (incl. split + embedded escapes), and the lone-ESC flush path.
//
#include <testinterface.h>

#include <string>
#include <vector>

#include "gansi/InputParser.h"

using namespace gnilk::ansi;

extern "C" {
DLL_EXPORT int test_inputparser(ITesting *t);
DLL_EXPORT int test_inputparser_ascii(ITesting *t);
DLL_EXPORT int test_inputparser_utf8(ITesting *t);
DLL_EXPORT int test_inputparser_utf8_split(ITesting *t);
DLL_EXPORT int test_inputparser_controls(ITesting *t);
DLL_EXPORT int test_inputparser_ctrl_letter(ITesting *t);
DLL_EXPORT int test_inputparser_arrows(ITesting *t);
DLL_EXPORT int test_inputparser_shift_arrow(ITesting *t);
DLL_EXPORT int test_inputparser_ctrlalt_arrow(ITesting *t);
DLL_EXPORT int test_inputparser_homeend(ITesting *t);
DLL_EXPORT int test_inputparser_tilde_keys(ITesting *t);
DLL_EXPORT int test_inputparser_fkeys(ITesting *t);
DLL_EXPORT int test_inputparser_ss3(ITesting *t);
DLL_EXPORT int test_inputparser_kitty(ITesting *t);
DLL_EXPORT int test_inputparser_split_csi(ITesting *t);
DLL_EXPORT int test_inputparser_lone_esc(ITesting *t);
DLL_EXPORT int test_inputparser_alt_char(ITesting *t);
DLL_EXPORT int test_inputparser_mouse(ITesting *t);
DLL_EXPORT int test_inputparser_mouse_wheel(ITesting *t);
DLL_EXPORT int test_inputparser_mouse_mods(ITesting *t);
DLL_EXPORT int test_inputparser_focus(ITesting *t);
DLL_EXPORT int test_inputparser_paste(ITesting *t);
DLL_EXPORT int test_inputparser_paste_split(ITesting *t);
DLL_EXPORT int test_inputparser_paste_escapes(ITesting *t);
}

// --- helpers ---
static std::vector<Event> Parse(const std::string &bytes) {
    InputParser p;
    std::vector<Event> out;
    p.Feed(bytes, out);
    return out;
}

static const KeyEvent *AsKey(const Event &e) {
    return std::holds_alternative<KeyEvent>(e) ? &std::get<KeyEvent>(e) : nullptr;
}
static const MouseEvent *AsMouse(const Event &e) {
    return std::holds_alternative<MouseEvent>(e) ? &std::get<MouseEvent>(e) : nullptr;
}

DLL_EXPORT int test_inputparser(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_ascii(ITesting *t) {
    auto ev = Parse("abc");
    TR_ASSERT(t, ev.size() == 3);
    for (int i = 0; i < 3; ++i) {
        auto k = AsKey(ev[i]);
        TR_ASSERT(t, k != nullptr);
        TR_ASSERT(t, k->isChar);
        TR_ASSERT(t, k->ch == static_cast<char32_t>("abc"[i]));
        TR_ASSERT(t, k->mods == Mod::None);
    }
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_utf8(ITesting *t) {
    auto ev = Parse("\xc3\xa9");   // é
    TR_ASSERT(t, ev.size() == 1);
    auto k = AsKey(ev[0]);
    TR_ASSERT(t, k && k->isChar && k->ch == U'é');
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_utf8_split(ITesting *t) {
    // € = E2 82 AC, fed one byte at a time — must emit exactly once when complete.
    InputParser p;
    std::vector<Event> out;
    p.Feed(std::string("\xe2"), out);
    TR_ASSERT(t, out.empty());
    p.Feed(std::string("\x82"), out);
    TR_ASSERT(t, out.empty());
    p.Feed(std::string("\xac"), out);
    TR_ASSERT(t, out.size() == 1);
    auto k = AsKey(out[0]);
    TR_ASSERT(t, k && k->isChar && k->ch == U'€');
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_controls(ITesting *t) {
    {
        auto ev = Parse("\r");
        TR_ASSERT(t, ev.size() == 1 && AsKey(ev[0]) && AsKey(ev[0])->key == Key::Enter);
    }
    {
        auto ev = Parse("\t");
        TR_ASSERT(t, ev.size() == 1 && AsKey(ev[0]) && AsKey(ev[0])->key == Key::Tab);
    }
    {
        auto ev = Parse("\x7f");
        TR_ASSERT(t, ev.size() == 1 && AsKey(ev[0]) && AsKey(ev[0])->key == Key::Backspace);
    }
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_ctrl_letter(ITesting *t) {
    auto ev = Parse("\x01");   // Ctrl-A
    TR_ASSERT(t, ev.size() == 1);
    auto k = AsKey(ev[0]);
    TR_ASSERT(t, k && k->isChar && k->ch == U'a' && k->mods == Mod::Ctrl);
    return kTR_Pass;
}

static bool ArrowIs(const std::vector<Event> &ev, Key key, Mod mods) {
    if (ev.size() != 1) return false;
    auto k = std::holds_alternative<KeyEvent>(ev[0]) ? &std::get<KeyEvent>(ev[0]) : nullptr;
    return k && !k->isChar && k->key == key && k->mods == mods;
}

DLL_EXPORT int test_inputparser_arrows(ITesting *t) {
    TR_ASSERT(t, ArrowIs(Parse("\x1b[A"), Key::Up, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[B"), Key::Down, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[C"), Key::Right, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[D"), Key::Left, Mod::None));
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_shift_arrow(ITesting *t) {
    TR_ASSERT(t, ArrowIs(Parse("\x1b[1;2A"), Key::Up, Mod::Shift));
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_ctrlalt_arrow(ITesting *t) {
    // modifier param 7 -> mask 6 -> Alt|Ctrl
    TR_ASSERT(t, ArrowIs(Parse("\x1b[1;7C"), Key::Right, Mod::Alt | Mod::Ctrl));
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_homeend(ITesting *t) {
    TR_ASSERT(t, ArrowIs(Parse("\x1b[H"), Key::Home, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[F"), Key::End, Mod::None));
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_tilde_keys(ITesting *t) {
    TR_ASSERT(t, ArrowIs(Parse("\x1b[2~"), Key::Insert, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[3~"), Key::Delete, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[5~"), Key::PageUp, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[6~"), Key::PageDown, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[1~"), Key::Home, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[4~"), Key::End, Mod::None));
    // modified: Ctrl-Delete (param2 = 5 -> mask 4 -> Ctrl)
    TR_ASSERT(t, ArrowIs(Parse("\x1b[3;5~"), Key::Delete, Mod::Ctrl));
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_fkeys(ITesting *t) {
    TR_ASSERT(t, ArrowIs(Parse("\x1b[15~"), Key::F5, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[17~"), Key::F6, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1b[24~"), Key::F12, Mod::None));
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_ss3(ITesting *t) {
    TR_ASSERT(t, ArrowIs(Parse("\x1bOA"), Key::Up, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1bOP"), Key::F1, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1bOS"), Key::F4, Mod::None));
    TR_ASSERT(t, ArrowIs(Parse("\x1bOH"), Key::Home, Mod::None));
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_kitty(ITesting *t) {
    {   // codepoint 97 = 'a'
        auto ev = Parse("\x1b[97u");
        TR_ASSERT(t, ev.size() == 1);
        auto k = AsKey(ev[0]);
        TR_ASSERT(t, k && k->isChar && k->ch == U'a' && k->mods == Mod::None);
    }
    {   // 'a' with Shift (mods=2)
        auto ev = Parse("\x1b[97;2u");
        TR_ASSERT(t, ev.size() == 1);
        auto k = AsKey(ev[0]);
        TR_ASSERT(t, k && k->isChar && k->ch == U'a' && k->mods == Mod::Shift);
    }
    {   // Enter via CSI u
        auto ev = Parse("\x1b[13u");
        TR_ASSERT(t, ArrowIs(ev, Key::Enter, Mod::None));
    }
    {   // Escape via CSI u
        auto ev = Parse("\x1b[27u");
        TR_ASSERT(t, ArrowIs(ev, Key::Escape, Mod::None));
    }
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_split_csi(ITesting *t) {
    InputParser p;
    std::vector<Event> out;
    p.Feed(std::string("\x1b["), out);
    TR_ASSERT(t, out.empty());           // incomplete CSI buffered
    p.Feed(std::string("A"), out);
    TR_ASSERT(t, out.size() == 1);
    auto k = AsKey(out[0]);
    TR_ASSERT(t, k && k->key == Key::Up);
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_lone_esc(ITesting *t) {
    InputParser p;
    std::vector<Event> out;
    p.Feed(std::string("\x1b"), out);
    TR_ASSERT(t, out.empty());           // ESC held pending
    p.Flush(out);
    TR_ASSERT(t, out.size() == 1);
    auto k = AsKey(out[0]);
    TR_ASSERT(t, k && k->key == Key::Escape);
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_alt_char(ITesting *t) {
    auto ev = Parse("\x1b" "a");          // ESC a -> Alt+a (legacy fallback)
    TR_ASSERT(t, ev.size() == 1);
    auto k = AsKey(ev[0]);
    TR_ASSERT(t, k && k->isChar && k->ch == U'a' && k->mods == Mod::Alt);
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_mouse(ITesting *t) {
    {   // press left at col 10 row 5 (1-based) -> 0-based (9,4)
        auto ev = Parse("\x1b[<0;10;5M");
        TR_ASSERT(t, ev.size() == 1);
        auto m = AsMouse(ev[0]);
        TR_ASSERT(t, m && m->kind == MouseEvent::Kind::Press && m->button == 0 && m->col == 9 && m->row == 4);
    }
    {   // release
        auto ev = Parse("\x1b[<0;10;5m");
        TR_ASSERT(t, ev.size() == 1);
        auto m = AsMouse(ev[0]);
        TR_ASSERT(t, m && m->kind == MouseEvent::Kind::Release);
    }
    {   // motion (button byte 35 = 32 motion + 3 no-button)
        auto ev = Parse("\x1b[<35;3;3M");
        TR_ASSERT(t, ev.size() == 1);
        auto m = AsMouse(ev[0]);
        TR_ASSERT(t, m && m->kind == MouseEvent::Kind::Move);
    }
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_mouse_wheel(ITesting *t) {
    {   // wheel up (button 64)
        auto ev = Parse("\x1b[<64;1;1M");
        TR_ASSERT(t, ev.size() == 1);
        auto m = AsMouse(ev[0]);
        TR_ASSERT(t, m && m->kind == MouseEvent::Kind::Wheel && m->wheel == 1);
    }
    {   // wheel down (button 65)
        auto ev = Parse("\x1b[<65;1;1M");
        TR_ASSERT(t, ev.size() == 1);
        auto m = AsMouse(ev[0]);
        TR_ASSERT(t, m && m->kind == MouseEvent::Kind::Wheel && m->wheel == -1);
    }
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_mouse_mods(ITesting *t) {
    // button byte 4 = shift modifier + button 0
    auto ev = Parse("\x1b[<4;2;2M");
    TR_ASSERT(t, ev.size() == 1);
    auto m = AsMouse(ev[0]);
    TR_ASSERT(t, m && m->button == 0 && HasMod(m->mods, Mod::Shift));
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_focus(ITesting *t) {
    {
        auto ev = Parse("\x1b[I");
        TR_ASSERT(t, ev.size() == 1 && std::holds_alternative<FocusEvent>(ev[0]));
        TR_ASSERT(t, std::get<FocusEvent>(ev[0]).focused);
    }
    {
        auto ev = Parse("\x1b[O");
        TR_ASSERT(t, ev.size() == 1 && std::holds_alternative<FocusEvent>(ev[0]));
        TR_ASSERT(t, !std::get<FocusEvent>(ev[0]).focused);
    }
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_paste(ITesting *t) {
    auto ev = Parse("\x1b[200~hello\x1b[201~");
    TR_ASSERT(t, ev.size() == 1);
    TR_ASSERT(t, std::holds_alternative<PasteEvent>(ev[0]));
    TR_ASSERT(t, std::get<PasteEvent>(ev[0]).text == U"hello");
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_paste_split(ITesting *t) {
    InputParser p;
    std::vector<Event> out;
    p.Feed(std::string("\x1b[200~hel"), out);
    TR_ASSERT(t, out.empty());
    p.Feed(std::string("lo wor"), out);
    TR_ASSERT(t, out.empty());
    p.Feed(std::string("ld\x1b[201~"), out);
    TR_ASSERT(t, out.size() == 1);
    TR_ASSERT(t, std::holds_alternative<PasteEvent>(out[0]));
    TR_ASSERT(t, std::get<PasteEvent>(out[0]).text == U"hello world");
    return kTR_Pass;
}

DLL_EXPORT int test_inputparser_paste_escapes(ITesting *t) {
    // Escape sequences inside a paste are literal text, NOT interpreted.
    auto ev = Parse("\x1b[200~a\x1b[Ab\x1b[201~");
    TR_ASSERT(t, ev.size() == 1);
    TR_ASSERT(t, std::holds_alternative<PasteEvent>(ev[0]));
    std::u32string expected = U"a";
    expected += static_cast<char32_t>(0x1b);
    expected += U"[Ab";
    TR_ASSERT(t, std::get<PasteEvent>(ev[0]).text == expected);
    return kTR_Pass;
}
