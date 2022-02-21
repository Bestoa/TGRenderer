#include <iostream>
#include <glm/glm.hpp>
#include "buffer.hpp"
#include "texture.hpp"

namespace TGRenderer
{
    unsigned int gCurrentID = 1;

    void * TRBuffer::getRawData()
    {
        return mData;
    }

    uint32_t TRBuffer::getH() const
    {
        return mH;
    }

    uint32_t TRBuffer::getW() const
    {
        return mW;
    }

    void TRBuffer::setViewport(int x, int y, uint32_t w, uint32_t h)
    {
        mVX = x;
        mVY = y;
        mVW = w;
        mVH = h;
    }

    glm::vec2 TRBuffer::viewportTransform(glm::vec4 &ndc_v) const
    {
        return glm::vec2(mVX + (mVW - 1) * (ndc_v.x / 2 + 0.5), mVY + (mVH - 1) * (ndc_v.y / 2 + 0.5));
    }

    glm::uvec4 TRBuffer::getDrawArea()
    {
        int x = glm::clamp(mVX, 0, (int)mW);
        int y = glm::clamp(mVY, 0, (int)mH);
        int w = glm::min(mVX + mVW, mW);
        int h = glm::min(mVY + mVH, mH);
        return glm::uvec4(x, y, w, h);
    }

    void TRBuffer::setBgColor(float r, float g, float b)
    {
        mBgColor3i[0] = glm::clamp(int(r * 255 + 0.5), 0, 255);
        mBgColor3i[1] = glm::clamp(int(g * 255 + 0.5), 0, 255);
        mBgColor3i[2] = glm::clamp(int(b * 255 + 0.5), 0, 255);
        mBgColor3i[3] = 255;

        mBgColor3f[0] = r;
        mBgColor3f[1] = g;
        mBgColor3f[2] = b;
        mBgColor3f[3] = 1.0f;
    }

    void TRBuffer::clearColor()
    {
        for (size_t i = 0; i < mW * mH * BUFFER_CHANNEL; i += BUFFER_CHANNEL)
            for (size_t j = 0; j < BUFFER_CHANNEL; j++)
                mData[i + j] = mBgColor3i[j];
    }

    void TRBuffer::clearDepth()
    {
        for (size_t i = 0; i < mW * mH; i++)
            // add 1e-5 for skybox.
            mDepth[i] = 1.0f + 1e-5;
    }

    void TRBuffer::clearStencil()
    {
        for (size_t i = 0; i < mW * mH; i++)
            mStencil[i] = 0;
    }

    size_t TRBuffer::getStride() const
    {
        return mW;
    }

    size_t TRBuffer::getOffset(int x, int y) const
    {
        return y * mW + x;
    }

    void TRBuffer::drawPixel(int x, int y, float color[])
    {
        // flip Y here
        uint8_t *base = mData + ((mH - 1 - y) * mW + x) * BUFFER_CHANNEL;
        for (size_t i = 0; i < BUFFER_CHANNEL; i++)
            base[i] = uint8_t(color[i] * 255 + 0.5);
    }

    float TRBuffer::getDepth(size_t offset) const
    {
        return mDepth[offset];
    }

    void TRBuffer::updateDepth(size_t offset, float depth)
    {
        mDepth[offset] = depth;
    }

    uint8_t TRBuffer::getStencil(size_t offset) const
    {
        return mStencil[offset];
    }

    void TRBuffer::updateStencil(size_t offset, uint8_t stencil)
    {
        mStencil[offset] = stencil;
    }

#if __NEED_BUFFER_LOCK__
    std::mutex & TRBuffer::getMutex(size_t offset)
    {
        return mMutex[offset >> MUTEX_PIXEL_SHIT];
    }
#endif

    void TRBuffer::setExtBuffer(void *addr)
    {
        if (mOK && !mAlloc)
            mData = reinterpret_cast<uint8_t *>(addr);
    }

    TRBuffer::TRBuffer(int w, int h, bool alloc)
    {
        mId = gCurrentID++;
        std::cout << "Create TRBuffer: " << w << "x" << h << " alloc = " << alloc << " ID = " << mId << std::endl;
        mAlloc = alloc;
        if (mAlloc)
            mData = new uint8_t[w * h * BUFFER_CHANNEL];
        if (mAlloc && mData == nullptr)
            return;
        mDepth = new float[w * h];
        mStencil = new uint8_t[w * h];
        if (mDepth == nullptr || mStencil == nullptr)
            goto error;
#if __NEED_BUFFER_LOCK__
        mMutex = new std::mutex[((w * h) >> MUTEX_PIXEL_SHIT) + 1];
        if (mMutex == nullptr)
            goto error;
#endif
        mW = mVW = w;
        mH = mVH = h;
        if (mAlloc)
            clearColor();
        clearDepth();
        clearStencil();

        mOK = true;

        return;
error:
        if (mData)
            delete mData;
        if (mDepth)
            delete mDepth;
        if (mStencil)
            delete mStencil;
    }

    TRBuffer::~TRBuffer()
    {
        std::cout << "Destory TRBuffer, ID = " << mId << std::endl;
        if (!mOK)
            return;
        if (mAlloc && mData)
            delete mData;
        if (mDepth)
            delete mDepth;
        if (mStencil)
            delete mStencil;
#if __NEED_BUFFER_LOCK__
        if (mMutex)
            delete [] mMutex;
#endif
    }

    bool TRBuffer::OK() const
    {
        return mOK;
    }

    TRTextureBuffer::TRTextureBuffer(int w, int h) : TRBuffer::TRBuffer(w, h, false)
    {
        if (!mOK)
            return;
        mTexture = new TRTexture(w, h);
        if (!mTexture || !mTexture->OK())
            mOK = false;
    }

    TRTextureBuffer::~TRTextureBuffer()
    {
        if (mTexture)
            delete mTexture;
    }

    void TRTextureBuffer::clearColor()
    {
        float *base = mTexture->getBuffer();
        for (size_t i = 0; i < mW * mH * TEXTURE_CHANNEL; i += TEXTURE_CHANNEL)
            for (size_t j = 0; j < TEXTURE_CHANNEL; j++)
                base[i + j] = mBgColor3f[j];
    }

    void TRTextureBuffer::drawPixel(int x, int y, float color[])
    {
        float *base = mTexture->getBuffer() + (y * mW + x) * TEXTURE_CHANNEL;
        for (size_t i = 0; i < TEXTURE_CHANNEL; i++)
            base[i] = color[i];
    }

    TRTexture* TRTextureBuffer::getTexture()
    {
        return mTexture;
    }
}
