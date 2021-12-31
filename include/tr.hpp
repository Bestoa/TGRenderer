#ifndef __TOPGUN_RENDERER__
#define __TOPGUN_RENDERER__

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <glm/glm.hpp>

#include "buffer.hpp"
#include "texture.hpp"
#include "mat.hpp"

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
        void fillSpriteColor();
};

enum TRDrawType
{
    DRAW_WITH_TEXTURE,
    DRAW_WITH_COLOR,
};

enum TRDrawMode
{
    TR_FILL,
    TR_LINE,
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

        virtual void preInterp(TRVSData *, TRFSData &);
        virtual void interpVertex(float t, TRVSData &in1, TRVSData &in2, TRVSData &outV);

    private:
        float mUPC;
        float mVPC;

        virtual void loadVertexData(TRMeshData &, TRVSData *, size_t) = 0;
        virtual void vertex(TRVSData &) = 0;
        virtual bool fragment(TRFSData &, float color[3]/* Out */) = 0;

        void clipLineNear(TRVSData &in1, TRVSData &in2, TRVSData out[4], size_t &index);
        void clipNear(TRVSData in[3], TRVSData out[4], size_t &index);

        void drawTriangle(TRMeshData &, size_t index);
        void rasterization(glm::vec4 clip_v[3], TRFSData &);

        void rasterizationPoint(glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, glm::vec2 &point, TRFSData &fsdata, bool insideCheck);
        void rasterizationLine(glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, int p1, int p2, TRFSData &fsdata);
};

class VSDataBase
{
    public:
        glm::vec3 mVertex;
        glm::vec2 mTexCoord;
        glm::vec3 mNormal;
        glm::vec3 mColor;
        glm::vec3 mTangent;

        glm::vec4 mClipV; /* out */
};

class FSDataBase
{
    public:
        glm::vec4 mClipV[3];
        glm::vec2 mTexCoord[3];
        glm::vec3 mNormal[3];
        glm::vec3 mColor[3];
};

class ColorProgram : public TRProgramBase<VSDataBase, FSDataBase>
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase &);
    bool fragment(FSDataBase &, float color[3]);
};

class TextureMapProgram : public TRProgramBase<VSDataBase, FSDataBase>
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase &);
    bool fragment(FSDataBase &, float color[3]);
};

class PhongVSData : public VSDataBase
{
    public:
        glm::vec3 mViewFragmentPosition;
        glm::vec3 mTangentFragmentPosition;
        glm::vec3 mLightPosition;
};

class PhongFSData : public FSDataBase
{
    public:
        glm::vec3 mViewFragmentPosition[3];
        glm::vec3 mTangentFragmentPosition[3];
        glm::vec3 mLightPosition;
        glm::vec3 mTangentLightPosition[3];
};

class ColorPhongProgram : public TRProgramBase<PhongVSData, PhongFSData>
{
    void loadVertexData(TRMeshData &, PhongVSData *, size_t);
    void vertex(PhongVSData &);
    void preInterp(PhongVSData *, PhongFSData &);
    bool fragment(PhongFSData &, float color[3]);
    void interpVertex(float t, PhongVSData &in1, PhongVSData &in2, PhongVSData &outV);
};

class TextureMapPhongProgram : public TRProgramBase<PhongVSData, PhongFSData>
{
    void loadVertexData(TRMeshData &, PhongVSData *, size_t);
    void vertex(PhongVSData &);
    void preInterp(PhongVSData *, PhongFSData &);
    bool fragment(PhongFSData &, float color[3]);
    void interpVertex(float t, PhongVSData &in1, PhongVSData &in2, PhongVSData &outV);
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
void trSetMat3(glm::mat3 mat, MAT_INDEX_TYPE type);
void trSetMat4(glm::mat4 mat, MAT_INDEX_TYPE type);
void trTriangles(TRMeshData &data, TRDrawType type);
void trDrawMode(TRDrawMode mode);
TRBuffer* trCreateRenderTarget(int w, int h);
void trSetCurrentRenderTarget(TRBuffer *traget);
#endif
