#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <glm/ext.hpp>

#include "tr.hpp"

namespace TGRenderer
{
    // global value
    TRBuffer *gRenderTarget = nullptr;
    TRTexture *gTexture[TEXTURE_INDEX_MAX] = { nullptr };
    void *gUniform = nullptr;

    glm::mat4 gDefaultMat4[MAT_INDEX_MAX] =
    {
        glm::mat4(1.0f), // model mat
        glm::mat4(1.0f), // view mat
        // defult proj mat should swap z-order
        glm::mat4({1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}), // proj mat
    };

    glm::mat4 gMat4[MAT_INDEX_MAX] =
    {
        gDefaultMat4[MAT4_MODEL],
        gDefaultMat4[MAT4_VIEW],
        gDefaultMat4[MAT4_PROJ],
    };

    glm::mat3 gDefaultMat3[MAT_INDEX_MAX] =
    {
        glm::mat3(1.0f), // normal mat
    };

    glm::mat3 gMat3[MAT_INDEX_MAX] =
    {
        gDefaultMat3[MAT3_NORMAL],
    };

    size_t gThreadNum = 4;
    int gDrawMode = TR_FILL;
    int gCullFace = TR_CCW;
    bool gEnableDepthTest = true;
    bool gEnableStencilTest = false;
    bool gEnableStencilWrite = false;

    // internal function
    void TRMeshData::computeTangent()
    {
        if (tangents.size() != 0)
            return;

        for (size_t i = 0; i < vertices.size(); i += 3)
        {
            // Edges of the triangle : postion delta
            glm::vec3 deltaPos1 = vertices[i+1] - vertices[i];
            glm::vec3 deltaPos2 = vertices[i+2] - vertices[i];

            // UV delta
            glm::vec2 deltaUV1 = uvs[i+1]-uvs[i];
            glm::vec2 deltaUV2 = uvs[i+2]-uvs[i];

            float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);

            glm::vec3 T = (deltaPos1 * deltaUV2.y   - deltaPos2 * deltaUV1.y) * r;
            tangents.push_back(T);
        }
    }

    void TRMeshData::fillSpriteColor()
    {
        if (colors.size() != 0)
            return;

        glm::vec3 color[3] = { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) };

        for (size_t i = 0; i < vertices.size(); i++)
            colors.push_back(color[i % 3]);
    }

    static inline void __compute_premultiply_mat__()
    {
        gMat4[MAT4_MODELVIEW] =  gMat4[MAT4_VIEW] * gMat4[MAT4_MODEL];
        gMat4[MAT4_MVP] = gMat4[MAT4_PROJ] * gMat4[MAT4_MODELVIEW];
        gMat3[MAT3_NORMAL] = glm::transpose(glm::inverse(gMat4[MAT4_MODELVIEW]));
    }

    VSOutData *Program::allocVSOutData()
    {
        return dynamic_cast<VSOutData *>(&mVSOutData[mAllocIndex++]);
    }

    FSInData *Program::allocFSInData()
    {
        return dynamic_cast<FSInData *>(&mFSInData);
    }

    void Program::freeShaderData()
    {
        mAllocIndex = 0;
    }

    void Program::prepareFragmentData(VSOutData *vsdata[3], FSInData *fsdata)
    {
        size_t v2n, v3n, v4n;
        getVaryingNum(v2n, v3n, v4n);

        fsdata->tr_PositionPrim[0] = vsdata[0]->tr_Position;
        fsdata->tr_PositionPrim[1] = vsdata[1]->tr_Position - vsdata[0]->tr_Position;
        fsdata->tr_PositionPrim[2] = vsdata[2]->tr_Position - vsdata[0]->tr_Position;

        for (size_t i = 0; i < v2n; i++)
        {
            fsdata->mVaryingVec2Prim[i][0] = vsdata[0]->mVaryingVec2[i];
            fsdata->mVaryingVec2Prim[i][1] = vsdata[1]->mVaryingVec2[i] - vsdata[0]->mVaryingVec2[i];
            fsdata->mVaryingVec2Prim[i][2] = vsdata[2]->mVaryingVec2[i] - vsdata[0]->mVaryingVec2[i];
        }
        for (size_t i = 0; i < v3n; i++)
        {
            fsdata->mVaryingVec3Prim[i][0] = vsdata[0]->mVaryingVec3[i];
            fsdata->mVaryingVec3Prim[i][1] = vsdata[1]->mVaryingVec3[i] - vsdata[0]->mVaryingVec3[i];
            fsdata->mVaryingVec3Prim[i][2] = vsdata[2]->mVaryingVec3[i] - vsdata[0]->mVaryingVec3[i];
        }
        for (size_t i = 0; i < v4n; i++)
        {
            fsdata->mVaryingVec4Prim[i][0] = vsdata[0]->mVaryingVec4[i];
            fsdata->mVaryingVec4Prim[i][1] = vsdata[1]->mVaryingVec4[i] - vsdata[0]->mVaryingVec4[i];
            fsdata->mVaryingVec4Prim[i][2] = vsdata[2]->mVaryingVec4[i] - vsdata[0]->mVaryingVec4[i];
        }
    }

    constexpr float W_CLIPPING_PLANE = 0.1f;

    void Program::getIntersectionVertex(VSOutData *in1, VSOutData *in2, VSOutData *outV)
    {
        size_t v2n, v3n, v4n;
        getVaryingNum(v2n, v3n, v4n);

        float t = (in2->tr_Position.w - W_CLIPPING_PLANE)/(in2->tr_Position.w - in1->tr_Position.w);

        for (size_t i = 0; i < v2n; i++)
            outV->mVaryingVec2[i] = in2->mVaryingVec2[i] + t * (in1->mVaryingVec2[i] - in2->mVaryingVec2[i]);

        for (size_t i = 0; i < v3n; i++)
            outV->mVaryingVec3[i] = in2->mVaryingVec3[i] + t * (in1->mVaryingVec3[i] - in2->mVaryingVec3[i]);

        for (size_t i = 0; i < v4n; i++)
            outV->mVaryingVec4[i] = in2->mVaryingVec4[i] + t * (in1->mVaryingVec4[i] - in2->mVaryingVec4[i]);

        outV->tr_Position = in2->tr_Position + t * (in1->tr_Position - in2->tr_Position);
    }

    void Program::clipLineOnWAxis(VSOutData *in1, VSOutData *in2, VSOutData *out[3], size_t &index)
    {
        // Sutherland-Hodgeman clip
        // t = w1 / (w1 - w2)
        // V = V1 + t * (V2 - V1)
        if (in1->tr_Position.w > W_CLIPPING_PLANE && in2->tr_Position.w > W_CLIPPING_PLANE)
        {
            out[index++] = in2;
        }
        else if (in1->tr_Position.w > W_CLIPPING_PLANE && in2->tr_Position.w < W_CLIPPING_PLANE)
        {
            VSOutData *outV = allocVSOutData();
            // get interp V and put V into output
            getIntersectionVertex(in1, in2, outV);
            out[index++] = outV;
        }
        else if (in1->tr_Position.w < W_CLIPPING_PLANE && in2->tr_Position.w > W_CLIPPING_PLANE)
        {
            VSOutData *outV = allocVSOutData();
            // get interp V and put V and V2 into output
            getIntersectionVertex(in2, in1, outV);
            out[index++] = outV;
            out[index++] = in2;
        }
    }

    void Program::clipOnWAxis(VSOutData *in[3], VSOutData *out[4], size_t &index)
    {
        index = 0;
        clipLineOnWAxis(in[0], in[1], out, index);
        clipLineOnWAxis(in[1], in[2], out, index);
        clipLineOnWAxis(in[2], in[0], out, index);
    }

    void Program::drawTriangle(TRMeshData &mesh, size_t index)
    {
        VSOutData *vsdata[3];
        for (size_t i = 0; i < 3; i++)
        {
            vsdata[i] = allocVSOutData();
            vertex(mesh, vsdata[i], index * 3 + i);
        }

        if (vsdata[0]->tr_Position.w >= W_CLIPPING_PLANE
                && vsdata[1]->tr_Position.w >= W_CLIPPING_PLANE
                && vsdata[2]->tr_Position.w >= W_CLIPPING_PLANE)
        {
            rasterization(vsdata);
            freeShaderData();
            return;
        }

        // Max is 4 output.
        VSOutData *out[4] = { nullptr };
        size_t total = 0;
        clipOnWAxis(vsdata, out, total);

        for (size_t i = 0; total > 2 && i < total - 2; i++)
        {
            vsdata[0] = out[0];
            vsdata[1] = out[i + 1];
            vsdata[2] = out[i + 2];

            rasterization(vsdata);
        }
        freeShaderData();
    }

    void Program::drawTrianglesInstanced(TRBuffer *buffer, TRMeshData &mesh, size_t index, size_t num)
    {
        size_t i = 0, j = 0;
        size_t trianglesNum = mesh.vertices.size() / 3;

        mBuffer = buffer;

        for (i = index, j = 0; i < trianglesNum && j < num; i++, j++)
            drawTriangle(mesh, i);
    }

    void Program::rasterizationPoint(
            glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, glm::vec2 &point, FSInData *fsdata, bool insideCheck)
    {
        float w0 = edge(screen_v[1], screen_v[2], point);
        float w1 = edge(screen_v[2], screen_v[0], point);
        float w2 = edge(screen_v[0], screen_v[1], point);
        if (insideCheck)
        {
            switch (gCullFace)
            {
                case TR_CCW:
                    if (w0 < 0 || w1 < 0 || w2 < 0)
                        return;
                    break;
                case TR_CW:
                    if (w0 > 0 || w1 > 0 || w2 > 0)
                        return;
                    break;
                default:
                    if (!((w0 > 0 && w1 > 0 && w2 > 0) || (w0 < 0 && w1 < 0 && w2 < 0)))
                        return;
                    break;
            }
        }

        int x = (int)point.x;
        int y = (int)point.y;

        /* Barycentric coordinate */
        w0 /= area;
        w1 /= area;
        w2 /= area;

        /* Using the ndc.z to calculate depth, faster then using the interpolated clip.z / clip.w. */
        float depth = w0 * ndc_v[0].z + w1 * ndc_v[1].z + w2 * ndc_v[2].z;
        depth = depth / 2.0f + 0.5f;
        /* z in ndc of opengl should between 0.0f to 1.0f */
        if (depth < 0.0f)
            return;

        /* Perspective-Correct */
        w0 /= clip_v[0].w;
        w1 /= clip_v[1].w;
        w2 /= clip_v[2].w;

        float areaPC = (w0 + w1 + w2);
        /* One more special case... */
        if (!insideCheck && areaPC == 0)
            areaPC = 1e-6;
        mUPC = w1 / areaPC;
        mVPC = w2 / areaPC;

        size_t offset = mBuffer->getOffset(x, y);
        /* easy-z */
        /* Do not use mutex here to speed up */
        if (gEnableDepthTest && !mBuffer->depthTest(offset, depth))
            return;

        float color[3];
        if (!fragment(fsdata, color))
            return;

#if __NEED_BUFFER_LOCK__
        std::lock_guard<std::mutex> lck(mBuffer->getMutex(offset));
#endif
        if (gEnableStencilTest && !mBuffer->stencilTest(offset))
            return;

        /* depth test */
        if (gEnableDepthTest && !mBuffer->depthTest(offset, depth))
            return;

        /* Write stencil buffer need to pass depth test */
        if (gEnableStencilWrite)
            mBuffer->stencilFunc(offset);

        mBuffer->setColor(offset, color);
    }

    void Program::rasterizationLine(
            glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, int p1, int p2, FSInData *fsdata)
    {
        float x0 = screen_v[p1].x, x1 = screen_v[p2].x;
        float y0 = screen_v[p1].y, y1 = screen_v[p2].y;

        // Bresenham's line algorithm
        bool steep = abs(y1 - y0) > abs(x1 - x0);
        if (steep)
        {
            std::swap(x0, y0);
            std::swap(x1, y1);
        }

        if (x0 > x1)
        {
            std::swap(x0, x1);
            std::swap(y0, y1);
        }
        int deltax = int(x1 - x0);
        int deltay = int(abs(y1 - y0));
        float error = 0;
        float deltaerr = (float)deltay / (float)deltax;
        int ystep;
        int y = y0;
        if (y0 < y1)
            ystep = 1;
        else
            ystep = -1;
        for (int x = int(x0 + 0.5); x < x1; x++)
        {
            glm::vec2 v(x, y);
            if (steep)
                v = glm::vec2(y, x);
            // Not good, but works well
            if (!(v.x > (int)mBuffer->mW || v.y > (int)mBuffer->mH || v.x < 0 || v.y < 0))
                // skip inside check for draw line
                rasterizationPoint(clip_v, ndc_v, screen_v, area, v, fsdata, false);

            error += deltaerr;
            if (error >= 0.5)
            {
                y += ystep;
                error -= 1.0;
            }
        }
    }

    void Program::rasterization(VSOutData *vsdata[3])
    {
        glm::vec4 clip_v[3] = { vsdata[0]->tr_Position, vsdata[1]->tr_Position, vsdata[2]->tr_Position };
        glm::vec4 ndc_v[3];
        glm::vec2 screen_v[3];

        for (int i = 0; i < 3; i++)
        {
            ndc_v[i] = clip_v[i] / clip_v[i].w;
            mBuffer->viewport(screen_v[i], ndc_v[i]);
        }

        float area = edge(screen_v[0], screen_v[1], screen_v[2]);

        if (gCullFace == TR_CCW && area <= 0)
            return;
        else if (gCullFace == TR_CW && area >= 0)
            return;
        else if (area == 0)
            /* Special case */
            area = 1e-6;

        FSInData *fsdata = allocFSInData();
        prepareFragmentData(vsdata, fsdata);

        if (gDrawMode == TR_LINE)
        {
            rasterizationLine(clip_v, ndc_v, screen_v, area, 0, 1, fsdata);
            rasterizationLine(clip_v, ndc_v, screen_v, area, 1, 2, fsdata);
            rasterizationLine(clip_v, ndc_v, screen_v, area, 2, 0, fsdata);
            return;
        }

        int xStart = glm::max(0.0f, glm::min(glm::min(screen_v[0].x, screen_v[1].x), screen_v[2].x)) + 0.5;
        int yStart = glm::max(0.0f, glm::min(glm::min(screen_v[0].y, screen_v[1].y), screen_v[2].y)) + 0.5;
        int xEnd = glm::min(float(mBuffer->mW - 1), glm::max(glm::max(screen_v[0].x, screen_v[1].x), screen_v[2].x)) + 1.5;
        int yEnd = glm::min(float(mBuffer->mH - 1), glm::max(glm::max(screen_v[0].y, screen_v[1].y), screen_v[2].y)) + 1.5;

        for (int y = yStart; y < yEnd; y++) {
            for (int x = xStart; x < xEnd; x++) {
                glm::vec2 point(x, y);
                rasterizationPoint(clip_v, ndc_v, screen_v, area, point, fsdata, true);
            }
        }
    }

    void trTrianglesInstanced(TRMeshData &mesh, Program *prog, size_t index, size_t num)
    {
        prog->drawTrianglesInstanced(gRenderTarget, mesh, index, num);
    }

    void trTrianglesMT(TRMeshData &mesh, Program *prog)
    {
        Program *progList[THREAD_MAX] = { nullptr };
        size_t trianglesNum = mesh.vertices.size() / 3;
        if (gThreadNum > 1)
        {
            std::vector<std::thread> thread_pool;
            size_t index_step = trianglesNum / gThreadNum;
            if (!index_step)
                index_step = 1;

            for (size_t i = 0; i < gThreadNum; i++)
            {
                size_t start = i * index_step;
                if (start > trianglesNum - 1)
                    break;
                if (i == gThreadNum - 1)
                    index_step = trianglesNum - start;

                progList[i] = prog->clone();
                thread_pool.push_back(std::thread(trTrianglesInstanced, std::ref(mesh), progList[i], start, index_step));
            }

            for (auto &th : thread_pool)
                if (th.joinable())
                    th.join();
            for (size_t i = 0; i < gThreadNum; i++)
                delete progList[i];

        } else {
            trTrianglesInstanced(mesh, prog, 0, trianglesNum);
        }
    }

    void trSetUniformData(void *data)
    {
        gUniform = data;
    }

    void* trGetUniformData()
    {
        return gUniform;
    }

    void trTriangles(TRMeshData &mesh, Program *prog)
    {
        __compute_premultiply_mat__();

        trTrianglesMT(mesh, prog);
    }

    void trSetRenderThreadNum(size_t num)
    {
        gThreadNum = num;
        if (gThreadNum > THREAD_MAX)
            gThreadNum = THREAD_MAX;
    }

    void trViewport(int x, int y, int w, int h)
    {
        gRenderTarget->setViewport(x, y, w, h);
    }

    void trMakeCurrent(TRBuffer *buffer)
    {
        gRenderTarget = buffer;
    }

    void trBindTexture(TRTexture *texture, int type)
    {
        if (type < TEXTURE_INDEX_MAX)
            gTexture[type] = texture;
    }

    TRTexture *trGetTexture(int type)
    {
        if (type < TEXTURE_INDEX_MAX)
            return gTexture[type];
        else
            return nullptr;
    }

    void trEnableStencilTest(bool enable)
    {
        gEnableStencilTest = enable;
    }

    void trEnableStencilWrite(bool enable)
    {
        gEnableStencilWrite = enable;
    }

    void trEnableDepthTest(bool enable)
    {
        gEnableDepthTest = enable;
    }

    void trClear(int mode)
    {
        if (mode & TR_CLEAR_COLOR_BIT)
            gRenderTarget->clearColor();
        if (mode & TR_CLEAR_DEPTH_BIT)
            gRenderTarget->clearDepth();
        if (mode & TR_CLEAR_STENCIL_BIT)
            gRenderTarget->clearStencil();
    }

    void trClearColor3f(float r, float g, float b)
    {
        gRenderTarget->setBgColor(r, g, b);
    }

    void trSetMat3(glm::mat3 mat, MAT_INDEX_TYPE type)
    {
        if (type < MAT_INDEX_MAX)
            gMat3[type] = mat;
    }

    void trSetMat4(glm::mat4 mat, MAT_INDEX_TYPE type)
    {
        if (type < MAT_INDEX_MAX)
            gMat4[type] = mat;
    }

    glm::mat3 &trGetMat3(MAT_INDEX_TYPE type)
    {
        if (type < MAT_INDEX_MAX)
            return gMat3[type];
        else
            return gMat3[0];
    }

    glm::mat4 &trGetMat4(MAT_INDEX_TYPE type)
    {
        if (type < MAT_INDEX_MAX)
            return gMat4[type];
        else
            return gMat4[0];
    }

    void trResetMat3(MAT_INDEX_TYPE type)
    {
        if (type < MAT_INDEX_MAX)
            gMat3[type] = gDefaultMat3[type];
    }

    void trResetMat4(MAT_INDEX_TYPE type)
    {
        if (type < MAT_INDEX_MAX)
            gMat4[type] = gDefaultMat4[type];
    }

    void trDrawMode(TRDrawMode mode)
    {
        gDrawMode = mode;
    }

    void trCullFaceMode(TRCullFaceMode mode)
    {
        gCullFace = mode;
    }

    TRBuffer * trCreateRenderTarget(int w, int h)
    {
        return new TRBuffer(w, h, true);
    }

    void trSetCurrentRenderTarget(TRBuffer *buffer)
    {
        gRenderTarget = buffer;
    }
}
