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
        return;

    if (SDL_CreateWindowAndRenderer(mWidth, mHeight, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE, &mWindow, &mRenderer))
        goto error;
    SDL_SetWindowTitle(mWindow, name);

    if (createTextureAndBuffer(true) == false)
        goto error;

    mOK = true;
    return;

error:
    if (mWindow)
        SDL_DestroyWindow(mWindow);

    mWidth = 0;
    mHeight = 0;
}

bool TRWindow::createTextureAndBuffer(bool first)
{
    SDL_Texture *texture = nullptr;
    TRBuffer *buffer = nullptr;

    texture = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, mWidth, mHeight);
    if (texture == nullptr)
        return false;

    buffer = new TRBuffer(mWidth, mHeight, false);
    if (!buffer || !buffer->OK())
    {
        goto error;
    }

    void *addr;
    int pitch;
    if (SDL_LockTexture(texture, nullptr, &addr, &pitch))
        goto error;
    assert(pitch == int(buffer->getStride() * BUFFER_CHANNEL));
    buffer->setExtBuffer(addr);

    if (!first)
    {
        SDL_DestroyTexture(mTexture);
        delete mBuffer;
    }

    mTexture = texture;
    mBuffer = buffer;

    trSetRenderTarget(mBuffer);


    return true;

error:
    if (texture)
        SDL_DestroyTexture(texture);
    if (buffer)
        delete buffer;
    return false;
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
        switch (event.type)
        {
            case SDL_QUIT:
                mShouldStop = true;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                    mShouldStop = true;
                else if (mKcb)
                    mKcb(event.key.keysym.scancode);
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    mWidth = event.window.data1;
                    mHeight = event.window.data2;
                    createTextureAndBuffer(false);
                }
                break;
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
