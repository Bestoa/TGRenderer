#ifndef __TR_OBJS__
#define __TR_OBJS__
#include "trapi.hpp"
#include "program.hpp"

class TRObj
{
    public:
        TRObj(const char *confg);
        TRObj(const TRObj &&) = delete;
        ~TRObj();

        bool OK() const;
        bool draw(int id = 3);
        bool drawShadowMap();
        float getFloorYAxis() const;

    private:
        TGRenderer::TRMeshData mMeshData;

        ColorShader mColorShader;
        TextureMapShader mTextureMapShader;
        ColorPhongShader mColorPhongShader;
        TextureMapPhongShader mTextureMapPhongShader;
        ShadowMapShader mShadowShader;
        TGRenderer::Shader *mShaders[4] =
        {
            &mColorShader,
            &mTextureMapShader,
            &mColorPhongShader,
            &mTextureMapPhongShader
        };

        struct
        {
            float Ns = 32.0f; /* Range 0-1000 */
            float sharpness = 200.0f; /* Range 0-1000 need to be remapped to 0.0-1.0f */
            TGRenderer::TRTexture *map_Kd = nullptr;
            TGRenderer::TRTexture *map_Ks = nullptr;
            TGRenderer::TRTexture *map_Ke = nullptr;
            TGRenderer::TRTexture *map_Kn = nullptr;
        } mAttribute;

        bool mOK = false;
};

#endif
