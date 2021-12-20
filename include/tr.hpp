#ifndef __TOPGUN_RENDERER__
#define __TOPGUN_RENDERER__

#include <iostream>
#include <string>
#include <vector>
#include <mutex>
#include <glm/glm.hpp>

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

#define BPP (3)
class TRBuffer
{
    public:
        uint8_t *mData = nullptr;
        uint32_t mW = 0;
        uint32_t mH = 0;
        uint32_t mStride = 0;

        std::mutex mDepthMutex;

        void setViewport(int x, int y, int w, int h);
        void viewport(glm::vec2 &screen_v, glm::vec4 &ndc_v);
        void setBGColor(float r, float g, float b);
        void clearColor();
        void clearDepth();
        bool depthTest(int x, int y, float depth);

        void setExtBuffer(void *addr);

        TRBuffer() = delete;
        TRBuffer(const TRBuffer &) = delete;
        TRBuffer& operator=(const TRBuffer &) = delete;

        ~TRBuffer();

        static TRBuffer* create(int w, int h, bool ext = false);

    private:
        float *mDepth = nullptr;

        uint32_t mVX = 0;
        uint32_t mVY = 0;
        uint32_t mVW = 0;
        uint32_t mVH = 0;

        uint8_t mBGColor[BPP] = { 0, 0, 0 };
        // data was not allocated by us.
        bool mExtBuffer = false;
        bool mValid = false;

        TRBuffer(int w, int h, bool ext);
};

template <class T>
static inline float interpolation(T v[3], int i, float w0, float w1, float w2)
{
    return v[0][i] * w0 + v[1][i] * w1 + v[2][i] * w2;
}

static inline float edge(glm::vec2 &a, glm::vec2 &b, glm::vec2 &c)
{
    return (c.x - a.x)*(b.y - a.y) - (c.y - a.y)*(b.x - a.x);
}

template <class TRVSData, class TRFSData>
class TRProgramBase
{
    public:
        TRVSData mVSData[3];
        TRFSData mFSData;
        TRBuffer *mBuffer;

        virtual ~TRProgramBase() {};

        void drawTriangle(TRBuffer *buffer, TRMeshData &mesh, size_t i);

    private:

        virtual void loadVertexData(TRMeshData &, size_t) = 0;
        virtual void vertex(size_t, glm::vec4 &) = 0;
        virtual bool geometry() = 0;
        virtual bool fragment(float w0, float w1, float w2, uint8_t color[3]) = 0;

        void rasterization(glm::vec4 clip_v[3]);
};

class WireframeVSData
{
    public:
        glm::vec3 mVertex;
};

class WireframeFSData
{
};

class WireframeProgram : public TRProgramBase<WireframeVSData, WireframeFSData>
{
    void loadVertexData(TRMeshData &, size_t);
    void vertex(size_t, glm::vec4 &clipV);
    bool geometry();
    bool fragment(float w0, float w1, float w2, uint8_t color[3]);
};

class ColorVSData
{
    public:
        glm::vec3 mVertex;
        glm::vec3 mColor;
};

class ColorFSData
{
    public:
        glm::vec3 mColor[3];
};

class ColorProgram : public TRProgramBase<ColorVSData, ColorFSData>
{
    void loadVertexData(TRMeshData &, size_t);
    void vertex(size_t, glm::vec4 &clipV);
    bool geometry();
    bool fragment(float w0, float w1, float w2, uint8_t color[3]);
};

class TextureMapVSData
{
    public:
        glm::vec3 mVertex;
        glm::vec2 mTexCoord;
};

class TextureMapFSData
{
    public:
        glm::vec2 mTexCoord[3];
};

class TextureMapProgram : public TRProgramBase<TextureMapVSData, TextureMapFSData>
{
    void loadVertexData(TRMeshData &, size_t);
    void vertex(size_t, glm::vec4 &clipV);
    bool geometry();
    bool fragment(float w0, float w1, float w2, uint8_t color[3]);
};

class PhongVSData
{
    public:
        glm::vec3 mVertex;
        glm::vec2 mTexCoord;
        glm::vec3 mViewFragPosition;
        glm::vec3 mNormal;
        glm::vec3 mLightPosition;
        glm::vec3 mTangent;
};

class PhongFSData
{
    public:
        glm::vec2 mTexCoord[3];
        glm::vec3 mViewFragPosition[3];
        glm::vec3 mNormal[3];
        glm::vec3 mLightPosition;
        glm::vec3 mTangent;
};

class PhongProgram : public TRProgramBase<PhongVSData, PhongFSData>
{
    void loadVertexData(TRMeshData &, size_t);
    void vertex(size_t, glm::vec4 &clipV);
    bool geometry();
    bool fragment(float w0, float w1, float w2, uint8_t color[3]);
};

#ifndef __BLINN_PHONG__
#define __BLINN_PHONG__ 1
#endif

void trSetRenderThreadNum(size_t num);
void trEnableLighting(bool enable);
void trSetAmbientStrength(float v);
void trSetLightColor3f(float r, float g, float b);
void trSetLightPosition3f(float x, float y, float z);
void trViewport(int x, int y, int w, int h);
void trMakeCurrent(TRBuffer *buffer);
void trBindTexture(TRTexture *texture, int index);
void trClear();
void trClearColor3f(float r, float g, float b);
void trSetModelMat(glm::mat4 mat);
void trSetViewMat(glm::mat4 mat);
void trSetProjMat(glm::mat4 mat);
void trTriangles(TRMeshData &data, TRDrawMode mode);
#endif
