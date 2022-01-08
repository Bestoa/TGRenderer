#ifndef __TOPGUN_RENDERER__
#define __TOPGUN_RENDERER__

#include <vector>
#include <glm/glm.hpp>

#include "buffer.hpp"
#include "texture.hpp"
#include "mat.hpp"

#ifndef __BLINN_PHONG__
#define __BLINN_PHONG__ 1
#endif

namespace TGRenderer
{
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

    enum TRCullFaceMode
    {
        TR_NONE,
        TR_CCW,
        TR_CW,
    };

    enum TRClearBit
    {
        TR_CLEAR_COLOR_BIT = 1,
        TR_CLEAR_DEPTH_BIT = 2,
        TR_CLEAR_STENCIL_BIT = 4,
    };

    static inline float edge(glm::vec2 &a, glm::vec2 &b, glm::vec2 &c)
    {
        return (c.x - a.x)*(b.y - a.y) - (c.y - a.y)*(b.x - a.x);
    }

    constexpr int SHADER_VARYING_NUM_MAX = 16;
    constexpr int THREAD_MAX = 10;

    class VSOutData
    {
        public:
            glm::vec4 tr_Position;

            glm::vec2 mVaryingVec2[SHADER_VARYING_NUM_MAX];
            glm::vec3 mVaryingVec3[SHADER_VARYING_NUM_MAX];
            glm::vec4 mVaryingVec4[SHADER_VARYING_NUM_MAX];
    };

    class FSInData
    {
        public:
            glm::vec4 tr_PositionPrim[3];

            glm::vec2 mVaryingVec2Prim[SHADER_VARYING_NUM_MAX][3];
            glm::vec3 mVaryingVec3Prim[SHADER_VARYING_NUM_MAX][3];
            glm::vec4 mVaryingVec4Prim[SHADER_VARYING_NUM_MAX][3];
    };

    constexpr int MAX_VSDATA_NUM = 10;

    class Program
    {
        public:
            virtual ~Program() = default;
            virtual Program* clone() = 0;
            void drawTrianglesInstanced(TRBuffer *buffer, TRMeshData &mesh, size_t index, size_t num);

        protected:
            /* Fast interpolation funciotn , v'0 = v0, v'1 = v1 - v0, v'2 = v2 - v0 , pre-calculated */
            template <class T> inline T interpFast(T v[3])
            {
                return v[0] + v[1] * mUPC + v[2] * mVPC;
            }

        private:
            TRBuffer *mBuffer = nullptr;

            VSOutData mVSOutData[MAX_VSDATA_NUM];
            FSInData mFSInData;
            int mAllocIndex = 0;

            float mUPC = 0.f;
            float mVPC = 0.f;

            virtual void vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index) = 0;
            virtual bool fragment(FSInData *fsdata, float color[3]/* Out */) = 0;
            virtual void getVaryingNum(size_t &v2, size_t &v3, size_t &v4) = 0;

            VSOutData *allocVSOutData();
            FSInData *allocFSInData();
            /* Free vsdata/fsdata which were allocated by us.
             * Suggest to use pre-allocated mode, just need to reset the index.
             * Otherwise you need to record all the vsdata/fsdata pointer and
             * free them. */
            void freeShaderData();

            /* Prepare fragment data for interpolation. Put { V0.AAA, V1.AAA - V0.AAA, V2.AAA - V0.AAA } into FSInData. */
            void prepareFragmentData(VSOutData *vsdata[3], FSInData *fsdata);

            /* Add a new vertex if needed when clipping on W axis. Formula: new V.AAA = in2.AAA + t * (in1.AAA -in2.AAA) */
            void getIntersectionVertex(VSOutData *in1, VSOutData *in2, VSOutData *outV);

            void clipLineOnWAxis(VSOutData *in1, VSOutData *in2, VSOutData *out[4], size_t &index);
            void clipOnWAxis(VSOutData *in[3], VSOutData *out[4], size_t &index);

            void drawTriangle(TRMeshData &mesh, size_t index);
            void rasterization(VSOutData *vsdata[3]);
            void rasterizationPoint(glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, glm::vec2 &point, FSInData *fsdata, bool insideCheck);
            void rasterizationLine(glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, int p1, int p2, FSInData *fsdata);
    };

    // Matrix related API
    void trSetMat3(glm::mat3 mat, MAT_INDEX_TYPE type);
    void trSetMat4(glm::mat4 mat, MAT_INDEX_TYPE type);
    glm::mat3 &trGetMat3(MAT_INDEX_TYPE type);
    glm::mat4 &trGetMat4(MAT_INDEX_TYPE type);
    void trResetMat3(MAT_INDEX_TYPE type);
    void trResetMat4(MAT_INDEX_TYPE type);
    // Draw related API
    void trTriangles(TRMeshData &mesh, Program *prog);
    // Core state related API
    void trSetRenderThreadNum(size_t num);
    void trEnableStencilWrite(bool enable);
    void trEnableStencilTest(bool enable);
    void trEnableDepthTest(bool enable);
    void trDrawMode(TRDrawMode mode);
    void trCullFaceMode(TRCullFaceMode mode);
    // Buffer related API
    TRBuffer *trCreateRenderTarget(int w, int h);
    void trSetCurrentRenderTarget(TRBuffer *traget);
    void trViewport(int x, int y, int w, int h);
    void trClear(int mode);
    void trClearColor3f(float r, float g, float b);
    // Texture related API
    void trBindTexture(TRTexture *texture, int type);
    TRTexture *trGetTexture(int type);
    // Uniform data related API
    void trSetUniformData(void *data);
    void *trGetUniformData();
}
#endif
