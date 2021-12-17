#ifndef __WINDOW_TC__
#define __WINDOW_TC__

#include <SDL.h>
#include <SDL_image.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "tr.h"

class TRWindow {
    public:
        TRWindow(int w, int h, const char *name = "ToyRenderer");
        ~TRWindow();

        // Old style method for swap buffer
        bool lock(void **addr);
        void unlock();

        bool isRunning();
        bool shouldStop();
        bool createSurfaceRenderTarget(TRBuffer &buffer, int width, int height);
        bool swapBuffer(TRBuffer &buffer);

    private:
        int mWidth;
        int mHeight;
        SDL_Window *mWindow;
        SDL_Renderer *mRenderer;
        SDL_Texture *mTexture;
        std::mutex mMutex;
        std::condition_variable mCV;
        std::thread mDisplayThread;
        bool mRunning;

        static void __disp_func__(TRWindow *);
};

#endif
