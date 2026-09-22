#ifndef __TOPGUN_PROGRAM__
#define __TOPGUN_PROGRAM__

#include "trapi.hpp"

enum
{
    SH_TEXCOORD,
    SH_VEC2_BASE_MAX,
};

enum
{
    SH_NORMAL,
    SH_COLOR,
    SH_VEC3_BASE_MAX,
};

enum
{
    SH_VEC4_BASE_MAX,
};

enum
{
    SH_VIEW_FRAG_POSITION = SH_VEC3_BASE_MAX,
    SH_WORLD_FRAG_POSITION,
    SH_WORLD_NORMAL,
    SH_TANGENT_FRAG_POSITION,
    SH_TANGENT_LIGHT_POSITION,
    SH_VEC3_PHONG_MAX,
    // ColorPhongShader has no tangent/world space data
    SH_VEC3_COLOR_PHONG_MAX = SH_WORLD_FRAG_POSITION,
};

enum
{
    SH_LIGHT_FRAG_POSITION = SH_VEC4_BASE_MAX,
    SH_VEC4_PHONG_MAX,
};

void textureCoordWrap(glm::vec2 &coord);
float *texture2D(int type, float u, float v);
// Sample the cube texture bound by trBindCubeTexture(), nullptr when unbound
float *textureCube(const glm::vec3 &dir);

class PhongUniformData
{
    public:
        float mAmbientStrength = 0.1;
        float mSpecularStrength = 0.2;
        int mShininess = 32;
        glm::vec3 mLightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        glm::vec3 mLightPosition = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 mViewLightPosition = glm::vec3(0.0f, 0.0f, 0.0f);
        // Environment reflection strength, 0 = no reflection, 1 = pure mirror
        float mReflectivity = 0.5f;
        // Eye position in world space, needed by the world space reflection
        glm::vec3 mEyeWorldPosition = glm::vec3(0.0f, 0.0f, 0.0f);
        // Fresnel: reflection weight grows toward grazing angles,
        // mFresnelFactor is the base reflectance at normal incidence.
        // 0 disables the fresnel term (uniform reflection strength).
        float mFresnelFactor = 0.0f;
        float mFresnelPower = 5.0f;
        // Attenuate the reflection by the shadow factor, so faces the light
        // can not reach do not show a strong mirror image.
        bool mReflectShadowMod = false;
};

class ColorShader : public TGRenderer::Shader
{
    void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t );
    bool fragment(TGRenderer::FSInData *, float color[]);
    void getVaryingNum(size_t &, size_t &, size_t &);
};

class TextureMapShader : public TGRenderer::Shader
{
    void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t );
    bool fragment(TGRenderer::FSInData *, float color[]);
    void getVaryingNum(size_t &, size_t &, size_t &);
};

class ColorPhongShader : public TGRenderer::Shader
{
    void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t);
    bool fragment(TGRenderer::FSInData *, float color[]);
    void getVaryingNum(size_t &, size_t &, size_t &);
};

class TextureMapPhongShader : public TGRenderer::Shader
{
    void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t);
    bool fragment(TGRenderer::FSInData *, float color[]);
    void getVaryingNum(size_t &, size_t &, size_t &);
};

class ShadowMapShader : public TGRenderer::Shader
{
    private:
        void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t);
        bool fragment(TGRenderer::FSInData *, float color[]);
        void getVaryingNum(size_t &, size_t &, size_t &);

    public:
        constexpr static float BIAS = 0.001f;
        constexpr static float FACTOR = 0.2f;
};
#endif
