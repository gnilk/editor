//
// Created by gnilk on 29.03.23.
//
// Note about coordinate translation...
// We keep EVERYTHING in editor coordinate (character row/col) until we must communicate with SDL
//
//

#include "SDLWindow.h"
#include "SDLCursor.h"
#include "SDLDrawContext.h"
#include "SDLTranslate.h"
#include "SDLScreen.h"
#include "SDLFontManager.h"
#include "SDLCursor.h"

#include <SDL2/SDL.h>

#include "ext/stbttf.h"

using namespace gedit;

static bool glbDebugSDLWindows = false;

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

void SDLWindow::Update(const gedit::Rect &newRect, WindowBase::kWinFlags newFlags, WindowBase::kWinDecoration newDecoFlags) {
    flags = newFlags;
    decorationFlags = newDecoFlags;
    windowRect = newRect;

    // The client context is created on GetContentDC event for Invisible windows - we can ALWAYS delete it here
    if (clientContext != nullptr) {
        delete clientContext;
        clientContext = nullptr;
    }

    if (flags & WindowBase::kWin_Visible) {
        CreateSDLBackBuffer();
    }
}

void SDLWindow::CreateSDLBackBuffer() {
    if (windowBackBuffer != nullptr) {
        SDL_DestroyTexture(windowBackBuffer);
    }

    auto winPixRect = SDLTranslate::RowColToPixel(windowRect);

//    windowBackBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, winPixRect.Width(), winPixRect.Height());
//    SDL_SetTextureScaleMode(windowBackBuffer, SDL_ScaleMode::SDL_SCALEMODE_BEST);

    windowBackBuffer = nullptr;
    winptr = windowBackBuffer;
    if (drawContext) {
        delete drawContext;
    }
    drawContext = new SDLDrawContext(renderer, windowBackBuffer, windowRect);

    // Now the client window
    if (clientBackBuffer != nullptr) {
        SDL_DestroyTexture(clientBackBuffer);
    }
    auto clientRect = windowRect;
    if (decorationFlags & kWinDeco_DrawCaption) {
        clientRect.SetHeight(clientRect.Height()-1);
        clientRect.Move(0,1);
    }

    auto clientPixRect = SDLTranslate::RowColToPixel(clientRect);

    //clientBackBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, clientPixRect.Width(), clientPixRect.Height());
    //SDL_SetTextureScaleMode(clientBackBuffer, SDL_ScaleMode::SDL_SCALEMODE_BEST);

    clientBackBuffer = nullptr;
    if (clientContext != nullptr) {
        delete clientContext;
    }
    clientContext = new SDLDrawContext(renderer, clientBackBuffer, clientRect);
}

DrawContext &SDLWindow::GetContentDC() {
    if (clientContext == nullptr) {
        clientContext = new SDLDrawContext(nullptr, nullptr, windowRect);
    }
    return *clientContext;
}

void SDLWindow::Clear() {

    SDL_SetRenderTarget(renderer, windowBackBuffer);

    auto theme = Config::Instance().GetTheme();
    SDLColor bgColor(theme->GetGlobalColors().GetColor("background"));
    bgColor.Use(renderer);

    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, nullptr);
}

void SDLWindow::DrawWindowDecoration() {
    if ((flags & WindowBase::kWin_Invisible) && (!glbDebugSDLWindows)) {
        return;
    }
    if (windowBackBuffer == nullptr) {
        return;
    }

    SDL_SetRenderTarget(renderer, windowBackBuffer);

    Point pxTopLeft = {0,0}; //SDLTranslate::RowColToPixel(windowRect.TopLeft());
    Point pxBottomRight {windowRect.Width(), windowRect.Height()};
    pxBottomRight = SDLTranslate::RowColToPixel(pxBottomRight);

    // FIXME: Figure out which color is the windowing color (we don't have that themed)
    SDL_SetRenderDrawColor(renderer, 255,255,255,255);

    // NOTE: We can have a much more filled border - as the border is always one char...
    if (glbDebugSDLWindows || decorationFlags & kWinDeco_TopBorder) {
        SDL_RenderDrawLine(renderer, pxTopLeft.x-1, pxTopLeft.y-1, pxBottomRight.x, pxTopLeft.y-1);
    }
    if (glbDebugSDLWindows ||decorationFlags & kWinDeco_BottomBorder) {
        SDL_RenderDrawLine(renderer, pxTopLeft.x, pxBottomRight.y+1, pxBottomRight.x-1, pxBottomRight.y+1);
    }
    if (glbDebugSDLWindows ||decorationFlags & kWinDeco_LeftBorder) {
        SDL_RenderDrawLine(renderer, pxTopLeft.x-1, pxTopLeft.y, pxTopLeft.x-1, pxBottomRight.y+1);
    }
    if (glbDebugSDLWindows ||decorationFlags & kWinDeco_RightBorder) {
        SDL_RenderDrawLine(renderer, pxBottomRight.x-1, pxTopLeft.y, pxBottomRight.x-1, pxBottomRight.y+1);
    }

    if (glbDebugSDLWindows || decorationFlags & kWinDeco_DrawCaption) {
        // Need a font-class to store this - should be initalized by screen...
        auto &dcWin = GetWindowDC();
        dcWin.FillLine(0, kTextAttributes::kInverted, ' ');
        dcWin.DrawStringAt(0,0,caption.c_str());
    }

    //SDL_RenderDrawLine(renderer, pxTopLeft.x, pxTopLeft.y, pxBottomRight.x, pxBottomRight.y);
    SDL_SetRenderTarget(renderer, nullptr);

}

void SDLWindow::Refresh() {

    auto pixRect = SDLTranslate::RowColToPixel(windowRect);
    SDL_FRect dstRect = {(float)pixRect.TopLeft().x, (float)pixRect.TopLeft().y, (float)pixRect.Width(), (float)pixRect.Height()};

    SDL_SetRenderTarget(renderer, nullptr);
    if (windowBackBuffer != nullptr) {
        SDL_RenderCopyF(renderer, windowBackBuffer, nullptr, &dstRect);
    }

    if (clientContext != nullptr) {
        auto pixRectClient = SDLTranslate::RowColToPixel(clientContext->GetRect());
        dstRect = {(float) pixRectClient.TopLeft().x, (float) pixRectClient.TopLeft().y, (float) pixRectClient.Width(),
                   (float) pixRectClient.Height()};

        if (clientBackBuffer != nullptr) {
            SDL_RenderCopyF(renderer, clientBackBuffer, nullptr, &dstRect);
        }
    }
}

// The 'View' handles what is active and not and will set the cursor to the underlying window
// we can simply update the global cursor from here...
void SDLWindow::SetCursor(const Cursor &newCursor) {
    // Just set the global SDL cursor
    SDLCursor::Instance().SetCursor(newCursor,[this](const Cursor &c)->void {
        OnDrawCursor(c);
    });
    // Cache it locally so - not sure why right now..
    cursor = newCursor;
}

void SDLWindow::OnDrawCursor(const Cursor &cursor) {
    //clientContext.
    if (clientContext == nullptr) {
        return;
    }
    // SDL_PIXELFORMAT_RGBA8888
    auto dc = static_cast<SDLDrawContext *>(clientContext);
    auto theme = Config::Instance().GetTheme();
    // FillRect assumes the render target has been set..
    SDL_SetRenderTarget(renderer, dc->renderTarget);
    dc->SetFGColor(theme->GetGlobalColors().GetColor("caret"));

    //dc->FillRect(cursor.position.x, cursor.position.y,1,1);
    dc->DrawLine(cursor.position.x, cursor.position.y, cursor.position.x, cursor.position.y + 1);
}

