//
// gedit::Gansi screen — the ScreenBase implementation that composites every window into a single
// gnilk::ansi::Terminal grid and flushes it as minimal ANSI. Cells are the native unit (no pixels).
//
#ifndef GEDIT_GANSI_GANSISCREEN_H
#define GEDIT_GANSI_GANSISCREEN_H

#include <atomic>
#include <memory>

#include "Core/UI/Graphics/ScreenBase.h"
#include "Core/UI/Graphics/WindowBase.h"
#include "gansi/Event.h"
#include "gansi/Terminal.h"
#include "logger.h"

namespace gedit::Gansi {

    class GansiScreen : public ScreenBase {
    public:
        // gnilk::ansi::UserEvent code of the event WakeUp pushes (SDLScreen's wakeEventType)
        static constexpr int32_t kWakeEventCode = 1;
    public:
        explicit GansiScreen(std::unique_ptr<gnilk::ansi::Terminal> terminal);
        ~GansiScreen() override;

        // Production factory: a Terminal over the real Posix TTY.
        static ScreenBase::Ref Create();

        bool Open() override;
        void Close() override;
        void Clear() override;
        void Update() override;
        void CopyToTexture() override;
        void ClearWithTexture() override;

        Rect Dimensions() override;
        WindowBase *CreateWindow(const Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override;
        WindowBase *UpdateWindow(WindowBase *window, const Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override;
        void OnSizeChanged() override;
        void PollEvents() override;
        void WakeUp() override;

        // Called by the active window when it places the caret. Last call before Update() wins (one
        // hardware cursor), mirroring the SDL global-cursor model.
        void SetActiveCursor(int col, int row, bool visible);

        // Access the underlying library terminal (used by windows + tests).
        gnilk::ansi::Terminal *GetTerminal() { return terminal.get(); }

    private:
        void DispatchEvent(const gnilk::ansi::Event &ev);

    private:
        gnilk::ILogger *logger = nullptr;
        std::unique_ptr<gnilk::ansi::Terminal> terminal;

        int cursorCol = 0;
        int cursorRow = 0;
        bool cursorVisible = true;
        bool isOpen = false;
        // One wake in flight at a time - a busy pty posting per read doesn't write the wake pipe per post
        std::atomic<bool> isWakePending = false;
    };

}

#endif // GEDIT_GANSI_GANSISCREEN_H
