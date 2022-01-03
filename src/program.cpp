#include <vector>
#include <algorithm>
#include <glm/ext.hpp>

#include "tr.hpp"

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

float *texture2D(int type, float u, float v)
{
    return trGetTexture(type)->getColor(u, v);
}

float calcShadowFast(float depth, float x, float y)
{
    if (x <= 1.0f && x >= 0.0f && y <= 1.0f && y >= 0.0f
            && (depth > *(texture2D(TEXTURE_SHADOWMAP, x, y)) + ShadowMapProgram::BIAS))
        return ShadowMapProgram::FACTOR;
    else
        return 1.0f;
}

float calcShadow(float depth, float x, float y)
{
    TRTexture *st = trGetTexture(TEXTURE_SHADOWMAP);
    float xstep = 1.f / st->getW();
    float ystep = 1.f / st->getH();
    float shadow = 0.f;

    float uvs[] =
    {
        x - xstep, y - ystep,
        x, y - ystep,
        x + xstep, y - ystep,
        x - xstep, y,
        x, y,
        x + xstep, y,
        x - xstep, y + ystep,
        x, y + ystep,
        x + xstep, y + ystep,
    };
    for (size_t i = 0; i < 9; i++)
        shadow += calcShadowFast(depth, uvs[i*2], uvs[i*2+1]);

    return shadow / 9.0f;
}

void ColorProgram::loadVertexData(TRMeshData &mesh, VSDataBase *vsdata, size_t index)
{
    vsdata->mVertex = mesh.vertices[index];
    vsdata->mColor = mesh.colors[index];
}

void ColorProgram::vertex(VSDataBase *vsdata)
{
    vsdata->mClipV = trGetMat4(MAT4_MVP) * glm::vec4(vsdata->mVertex, 1.0f);
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
    vsdata->mClipV = trGetMat4(MAT4_MVP) * glm::vec4(vsdata->mVertex, 1.0f);
}

bool TextureMapProgram::fragment(FSDataBase *fsdata, float color[3])
{
    glm::vec2 texCoord = interpFast(fsdata->mTexCoord);
    textureCoordWrap(texCoord);

    float *c = texture2D(TEXTURE_DIFFUSE, texCoord.x, texCoord.y);

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
    glm::vec3 viewLightPostion = trGetMat4(MAT4_VIEW) * glm::vec4(trGetLightInfo().mPosition, 1.0f);
    PhongVSData *vsdata = dynamic_cast<PhongVSData *>(v);

    vsdata->mVertex = mesh.vertices[index];
    vsdata->mNormal = mesh.normals[index];
    vsdata->mColor = mesh.colors[index];
    vsdata->mViewLightPosition = viewLightPostion;
}

void ColorPhongProgram::vertex(VSDataBase *v)
{
    PhongVSData *vsdata = dynamic_cast<PhongVSData *>(v);

    vsdata->mClipV = trGetMat4(MAT4_MVP) * glm::vec4(vsdata->mVertex, 1.0f);
    vsdata->mViewFragmentPosition = trGetMat4(MAT4_MODELVIEW) * glm::vec4(vsdata->mVertex, 1.0f);
    vsdata->mNormal = trGetMat3(MAT3_NORMAL) * vsdata->mNormal;

    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
        vsdata->mLightClipV = trGetMat4(MAT4_LIGHT_MVP) * glm::vec4(vsdata->mVertex, 1.0f);
}

void ColorPhongProgram::prepareFragmentData(VSDataBase *v[3], FSDataBase *f)
{
    TRProgramBase::prepareFragmentData(v, f);

    PhongVSData **vsdata = reinterpret_cast<PhongVSData **>(v);
    PhongFSData *fsdata = dynamic_cast<PhongFSData *>(f);

    fsdata->mViewFragmentPosition[0] = vsdata[0]->mViewFragmentPosition;
    fsdata->mViewFragmentPosition[1] = vsdata[1]->mViewFragmentPosition - vsdata[0]->mViewFragmentPosition;
    fsdata->mViewFragmentPosition[2] = vsdata[2]->mViewFragmentPosition - vsdata[0]->mViewFragmentPosition;

    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
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
    glm::vec3 diffuseColor = interpFast(fsdata->mColor);

    LightInfo &light = trGetLightInfo();

    normal = glm::normalize(normal);
    // from fragment to light
    glm::vec3 lightDirection = glm::normalize(fsdata->mViewLightPosition - fragmentPosition);

    float diff = glm::max(dot(normal, lightDirection), 0.0f);

    // in camera space, eys always in (0.0, 0.0, 0.0), from fragment to eye
    glm::vec3 eyeDirection = glm::normalize(-fragmentPosition);
#if __BLINN_PHONG__
    glm::vec3 halfwayDirection = glm::normalize(lightDirection + eyeDirection);
    float spec = glm::pow(glm::max(dot(normal, halfwayDirection), 0.0f), light.mShininess * 2);
#else
    glm::vec3 reflectDirection = glm::reflect(-lightDirection, normal);
    float spec = glm::pow(glm::max(dot(eyeDirection, reflectDirection), 0.0f), light.mShininess);
#endif
    glm::vec3 result = ((light.mAmbientStrength + diff) * diffuseColor + spec * light.mSpecularStrength) * light.mColor;

    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
    {
        glm::vec4 lightClipV = interpFast(fsdata->mLightClipV);
        lightClipV = (lightClipV / lightClipV.w) * 0.5f + 0.5f;

        result *= calcShadow(lightClipV.z, lightClipV.x, lightClipV.y);
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
    glm::vec3 viewLightPostion = trGetMat4(MAT4_VIEW) * glm::vec4(trGetLightInfo().mPosition, 1.0f);
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

    vsdata->mClipV = trGetMat4(MAT4_MVP) * glm::vec4(vsdata->mVertex, 1.0f);
    vsdata->mViewFragmentPosition = trGetMat4(MAT4_MODELVIEW) * glm::vec4(vsdata->mVertex, 1.0f);
    vsdata->mNormal = trGetMat3(MAT3_NORMAL) * vsdata->mNormal;

    if (trGetTexture(TEXTURE_NORMAL) != nullptr)
    {
        glm::vec3 N = glm::normalize(vsdata->mNormal);
        glm::vec3 T = glm::normalize(trGetMat3(MAT3_NORMAL) * vsdata->mTangent);
        T = glm::normalize(T - dot(T, N) * N);
        glm::vec3 B = glm::cross(N, T);
        // Mat3 from view space to tangent space
        glm::mat3 TBN = glm::transpose(glm::mat3(T, B, N));
        vsdata->mTangentFragmentPosition = TBN * vsdata->mViewFragmentPosition;
        // Light Position is fixed in view space, but will changed in tangent space
        vsdata->mTangentLightPosition = TBN * vsdata->mViewLightPosition;
    }

    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
        vsdata->mLightClipV = trGetMat4(MAT4_LIGHT_MVP) * glm::vec4(vsdata->mVertex, 1.0f);
}

void TextureMapPhongProgram::prepareFragmentData(VSDataBase *v[3], FSDataBase *f)
{
    TRProgramBase::prepareFragmentData(v, f);

    PhongVSData **vsdata = reinterpret_cast<PhongVSData **>(v);
    PhongFSData *fsdata = dynamic_cast<PhongFSData *>(f);

    if (trGetTexture(TEXTURE_NORMAL) != nullptr)
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

    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
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

    glm::vec2 texCoord = interpFast(fsdata->mTexCoord);
    textureCoordWrap(texCoord);

    glm::vec3 fragmentPosition;
    glm::vec3 normal;
    glm::vec3 lightPosition;

    LightInfo &light = trGetLightInfo();

    glm::vec3 diffuseColor = glm::make_vec3(texture2D(TEXTURE_DIFFUSE, texCoord.x, texCoord.y));

    if (trGetTexture(TEXTURE_NORMAL) != nullptr)
    {
        fragmentPosition = interpFast(fsdata->mTangentFragmentPosition);
        normal = glm::make_vec3(texture2D(TEXTURE_NORMAL, texCoord.x, texCoord.y)) * 2.0f - 1.0f;
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
    float spec = glm::pow(glm::max(dot(normal, halfwayDirection), 0.0f), light.mShininess * 2);
#else
    glm::vec3 reflectDirection = glm::reflect(-lightDirection, normal);
    float spec = glm::pow(glm::max(dot(eyeDirection, reflectDirection), 0.0f), light.mShininess);
#endif
    glm::vec3 specColor(1.0f);
    if (trGetTexture(TEXTURE_SPECULAR) != nullptr)
        specColor = glm::make_vec3(texture2D(TEXTURE_SPECULAR, texCoord.x, texCoord.y));
    else
        specColor *= light.mSpecularStrength;

    glm::vec3 result = ((light.mAmbientStrength + diff) * diffuseColor + spec * specColor) * light.mColor;

    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
    {
        glm::vec4 lightClipV = interpFast(fsdata->mLightClipV);
        lightClipV = (lightClipV / lightClipV.w) * 0.5f + 0.5f;

        result *= calcShadow(lightClipV.z, lightClipV.x, lightClipV.y);
    }

    /* glow shouldn't be affectd by shadow */
    if (trGetTexture(TEXTURE_GLOW) != nullptr)
        result += glm::make_vec3(texture2D(TEXTURE_GLOW, texCoord.x, texCoord.y));

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
    vsdata->mClipV = trGetMat4(MAT4_MVP) * glm::vec4(vsdata->mVertex, 1.0f);
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

