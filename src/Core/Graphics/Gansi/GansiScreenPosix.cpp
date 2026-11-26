//
// GansiScreen::Create - the screen on the POSIX terminal the editor runs in (the ANSI backend). Its own file: the rest
// of the Gansi sources are built on every platform (the headless screen draws into a GansiScreen), this one only where
// there is a POSIX terminal (GEDIT_BUILD_ANSI).
//

#include "Core/Graphics/Gansi/GansiScreen.h"
#include "gansi/Capabilities.h"
#include "gansi/PosixTerminalIO.h"

using namespace gedit;
using namespace gedit::Gansi;

ScreenBase::Ref GansiScreen::Create() {
    auto io = std::make_unique<gnilk::ansi::PosixTerminalIO>();
    // Detect what this terminal actually supports (TERM / TERM_PROGRAM / COLORTERM) so we never emit a
    // sequence it mis-handles — e.g. Apple_Terminal gets no Kitty keyboard / focus / SGR mouse / OSC 52
    // and falls back to 256-colour, which renders correctly there.
    auto caps = gnilk::ansi::Capabilities::Detect();
    auto term = std::make_unique<gnilk::ansi::Terminal>(std::move(io), caps);
    return std::make_shared<GansiScreen>(std::move(term));
}
