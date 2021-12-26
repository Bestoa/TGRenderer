#ifndef __TOPGUN_RENDERER__
#define __TOPGUN_RENDERER__

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <glm/glm.hpp>

#include "buffer.hpp"
#include "texture.hpp"

enum MAT3_SYSTEM_TYPE
{
    MAT3_NORMAL,
    MAT3_SYSTEM_TYPE_MAX,
};

enum MAT4_SYSTEM_TYPE
{
    MAT4_MODEL,
    MAT4_VIEW,
    MAT4_PROJ,
    MAT4_MODELVIEW,
    MAT4_MVP,
    MAT4_SYSTEM_TYPE_MAX,
};

enum MAT_INDEX
{
    MAT0, MAT1, MAT2, MAT3, MAT4, MAT5, MAT6, MAT7, MAT8, MAT9, MAT10, MAT11, MAT12, MAT13, MAT14, MAT15,
    MAT_INDEX_MAX,
};

const int MAT3_USER_0 = MAT_INDEX_MAX - MAT3_SYSTEM_TYPE_MAX;
const int MAT4_USER_0 = MAT_INDEX_MAX - MAT4_SYSTEM_TYPE_MAX;

class TRMeshData
{
    public:
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec3> colors;
        std::vector<glm::vec3> tangents;

        TRMeshData() {};
        TRMeshData(const TRMeshData &) = delete;
        TRMeshData& operator=(const TRMeshData &) = delete;

        void computeTangent();
        void fillDemoColor();
};

enum TRDrawMode
{
    DRAW_WITH_TEXTURE,
    DRAW_WITH_COLOR,
    DRAW_WITH_DEMO_COLOR,
    DRAW_WIREFRAME,
};

static inline float edge(glm::vec2 &a, glm::vec2 &b, glm::vec2 &c)
{
    return (c.x - a.x)*(b.y - a.y) - (c.y - a.y)*(b.x - a.x);
}

template <class TRVSData, class TRFSData>
class TRProgramBase
{
    public:
        virtual ~TRProgramBase() {}
        void drawTrianglesInstanced(TRBuffer *buffer, TRMeshData &mesh, size_t index, size_t num);

    protected:
        TRBuffer *mBuffer;

        /* Slow interpolation funciotn */
        template <class T> T interp(T v[3])
        {
            return v[0] + (v[1] - v[0]) * mUPC + (v[2] - v[0]) * mVPC;
        }

        /* Fast interpolation funciotn , v'0 = v0, v'1 = v1 - v0, v'2 = v2 - v0 , pre-calculated */
        template <class T> inline T interpFast(T v[3])
        {
            return v[0] + v[1] * mUPC + v[2] * mVPC;
        }

    private:
        TRVSData mVSData[3];
        TRFSData mFSData;
        float mUPC;
        float mVPC;

        virtual void loadVertexData(TRMeshData &, TRVSData *, size_t) = 0;
        virtual void vertex(TRVSData &, glm::vec4 &clipV /* Out */) = 0;
        virtual bool geometry(TRVSData *, TRFSData &) = 0;
        virtual bool fragment(TRFSData &, float color[3]/* Out */) = 0;

        void drawTriangle(TRMeshData &, size_t index);
        void rasterization(glm::vec4 clip_v[3]);
};

class VSDataBase
{
    public:
        glm::vec3 mVertex;
        glm::vec2 mTexCoord;
        glm::vec3 mNormal;
        glm::vec3 mColor;
        glm::vec3 mTangent;
};

class FSDataBase
{
    public:
        glm::vec2 mTexCoord[3];
        glm::vec3 mNormal[3];
        glm::vec3 mColor[3];
        glm::vec3 mTangent;
};

class WireframeProgram : public TRProgramBase<VSDataBase, FSDataBase>
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase &, glm::vec4 &);
    bool geometry(VSDataBase *, FSDataBase &);
    bool fragment(FSDataBase &, float color[3]);
};

class ColorProgram : public TRProgramBase<VSDataBase, FSDataBase>
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase &, glm::vec4 &);
    bool geometry(VSDataBase *, FSDataBase &);
    bool fragment(FSDataBase &, float color[3]);
};

class TextureMapProgram : public TRProgramBase<VSDataBase, FSDataBase>
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase &, glm::vec4 &clipV);
    bool geometry(VSDataBase *, FSDataBase &);
    bool fragment(FSDataBase &, float color[3]);
};

class PhongVSData : public VSDataBase
{
    public:
        glm::vec3 mFragmentPosition;
        glm::vec3 mLightPosition;
};

class PhongFSData : public FSDataBase
{
    public:
        glm::vec3 mFragmentPosition[3];
        glm::vec3 mLightPosition;
        glm::vec3 mTangentLightPosition[3];
};

class ColorPhongProgram : public TRProgramBase<PhongVSData, PhongFSData>
{
    void loadVertexData(TRMeshData &, PhongVSData *, size_t);
    void vertex(PhongVSData &, glm::vec4 &clipV);
    bool geometry(PhongVSData *, PhongFSData &);
    bool fragment(PhongFSData &, float color[3]);
};

class TextureMapPhongProgram : public TRProgramBase<PhongVSData, PhongFSData>
{
    void loadVertexData(TRMeshData &, PhongVSData *, size_t);
    void vertex(PhongVSData &, glm::vec4 &clipV);
    bool geometry(PhongVSData *, PhongFSData &);
    bool fragment(PhongFSData &, float color[3]);
};

#ifndef __BLINN_PHONG__
#define __BLINN_PHONG__ 1
#endif

void trSetRenderThreadNum(size_t num);
void trEnableLighting(bool enable);
void trSetAmbientStrength(float v);
void trSetSpecularStrength(float v);
void trSetLightColor3f(float r, float g, float b);
void trSetLightPosition3f(float x, float y, float z);
void trViewport(int x, int y, int w, int h);
void trBindTexture(TRTexture *texture, int index);
void trClear();
void trClearColor3f(float r, float g, float b);
void trSetModelMat(glm::mat4 mat);
void trSetViewMat(glm::mat4 mat);
void trSetProjMat(glm::mat4 mat);
void trTriangles(TRMeshData &data, TRDrawMode mode);
TRBuffer* trCreateRenderTarget(int w, int h);
void trSetCurrentRenderTarget(TRBuffer *traget);
#endif
