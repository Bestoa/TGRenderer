#ifndef __TR_OBJS__
#define __TR_OBJS__
#include "tr.h"

class TRObj
{
    public:
        TRObj();
        ~TRObj();
        bool load_from_config_file(const char *name);
        bool is_valid();
        bool draw();

    private:
        TRMeshData mMeshData;

        TRTexture mTextureDiffuse;
        TRTexture mTextureSpecular;
        TRTexture mTextureGlow;
        TRTexture mTextureNormal;
};

#endif
