#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <glm/ext.hpp>

#include "tr.hpp"

// global value
TRBuffer *gRenderTarget = nullptr;
TRTexture *gTexture[TEXTURE_INDEX_MAX] = { nullptr };

glm::mat4 gMat4[MAT_INDEX_MAX] =
{
    glm::mat4(1.0f), // model mat
    glm::mat4(1.0f), // view mat
    // defult proj mat should swap z-order
    glm::mat4({1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}), // proj mat
};

glm::mat3 gMat3[MAT_INDEX_MAX] =
{
    glm::mat3(1.0f), // normal mat
};

LightInfo gLightInfo;

size_t gThreadNum = 4;

int gDrawMode = TR_FILL;

int gCullFace = TR_CCW;

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

void TRProgramBase::prepareFragmentData(VSDataBase *vsdata[3], FSDataBase *fsdata)
{
    fsdata->mClipV[0] = vsdata[0]->mClipV;
    fsdata->mClipV[1] = vsdata[1]->mClipV - vsdata[0]->mClipV;
    fsdata->mClipV[2] = vsdata[2]->mClipV - vsdata[0]->mClipV;

    fsdata->mTexCoord[0] = vsdata[0]->mTexCoord;
    fsdata->mTexCoord[1] = vsdata[1]->mTexCoord - vsdata[0]->mTexCoord;
    fsdata->mTexCoord[2] = vsdata[2]->mTexCoord - vsdata[0]->mTexCoord;

    fsdata->mNormal[0] = vsdata[0]->mNormal;
    fsdata->mNormal[1] = vsdata[1]->mNormal - vsdata[0]->mNormal;
    fsdata->mNormal[2] = vsdata[2]->mNormal - vsdata[0]->mNormal;

    fsdata->mColor[0] = vsdata[0]->mColor;
    fsdata->mColor[1] = vsdata[1]->mColor - vsdata[0]->mColor;
    fsdata->mColor[2] = vsdata[2]->mColor - vsdata[0]->mColor;
}

void TRProgramBase::interpVertex(float t, VSDataBase *in1, VSDataBase *in2, VSDataBase *outV)
{
    outV->mTexCoord = in2->mTexCoord + t * (in1->mTexCoord - in2->mTexCoord);
    outV->mNormal = in2->mNormal + t * (in1->mNormal - in2->mNormal);
    outV->mColor = in2->mColor + t * (in1->mColor - in2->mColor);
    outV->mClipV = in2->mClipV + t * (in1->mClipV - in2->mClipV);

}
#define W_NEAR (0.1f)

void TRProgramBase::clipLineNear(VSDataBase *in1, VSDataBase *in2, VSDataBase *out[3], size_t &index)
{
    // Sutherland-Hodgeman clip
    // t = w1 / (w1 - w2)
    // V = V1 + t * (V2 - V1)
    if (in1->mClipV.w > 0 && in2->mClipV.w > 0)
    {
        out[index++] = in2;
    }
    else if (in1->mClipV.w > 0 && in2->mClipV.w < 0)
    {
        float t = (in2->mClipV.w - W_NEAR)/(in2->mClipV.w - in1->mClipV.w);
        VSDataBase *outV = allocVSData();
        // get interp V and put V into output
        interpVertex(t, in1, in2, outV);
        out[index++] = outV;
    }
    else if (in1->mClipV.w < 0 && in2->mClipV.w > 0)
    {
        float t = (in1->mClipV.w - W_NEAR) /(in1->mClipV.w - in2->mClipV.w);
        VSDataBase *outV = allocVSData();
        // get interp V and put V and V2 into output
        interpVertex(t, in2, in1, outV);
        out[index++] = outV;
        out[index++] = in2;
    }
}

void TRProgramBase::clipNear(VSDataBase *in[3], VSDataBase *out[4], size_t &index)
{
    index = 0;
    clipLineNear(in[0], in[1], out, index);
    clipLineNear(in[1], in[2], out, index);
    clipLineNear(in[2], in[0], out, index);
}

VSDataBase *TRProgramBase::allocVSData()
{
    return &__VSData__[mPreAllocVSIndex++];
}

FSDataBase *TRProgramBase::allocFSData()
{
    return &__FSData__;
}

void TRProgramBase::freeShaderData()
{
    mPreAllocVSIndex = 0;
}

void TRProgramBase::drawTriangle(TRMeshData &mesh, size_t index)
{
    VSDataBase *vsdata[3];
    FSDataBase *fsdata = allocFSData();

    for (size_t i = 0; i < 3; i++)
    {
        vsdata[i] = allocVSData();
        loadVertexData(mesh, vsdata[i], index * 3 + i);
        vertex(vsdata[i]);
    }

    // Max is 4 output.
    VSDataBase *out[4] = { nullptr };
    size_t total = 0;
    clipNear(vsdata, out, total);

    for (size_t i = 0; total > 2 && i < total - 2; i++)
    {
        vsdata[0] = out[0];
        vsdata[1] = out[i + 1];
        vsdata[2] = out[i + 2];
        glm::vec4 clipV[3] = { vsdata[0]->mClipV, vsdata[1]->mClipV, vsdata[2]->mClipV };
        prepareFragmentData(vsdata, fsdata);
        rasterization(clipV, fsdata);
    }
    freeShaderData();
}

void TRProgramBase::drawTrianglesInstanced(TRBuffer *buffer, TRMeshData &mesh, size_t index, size_t num)
{
    size_t i = 0, j = 0;
    size_t trianglesNum = mesh.vertices.size() / 3;

    mBuffer = buffer;

    for (i = index, j = 0; i < trianglesNum && j < num; i++, j++)
        drawTriangle(mesh, i);
}

void TRProgramBase::rasterizationPoint(
        glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, glm::vec2 &point, FSDataBase *fsdata, bool insideCheck)
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

    float areaPC =  1 / (w0 + w1 + w2);
    mUPC = w1 * areaPC;
    mVPC = w2 * areaPC;

    size_t offset = mBuffer->getOffset(x, y);
    /* easy-z */
    /* Do not use mutex here to speed up */
    if (!mBuffer->depthTest(offset, depth))
        return;

    float color[3];
    if (!fragment(fsdata, color))
        return;

    /* depth test */
#ifdef DEPTH_LOCK
    std::lock_guard<std::mutex> lck(mBuffer->mDepthMutex);
#endif
    if (!mBuffer->depthTest(offset, depth))
        return;

    mBuffer->setColor(offset, color);
}

void TRProgramBase::rasterizationLine(
        glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, int p1, int p2, FSDataBase *fsdata)
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

void TRProgramBase::rasterization(glm::vec4 clip_v[3], FSDataBase *fsdata)
{
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

    /* Special case */
    if (area == 0)
        area = 1e-6;

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

void trTrianglesInstanced(TRMeshData &mesh, TRProgramBase *prog, size_t index, size_t num)
{
    prog->drawTrianglesInstanced(gRenderTarget, mesh, index, num);
}

#define THREAD_MAX (10)
void trTrianglesMT(TRMeshData &mesh, TRProgramBase *prog)
{
    TRProgramBase *progList[THREAD_MAX] = { nullptr };
    if (gThreadNum > 1)
    {
        std::vector<std::thread> thread_pool;
        size_t index_step = mesh.vertices.size() / 3 / gThreadNum + 1;

        for (size_t i = 0; i < gThreadNum; i++)
        {
            progList[i] = prog->clone();
            thread_pool.push_back(std::thread(trTrianglesInstanced, std::ref(mesh), progList[i], i * index_step, index_step));
        }

        for (auto &th : thread_pool)
            if (th.joinable())
                th.join();
        for (size_t i = 0; i < gThreadNum; i++)
            delete progList[i];

    } else {
        trTrianglesInstanced(mesh, prog, 0, mesh.vertices.size() / 3);
    }
}

void trTriangles(TRMeshData &mesh, TRProgramBase *prog)
{
    __compute_premultiply_mat__();

    trTrianglesMT(mesh, prog);
}

void trSetRenderThreadNum(size_t num)
{
    gThreadNum = num;
}

LightInfo &trGetLightInfo()
{
    return gLightInfo;
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

TRTexture * trGetTexture(int type)
{
    if (type < TEXTURE_INDEX_MAX)
        return gTexture[type];
    else
        return nullptr;
}

void trClear()
{
    gRenderTarget->clearColor();
    gRenderTarget->clearDepth();
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
