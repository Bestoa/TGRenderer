#ifndef __TR_OBJS__
#define __TR_OBJS__
#include "tr.hpp"

class TRObj
{
    public:
        TRObj(const char *confg);
        ~TRObj();
        bool isValid();
        bool draw();

        // Disable copy init.
        TRObj() = delete;
        TRObj(const TRObj &) = delete;
        TRObj& operator=(const TRObj &) = delete;

    private:

        TRMeshData mMeshData;

        TextureMapPhongProgram mProg;

        TRTexture *mTextureDiffuse = nullptr;
        TRTexture *mTextureSpecular = nullptr;
        TRTexture *mTextureGlow = nullptr;
        TRTexture *mTextureNormal = nullptr;

        bool mValid = false;
};

#endif
