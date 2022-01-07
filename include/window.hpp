#ifndef __WINDOW_TC__
#define __WINDOW_TC__

#include <SDL.h>
#include <SDL_image.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "tr.hpp"

typedef void (*KeyEventCb)(int scancode);

class TRWindow {
    public:
        TRWindow(int w, int h, const char *name = "ToyRenderer");
        ~TRWindow();

        // Old style method for swap buffer
        bool lock(void **addr);
        void unlock();

        bool isRunning();
        void pollEvent();
        void registerKeyEventCb(KeyEventCb func);
        void removeKeyEventCb();
        bool shouldStop();
        bool swapBuffer();
        TGRenderer::TRBuffer* getBuffer() { return mBuffer; };

        // Disable copy init.
        TRWindow(const TRWindow &) = delete;
        TRWindow& operator=(const TRWindow &) = delete;

    private:
        TRWindow() = delete;

        TGRenderer::TRBuffer *mBuffer = nullptr;
        int mWidth = 0;
        int mHeight = 0;
        SDL_Window *mWindow = nullptr;
        SDL_Renderer *mRenderer = nullptr;
        SDL_Texture *mTexture = nullptr;
        std::mutex mMutex;
        std::condition_variable mCV;
        std::thread mDisplayThread;
        bool mRunning = false;
        bool mShouldStop = false;

        KeyEventCb mKcb = nullptr;

        static void __disp_func__(TRWindow *);
};

#endif
