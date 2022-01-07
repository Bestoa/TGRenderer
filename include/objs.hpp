#ifndef __TR_OBJS__
#define __TR_OBJS__
#include "tr.hpp"
#include "program.hpp"

class TRObj
{
    public:
        TRObj(const char *confg);
        ~TRObj();
        bool isValid();
        bool draw(int id = 3);
        bool drawShadowMap();

        // Disable copy init.
        TRObj() = delete;
        TRObj(const TRObj &) = delete;
        TRObj& operator=(const TRObj &) = delete;

    private:

        TGRenderer::TRMeshData mMeshData;

        ColorProgram mCProg;
        TextureMapProgram mTMProg;
        ColorPhongProgram mCPProg;
        TextureMapPhongProgram mTMPProg;

        ShadowMapProgram mShadowProg;

        TGRenderer::TRTexture *mTextureDiffuse = nullptr;
        TGRenderer::TRTexture *mTextureSpecular = nullptr;
        TGRenderer::TRTexture *mTextureGlow = nullptr;
        TGRenderer::TRTexture *mTextureNormal = nullptr;

        TGRenderer::Program *mProg[4] = { &mCProg, &mTMProg, &mCPProg, &mTMPProg };

        bool mValid = false;
};

#endif
