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

float gAmbientStrength = 0.1;
float gSpecularStrength = 0.2;
int gShininess = 32;
glm::vec3 gLightColor = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 gLightPosition = glm::vec3(0.0f, 0.0f, 0.0f);

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

void ColorProgram::loadVertexData(TRMeshData &mesh, VSDataBase *vsdata, size_t index)
{
    vsdata->mVertex = mesh.vertices[index];
    vsdata->mColor = mesh.colors[index];
}

void ColorProgram::vertex(VSDataBase *vsdata)
{
    vsdata->mClipV = gMat4[MAT4_MVP] * glm::vec4(vsdata->mVertex, 1.0f);
}

bool ColorProgram::fragment(FSDataBase *fsdata, float color[3])
{
    glm::vec3 C = interpFast(fsdata->mColor);
    color[0] = C[0];
    color[1] = C[1];
    color[2] = C[2];
    return true;
}

TRProgramBase* ColorProgram::clone()
{
    return new ColorProgram();
}

void TextureMapProgram::loadVertexData(TRMeshData &mesh, VSDataBase *vsdata, size_t index)
{
    vsdata->mVertex = mesh.vertices[index];
    vsdata->mTexCoord = mesh.uvs[index];
}

void TextureMapProgram::vertex(VSDataBase *vsdata)
{
    vsdata->mClipV = gMat4[MAT4_MVP] * glm::vec4(vsdata->mVertex, 1.0f);
}

bool TextureMapProgram::fragment(FSDataBase *fsdata, float color[3])
{
    glm::vec2 texCoord = interpFast(fsdata->mTexCoord);
    textureCoordWrap(texCoord);

    float *c = gTexture[TEXTURE_DIFFUSE]->getColor(texCoord.x, texCoord.y);

    color[0] = c[0];
    color[1] = c[1];
    color[2] = c[2];
    return true;
}

TRProgramBase* TextureMapProgram::clone()
{
    return new TextureMapProgram();
}

void ColorPhongProgram::loadVertexData(TRMeshData &mesh, VSDataBase *v, size_t index)
{
    glm::vec3 viewLightPostion = gMat4[MAT4_VIEW] * glm::vec4(gLightPosition, 1.0f);
    PhongVSData *vsdata = dynamic_cast<PhongVSData *>(v);

    vsdata->mVertex = mesh.vertices[index];
    vsdata->mNormal = mesh.normals[index];
    vsdata->mColor = mesh.colors[index];
    vsdata->mViewLightPosition = viewLightPostion;
}

void ColorPhongProgram::vertex(VSDataBase *v)
{
    PhongVSData *vsdata = dynamic_cast<PhongVSData *>(v);

    vsdata->mClipV = gMat4[MAT4_MVP] * glm::vec4(vsdata->mVertex, 1.0f);
    vsdata->mViewFragmentPosition = gMat4[MAT4_MODELVIEW] * glm::vec4(vsdata->mVertex, 1.0f);
    vsdata->mNormal = gMat3[MAT3_NORMAL] * vsdata->mNormal;

    if (gTexture[TEXTURE_SHADOWMAP] != nullptr)
        vsdata->mLightClipV = gMat4[MAT4_LIGHT_MVP] * glm::vec4(vsdata->mVertex, 1.0f);
}

void ColorPhongProgram::prepareFragmentData(VSDataBase *v[3], FSDataBase *f)
{
    TRProgramBase::prepareFragmentData(v, f);

    PhongVSData **vsdata = reinterpret_cast<PhongVSData **>(v);
    PhongFSData *fsdata = dynamic_cast<PhongFSData *>(f);

    fsdata->mViewFragmentPosition[0] = vsdata[0]->mViewFragmentPosition;
    fsdata->mViewFragmentPosition[1] = vsdata[1]->mViewFragmentPosition - vsdata[0]->mViewFragmentPosition;
    fsdata->mViewFragmentPosition[2] = vsdata[2]->mViewFragmentPosition - vsdata[0]->mViewFragmentPosition;

    if (gTexture[TEXTURE_SHADOWMAP] != nullptr)
    {
        fsdata->mLightClipV[0] = vsdata[0]->mLightClipV;
        fsdata->mLightClipV[1] = vsdata[1]->mLightClipV - vsdata[0]->mLightClipV;
        fsdata->mLightClipV[2] = vsdata[2]->mLightClipV - vsdata[0]->mLightClipV;
    }

    fsdata->mViewLightPosition = vsdata[0]->mViewLightPosition;
}

void ColorPhongProgram::interpVertex(float t, VSDataBase *in1, VSDataBase *in2, VSDataBase *outV)
{
    TRProgramBase::interpVertex(t, in1, in2, outV);

    PhongVSData *nin1 = dynamic_cast<PhongVSData *>(in1);
    PhongVSData *nin2 = dynamic_cast<PhongVSData *>(in2);
    PhongVSData *noutV = dynamic_cast<PhongVSData *>(outV);

    noutV->mViewFragmentPosition = nin2->mViewFragmentPosition + t * (nin1->mViewFragmentPosition - nin2->mViewFragmentPosition);
    noutV->mViewLightPosition = nin2->mViewLightPosition + t * (nin1->mViewLightPosition - nin2->mViewLightPosition);
    noutV->mLightClipV = nin2->mLightClipV + t * (nin1->mLightClipV - nin2->mLightClipV);
}

bool ColorPhongProgram::fragment(FSDataBase *f, float color[3])
{
    PhongFSData *fsdata = dynamic_cast<PhongFSData *>(f);

    glm::vec3 fragmentPosition = interpFast(fsdata->mViewFragmentPosition);
    glm::vec3 normal = interpFast(fsdata->mNormal);
    glm::vec3 baseColor = interpFast(fsdata->mColor);

    normal = glm::normalize(normal);
    // from fragment to light
    glm::vec3 lightDirection = glm::normalize(fsdata->mViewLightPosition - fragmentPosition);

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

    if (gTexture[TEXTURE_SHADOWMAP] != nullptr)
    {
        float shadow = 1.0f;
        glm::vec4 lightClipV = interpFast(fsdata->mLightClipV);
        lightClipV = (lightClipV / lightClipV.w) * 0.5f + 0.5f;
        if (lightClipV.x > 1.0f || lightClipV.x < 0.0f || lightClipV.y > 1.0f || lightClipV.y < 0.0f)
            shadow = 1.0f;
        else if (lightClipV.z > *gTexture[TEXTURE_SHADOWMAP]->getColor(lightClipV.x, lightClipV.y) + ShadowMapProgram::BIAS)
            shadow = ShadowMapProgram::FACTOR;

        result *= shadow;
    }

    color[0] = glm::min(result[0], 1.f);
    color[1] = glm::min(result[1], 1.f);
    color[2] = glm::min(result[2], 1.f);

    return true;
}

VSDataBase * ColorPhongProgram::allocVSData()
{
    return dynamic_cast<VSDataBase *>(&mVSData[mAllocIndex++]);
}

FSDataBase * ColorPhongProgram::allocFSData()
{
    return dynamic_cast<FSDataBase *>(&mFSData);
}

void ColorPhongProgram::freeShaderData()
{
    mAllocIndex = 0;
}

TRProgramBase* ColorPhongProgram::clone()
{
    return new ColorPhongProgram();
}

void TextureMapPhongProgram::loadVertexData(TRMeshData &mesh, VSDataBase *v, size_t index)
{
    glm::vec3 viewLightPostion = gMat4[MAT4_VIEW] * glm::vec4(gLightPosition, 1.0f);
    PhongVSData *vsdata = dynamic_cast<PhongVSData *>(v);

    vsdata->mVertex = mesh.vertices[index];
    vsdata->mTexCoord = mesh.uvs[index];
    vsdata->mNormal = mesh.normals[index];
    vsdata->mViewLightPosition = viewLightPostion;
    vsdata->mTangent = mesh.tangents[index / 3];
}

void TextureMapPhongProgram::vertex(VSDataBase *v)
{
    PhongVSData *vsdata = dynamic_cast<PhongVSData *>(v);

    vsdata->mClipV = gMat4[MAT4_MVP] * glm::vec4(vsdata->mVertex, 1.0f);
    vsdata->mViewFragmentPosition = gMat4[MAT4_MODELVIEW] * glm::vec4(vsdata->mVertex, 1.0f);
    vsdata->mNormal = gMat3[MAT3_NORMAL] * vsdata->mNormal;

    if (gTexture[TEXTURE_NORMAL] != nullptr)
    {
        glm::vec3 N = glm::normalize(vsdata->mNormal);
        glm::vec3 T = glm::normalize(gMat3[MAT3_NORMAL] * vsdata->mTangent);
        T = glm::normalize(T - dot(T, N) * N);
        glm::vec3 B = glm::cross(N, T);
        // Mat3 from view space to tangent space
        glm::mat3 TBN = glm::transpose(glm::mat3(T, B, N));
        vsdata->mTangentFragmentPosition = TBN * vsdata->mViewFragmentPosition;
        // Light Position is fixed in view space, but will changed in tangent space
        vsdata->mTangentLightPosition = TBN * vsdata->mViewLightPosition;
    }

    if (gTexture[TEXTURE_SHADOWMAP] != nullptr)
        vsdata->mLightClipV = gMat4[MAT4_LIGHT_MVP] * glm::vec4(vsdata->mVertex, 1.0f);
}

void TextureMapPhongProgram::prepareFragmentData(VSDataBase *v[3], FSDataBase *f)
{
    TRProgramBase::prepareFragmentData(v, f);

    PhongVSData **vsdata = reinterpret_cast<PhongVSData **>(v);
    PhongFSData *fsdata = dynamic_cast<PhongFSData *>(f);

    if (gTexture[TEXTURE_NORMAL] != nullptr)
    {
        fsdata->mTangentFragmentPosition[0] = vsdata[0]->mTangentFragmentPosition;
        fsdata->mTangentFragmentPosition[1] = vsdata[1]->mTangentFragmentPosition - vsdata[0]->mTangentFragmentPosition;
        fsdata->mTangentFragmentPosition[2] = vsdata[2]->mTangentFragmentPosition - vsdata[0]->mTangentFragmentPosition;

        fsdata->mTangentLightPosition[0] = vsdata[0]->mTangentLightPosition;
        fsdata->mTangentLightPosition[1] = vsdata[1]->mTangentLightPosition - vsdata[0]->mTangentLightPosition;
        fsdata->mTangentLightPosition[2] = vsdata[2]->mTangentLightPosition - vsdata[0]->mTangentLightPosition;
    } else {
        fsdata->mViewFragmentPosition[0] = vsdata[0]->mViewFragmentPosition;
        fsdata->mViewFragmentPosition[1] = vsdata[1]->mViewFragmentPosition - vsdata[0]->mViewFragmentPosition;
        fsdata->mViewFragmentPosition[2] = vsdata[2]->mViewFragmentPosition - vsdata[0]->mViewFragmentPosition;

        fsdata->mViewLightPosition = vsdata[0]->mViewLightPosition;
    }

    if (gTexture[TEXTURE_SHADOWMAP] != nullptr)
    {
        fsdata->mLightClipV[0] = vsdata[0]->mLightClipV;
        fsdata->mLightClipV[1] = vsdata[1]->mLightClipV - vsdata[0]->mLightClipV;
        fsdata->mLightClipV[2] = vsdata[2]->mLightClipV - vsdata[0]->mLightClipV;
    }

}

void TextureMapPhongProgram::interpVertex(float t, VSDataBase *in1, VSDataBase *in2, VSDataBase *outV)
{
    TRProgramBase::interpVertex(t, in1, in2, outV);

    PhongVSData *nin1 = dynamic_cast<PhongVSData *>(in1);
    PhongVSData *nin2 = dynamic_cast<PhongVSData *>(in2);
    PhongVSData *noutV = dynamic_cast<PhongVSData *>(outV);

    noutV->mViewFragmentPosition = nin2->mViewFragmentPosition + t * (nin1->mViewFragmentPosition - nin2->mViewFragmentPosition);
    noutV->mViewLightPosition = nin2->mViewLightPosition + t * (nin1->mViewLightPosition - nin2->mViewLightPosition);
    noutV->mTangentFragmentPosition = nin2->mTangentFragmentPosition + t * (nin1->mTangentFragmentPosition - nin2->mTangentFragmentPosition);
    noutV->mTangentLightPosition = nin2->mTangentLightPosition + t * (nin1->mTangentLightPosition - nin2->mTangentLightPosition);
    noutV->mLightClipV = nin2->mLightClipV + t * (nin1->mLightClipV - nin2->mLightClipV);
}

bool TextureMapPhongProgram::fragment(FSDataBase *f, float color[3])
{
    PhongFSData *fsdata = dynamic_cast<PhongFSData *>(f);

    glm::vec3 fragmentPosition;
    glm::vec2 texCoord = interpFast(fsdata->mTexCoord);
    textureCoordWrap(texCoord);
    glm::vec3 normal;
    glm::vec3 lightPosition;

    glm::vec3 baseColor = glm::make_vec3(gTexture[TEXTURE_DIFFUSE]->getColor(texCoord.x, texCoord.y));

    if (gTexture[TEXTURE_NORMAL] != nullptr)
    {
        fragmentPosition = interpFast(fsdata->mTangentFragmentPosition);
        normal = glm::make_vec3(gTexture[TEXTURE_NORMAL]->getColor(texCoord.x, texCoord.y)) * 2.0f - 1.0f;
        lightPosition = interpFast(fsdata->mTangentLightPosition);
    } else {
        fragmentPosition = interpFast(fsdata->mViewFragmentPosition);
        normal = interpFast(fsdata->mNormal);
        lightPosition = fsdata->mViewLightPosition;
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

    if (gTexture[TEXTURE_SHADOWMAP] != nullptr)
    {
        float shadow = 1.0f;
        glm::vec4 lightClipV = interpFast(fsdata->mLightClipV);
        lightClipV = (lightClipV / lightClipV.w) * 0.5f + 0.5f;
        if (lightClipV.x > 1.0f || lightClipV.x < 0.0f || lightClipV.y > 1.0f || lightClipV.y < 0.0f)
            shadow = 1.0f;
        else if (lightClipV.z > *gTexture[TEXTURE_SHADOWMAP]->getColor(lightClipV.x, lightClipV.y) + ShadowMapProgram::BIAS)
            shadow = ShadowMapProgram::FACTOR;

        result *= shadow;
    }

    /* glow shouldn't be affectd by shadow */
    if (gTexture[TEXTURE_GLOW] != nullptr)
        result += glm::make_vec3(gTexture[TEXTURE_GLOW]->getColor(texCoord.x, texCoord.y));

    color[0] = glm::min(result[0], 1.f);
    color[1] = glm::min(result[1], 1.f);
    color[2] = glm::min(result[2], 1.f);

    return true;
}

VSDataBase * TextureMapPhongProgram::allocVSData()
{
    return dynamic_cast<VSDataBase *>(&mVSData[mAllocIndex++]);
}

FSDataBase * TextureMapPhongProgram::allocFSData()
{
    return dynamic_cast<FSDataBase *>(&mFSData);
}

void TextureMapPhongProgram::freeShaderData()
{
    mAllocIndex = 0;
}

TRProgramBase* TextureMapPhongProgram::clone()
{
    return new TextureMapPhongProgram();
}

void ShadowMapProgram::loadVertexData(TRMeshData &mesh, VSDataBase *vsdata, size_t index)
{
    vsdata->mVertex = mesh.vertices[index];
}

void ShadowMapProgram::vertex(VSDataBase *vsdata)
{
    vsdata->mClipV = gMat4[MAT4_MVP] * glm::vec4(vsdata->mVertex, 1.0f);
}

bool ShadowMapProgram::fragment(FSDataBase *fsdata, float color[3])
{
    glm::vec4 clipV = interpFast(fsdata->mClipV);
    float depth = glm::clamp((clipV.z / clipV.w) * 0.5f + 0.5f, 0.0f, 1.0f);
    color[0] = depth;
    color[1] = depth;
    color[2] = depth;

    return true;
}

TRProgramBase * ShadowMapProgram::clone()
{
    return new ShadowMapProgram();
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

    size_t offset = mBuffer->getOffset(x, y);
    /* easy-z */
    /* Do not use mutex here to speed up */
    if (!mBuffer->depthTest(offset, depth))
        return;

    float color[3];
    if (!fragment(fsdata, color))
        return;

    /* depth test */
    std::lock_guard<std::mutex> lck(mBuffer->mDepthMutex);
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

void trDrawMode(TRDrawMode mode)
{
    gDrawMode = mode;
}

TRBuffer * trCreateRenderTarget(int w, int h)
{
    return new TRBuffer(w, h, true);
}

void trSetCurrentRenderTarget(TRBuffer *buffer)
{
    gRenderTarget = buffer;
}
