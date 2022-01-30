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

#define __DEBUG_FINISH_CB__ 0

namespace TGRenderer
{
    class TRMeshData
    {
        public:
            std::vector<glm::vec3> vertices;
            std::vector<glm::vec2> texcoords;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec3> colors;
            std::vector<glm::vec3> tangents;

            TRMeshData() = default;
            TRMeshData(const TRMeshData &&) = delete;

            void computeTangent();
            void fillSpriteColor();
    };

    enum TRDrawMode
    {
        TR_TRIANGLES,
    };

    enum TRPolygonMode
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
            inline glm::vec4 getPosition() const
            {
                return tr_PositionPrim[0] + tr_PositionPrim[1] * mUPC + tr_PositionPrim[2] * mVPC;
            }

            /* Fast interpolation funciotn , v'0 = v0, v'1 = v1 - v0, v'2 = v2 - v0 , pre-calculated */
            inline glm::vec2 getVec2(int index) const
            {
                return mVaryingVec2Prim[index][0] + mVaryingVec2Prim[index][1] * mUPC + mVaryingVec2Prim[index][2] * mVPC;
            }

            inline glm::vec3 getVec3(int index) const
            {
                return mVaryingVec3Prim[index][0] + mVaryingVec3Prim[index][1] * mUPC + mVaryingVec3Prim[index][2] * mVPC;
            }

            inline glm::vec4 getVec4(int index) const
            {
                return mVaryingVec4Prim[index][0] + mVaryingVec4Prim[index][1] * mUPC + mVaryingVec4Prim[index][2] * mVPC;
            }

        private:
            glm::vec4 tr_PositionPrim[3];

            glm::vec2 mVaryingVec2Prim[SHADER_VARYING_NUM_MAX][3];
            glm::vec3 mVaryingVec3Prim[SHADER_VARYING_NUM_MAX][3];
            glm::vec4 mVaryingVec4Prim[SHADER_VARYING_NUM_MAX][3];

            float mUPC = 0.f;
            float mVPC = 0.f;

            friend class Program;
    };

    class Shader
    {
        public:
            virtual ~Shader() = default;

            virtual void vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index) = 0;
            virtual bool fragment(FSInData *fsdata, float color[3]/* Out */) = 0;
            virtual void getVaryingNum(size_t &v2, size_t &v3, size_t &v4) = 0;
    };

#if __DEBUG_FINISH_CB__
    typedef void (*fcb)(void *);
#endif

    // Matrix related API
    void trSetMat3(glm::mat3 mat, MAT_INDEX_TYPE type);
    void trSetMat4(glm::mat4 mat, MAT_INDEX_TYPE type);
    glm::mat3 &trGetMat3(MAT_INDEX_TYPE type);
    glm::mat4 &trGetMat4(MAT_INDEX_TYPE type);
    void trResetMat3(MAT_INDEX_TYPE type);
    void trResetMat4(MAT_INDEX_TYPE type);
    // Draw related API
    void trTriangles(TRMeshData &mesh, Shader *shader);
    // Core state related API
    void trSetRenderThreadNum(size_t num);
    void trEnableStencilTest(bool enable);
    void trEnableStencilWrite(bool enable);
    void trEnableDepthTest(bool enable);
    void trPolygonMode(TRPolygonMode mode);
    void trCullFaceMode(TRCullFaceMode mode);
    // Buffer related API
    TRBuffer *trCreateRenderTarget(int w, int h);
    void trSetRenderTarget(TRBuffer *traget);
    TRBuffer *trGetRenderTarget();
    void trViewport(int x, int y, int w, int h);
    void trClear(int mode);
    void trClearColor3f(float r, float g, float b);
    // Texture related API
    void trBindTexture(TRTexture *texture, int type);
    void trUnbindTextureAll();
    TRTexture *trGetTexture(int type);
    // Uniform data related API
    void trSetUniformData(void *data);
    void *trGetUniformData();
#if __DEBUG_FINISH_CB__
    // Debug related API
    void trSetFinishCB(fcb func, void *data);
#endif
}
#endif
