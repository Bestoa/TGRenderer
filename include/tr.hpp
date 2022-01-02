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

enum TRDrawMode
{
    TR_FILL,
    TR_LINE,
};

static inline float edge(glm::vec2 &a, glm::vec2 &b, glm::vec2 &c)
{
    return (c.x - a.x)*(b.y - a.y) - (c.y - a.y)*(b.x - a.x);
}

class VSDataBase
{
    public:
        glm::vec3 mVertex;
        glm::vec2 mTexCoord;
        glm::vec3 mNormal;
        glm::vec3 mColor;
        glm::vec3 mTangent;

        glm::vec4 mClipV; /* out */

        virtual ~VSDataBase() = default;
};

class FSDataBase
{
    public:
        glm::vec4 mClipV[3];
        glm::vec2 mTexCoord[3];
        glm::vec3 mNormal[3];
        glm::vec3 mColor[3];

        virtual ~FSDataBase() = default;
};

#define MAX_VSDATA_NUM (10)

class TRProgramBase
{
    public:
        virtual ~TRProgramBase() = default;
        virtual TRProgramBase* clone() = 0;

        void drawTrianglesInstanced(TRBuffer *buffer, TRMeshData &mesh, size_t index, size_t num);

    protected:
        TRBuffer *mBuffer = nullptr;

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

        /* Prepare fragment data for interpolation. If you want the user added variable
         * (such like AAA) in VSDataBase can be interpolated for each fragment,
         * just put { V0.AAA, V1.AAA - V0.AAA, V2.AAA - V0.AAA } into FSDataBase.
         * For uniform variables, only need to copy from VSDataBase to FSDataBase. */
        virtual void prepareFragmentData(VSDataBase *vsdata[3], FSDataBase *fsdata);

        /* Add a new vertex if needed when clipping near plane. Formula:
         * new V.AAA = in2.AAA + t * (in1.AAA -in2.AAA) */
        virtual void interpVertex(float t, VSDataBase *in1, VSDataBase *in2, VSDataBase *outV);

    private:
        float mUPC = 0.f;
        float mVPC = 0.f;

        /* Pre allocated vsdata/fsdata, 5 vsdata + 1 fsdata is engouth.
         * 3 for original vertices and 2 for new vertices */
        VSDataBase __VSData__[MAX_VSDATA_NUM];
        FSDataBase __FSData__;
        int mPreAllocVSIndex = 0;

        virtual void loadVertexData(TRMeshData &mesh, VSDataBase *vsdata, size_t index) = 0;
        virtual void vertex(VSDataBase *vsdata) = 0;
        virtual bool fragment(FSDataBase *fsdata, float color[3]/* Out */) = 0;
        virtual VSDataBase *allocVSData();
        virtual FSDataBase *allocFSData();
        /* Free vsdata/fsdata which were allocated by us.
         * Suggest to use pre-allocated mode, just need to reset the index.
         * Otherwise you need to record all the vsdata/fsdata pointer and
         * free them. */
        virtual void freeShaderData();

        void clipLineNear(VSDataBase *in1, VSDataBase *in2, VSDataBase *out[4], size_t &index);
        void clipNear(VSDataBase *in[3], VSDataBase *out[4], size_t &index);

        void drawTriangle(TRMeshData &mesh, size_t index);
        void rasterization(glm::vec4 clip_v[3], FSDataBase *fsdata);

        void rasterizationPoint(glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, glm::vec2 &point, FSDataBase *fsdata, bool insideCheck);
        void rasterizationLine(glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, int p1, int p2, FSDataBase *fsdata);
};

class ColorProgram : public TRProgramBase
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase *);
    bool fragment(FSDataBase *, float color[3]);

    TRProgramBase *clone();
};

class TextureMapProgram : public TRProgramBase
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase *);
    bool fragment(FSDataBase *, float color[3]);

    TRProgramBase *clone();
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

class ColorPhongProgram : public TRProgramBase
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase *);
    void prepareFragmentData(VSDataBase *[3], FSDataBase *);
    bool fragment(FSDataBase *, float color[3]);
    void interpVertex(float , VSDataBase *, VSDataBase *, VSDataBase *);

    void freeShaderData();
    VSDataBase *allocVSData();
    FSDataBase *allocFSData();

    TRProgramBase *clone();

    PhongVSData mVSData[MAX_VSDATA_NUM];
    PhongFSData mFSData;
    int mAllocIndex = 0;
};

class TextureMapPhongProgram : public TRProgramBase
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase *);
    void prepareFragmentData(VSDataBase *[3], FSDataBase *);
    bool fragment(FSDataBase *, float color[3]);
    void interpVertex(float , VSDataBase *, VSDataBase *, VSDataBase *);

    void freeShaderData();
    VSDataBase *allocVSData();
    FSDataBase *allocFSData();

    TRProgramBase *clone();

    PhongVSData mVSData[MAX_VSDATA_NUM];
    PhongFSData mFSData;
    int mAllocIndex = 0;
};

#ifndef __BLINN_PHONG__
#define __BLINN_PHONG__ 1
#endif

void trSetRenderThreadNum(size_t num);
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
void trTriangles(TRMeshData &mesh, TRProgramBase *prog);
void trDrawMode(TRDrawMode mode);
TRBuffer* trCreateRenderTarget(int w, int h);
void trSetCurrentRenderTarget(TRBuffer *traget);
#endif
