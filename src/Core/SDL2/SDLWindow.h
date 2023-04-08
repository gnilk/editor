//
// Created by gnilk on 29.03.23.
//

#ifndef STBMEETSDL_SDLWINDOW_H
#define STBMEETSDL_SDLWINDOW_H

#include <SDL2/SDL.h>

#include "SDLScreen.h"
#include "Core/WindowBase.h"
#include "Core/Rect.h"
#include "Core/Cursor.h"

namespace gedit {
    class SDLWindow : public WindowBase {
        friend SDLScreen;
    public:
        SDLWindow() = default;
        explicit SDLWindow(const Rect &rect) : WindowBase(rect) {
        }
        SDLWindow(const Rect &rect, SDLWindow *other);

        virtual ~SDLWindow() noexcept;

        void Initialize(WindowBase::kWinFlags flags, WindowBase::kWinDecoration newDecoFlags) override;

        DrawContext &GetContentDC() override;
        void Clear() override;

        void Refresh() override;

        void TestRefreshEx() override {
        }

        void SetCursor(const Cursor &newCursor) override;
        void DrawWindowDecoration() override;
    protected:
        void Update(const gedit::Rect &newRect, WindowBase::kWinFlags newFlags, WindowBase::kWinDecoration newDecoFlags);
        void CreateSDLBackBuffer();
        void OnDrawCursor(const Cursor &cursor);
    protected:
        // Assigned by SDLScreen when window is created...
        SDL_Renderer *renderer = nullptr;
    private:
        Cursor cursor;
        DrawContext *clientContext = nullptr;
        SDL_Texture *windowBackBuffer = nullptr;    // the 'window' is a texture
        SDL_Texture *clientBackBuffer = nullptr;
    };
}


#endif //STBMEETSDL_SDLWINDOW_H
