#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "window.hpp"

using namespace std;
using namespace TGRenderer;

void TRWindow::__disp_func__(TRWindow *w)
{
    cout << "[TRWindow]: Start display thread." << endl;
    unique_lock<mutex> lcx(w->mMutex);
    while (true)
    {
        w->mCV.wait(lcx);
        if (!w->mRunning)
            break;
        if (!SDL_RenderCopy(w->mRenderer, w->mTexture, nullptr, nullptr))
            SDL_RenderPresent(w->mRenderer);
    }
    cout << "[TRWindow]: Stop display thread." << endl;
}

TRWindow::TRWindow(int w, int h, const char *name)
{
    mWidth = w;
    mHeight = h;

    mWindow = nullptr;
    mRenderer = nullptr;
    mTexture = nullptr;

    if (SDL_Init(SDL_INIT_VIDEO))
        goto error;

    mWindow = SDL_CreateWindow(name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, mWidth, mHeight, 0);
    if (mWindow == nullptr)
        goto error;

    mRenderer = SDL_CreateRenderer(mWindow, -1, SDL_RENDERER_SOFTWARE);
    if (mRenderer == nullptr)
        goto error;

    mTexture = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, mWidth, mHeight);
    if (mTexture == nullptr)
        goto error;

    mBuffer = new TRBuffer(mWidth, mHeight, false);
    if (!mBuffer || !mBuffer->isValid())
        goto error;

    void *addr;
    if (!lock(&addr))
        goto error;

    mBuffer->setExtBuffer(addr);

    trSetCurrentRenderTarget(mBuffer);

    mDisplayThread = thread(__disp_func__, this);
    if (!mDisplayThread.joinable())
        goto error;

    mRunning = true;

    return;

error:
    if (mBuffer)
        delete mBuffer;
    if (mTexture)
        SDL_DestroyTexture(mTexture);
    if (mRenderer)
        SDL_DestroyRenderer(mRenderer);
    if (mWindow)
        SDL_DestroyWindow(mWindow);

    SDL_Quit();
    mWidth = 0;
    mHeight = 0;
    mRunning = false;
}

bool TRWindow::isRunning()
{
    return mRunning;
}

bool TRWindow::lock(void **addr)
{
    int pitch;
    unique_lock<mutex> lck(mMutex);
    return !SDL_LockTexture(mTexture, nullptr, addr, &pitch);
}

void TRWindow::unlock()
{
    unique_lock<mutex> lck(mMutex);
    SDL_UnlockTexture(mTexture);
    mCV.notify_all();
}

bool TRWindow::swapBuffer()
{
    unlock();
    void *addr;
    if (lock(&addr))
        return false;
    mBuffer->setExtBuffer(addr);
    return true;
};

bool TRWindow::shouldStop()
{
    return mShouldStop;
}

void TRWindow::registerKeyEventCb(KeyEventCb func)
{
   mKcb = func;
}

void TRWindow::removeKeyEventCb()
{
   mKcb = nullptr;
}

void TRWindow::pollEvent()
{
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0)
    {
        if (event.type == SDL_QUIT)
            mShouldStop = true;
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                mShouldStop = true;
            if (mKcb)
                mKcb(event.key.keysym.scancode);
        }
    }
}

TRWindow::~TRWindow()
{
    if (mDisplayThread.joinable())
    {
        unique_lock<mutex> lck(mMutex);
        mRunning = false;
        // must unlock it manullay before join
        lck.unlock();
        mCV.notify_all();
        mDisplayThread.join();
        cout << "[TRWindow]: Display thread exit." << endl;

        delete mBuffer;
        SDL_DestroyTexture(mTexture);
        SDL_DestroyRenderer(mRenderer);
        SDL_DestroyWindow(mWindow);
        SDL_Quit();
    }
}
