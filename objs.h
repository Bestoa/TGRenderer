#ifndef __TR_OBJS__
#define __TR_OBJS__
#include "tr.h"

class TRObj
{
    public:
        TRObj();
        ~TRObj();
        bool load(const char *name);
        bool isValid();
        bool draw();

    private:
        TRMeshData mMeshData;

        TRTexture mTextureDiffuse;
        TRTexture mTextureSpecular;
        TRTexture mTextureGlow;
        TRTexture mTextureNormal;
};

#endif
