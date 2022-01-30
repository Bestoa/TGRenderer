#ifndef __WINDOW_TC__
#define __WINDOW_TC__

#include <SDL.h>
#include <SDL_image.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "trapi.hpp"

typedef void (*KeyEventCb)(int scancode);

class TRWindow {
    public:
        TRWindow(int w, int h, const char *name = "TopGun Renderer");
        TRWindow(const TRWindow &&) = delete;
        ~TRWindow();

        bool isRunning() const;
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
        SDL_Texture *mDrawTexture = nullptr;
        SDL_Texture *mDispTexture = nullptr;
        std::mutex mMutex;
        std::condition_variable mCV;
        std::thread *mDisplayThread = nullptr;
        bool mRunning = false;
        bool mShouldStop = false;

        KeyEventCb mKcb = nullptr;

        static void __disp_func__(TRWindow *);
};

#endif
