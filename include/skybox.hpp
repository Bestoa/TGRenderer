#ifndef __TR_SKYBOX_H__
#define __TR_SKYBOX_H__

#include <string>
#include "program.hpp"
#include "texture.hpp"

class SkyboxShader : public TextureMapShader
{
    private:
        void vertex(TGRenderer::TRMeshData &mesh, TGRenderer::VSOutData *vsdata, size_t index)
        {
            glm::mat4 viewMat = glm::mat4(glm::mat3(trGetMat4(TGRenderer::MAT4_VIEW)));
            vsdata->tr_Position = trGetMat4(TGRenderer::MAT4_PROJ) * viewMat  * glm::vec4(mesh.vertices[index], 1.0f);
            vsdata->tr_Position.z = vsdata->tr_Position.w;
            vsdata->mVaryingVec2[SH_TEXCOORD] = mesh.texcoords[index];
        }
};

class TRSkyBox
{
    public:
        TRSkyBox(std::string cubeTextureNames[6]);
        TRSkyBox(const TRSkyBox &&) = delete;
        ~TRSkyBox();

        bool OK();
        void draw();

    private:
        SkyboxShader mShader;

        TGRenderer::TRTexture *mCubeTexture[6];
        TGRenderer::TRMeshData mCubeData[6];

        bool mOK = false;
};

#endif
