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
        // Distance attenuation of the point light: factor 1/(1+k*d*d).
        // 0 disables (constant intensity regardless of distance).
        float mLightAttenuation = 0.0f;
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
        // Fragment opacity for blending, only meaningful with trEnableBlend()
        float mOpacity = 1.0f;
        // Color bleeding (one-bounce indirect light): samples the bound cube
        // environment along the fragment normal and adds it scaled by the
        // surface color. 0 disables (default).
        float mIndirectStrength = 0.0f;
        // World position of the probe that captured the bound cube texture.
        // Used to parallax-correct the indirect hemisphere gather: each tap
        // direction is intersected with the room box from the fragment
        // position, then re-expressed as a direction from the probe, so
        // fragments near a colored wall pick up that wall's tint instead of
        // a uniform room average. Zero disables the correction.
        glm::vec3 mProbePosition = glm::vec3(0.0f);
        // Index of refraction. > 1.0 turns the fragment into glass shading:
        // fresnel weighted reflection + double refraction through
        // mRefractionSphere replaces the regular Phong body.
        float mIOR = 1.0f;
        // World space sphere (center.xyz, radius.w) for the analytic double
        // refraction: enter at the fragment, exit where the inner ray hits
        // the sphere again. w <= 0 disables the analytic path.
        glm::vec4 mRefractionSphere = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        // Analytic refraction scene: the exit ray hits this floor plane
        // (world Y) when pointing down and samples TEXTURE_REFRACTION at the
        // exact hit point, otherwise it samples the skybox cube. Exact for
        // the floor (no probe parallax), exact for the sky (at infinity).
        float mRefractionFloorY = 0.0f;
        // Floor texture world size for the refraction lookup
        float mRefractionFloorSize = 20.0f;
        // Half extent of the floor in world units, rays hitting beyond it
        // see the far field (skybox horizon) instead
        float mRefractionFloorExtent = 20.0f;
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

// Back face render for the mesh refraction (glmark2 style): outputs the
// view space normal, meant to be rendered with front faces culled.
class BackNormalShader : public TGRenderer::Shader
{
    void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t);
    bool fragment(TGRenderer::FSInData *, float color[]);
    void getVaryingNum(size_t &, size_t &, size_t &);
};

// Back face render companion: outputs the view space depth (distance
// along the view axis) in the red channel.
class BackDepthShader : public TGRenderer::Shader
{
    void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t);
    bool fragment(TGRenderer::FSInData *, float color[]);
    void getVaryingNum(size_t &, size_t &, size_t &);
};
#endif
