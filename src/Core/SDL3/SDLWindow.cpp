//
// Created by gnilk on 29.03.23.
//
// Note about coordinate translation...
// We keep EVERYTHING in editor coordinate (character row/col) until we must communicate with SDL
//
//

#include "SDLWindow.h"
#include "SDLDrawContext.h"
#include "SDLTranslate.h"
#include "SDLScreen.h"
#include "SDLFontManager.h"
#include <SDL3/SDL.h>

#include "ext/stbttf.h"

using namespace gedit;

SDLWindow::SDLWindow(const Rect &rect, SDLWindow *other) : WindowBase(rect) {
    caption = other->caption;
    flags = other->flags;
    decorationFlags = other->decorationFlags;
}

SDLWindow::~SDLWindow() noexcept {
    if (windowBackBuffer != nullptr) {
        SDL_DestroyTexture(windowBackBuffer);
    }
    if (clientBackBuffer != nullptr) {
        SDL_DestroyTexture(clientBackBuffer);
    }
    if (drawContext != nullptr) {
        delete drawContext;
    }
}

void SDLWindow::Initialize(WindowBase::kWinFlags flags, WindowBase::kWinDecoration newDecoFlags) {
    WindowBase::Initialize(flags, newDecoFlags);

    if (flags & WindowBase::kWin_Visible) {
        CreateSDLBackBuffer();
    }
}
void SDLWindow::CreateSDLBackBuffer() {
    if (windowBackBuffer != nullptr) {
        SDL_DestroyTexture(windowBackBuffer);
    }

    auto winPixRect = SDLTranslate::RowColToPixel(windowRect);

    windowBackBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, winPixRect.Width(), winPixRect.Height());
    winptr = windowBackBuffer;
    if (drawContext) {
        delete drawContext;
    }
    drawContext = new SDLDrawContext(windowBackBuffer, windowRect);

    // Now the client window
    if (clientBackBuffer != nullptr) {
        SDL_DestroyTexture(clientBackBuffer);
    }
    auto clientRect = windowRect;
    if (decorationFlags & kWinDeco_Border) {
        clientRect.Deflate(SDLTranslate::ColToXPos(1),SDLTranslate::RowToYPos(1));
    }
    auto clientPixRect = SDLTranslate::RowColToPixel(clientRect);

    clientBackBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, clientPixRect.Width(), clientPixRect.Height());
    if (clientContext != nullptr) {
        delete clientContext;
    }
    clientContext = new SDLDrawContext(clientBackBuffer, clientRect);


}

DrawContext &SDLWindow::GetContentDC() {
    if (clientContext == nullptr) {
        clientContext = new SDLDrawContext(nullptr, windowRect);
    }
    return *clientContext;
}

void SDLWindow::Clear() {
    SDL_SetRenderTarget(renderer, windowBackBuffer);
//    SDL_SetRenderDrawColor(renderer, 46, 54, 62, 255);
    SDL_SetRenderDrawColor(renderer, 255, 54, 62, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);
}

void SDLWindow::DrawWindowDecoration() {
    if (flags & WindowBase::kWin_Invisible) {
        return;
    }
    SDL_SetRenderTarget(renderer, windowBackBuffer);

    Point pxTopLeft = {0,0}; //SDLTranslate::RowColToPixel(windowRect.TopLeft());
    Point pxBottomRight {windowRect.Width(), windowRect.Height()};
    pxBottomRight = SDLTranslate::RowColToPixel(pxBottomRight);

    // FIXME: Figure out which color is the windowing color (we don't have that themed)
    SDL_SetRenderDrawColor(renderer, 255,255,255,255);

    // NOTE: We can have a much more filled border - as the border is always one char...
    if (decorationFlags & kWinDeco_TopBorder) {
        SDL_RenderLine(renderer, pxTopLeft.x, pxTopLeft.y, pxBottomRight.x-1, pxTopLeft.y);
    }
    if (decorationFlags & kWinDeco_BottomBorder) {
        SDL_RenderLine(renderer, pxTopLeft.x, pxBottomRight.y-1, pxBottomRight.x-1, pxBottomRight.y-1);
    }
    if (decorationFlags & kWinDeco_LeftBorder) {
        SDL_RenderLine(renderer, pxTopLeft.x, pxTopLeft.y, pxTopLeft.x, pxBottomRight.y-2);
    }
    if (decorationFlags & kWinDeco_RightBorder) {
        SDL_RenderLine(renderer, pxBottomRight.x-1, pxTopLeft.y, pxBottomRight.x-1, pxBottomRight.y-1);
    }

    if (decorationFlags & kWinDeco_DrawCaption) {
        // FIXME: Should fill a bloody rectangle
        // Need a font-class to store this - should be initalized by screen...
        auto font = SDLFontManager::Instance().GetActiveFont();
        STBTTF_RenderText(renderer, font, 0, font->size * 1, caption.c_str());
    }

    SDL_RenderLine(renderer, pxTopLeft.x, pxTopLeft.y, pxBottomRight.x, pxBottomRight.y);


    SDL_SetRenderTarget(renderer, nullptr);

}
void SDLWindow::Refresh() {
    auto pixRect = SDLTranslate::RowColToPixel(windowRect);
    SDL_FRect dstRect = {(float)pixRect.TopLeft().x, (float)pixRect.TopLeft().y, (float)pixRect.Width(), (float)pixRect.Height()};

    SDL_SetRenderTarget(renderer, nullptr);
    SDL_RenderTexture(renderer, windowBackBuffer, nullptr, &dstRect);
}

void SDLWindow::SetCursor(const Cursor &cursor) {
}

