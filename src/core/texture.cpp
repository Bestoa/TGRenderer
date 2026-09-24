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
            delete[] mData;
    }

    TRCubeTexture::TRCubeTexture(TRTexture *faces[6])
    {
        for (int i = 0; i < 6; i++)
            mFaces[i] = faces[i];
    }

    /* Face selection and UV projection are derived from the skybox cube vertex
     * table (src/core/skybox.cpp), so sampling a direction always returns the
     * same texel that is drawn on screen when looking along that direction.
     *
     * face    condition            u         v
     * bottom  y < 0, |y| dominant  (x+1)/2   (1-z)/2
     * top     y > 0, |y| dominant  (x+1)/2   (z+1)/2
     * front   z < 0, |z| dominant  (x+1)/2   (y+1)/2
     * back    z > 0, |z| dominant  (1-x)/2   (y+1)/2
     * left    x < 0, |x| dominant  (1-z)/2   (y+1)/2
     * right   x > 0, |x| dominant  (z+1)/2   (y+1)/2
     *
     * (x, y, z) here are the direction components projected onto the face
     * plane, i.e. dir divided by its dominant absolute component, so each
     * one is in [-1, 1].
     */
    float *TRCubeTexture::sample(const glm::vec3 &dir)
    {
        float ax = glm::abs(dir.x);
        float ay = glm::abs(dir.y);
        float az = glm::abs(dir.z);

        if (!(ax > 0.f || ay > 0.f || az > 0.f))
            return nullptr;

        TRTexture *face = nullptr;
        float u = 0.f;
        float v = 0.f;

        if (ay >= ax && ay >= az)
        {
            float s = 1.0f / ay;
            float x = dir.x * s;
            float z = dir.z * s;
            if (dir.y > 0.f)
            {
                // top
                face = mFaces[1];
                u = (x + 1.0f) * 0.5f;
                v = (z + 1.0f) * 0.5f;
            } else {
                // bottom
                face = mFaces[0];
                u = (x + 1.0f) * 0.5f;
                v = (1.0f - z) * 0.5f;
            }
        } else if (ax >= az) {
            float s = 1.0f / ax;
            float y = dir.y * s;
            float z = dir.z * s;
            if (dir.x < 0.f)
            {
                // left
                face = mFaces[4];
                u = (1.0f - z) * 0.5f;
                v = (y + 1.0f) * 0.5f;
            } else {
                // right
                face = mFaces[5];
                u = (z + 1.0f) * 0.5f;
                v = (y + 1.0f) * 0.5f;
            }
        } else {
            float s = 1.0f / az;
            float x = dir.x * s;
            float y = dir.y * s;
            if (dir.z < 0.f)
            {
                // front
                face = mFaces[2];
                u = (x + 1.0f) * 0.5f;
                v = (y + 1.0f) * 0.5f;
            } else {
                // back
                face = mFaces[3];
                u = (1.0f - x) * 0.5f;
                v = (y + 1.0f) * 0.5f;
            }
        }

        if (face == nullptr || !face->OK())
            return nullptr;
        // map the nominal 90 degree direction into the face's central
        // sub-region when the faces were rendered wider than 90 degrees
        u = 0.5f + (u - 0.5f) * mSampleScale;
        v = 0.5f + (v - 0.5f) * mSampleScale;
        return face->getColor(u, v);
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

    float TRTexture::getXStep() const
    {
        static float xstep = 1.0f / mW;
        return xstep;
    }

    float TRTexture::getYStep() const
    {
        static float ystep = 1.0f / mH;
        return ystep;
    }
}
