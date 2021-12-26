#ifndef __TOPGUN_TEXTURE__
#define __TOPGUN_TEXTURE__

#include <glm/glm.hpp>

#define TEXTURE_CHANNEL (3)
class TRTexture
{
    public:
        TRTexture(const char *);
        TRTexture() = delete;
        TRTexture(const TRTexture &) = delete;
        TRTexture& operator=(const TRTexture &) = delete;

        ~TRTexture()
        {
            if (mData)
                delete mData;
        };

        float* getColor(float u, float v)
        {
            int x = int(u * (mW - 1) + 0.5);
            // inverse y here
            int y = int(mH - 1 - v * (mH - 1) + 0.5);
            // Only support RGB texture
            return mData + y * mStride + x * TEXTURE_CHANNEL;
        };

        bool isValid() { return mValid; };

    private:
        bool mValid = false;
        float *mData = nullptr;
        int mStride = 0;
        int mW = 0;
        int mH = 0;
};

enum TRTextureType
{
    TEXTURE_DIFFUSE,
    TEXTURE_SPECULAR,
    TEXTURE_GLOW,
    TEXTURE_NORMAL,
    TEXTURE_TYPE_MAX,
};

enum TRTextureIndex
{
    TEXTURE0,
    TEXTURE1,
    TEXTURE2,
    TEXTURE3,
    TEXTURE4,
    TEXTURE5,
    TEXTURE6,
    TEXTURE7,
    TEXTURE_INDEX_MAX,
};
#endif
