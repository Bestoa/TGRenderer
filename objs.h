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
        std::vector<glm::vec3> mVs;
        std::vector<glm::vec2> mUVs;
        std::vector<glm::vec3> mNs;

        TRTexture mTextureDiffuse;
        TRTexture mTextureSpecular;
        TRTexture mTextureGlow;
        TRTexture mTextureNormal;
};

#endif
