#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
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

bool gEnableLighting = false;
float gAmbientStrength = 0.1;
float gSpecularStrength = 0.2;
int gShininess = 32;
glm::vec3 gLightColor = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 gLightPosition = glm::vec3(1.0f, 1.0f, 1.0f);

size_t gThreadNum = 4;

int gDrawMode = TR_FILL;

// internal function
void __texture_coord_repeat__(glm::vec2 &coord)
{
    coord.x = glm::fract(coord.x);
    coord.y = glm::fract(coord.y);
}

void __texture_coord_clamp_edge__(glm::vec2 &coord)
{
    coord.x = glm::clamp(coord.x, 0.0f, 1.0f);
    coord.y = glm::clamp(coord.y, 0.0f, 1.0f);
}

void textureCoordWrap(glm::vec2 &coord)
{
    if (coord.x < 0 || coord.x > 1 || coord.y < 0 || coord.y > 1)
       __texture_coord_repeat__(coord);
}

void TRMeshData::computeTangent()
{
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

void TRBuffer::setViewport(int x, int y, int w, int h)
{
    mVX = x;
    mVY = y;
    mVW = w;
    mVH = h;
}

void TRBuffer::viewport(glm::vec2 &screen_v, glm::vec4 &ndc_v)
{
    screen_v.x = mVX + (mW - 1) * (ndc_v.x / 2 + 0.5);
    // inverse y here
    screen_v.y = mVY + (mH - 1) - (mH - 1) * (ndc_v.y / 2 + 0.5);
}

void TRBuffer::setBGColor(float r, float g, float b)
{
    mBGColor[0] = glm::clamp(int(r * 255 + 0.5), 0, 255);
    mBGColor[1] = glm::clamp(int(g * 255 + 0.5), 0, 255);
    mBGColor[2] = glm::clamp(int(b * 255 + 0.5), 0, 255);
}

void TRBuffer::clearColor()
{
    for (uint32_t i = 0; i < mH; i++)
    {
        uint8_t *base = &mData[i * mStride];
        for (uint32_t j = 0; j < mW; j++)
        {
            base[j * CHANNEL + 0] = mBGColor[0];
            base[j * CHANNEL + 1] = mBGColor[1];
            base[j * CHANNEL + 2] = mBGColor[2];
        }
    }
}

void TRBuffer::clearDepth()
{
    for (size_t i = 0; i < mW * mH; i++)
        mDepth[i] = 1.0f;
}

bool TRBuffer::depthTest(int x, int y, float depth)
{
    size_t depth_offset = y * mW + x;
    /* projection matrix will inverse z-order. */
    if (mDepth[depth_offset] < depth)
        return false;
    else
        mDepth[depth_offset] = depth;
    return true;
}

void TRBuffer::setExtBuffer(void *addr)
{
    if (mValid && mExtBuffer)
        mData = reinterpret_cast<uint8_t *>(addr);
}

TRBuffer* TRBuffer::create(int w, int h, bool ext)
{
    TRBuffer *buffer = new TRBuffer(w, h, ext);
    if (buffer && buffer->mValid)
        return buffer;

    if (buffer)
        delete buffer;
    return nullptr;
}

TRBuffer::TRBuffer(int w, int h, bool ext)
{
    std::cout << "Create TRBuffer: " << w << "x" << h << " ext = " << ext << std::endl;
    mExtBuffer = ext;
    if (!ext)
    {
        mData = new uint8_t[w * h * CHANNEL];
        if (!mData)
            return;
    }
    mDepth = new float[w * h];
    if (!mDepth)
    {
        if (mData)
            delete mData;
        return;
    }
    mW = mVW = w;
    mH = mVH = h;
    mStride = w * CHANNEL;
    if (!ext)
        clearColor();
    clearDepth();

    mValid = true;
}

TRBuffer::~TRBuffer()
{
    std::cout << "Destory TRBuffer.\n";
    if (!mValid)
        return;
    if (!mExtBuffer && mData)
        delete mData;
    if (mDepth)
        delete mDepth;
}

static inline void __compute_premultiply_mat__()
{
    gMat4[MAT4_MODELVIEW] =  gMat4[MAT4_VIEW] * gMat4[MAT4_MODEL];
    gMat4[MAT4_MVP] = gMat4[MAT4_PROJ] * gMat4[MAT4_MODELVIEW];
    gMat3[MAT3_NORMAL] = glm::transpose(glm::inverse(gMat4[MAT4_MODELVIEW]));
}

void ColorProgram::loadVertexData(TRMeshData &mesh, VSDataBase vsdata[3], size_t index)
{
    for (int i = 0; i < 3; i++)
    {
        vsdata[i].mVertex = mesh.vertices[i + index];
        vsdata[i].mColor = mesh.colors[i + index];
    }
}

void ColorProgram::vertex(VSDataBase &vsdata)
{
    vsdata.mClipV = gMat4[MAT4_MVP] * glm::vec4(vsdata.mVertex, 1.0f);
}

bool ColorProgram::fragment(FSDataBase &fsdata, float color[3])
{
    glm::vec3 C = interpFast(fsdata.mColor);
    color[0] = C[0];
    color[1] = C[1];
    color[2] = C[2];
    return true;
}


void TextureMapProgram::loadVertexData(TRMeshData &mesh, VSDataBase vsdata[3], size_t index)
{
    for (int i = 0; i < 3; i++)
    {
        vsdata[i].mVertex = mesh.vertices[i + index];
        vsdata[i].mTexCoord = mesh.uvs[i + index];
    }
}

void TextureMapProgram::vertex(VSDataBase &vsdata)
{
    vsdata.mClipV = gMat4[MAT4_MVP] * glm::vec4(vsdata.mVertex, 1.0f);
}

bool TextureMapProgram::fragment(FSDataBase &fsdata, float color[3])
{
    glm::vec2 texCoord = interpFast(fsdata.mTexCoord);
    textureCoordWrap(texCoord);

    float *c = gTexture[TEXTURE_DIFFUSE]->getColor(texCoord.x, texCoord.y);

    color[0] = c[0];
    color[1] = c[1];
    color[2] = c[2];
    return true;
}

void ColorPhongProgram::loadVertexData(TRMeshData &mesh, PhongVSData vsdata[3], size_t index)
{
    glm::vec3 viewLightPostion = gMat4[MAT4_VIEW] * glm::vec4(gLightPosition, 1.0f);

    for (int i = 0; i < 3; i++)
    {
        vsdata[i].mVertex = mesh.vertices[i + index];
        vsdata[i].mNormal = mesh.normals[i + index];
        vsdata[i].mColor = mesh.colors[i + index];
        vsdata[i].mLightPosition = viewLightPostion;
    }
}

void ColorPhongProgram::vertex(PhongVSData &vsdata)
{
    vsdata.mClipV = gMat4[MAT4_MVP] * glm::vec4(vsdata.mVertex, 1.0f);
    vsdata.mViewFragmentPosition = gMat4[MAT4_MODELVIEW] * glm::vec4(vsdata.mVertex, 1.0f);
    vsdata.mNormal = gMat3[MAT3_NORMAL] * vsdata.mNormal;
}

void ColorPhongProgram::preInterp(PhongVSData vsdata[3], PhongFSData &fsdata)
{
    TRProgramBase::preInterp(vsdata, fsdata);
    fsdata.mViewFragmentPosition[0] = vsdata[0].mViewFragmentPosition;
    fsdata.mViewFragmentPosition[1] = vsdata[1].mViewFragmentPosition - vsdata[0].mViewFragmentPosition;
    fsdata.mViewFragmentPosition[2] = vsdata[2].mViewFragmentPosition - vsdata[0].mViewFragmentPosition;

    fsdata.mLightPosition = vsdata[0].mLightPosition;
}

void ColorPhongProgram::interpVertex(float t, PhongVSData &in1, PhongVSData &in2, PhongVSData &outV)
{
    TRProgramBase::interpVertex(t, in1, in2, outV);
    outV.mViewFragmentPosition = in2.mViewFragmentPosition + t * (in1.mViewFragmentPosition - in2.mViewFragmentPosition);
    outV.mLightPosition = in2.mLightPosition + t * (in1.mLightPosition - in2.mLightPosition);
}

bool ColorPhongProgram::fragment(PhongFSData &fsdata, float color[3])
{
    glm::vec3 fragmentPosition = interpFast(fsdata.mViewFragmentPosition);
    glm::vec3 normal = interpFast(fsdata.mNormal);
    glm::vec3 baseColor = interpFast(fsdata.mColor);

    normal = glm::normalize(normal);
    // from fragment to light
    glm::vec3 lightDirection = glm::normalize(fsdata.mLightPosition - fragmentPosition);

    float diff = glm::max(dot(normal, lightDirection), 0.0f);

    // in camera space, eys always in (0.0, 0.0, 0.0), from fragment to eye
    glm::vec3 eyeDirection = glm::normalize(-fragmentPosition);
#if __BLINN_PHONG__
    glm::vec3 halfwayDirection = glm::normalize(lightDirection + eyeDirection);
    float spec = glm::pow(glm::max(dot(normal, halfwayDirection), 0.0f), gShininess * 2);
#else
    glm::vec3 reflectDirection = glm::reflect(-lightDirection, normal);
    float spec = glm::pow(glm::max(dot(eyeDirection, reflectDirection), 0.0f), gShininess);
#endif
    glm::vec3 result = (gAmbientStrength + diff * gLightColor) * baseColor + spec * gSpecularStrength * gLightColor;

    color[0] = glm::min(result[0], 1.f);
    color[1] = glm::min(result[1], 1.f);
    color[2] = glm::min(result[2], 1.f);

    return true;
}

void TextureMapPhongProgram::loadVertexData(TRMeshData &mesh, PhongVSData vsdata[3], size_t index)
{
    glm::vec3 viewLightPostion = gMat4[MAT4_VIEW] * glm::vec4(gLightPosition, 1.0f);

    for (int i = 0; i < 3; i++)
    {
        vsdata[i].mVertex = mesh.vertices[i + index];
        vsdata[i].mTexCoord = mesh.uvs[i + index];
        vsdata[i].mNormal = mesh.normals[i + index];
        vsdata[i].mLightPosition = viewLightPostion;
        vsdata[i].mTangent = mesh.tangents[i / 3];
    }
}

void TextureMapPhongProgram::vertex(PhongVSData &vsdata)
{
    vsdata.mClipV = gMat4[MAT4_MVP] * glm::vec4(vsdata.mVertex, 1.0f);
    vsdata.mViewFragmentPosition = gMat4[MAT4_MODELVIEW] * glm::vec4(vsdata.mVertex, 1.0f);
    vsdata.mNormal = gMat3[MAT3_NORMAL] * vsdata.mNormal;

    if (gTexture[TEXTURE_NORMAL] != nullptr)
    {
        glm::vec3 N = glm::normalize(vsdata.mNormal);
        glm::vec3 T = glm::normalize(gMat3[MAT3_NORMAL] * vsdata.mTangent);
        T = glm::normalize(T - dot(T, N) * N);
        glm::vec3 B = glm::cross(N, T);
        // Mat3 from view space to tangent space
        glm::mat3 TBN = glm::transpose(glm::mat3(T, B, N));
        vsdata.mTangentFragmentPosition = TBN * vsdata.mViewFragmentPosition;
        // Light Position is fixed in view space, but will changed in tangent space
        vsdata.mLightPosition = TBN * vsdata.mLightPosition;
    }
}

void TextureMapPhongProgram::preInterp(PhongVSData vsdata[3], PhongFSData &fsdata)
{
    TRProgramBase::preInterp(vsdata, fsdata);

    if (gTexture[TEXTURE_NORMAL] != nullptr)
    {
        fsdata.mTangentFragmentPosition[0] = vsdata[0].mTangentFragmentPosition;
        fsdata.mTangentFragmentPosition[1] = vsdata[1].mTangentFragmentPosition - vsdata[0].mTangentFragmentPosition;
        fsdata.mTangentFragmentPosition[2] = vsdata[2].mTangentFragmentPosition - vsdata[0].mTangentFragmentPosition;

        fsdata.mTangentLightPosition[0] = vsdata[0].mLightPosition;
        fsdata.mTangentLightPosition[1] = vsdata[1].mLightPosition - vsdata[0].mLightPosition;
        fsdata.mTangentLightPosition[2] = vsdata[2].mLightPosition - vsdata[0].mLightPosition;
    } else {
        fsdata.mViewFragmentPosition[0] = vsdata[0].mViewFragmentPosition;
        fsdata.mViewFragmentPosition[1] = vsdata[1].mViewFragmentPosition - vsdata[0].mViewFragmentPosition;
        fsdata.mViewFragmentPosition[2] = vsdata[2].mViewFragmentPosition - vsdata[0].mViewFragmentPosition;

        fsdata.mLightPosition = vsdata[0].mLightPosition;
    }
}

void TextureMapPhongProgram::interpVertex(float t, PhongVSData &in1, PhongVSData &in2, PhongVSData &outV)
{
    TRProgramBase::interpVertex(t, in1, in2, outV);
    outV.mViewFragmentPosition = in2.mViewFragmentPosition + t * (in1.mViewFragmentPosition - in2.mViewFragmentPosition);
    outV.mTangentFragmentPosition = in2.mTangentFragmentPosition + t * (in1.mTangentFragmentPosition - in2.mTangentFragmentPosition);
    outV.mLightPosition = in2.mLightPosition + t * (in1.mLightPosition - in2.mLightPosition);
}

bool TextureMapPhongProgram::fragment(PhongFSData &fsdata, float color[3])
{
    glm::vec3 fragmentPosition;
    glm::vec2 texCoord = interpFast(fsdata.mTexCoord);
    textureCoordWrap(texCoord);
    glm::vec3 normal;
    glm::vec3 lightPosition;

    glm::vec3 baseColor = glm::make_vec3(gTexture[TEXTURE_DIFFUSE]->getColor(texCoord.x, texCoord.y));

    if (gTexture[TEXTURE_NORMAL] != nullptr)
    {
        fragmentPosition = interpFast(fsdata.mTangentFragmentPosition);
        normal = glm::make_vec3(gTexture[TEXTURE_NORMAL]->getColor(texCoord.x, texCoord.y)) * 2.0f - 1.0f;
        lightPosition = interpFast(fsdata.mTangentLightPosition);
    } else {
        fragmentPosition = interpFast(fsdata.mViewFragmentPosition);
        normal = interpFast(fsdata.mNormal);
        lightPosition = fsdata.mLightPosition;
    }
    normal = glm::normalize(normal);
    // from fragment to light
    glm::vec3 lightDirection = glm::normalize(lightPosition - fragmentPosition);

    float diff = glm::max(dot(normal, lightDirection), 0.0f);

    // in camera space, eys always in (0.0, 0.0, 0.0), from fragment to eye
    // even in tangent space, eys still in (0, 0, 0)
    glm::vec3 eyeDirection = glm::normalize(-fragmentPosition);
#if __BLINN_PHONG__
    glm::vec3 halfwayDirection = glm::normalize(lightDirection + eyeDirection);
    float spec = glm::pow(glm::max(dot(normal, halfwayDirection), 0.0f), gShininess * 2);
#else
    glm::vec3 reflectDirection = glm::reflect(-lightDirection, normal);
    float spec = glm::pow(glm::max(dot(eyeDirection, reflectDirection), 0.0f), gShininess);
#endif
    glm::vec3 specular = spec * gLightColor;

    if (gTexture[TEXTURE_SPECULAR] != nullptr)
        specular *= glm::make_vec3(gTexture[TEXTURE_SPECULAR]->getColor(texCoord.x, texCoord.y));
    else
        specular *= gSpecularStrength;

    glm::vec3 result = (gAmbientStrength + diff * gLightColor) * baseColor + specular;

    if (gTexture[TEXTURE_GLOW] != nullptr)
        result += glm::make_vec3(gTexture[TEXTURE_GLOW]->getColor(texCoord.x, texCoord.y));

    color[0] = glm::min(result[0], 1.f);
    color[1] = glm::min(result[1], 1.f);
    color[2] = glm::min(result[2], 1.f);

    return true;
}

template <class TRVSData, class TRFSData>
void TRProgramBase<TRVSData, TRFSData>::preInterp(TRVSData *vsdata, TRFSData &fsdata)
{
    fsdata.mClipV[0] = vsdata[0].mClipV;
    fsdata.mClipV[1] = vsdata[1].mClipV - vsdata[0].mClipV;
    fsdata.mClipV[2] = vsdata[2].mClipV - vsdata[0].mClipV;

    fsdata.mTexCoord[0] = vsdata[0].mTexCoord;
    fsdata.mTexCoord[1] = vsdata[1].mTexCoord - vsdata[0].mTexCoord;
    fsdata.mTexCoord[2] = vsdata[2].mTexCoord - vsdata[0].mTexCoord;

    fsdata.mNormal[0] = vsdata[0].mNormal;
    fsdata.mNormal[1] = vsdata[1].mNormal - vsdata[0].mNormal;
    fsdata.mNormal[2] = vsdata[2].mNormal - vsdata[0].mNormal;

    fsdata.mColor[0] = vsdata[0].mColor;
    fsdata.mColor[1] = vsdata[1].mColor - vsdata[0].mColor;
    fsdata.mColor[2] = vsdata[2].mColor - vsdata[0].mColor;
}

template <class TRVSData, class TRFSData>
void TRProgramBase<TRVSData, TRFSData>::interpVertex(float t, TRVSData &in1, TRVSData &in2, TRVSData &outV)
{
    outV.mTexCoord = in2.mTexCoord + t * (in1.mTexCoord - in2.mTexCoord);
    outV.mNormal = in2.mNormal + t * (in1.mNormal - in2.mNormal);
    outV.mColor = in2.mColor + t * (in1.mColor - in2.mColor);
    outV.mClipV = in2.mClipV + t * (in1.mClipV - in2.mClipV);

}
#define W_NEAR (0.1f)

template <class TRVSData, class TRFSData>
void TRProgramBase<TRVSData, TRFSData>::clipLineNear(TRVSData &in1, TRVSData &in2, TRVSData out[3], size_t &index)
{
    // Sutherland-Hodgeman clip
    // t = w1 / (w1 - w2)
    // V = V1 + t * (V2 - V1)
    if (in1.mClipV.w > 0 && in2.mClipV.w > 0)
    {
        out[index++] = in2;
    }
    else if (in1.mClipV.w > 0 && in2.mClipV.w < 0)
    {
        float t = (in2.mClipV.w - W_NEAR)/(in2.mClipV.w - in1.mClipV.w);
        TRVSData outV;
        // get interp V and put V into output
        interpVertex(t, in1, in2, outV);
        out[index++] = outV;
    }
    else if (in1.mClipV.w < 0 && in2.mClipV.w > 0)
    {
        float t = (in1.mClipV.w - W_NEAR) /(in1.mClipV.w - in2.mClipV.w);
        TRVSData outV;
        // get interp V and put V and V2 into output
        interpVertex(t, in2, in1, outV);
        out[index++] = outV;
        out[index++] = in2;
    }
}

template <class TRVSData, class TRFSData>
void TRProgramBase<TRVSData, TRFSData>::clipNear(TRVSData in[3], TRVSData out[4], size_t &index)
{
    index = 0;
    clipLineNear(in[0], in[1], out, index);
    clipLineNear(in[1], in[2], out, index);
    clipLineNear(in[2], in[0], out, index);
}

template <class TRVSData, class TRFSData>
void TRProgramBase<TRVSData, TRFSData>::drawTriangle(TRMeshData &mesh, size_t index)
{
    TRVSData vsdata[3];
    TRFSData fsdata;

    loadVertexData(mesh, vsdata, index * 3);
    for (size_t i = 0; i < 3; i++)
        vertex(vsdata[i]);

    // Max is 4 output.
    TRVSData out[4];
    size_t total = 0;
    clipNear(vsdata, out, total);

    for (size_t i = 0; i < total - 2; i++)
    {
        vsdata[0] = out[0];
        vsdata[1] = out[i + 1];
        vsdata[2] = out[i + 2];
        glm::vec4 clipV[3] = { vsdata[0].mClipV, vsdata[1].mClipV, vsdata[2].mClipV };
        preInterp(vsdata, fsdata);
        rasterization(clipV, fsdata);
    }
}

template <class TRVSData, class TRFSData>
void TRProgramBase<TRVSData, TRFSData>::drawTrianglesInstanced(TRBuffer *buffer, TRMeshData &mesh, size_t index, size_t num)
{
    size_t i = 0, j = 0;
    size_t trianglesNum = mesh.vertices.size() / 3;

    mBuffer = buffer;

    for (i = index, j = 0; i < trianglesNum && j < num; i++, j++)
        drawTriangle(mesh, i);
}

template <class TRVSData, class TRFSData>
void TRProgramBase<TRVSData, TRFSData>::rasterizationPoint(
        glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, glm::vec2 &point, TRFSData &fsdata, bool insideCheck)
{
    float w0 = edge(screen_v[1], screen_v[2], point);
    float w1 = edge(screen_v[2], screen_v[0], point);
    float w2 = edge(screen_v[0], screen_v[1], point);
    if (insideCheck && (w0 < 0 || w1 < 0 || w2 < 0))
        return;

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

    /* easy-z */
    /* Do not use mutex here to speed up */
    if (!mBuffer->depthTest(x, y, depth))
        return;

    float color[3];
    if (!fragment(fsdata, color))
        return;

    uint8_t *addr = &mBuffer->mData[(y * mBuffer->mW + x) * CHANNEL];
    /* depth test */
    std::lock_guard<std::mutex> lck(mBuffer->mDepthMutex);
    if (!mBuffer->depthTest(x, y, depth))
        return;

    addr[0] = uint8_t(color[0] * 255 + 0.5);
    addr[1] = uint8_t(color[1] * 255 + 0.5);
    addr[2] = uint8_t(color[2] * 255 + 0.5);
}

template <class TRVSData, class TRFSData>
void TRProgramBase<TRVSData, TRFSData>::rasterizationLine(
        glm::vec4 clip_v[3], glm::vec4 ndc_v[3], glm::vec2 screen_v[3], float area, int p1, int p2, TRFSData &fsdata)
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
        if (v.x > (int)mBuffer->mW || v.y > (int)mBuffer->mH || v.x < 0 || v.y < 0)
            continue;

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

template <class TRVSData, class TRFSData>
void TRProgramBase<TRVSData, TRFSData>::rasterization(glm::vec4 clip_v[3], TRFSData &fsdata)
{
    glm::vec4 ndc_v[3];
    glm::vec2 screen_v[3];

    for (int i = 0; i < 3; i++)
    {
        ndc_v[i] = clip_v[i] / clip_v[i].w;
        mBuffer->viewport(screen_v[i], ndc_v[i]);
    }

    float area = edge(screen_v[0], screen_v[1], screen_v[2]);
    if (area <= 0)
        return;

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

template<class Program>
void trTrianglesInstanced(TRMeshData &mesh, size_t index, size_t num)
{
    Program prog;
    prog.drawTrianglesInstanced(gRenderTarget, mesh, index, num);
}

template<class Program>
void trTrianglesMT(TRMeshData &mesh)
{
    if (gThreadNum > 1)
    {
        std::vector<std::thread> thread_pool;
        size_t index_step = mesh.vertices.size() / 3 / gThreadNum + 1;

        for (size_t i = 0; i < gThreadNum; i++)
            thread_pool.push_back(std::thread(trTrianglesInstanced<Program>, std::ref(mesh), i * index_step, index_step));

        for (auto &th : thread_pool)
            if (th.joinable())
                th.join();
    } else {
        trTrianglesInstanced<Program>(mesh, 0, mesh.vertices.size() / 3);
    }
}

void trTriangles(TRMeshData &mesh, TRDrawType type)
{
    __compute_premultiply_mat__();

    switch (type)
    {
        case DRAW_WITH_TEXTURE:
            if (mesh.tangents.size() == 0)
                mesh.computeTangent();
            if (gEnableLighting)
                trTrianglesMT<TextureMapPhongProgram>(mesh);
            else
                trTrianglesMT<TextureMapProgram>(mesh);
            break;
        case DRAW_WITH_COLOR:
            if (gEnableLighting)
                trTrianglesMT<ColorPhongProgram>(mesh);
            else
                trTrianglesMT<ColorProgram>(mesh);
            break;
    }
}

void trSetRenderThreadNum(size_t num)
{
    gThreadNum = num;
}

void trEnableLighting(bool enable)
{
    gEnableLighting = enable;
}

void trSetAmbientStrength(float v)
{
    gAmbientStrength = v;
}

void trSetSpecularStrength(float v)
{
    gSpecularStrength = v;
}

void trSetShininess(int v)
{
    gShininess = v;
}

void trSetLightColor3f(float r, float g, float b)
{
    gLightColor = glm::vec3(r, g, b);
}

void trSetLightPosition3f(float x, float y, float z)
{
    gLightPosition = glm::vec3(x, y, z);
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
    else
        std::cout << "Unknow texture type!\n";
}

void trClear()
{
    gRenderTarget->clearColor();
    gRenderTarget->clearDepth();
}

void trClearColor3f(float r, float g, float b)
{
    gRenderTarget->setBGColor(r, g, b);
}

void trSetModelMat(glm::mat4 mat)
{
    gMat4[MAT4_MODEL] = mat;
}

void trSetViewMat(glm::mat4 mat)
{
    gMat4[MAT4_VIEW] = mat;
}

void trSetProjMat(glm::mat4 mat)
{
    gMat4[MAT4_PROJ] = mat;
}

void trDrawMode(TRDrawMode mode)
{
    gDrawMode = mode;
}

TRBuffer * trCreateRenderTarget(int w, int h)
{
    return TRBuffer::create(w, h);
}

void trSetCurrentRenderTarget(TRBuffer *buffer)
{
    gRenderTarget = buffer;
}
