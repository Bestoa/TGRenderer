#ifndef __TOPGUN_PROGRAM__
#define __TOPGUN_PROGRAM__

#include "tr.hpp"

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
    SH_TANGENT_FRAG_POSITION,
    SH_TANGENT_LIGHT_POSITION,
    SH_VEC3_PHONG_MAX,
};

enum
{
    SH_LIGHT_FRAG_POSITION = SH_VEC4_BASE_MAX,
    SH_VEC4_PHONG_MAX,
};

class PhongUniformData
{
    public:
        float mAmbientStrength = 0.1;
        float mSpecularStrength = 0.2;
        int mShininess = 32;
        glm::vec3 mLightColor = glm::vec3(1.0f, 1.0f, 1.0f);
        glm::vec3 mLightPosition = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec3 mViewLightPosition = glm::vec3(0.0f, 0.0f, 0.0f);
};

class ColorProgram : public TGRenderer::Program
{
    private:
        void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t );
        bool fragment(TGRenderer::FSInData *, float color[3]);
        void getVaryingNum(size_t &, size_t &, size_t &);

    public:
        TGRenderer::Program *clone();
};

class TextureMapProgram : public TGRenderer::Program
{
    private:
        void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t );
        bool fragment(TGRenderer::FSInData *, float color[3]);
        void getVaryingNum(size_t &, size_t &, size_t &);

    public:
        TGRenderer::Program *clone();
};

class ColorPhongProgram : public TGRenderer::Program
{
    private:
        void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t);
        bool fragment(TGRenderer::FSInData *, float color[3]);
        void getVaryingNum(size_t &, size_t &, size_t &);

    public:
        TGRenderer::Program *clone();
};

class TextureMapPhongProgram : public TGRenderer::Program
{
    private:
        void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t);
        bool fragment(TGRenderer::FSInData *, float color[3]);
        void getVaryingNum(size_t &, size_t &, size_t &);

    public:
        TGRenderer::Program *clone();
};

class ShadowMapProgram : public TGRenderer::Program
{
    private:
        void vertex(TGRenderer::TRMeshData &, TGRenderer::VSOutData *, size_t);
        bool fragment(TGRenderer::FSInData *, float color[3]);
        void getVaryingNum(size_t &, size_t &, size_t &);

    public:
        TGRenderer::Program *clone();
        constexpr static float BIAS = 0.001f;
        constexpr static float FACTOR = 0.2f;
};
#endif
