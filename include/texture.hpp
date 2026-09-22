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
            TRTexture(const TRTexture &&) = delete;
            ~TRTexture();

            float* getColor(float u, float v);
            float* getBuffer();
            int getH() const;
            int getW() const;
            float getXStep() const;
            float getYStep() const;

            bool OK() const;

        private:
            bool mOK = false;
            float *mData = nullptr;
            int mPitch = 0;
            int mW = 0;
            int mH = 0;
    };

    // Aggregates 6 face textures for direction sampling.
    // Face order is the same as TRSkyBox: bottom, top, front, back, left, right.
    // The cube texture does not own the face textures.
    class TRCubeTexture
    {
        public:
            TRCubeTexture(TRTexture *faces[6]);
            TRCubeTexture(const TRCubeTexture &&) = delete;

            // Sample by a direction in cube space (any non-zero vector, no need to normalize).
            // Returns nullptr only when the selected face texture is missing.
            float *sample(const glm::vec3 &dir);

        private:
            TRTexture *mFaces[6] = { nullptr };
    };

    class TRTextureBuffer : public TRBuffer
    {
        public:
            TRTextureBuffer(int w, int h);
            TRTextureBuffer(const TRTextureBuffer &&) = delete;
            ~TRTextureBuffer();

            void clearColor();
            void drawPixel(int x, int y, float color[]);
            // blend into the texture memory (no y flip, float storage),
            // consistent with drawPixel above
            void blendPixel(int x, int y, float srcColor[4], TRBlendFactor srcFactor, TRBlendFactor dstFactor) override;
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
        TEXTURE_REFRACTION,
        // glmark2-style back face maps for the mesh refraction
        TEXTURE_BACK_NORMAL,
        TEXTURE_BACK_DEPTH,
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
