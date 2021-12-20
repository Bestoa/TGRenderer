#include <iostream>
#include "tr.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TRTexture::TRTexture(const char *name)
{
    int width, height, nrChannels;
    stbi_ldr_to_hdr_gamma(1.0f);
    float *texSrcData = stbi_loadf(name, &width, &height, &nrChannels, TEXTURE_CHANNEL);
    if (!texSrcData)
    {
        std::cout << "Load texture " << name << " failed.\n";
        return;
    }
    mW = width;
    mH = height;
    mStride = mW * TEXTURE_CHANNEL;
    mData = new float[mStride * mH];
    if (!mData)
        goto free_image;

    std::cout << "Loading texture " << name << ", size " << mW << "x" << mH << "x" << nrChannels << ".\n";

    for (int i = 0; i < mH; i++)
    {
        float *src = texSrcData + i * mStride;
        float *dst = mData + i * mStride;
        for (int j = 0; j < mW; j++)
        {
            dst[j * TEXTURE_CHANNEL + 0] = src[j * TEXTURE_CHANNEL + 0];
            dst[j * TEXTURE_CHANNEL + 1] = src[j * TEXTURE_CHANNEL + 1];
            dst[j * TEXTURE_CHANNEL + 2] = src[j * TEXTURE_CHANNEL + 2];
        }
    }

    mValid = true;

free_image:
    stbi_image_free(texSrcData);
}

