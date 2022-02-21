#include <string>
#include <vector>
#include <thread>
#include <mutex>

#include "trcore.hpp"

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

    thread_local Program gProgram;

    size_t gThreadNum = 4;
    TRPolygonMode gPolygonMode = TR_FILL;
    TRDrawMode gDrawMode = TR_TRIANGLES;
    TRCullFaceMode gCullFace = TR_NONE;
    bool gEnableDepthTest = true;
    bool gEnableStencilTest = false;
    bool gEnableStencilWrite = false;
#if __DEBUG_FINISH_CB__
    fcb gFCB = nullptr;
    void *gFCBData = nullptr;
#endif

    // internal function
    static inline void __compute_premultiply_mat__()
    {
        gMat4[MAT4_MODELVIEW] =  gMat4[MAT4_VIEW] * gMat4[MAT4_MODEL];
        gMat4[MAT4_MVP] = gMat4[MAT4_PROJ] * gMat4[MAT4_MODELVIEW];
        gMat3[MAT3_NORMAL] = glm::transpose(glm::inverse(gMat4[MAT4_MODELVIEW]));
    }

    static inline float __edge__(glm::vec2 &a, glm::vec2 &b, glm::vec2 &c)
    {
        return (c.x - a.x)*(b.y - a.y) - (c.y - a.y)*(b.x - a.x);
    }

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
            glm::vec2 deltaUV1 = texcoords[i+1]-texcoords[i];
            glm::vec2 deltaUV2 = texcoords[i+2]-texcoords[i];

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

    void TRMeshData::fillPureColor(glm::vec3 color)
    {
        if (colors.size() != 0)
            return;

        for (size_t i = 0; i < vertices.size(); i++)
            colors.push_back(color);
    }

    void Program::setBuffer(TRBuffer *buffer)
    {
        mBuffer = buffer;
    }

    void Program::setShader(Shader *shader)
    {
        mShader = shader;
    }

    VSOutData *Program::allocVSOutData()
    {
        return &mVSOutData[mAllocIndex++];
    }

    void Program::freeShaderData()
    {
        mAllocIndex = 0;
    }

    void Program::preDraw()
    {
        assert(mBuffer != nullptr);
        assert(mShader != nullptr);
#if __DEBUG_FINISH_CB__
        mDrawSth = false;
#endif
        freeShaderData();
    }

    void Program::postDraw()
    {
        freeShaderData();
#if __DEBUG_FINISH_CB__
        if (gFCB != nullptr && mDrawSth)
            gFCB(gFCBData);
#endif
    }

    void Program::prepareFragmentData(VSOutData *vsdata[], int num)
    {
        assert(num > 0 && num < 4);
        size_t v2n, v3n, v4n;
        mShader->getVaryingNum(v2n, v3n, v4n);

        mFSInData.tr_PositionPrim[0] = vsdata[0]->tr_Position;
        if (num > 1)
            mFSInData.tr_PositionPrim[1] = vsdata[1]->tr_Position - vsdata[0]->tr_Position;
        if (num > 2)
            mFSInData.tr_PositionPrim[2] = vsdata[2]->tr_Position - vsdata[0]->tr_Position;

        for (size_t i = 0; i < v2n; i++)
        {
            mFSInData.mVaryingVec2Prim[i][0] = vsdata[0]->mVaryingVec2[i];
            if (num > 1)
                mFSInData.mVaryingVec2Prim[i][1] = vsdata[1]->mVaryingVec2[i] - vsdata[0]->mVaryingVec2[i];
            if (num > 2)
                mFSInData.mVaryingVec2Prim[i][2] = vsdata[2]->mVaryingVec2[i] - vsdata[0]->mVaryingVec2[i];
        }
        for (size_t i = 0; i < v3n; i++)
        {
            mFSInData.mVaryingVec3Prim[i][0] = vsdata[0]->mVaryingVec3[i];
            if (num > 1)
                mFSInData.mVaryingVec3Prim[i][1] = vsdata[1]->mVaryingVec3[i] - vsdata[0]->mVaryingVec3[i];
            if (num > 2)
                mFSInData.mVaryingVec3Prim[i][2] = vsdata[2]->mVaryingVec3[i] - vsdata[0]->mVaryingVec3[i];
        }
        for (size_t i = 0; i < v4n; i++)
        {
            mFSInData.mVaryingVec4Prim[i][0] = vsdata[0]->mVaryingVec4[i];
            if (num > 1)
                mFSInData.mVaryingVec4Prim[i][1] = vsdata[1]->mVaryingVec4[i] - vsdata[0]->mVaryingVec4[i];
            if (num > 2)
                mFSInData.mVaryingVec4Prim[i][2] = vsdata[2]->mVaryingVec4[i] - vsdata[0]->mVaryingVec4[i];
        }
    }

    constexpr float W_CLIPPING_PLANE = 0.1f;

    void Program::getIntersectionVertex(VSOutData *in1, VSOutData *in2, VSOutData *outV)
    {
        size_t v2n, v3n, v4n;
        mShader->getVaryingNum(v2n, v3n, v4n);

        float t = (in2->tr_Position.w - W_CLIPPING_PLANE)/(in2->tr_Position.w - in1->tr_Position.w);

        for (size_t i = 0; i < v2n; i++)
            outV->mVaryingVec2[i] = in2->mVaryingVec2[i] + t * (in1->mVaryingVec2[i] - in2->mVaryingVec2[i]);

        for (size_t i = 0; i < v3n; i++)
            outV->mVaryingVec3[i] = in2->mVaryingVec3[i] + t * (in1->mVaryingVec3[i] - in2->mVaryingVec3[i]);

        for (size_t i = 0; i < v4n; i++)
            outV->mVaryingVec4[i] = in2->mVaryingVec4[i] + t * (in1->mVaryingVec4[i] - in2->mVaryingVec4[i]);

        outV->tr_Position = in2->tr_Position + t * (in1->tr_Position - in2->tr_Position);
    }

    void Program::clipLineOnWAxis(VSOutData *in1, VSOutData *in2, VSOutData *out[], size_t &index)
    {
        // Sutherland-Hodgeman clip
        // t = w1 / (w1 - w2)
        // V = V1 + t * (V2 - V1)
        if (in1->tr_Position.w > W_CLIPPING_PLANE && in2->tr_Position.w > W_CLIPPING_PLANE)
        {
            out[index++] = in2;
        }
        else if (in1->tr_Position.w > W_CLIPPING_PLANE && in2->tr_Position.w <= W_CLIPPING_PLANE)
        {
            VSOutData *outV = allocVSOutData();
            // get interp V and put V into output
            getIntersectionVertex(in1, in2, outV);
            out[index++] = outV;
        }
        else if (in1->tr_Position.w <= W_CLIPPING_PLANE && in2->tr_Position.w > W_CLIPPING_PLANE)
        {
            VSOutData *outV = allocVSOutData();
            // get interp V and put V and V2 into output
            getIntersectionVertex(in2, in1, outV);
            out[index++] = outV;
            out[index++] = in2;
        }
    }

    void Program::clipOnWAxis(VSOutData *in[3], VSOutData *out[], size_t &index)
    {
        index = 0;
        clipLineOnWAxis(in[0], in[1], out, index);
        clipLineOnWAxis(in[1], in[2], out, index);
        clipLineOnWAxis(in[2], in[0], out, index);
    }

    void Program::drawPoint(TRMeshData &mesh, size_t index)
    {
        preDraw();
        VSOutData *vsdata = allocVSOutData();
        mShader->vertex(mesh, vsdata, index);
        if (vsdata->tr_Position.w >= W_CLIPPING_PLANE)
            rasterizationPoint(vsdata);
        postDraw();
    }

    void Program::drawLine(TRMeshData &mesh, size_t index)
    {
        preDraw();
        VSOutData *vsdata[2];
        for (size_t i = 0; i < 2; i++)
        {
            vsdata[i] = allocVSOutData();
            mShader->vertex(mesh, vsdata[i], index * 2 + i);
        }

        VSOutData *out[2] = { nullptr };
        size_t total = 0;
        if (vsdata[0]->tr_Position.w >= W_CLIPPING_PLANE
                && vsdata[1]->tr_Position.w >= W_CLIPPING_PLANE)
        {
            // No need to clip on W
            rasterizationLine(vsdata);
        }
        else if (vsdata[0]->tr_Position.w <= W_CLIPPING_PLANE
                && vsdata[1]->tr_Position.w > W_CLIPPING_PLANE)
        {
            clipLineOnWAxis(vsdata[0], vsdata[1], out, total);
            assert(total == 2);
            rasterizationLine(out);
        }
        else if (vsdata[0]->tr_Position.w > W_CLIPPING_PLANE
                && vsdata[1]->tr_Position.w <= W_CLIPPING_PLANE)
        {
            clipLineOnWAxis(vsdata[1], vsdata[0], out, total);
            assert(total == 2);
            rasterizationLine(out);
        }
        postDraw();
    }

    void Program::drawTriangle(TRMeshData &mesh, size_t index)
    {
        preDraw();
        VSOutData *vsdata[3];
        for (size_t i = 0; i < 3; i++)
        {
            vsdata[i] = allocVSOutData();
            mShader->vertex(mesh, vsdata[i], index * 3 + i);
        }

        if (vsdata[0]->tr_Position.w >= W_CLIPPING_PLANE
                && vsdata[1]->tr_Position.w >= W_CLIPPING_PLANE
                && vsdata[2]->tr_Position.w >= W_CLIPPING_PLANE)
        {
            // No need to clip on W
            if (gPolygonMode == TR_LINE)
                rasterizationWireframe(vsdata);
            else
                rasterizationTriangle(vsdata);
        }
        else
        {
            // Max is 4 output.
            VSOutData *out[4] = { nullptr };
            size_t total = 0;
            clipOnWAxis(vsdata, out, total);
            for (size_t i = 0; total > 2 && i < total - 2; i++)
            {
                vsdata[0] = out[0];
                vsdata[1] = out[i + 1];
                vsdata[2] = out[i + 2];
                if (gPolygonMode == TR_LINE)
                    rasterizationWireframe(vsdata);
                else
                    rasterizationTriangle(vsdata);
            }
        }
        postDraw();
    }

    void Program::drawPrimsInstranced(TRMeshData &mesh, size_t index, size_t num)
    {
        size_t i = 0, j = 0;
        size_t primsCount = mesh.vertices.size() / gDrawMode;

        for (i = index, j = 0; i < primsCount && j < num; i++, j++)
            switch (gDrawMode)
            {
                case TR_POINTS: drawPoint(mesh, i); break;
                case TR_LINES: drawLine(mesh, i); break;
                case TR_TRIANGLES: drawTriangle(mesh, i); break;
                default: assert(false); break;
            }
    }

    void Program::drawPixel(int x, int y, float depth)
    {
        size_t offset = mBuffer->getOffset(x, y);
        /* easy-z */
        /* Do not use mutex here to speed up */
        if (gEnableDepthTest && mBuffer->getDepth(offset) < depth)
            return;

        float color[4];
        color[3] = 1.0f;
        if (!mShader->fragment(&mFSInData, color))
            return;

#if __NEED_BUFFER_LOCK__
        std::lock_guard<std::mutex> lck(mBuffer->getMutex(offset));
#endif
        if (gEnableStencilTest && mBuffer->getStencil(offset) != 0)
            return;

        /* depth test */
        if (gEnableDepthTest && mBuffer->getDepth(offset) < depth)
            return;

        mBuffer->updateDepth(offset, depth);
        /* Write stencil buffer need to pass depth test */
        if (gEnableStencilWrite)
            mBuffer->updateStencil(offset, 1);

        mBuffer->drawPixel(x, y, color);
#if __DEBUG_FINISH_CB__
        mDrawSth = true;
#endif
    }

    void Program::rasterizationPoint(VSOutData *vsdata)
    {
        glm::vec4 clip = vsdata->tr_Position;
        glm::vec4 ndc = clip / clip.w;
        glm::vec2 screen = mBuffer->viewportTransform(ndc);
        glm::uvec4 drawArea = mBuffer->getDrawArea();
        if (screen.x < drawArea[2] && screen.x >= drawArea[0]
                && screen.y < drawArea[3] && screen.y >= drawArea[1])
        {
            float depth = ndc.z / 2.0f + 0.5f;
            if (depth < 0.0f)
                return;
            prepareFragmentData(&vsdata, 1);
            mFSInData.mUPC = 0;
            mFSInData.mVPC = 0;
            drawPixel(screen.x, screen.y, depth);
        }
    }

    void Program::rasterizationLine(VSOutData *vsdata[2])
    {
        glm::vec4 clip[2];
        glm::vec4 ndc[2];
        glm::vec2 screen[2];

        for (int i = 0; i < 2; i++)
        {
            clip[i] = vsdata[i]->tr_Position;
            ndc[i] = clip[i] / clip[i].w;
            screen[i] = mBuffer->viewportTransform(ndc[i]);
        }

        prepareFragmentData(vsdata, 2);

        float x0 = screen[0].x, x1 = screen[1].x;
        float y0 = screen[0].y, y1 = screen[1].y;
        glm::vec2 p0(x0, y0);
        glm::vec2 p1(x1, y1);
        float L = glm::length(p0 - p1);
        glm::uvec4 drawArea = mBuffer->getDrawArea();

        // Bresenham's line algorithm
        bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
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
        int deltay = int(std::abs(y1 - y0));
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
            if (v.x < drawArea[2] && v.y < drawArea[3] && v.x >= drawArea[0] && v.y >= drawArea[1])
            {
                float l0 = glm::length(v - p1) / L;
                float l1 = glm::length(v - p0) / L;

                float depth = l0 * ndc[0].z + l1 * ndc[1].z;
                depth = depth / 2.0f + 0.5f;
                if (depth < 0.0f)
                    return;

                l0 /= clip[0].w;
                l1 /= clip[1].w;
                float LPC = l0 + l1;
                mFSInData.mUPC = l1 / LPC;
                mFSInData.mVPC = 0;
                drawPixel(v.x, v.y, depth);
            }

            error += deltaerr;
            if (error >= 0.5)
            {
                y += ystep;
                error -= 1.0;
            }
        }
    }

    void Program::rasterizationWireframe(VSOutData *vsdata[3])
    {
        VSOutData *vs[2];
        vs[0] = vsdata[0];
        vs[1] = vsdata[1];
        rasterizationLine(vs);
        vs[0] = vsdata[1];
        vs[1] = vsdata[2];
        rasterizationLine(vs);
        vs[0] = vsdata[2];
        vs[1] = vsdata[0];
        rasterizationLine(vs);
    }

    void Program::rasterizationTriangle(VSOutData *vsdata[3])
    {
        glm::vec4 clip[3];
        glm::vec4 ndc[3];
        glm::vec2 screen[3];

        for (int i = 0; i < 3; i++)
        {
            clip[i] = vsdata[i]->tr_Position;
            ndc[i] = clip[i] / clip[i].w;
            screen[i] = mBuffer->viewportTransform(ndc[i]);
        }

        float area = __edge__(screen[0], screen[1], screen[2]);
        if (gCullFace == TR_CCW && area >= 0)
            return;
        else if (gCullFace == TR_CW && area <= 0)
            return;
        else if (area == 0)
            /* Special case */
            return;

        prepareFragmentData(vsdata, 3);

        glm::uvec4 drawArea = mBuffer->getDrawArea();
        int xStart = glm::max(float(drawArea[0]), glm::min(glm::min(screen[0].x, screen[1].x), screen[2].x)) + 0.5;
        int yStart = glm::max(float(drawArea[1]), glm::min(glm::min(screen[0].y, screen[1].y), screen[2].y)) + 0.5;
        int xEnd = glm::min(float(drawArea[2] - 1), glm::max(glm::max(screen[0].x, screen[1].x), screen[2].x)) + 1.5;
        int yEnd = glm::min(float(drawArea[3] - 1), glm::max(glm::max(screen[0].y, screen[1].y), screen[2].y)) + 1.5;

        for (int y = yStart; y < yEnd; y++)
        {
            for (int x = xStart; x < xEnd; x++)
            {
                glm::vec2 point(x, y);
                float w0 = __edge__(screen[1], screen[2], point);
                float w1 = __edge__(screen[2], screen[0], point);
                float w2 = __edge__(screen[0], screen[1], point);
                switch (gCullFace)
                {
                    case TR_CW: if (w0 < 0 || w1 < 0 || w2 < 0) continue; break;
                    case TR_CCW: if (w0 > 0 || w1 > 0 || w2 > 0) continue; break;
                    default: if (!((w0 > 0 && w1 > 0 && w2 > 0) || (w0 < 0 && w1 < 0 && w2 < 0))) continue; break;
                }

                /* Barycentric coordinate */
                w0 /= area;
                w1 /= area;
                w2 /= area;

                /* Using the ndc.z to calculate depth, faster then using the interpolated clip.z / clip.w. */
                float depth = w0 * ndc[0].z + w1 * ndc[1].z + w2 * ndc[2].z;
                depth = depth / 2.0f + 0.5f;
                /* z in ndc of opengl should between 0.0f to 1.0f */
                if (depth < 0.0f)
                    return;

                /* Perspective-Correct */
                w0 /= clip[0].w;
                w1 /= clip[1].w;
                w2 /= clip[2].w;
                float areaPC = (w0 + w1 + w2);
                mFSInData.mUPC = w1 / areaPC;
                mFSInData.mVPC = w2 / areaPC;
                drawPixel(x, y, depth);
            }
        }
    }

    void trPrimsInstanced(TRMeshData &mesh, Shader *shader, size_t index, size_t num)
    {
        gProgram.setBuffer(gRenderTarget);
        gProgram.setShader(shader);
        gProgram.drawPrimsInstranced(mesh, index, num);
    }

    void trPrimsMT(TRMeshData &mesh, Shader *shader)
    {
        size_t primsCount = mesh.vertices.size() / gDrawMode;
        if (gThreadNum > 1)
        {
            std::vector<std::thread> thread_pool;
            size_t index_step = primsCount / gThreadNum;
            if (!index_step)
                index_step = 1;

            for (size_t i = 0; i < gThreadNum; i++)
            {
                size_t start = i * index_step;
                if (start > primsCount - 1)
                    break;
                if (i == gThreadNum - 1)
                    index_step = primsCount - start;

                thread_pool.push_back(std::thread(trPrimsInstanced, std::ref(mesh), shader, start, index_step));
            }

            for (auto &th : thread_pool)
                if (th.joinable())
                    th.join();

        }
        else
        {
            trPrimsInstanced(mesh, shader, 0, primsCount);
        }
    }

    // Matrix related API
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

    // Draw related API
    void trDrawArrays(TRDrawMode mode, TRMeshData &mesh, Shader *shader)
    {
        __compute_premultiply_mat__();

        gDrawMode = mode;
        trPrimsMT(mesh, shader);
    }

    // Core state related API
    void trSetRenderThreadNum(size_t num)
    {
        gThreadNum = num;
        if (gThreadNum > THREAD_MAX)
            gThreadNum = THREAD_MAX;
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

    void trPolygonMode(TRPolygonMode mode)
    {
        gPolygonMode = mode;
    }

    void trCullFaceMode(TRCullFaceMode mode)
    {
        gCullFace = mode;
    }

    TRCullFaceMode trGetCullFaceMode()
    {
        return gCullFace;
    }

    // Buffer related API
    TRBuffer * trCreateRenderTarget(int w, int h)
    {
        gRenderTarget = new TRBuffer(w, h, true);
        return gRenderTarget;
    }

    void trSetRenderTarget(TRBuffer *buffer)
    {
        gRenderTarget = buffer;
    }

    TRBuffer * trGetRenderTarget()
    {
        return gRenderTarget;
    }

    void trViewport(int x, int y, int w, int h)
    {
        gRenderTarget->setViewport(x, y, w, h);
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

    // Texture related API
    void trBindTexture(TRTexture *texture, int type)
    {
        if (type < TEXTURE_INDEX_MAX)
            gTexture[type] = texture;
    }

    void trUnbindTextureAll()
    {
        for (int i = 0; i < TEXTURE_INDEX_MAX; i++)
            gTexture[i] = nullptr;
    }

    TRTexture *trGetTexture(int type)
    {
        if (type < TEXTURE_INDEX_MAX)
            return gTexture[type];
        else
            return nullptr;
    }

    // Uniform data related API
    void trSetUniformData(void *data)
    {
        gUniform = data;
    }

    void* trGetUniformData()
    {
        return gUniform;
    }

#if __DEBUG_FINISH_CB__
    // Debug related API
    void trSetFinishCB(fcb func, void *data)
    {
        gFCB = func;
        gFCBData = data;
    }
#endif
}
