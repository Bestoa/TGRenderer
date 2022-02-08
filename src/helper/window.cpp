#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "window.hpp"

using namespace std;
using namespace TGRenderer;

TRWindow::TRWindow(int w, int h, const char *name)
{
    mWidth = w;
    mHeight = h;

    if (SDL_Init(SDL_INIT_VIDEO))
        goto error;

    if (SDL_CreateWindowAndRenderer(mWidth, mHeight, SDL_WINDOW_HIDDEN, &mWindow, &mRenderer))
        goto error;
    SDL_SetWindowTitle(mWindow, name);

    mTexture = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, mWidth, mHeight);
    if (mTexture == nullptr)
        goto error;

    mBuffer = new TRBuffer(mWidth, mHeight, false);
    if (!mBuffer || !mBuffer->OK())
        goto error;

    void *addr;
    int pitch;
    if (SDL_LockTexture(mTexture, nullptr, &addr, &pitch))
        goto error;
    assert(pitch == int(mBuffer->getStride() * BUFFER_CHANNEL));
    mBuffer->setExtBuffer(addr);
    trSetRenderTarget(mBuffer);

    mOK = true;

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
}

bool TRWindow::OK() const
{
    return mOK;
}


bool TRWindow::swapBuffer()
{
    if (!mShown)
    {
        mShown = true;
        SDL_ShowWindow(mWindow);
    }
    SDL_UnlockTexture(mTexture);
    if (!SDL_RenderCopy(mRenderer, mTexture, nullptr, nullptr))
        SDL_RenderPresent(mRenderer);

    void *addr;
    int pitch, ret;
    ret = SDL_LockTexture(mTexture, nullptr, &addr, &pitch);
    mBuffer->setExtBuffer(addr);

    return ret;
}

bool TRWindow::shouldStop() const
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
    if (mBuffer)
        delete mBuffer;
    SDL_DestroyTexture(mTexture);
    SDL_DestroyRenderer(mRenderer);
    SDL_DestroyWindow(mWindow);
    SDL_Quit();
}
