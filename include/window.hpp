#ifndef __WINDOW_TC__
#define __WINDOW_TC__

#include <SDL.h>
#include "trapi.hpp"

typedef void (*KeyEventCb)(int scancode);

class TRWindow {
    public:
        TRWindow(int w, int h, const char *name = "TopGun Renderer");
        TRWindow(const TRWindow &&) = delete;
        ~TRWindow();

        bool OK() const;
        void pollEvent();
        void registerKeyEventCb(KeyEventCb func);
        void removeKeyEventCb();
        bool shouldStop() const;
        bool swapBuffer();

    private:
        TGRenderer::TRBuffer *mBuffer = nullptr;
        int mWidth = 0;
        int mHeight = 0;
        SDL_Window *mWindow = nullptr;
        SDL_Renderer *mRenderer = nullptr;
        SDL_Texture *mTexture = nullptr;
        bool mShouldStop = false;
        bool mOK = false;
        bool mShown = false;
        KeyEventCb mKcb = nullptr;
};

#endif
