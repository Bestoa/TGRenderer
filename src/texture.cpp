#include <iostream>
#include "tr.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TRTexture::TRTexture(const char *name)
{
    int width, height, nrChannels;
    unsigned char *data = stbi_load(name, &width, &height, &nrChannels, 0);
    if (!data)
    {
        std::cout << "Load texture " << name << " failed.\n";
        return;
    }
    if (nrChannels != 3 && nrChannels != 4)
    {
        std::cout << "Only support RGB/RGBA texture.\n";
        goto free_image;
    }
    mW = width;
    mH = height;
    mStride = mW * TEXTURE_BPP;
    mData = new uint8_t[mStride * mH];
    if (!mData)
        goto free_image;

    std::cout << "Loading texture " << name << ", size " << mW << "x" << mH << ".\n";

    for (int i = 0; i < mH; i++)
    {
        for (int j = 0; j < mW; j++)
        {
            mData[i * mStride + j * nrChannels + 0] = *(data + i * (nrChannels * width) + j * nrChannels + 0);
            mData[i * mStride + j * nrChannels + 1] = *(data + i * (nrChannels * width) + j * nrChannels + 1);
            mData[i * mStride + j * nrChannels + 2] = *(data + i * (nrChannels * width) + j * nrChannels + 2);
        }
    }

    mValid = true;

free_image:
    stbi_image_free(data);
}

