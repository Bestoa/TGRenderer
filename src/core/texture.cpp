#include <iostream>
#include "texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace TGRenderer
{
    TRTexture::TRTexture(const char *name)
    {
        int width, height, nrChannels;
        stbi_ldr_to_hdr_gamma(1.0f);
        stbi_set_flip_vertically_on_load(true);
        float *texSrcData = stbi_loadf(name, &width, &height, &nrChannels, TEXTURE_CHANNEL);
        if (!texSrcData)
        {
            std::cout << "Load texture " << name << " failed.\n";
            return;
        }
        mW = width;
        mH = height;
        mPitch = mW * TEXTURE_CHANNEL;
        mData = new float[mPitch * mH];
        if (!mData)
            goto free_image;

        std::cout << "Loading texture " << name << ", size " << mW << "x" << mH << "x" << nrChannels << ".\n";

        for (int i = 0; i < mH; i++)
        {
            float *src = texSrcData + i * mPitch;
            float *dst = mData + i * mPitch;
            for (int j = 0; j < mW; j++)
            {
                dst[j * TEXTURE_CHANNEL + 0] = src[j * TEXTURE_CHANNEL + 0];
                dst[j * TEXTURE_CHANNEL + 1] = src[j * TEXTURE_CHANNEL + 1];
                dst[j * TEXTURE_CHANNEL + 2] = src[j * TEXTURE_CHANNEL + 2];
            }
        }

        mOK = true;

free_image:
        stbi_image_free(texSrcData);
    }

    TRTexture::TRTexture(int w, int h)
    {
        mW = w;
        mH = h;
        mPitch = mW * TEXTURE_CHANNEL;
        mData = new float[mPitch * mH];
        if (mData)
            mOK = true;
    }

    TRTexture::~TRTexture()
    {
        if (mData)
            delete mData;
    }

    float* TRTexture::getColor(float u, float v)
    {
        int x = int(u * (mW - 1) + 0.5);
        int y = int(v * (mH - 1) + 0.5);
        // Only support RGB texture now, but it should work for RGBA texture.
        return mData + y * mPitch + x * TEXTURE_CHANNEL;
    }

    float* TRTexture::getBuffer()
    {
        return mData;
    }

    bool TRTexture::OK() const
    {
        return mOK;
    }

    int TRTexture::getW() const
    {
        return mW;
    }

    int TRTexture::getH() const
    {
        return mH;
    }
}
