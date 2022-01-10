#ifndef __TOPGUN_TEXTURE__
#define __TOPGUN_TEXTURE__

#include <glm/glm.hpp>
#include "buffer.hpp"

namespace TGRenderer
{
    constexpr int TEXTURE_CHANNEL = 3;
    class TRTexture
    {
        public:
            TRTexture(const char *);
            // Empty texture
            TRTexture(int w, int h);
            TRTexture() = delete;
            TRTexture(const TRTexture &) = delete;
            TRTexture& operator=(const TRTexture &) = delete;

            ~TRTexture();

            float* getColor(float u, float v);
            float* getBuffer();
            int getH();
            int getW();

            bool isValid();

        private:
            bool mValid = false;
            float *mData = nullptr;
            int mPitch = 0;
            int mW = 0;
            int mH = 0;
    };

    class TRTextureBuffer : public TRBuffer
    {
        public:
            TRTextureBuffer(int w, int h);
            TRTextureBuffer() = delete;
            TRTextureBuffer(const TRTextureBuffer &) = delete;
            TRTextureBuffer& operator=(const TRTextureBuffer &) = delete;

            ~TRTextureBuffer();

            void clearColor();
            void drawPixel(int x, int y, float color[]);
            TRTexture *getTexture();

        private:
            TRTexture *mTexture = nullptr;
    };

    enum TRTextureType
    {
        TEXTURE_DIFFUSE,
        TEXTURE_SPECULAR,
        TEXTURE_GLOW,
        TEXTURE_NORMAL,
        TEXTURE_SHADOWMAP,
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
}
#endif
