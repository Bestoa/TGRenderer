#ifndef __TR_OBJS__
#define __TR_OBJS__
#include "tr.hpp"

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

        TRMeshData mMeshData;

        ColorProgram mCProg;
        TextureMapProgram mTMProg;
        ColorPhongProgram mCPProg;
        TextureMapPhongProgram mTMPProg;

        ShadowMapProgram mShadowProg;

        TRTexture *mTextureDiffuse = nullptr;
        TRTexture *mTextureSpecular = nullptr;
        TRTexture *mTextureGlow = nullptr;
        TRTexture *mTextureNormal = nullptr;

        TRProgramBase *mProg[4] = { &mCProg, &mTMProg, &mCPProg, &mTMPProg };

        bool mValid = false;
};

#endif
