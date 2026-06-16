//
// Created by gnilk on 28.05.2026.
//

#ifndef GOATEDIT_HLSCREEN_H
#define GOATEDIT_HLSCREEN_H

#include "Core/UI/Graphics/ScreenBase.h"
#include "Core/UI/Graphics/WindowBase.h"
#include "Core/UI/Graphics/DrawContext.h"
#include "Core/UI/Graphics/KeyboardDriverBase.h"

namespace gedit {
    class HLWindow : public WindowBase {
    public:
        HLWindow(const gedit::Rect &rect) : WindowBase(rect) {
            dummyDC = DrawContext(rect);
            drawContext = &dummyDC;
        }
        virtual ~HLWindow() = default;
    private:
        DrawContext dummyDC;
    };

    class HLScreen : public ScreenBase {
    public:
        HLScreen() = default;
        ~HLScreen() = default;

        bool Open() override { return true; }

        static ScreenBase::Ref Create() {
            return std::make_shared<HLScreen>();
        }

        WindowBase *CreateWindow(const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override {
            auto win = new HLWindow(rect);
            return win;
        }
        WindowBase *UpdateWindow(WindowBase *window, const gedit::Rect &rect, WindowBase::kWinFlags flags, WindowBase::kWinDecoration decoFlags) override {
            return window;
        }

        Rect Dimensions() override {
            return Rect(100,100);
        };

    };

    class HLKeyboardDriver : public KeyboardDriverBase {
    public:
        virtual ~HLKeyboardDriver() = default;
        virtual bool Initialize() { return true; };

        static KeyboardDriverBase::Ref Create() {
            return std::make_shared<HLKeyboardDriver>();
        }
    };

    // // Not quite needed
    // class HLDrawContext : public DrawContext {
    // public:
    //     HLDrawContext() = default;
    //     ~HLDrawContext() = default;
    // };

}

#endif //GOATEDIT_HLSCREEN_H
