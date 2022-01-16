#ifndef __TR_OBJS__
#define __TR_OBJS__
#include "trapi.hpp"
#include "program.hpp"

class TRObj
{
    public:
        TRObj(const char *confg);
        ~TRObj();
        bool isValid();
        bool draw(int id = 3);
        bool drawShadowMap();

        float getFloorYAxis();

        // Disable copy init.
        TRObj() = delete;
        TRObj(const TRObj &) = delete;
        TRObj& operator=(const TRObj &) = delete;

    private:

        TGRenderer::TRMeshData mMeshData;

        ColorShader mColorShader;
        TextureMapShader mTextureMapShader;
        ColorPhongShader mColorPhongShader;
        TextureMapPhongShader mTextureMapPhongShader;

        ShadowMapShader mShadowShader;

        TGRenderer::TRTexture *mTextureDiffuse = nullptr;
        TGRenderer::TRTexture *mTextureSpecular = nullptr;
        TGRenderer::TRTexture *mTextureGlow = nullptr;
        TGRenderer::TRTexture *mTextureNormal = nullptr;

        TGRenderer::Shader *mShaders[4] = { &mColorShader, &mTextureMapShader, &mColorPhongShader, &mTextureMapPhongShader };

        bool mValid = false;
};

#endif
