#ifndef __WINDOW_TC__
#define __WINDOW_TC__

#include <SDL.h>
#include <SDL_image.h>
#include <thread>
#include <mutex>
#include <condition_variable>

class TRWindow {
    public:
        TRWindow(int w, int h);
        ~TRWindow();

        bool fail();
        bool lock(void **addr);
        void unlock();
        bool should_exit();

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
