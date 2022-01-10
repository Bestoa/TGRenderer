#include <vector>
#include <algorithm>
#include <glm/ext.hpp>

#include "trapi.hpp"
#include "program.hpp"

using namespace TGRenderer;

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
            && (depth > *(texture2D(TEXTURE_SHADOWMAP, x, y)) + ShadowMapShader::BIAS))
        return ShadowMapShader::FACTOR;
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

void ColorShader::vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index)
{
    vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
    vsdata->mVaryingVec3[SH_COLOR] = mesh.colors[index];
}

bool ColorShader::fragment(FSInData *fsdata, float color[3])
{
    glm::vec3 C = fsdata->getVec3(SH_COLOR);
    color[0] = C[0];
    color[1] = C[1];
    color[2] = C[2];
    return true;
}

void ColorShader::getVaryingNum(size_t &v2, size_t &v3, size_t &v4)
{
    v2 = SH_VEC2_BASE_MAX;
    v3 = SH_VEC3_BASE_MAX;
    v4 = SH_VEC4_BASE_MAX;
}

void TextureMapShader::vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index)
{
    vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
    vsdata->mVaryingVec2[SH_TEXCOORD] = mesh.uvs[index];
}

bool TextureMapShader::fragment(FSInData *fsdata, float color[3])
{
    glm::vec2 texCoord = fsdata->getVec2(SH_TEXCOORD);
    textureCoordWrap(texCoord);

    float *c = texture2D(TEXTURE_DIFFUSE, texCoord.x, texCoord.y);

    color[0] = c[0];
    color[1] = c[1];
    color[2] = c[2];
    return true;
}

void TextureMapShader::getVaryingNum(size_t &v2, size_t &v3, size_t &v4)
{
    v2 = SH_VEC2_BASE_MAX;
    v3 = SH_VEC3_BASE_MAX;
    v4 = SH_VEC4_BASE_MAX;
}

void ColorPhongShader::vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index)
{
    vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
    vsdata->mVaryingVec3[SH_VIEW_FRAG_POSITION] = trGetMat4(MAT4_MODELVIEW) * glm::vec4(mesh.vertices[index], 1.0f);
    vsdata->mVaryingVec3[SH_NORMAL] = trGetMat3(MAT3_NORMAL) * mesh.normals[index];
    vsdata->mVaryingVec3[SH_COLOR] = mesh.colors[index];

    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
        vsdata->mVaryingVec4[SH_LIGHT_FRAG_POSITION] = trGetMat4(MAT4_LIGHT_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
}

bool ColorPhongShader::fragment(FSInData *fsdata, float color[3])
{
    PhongUniformData *unidata = reinterpret_cast<PhongUniformData *>(trGetUniformData());

    glm::vec3 fragmentPosition = fsdata->getVec3(SH_VIEW_FRAG_POSITION);
    glm::vec3 normal = fsdata->getVec3(SH_NORMAL);
    glm::vec3 diffuseColor = fsdata->getVec3(SH_COLOR);

    normal = glm::normalize(normal);
    // from fragment to light
    glm::vec3 lightDirection = glm::normalize(unidata->mViewLightPosition - fragmentPosition);

    float diff = glm::max(dot(normal, lightDirection), 0.0f);

    // in camera space, eys always in (0.0, 0.0, 0.0), from fragment to eye
    glm::vec3 eyeDirection = glm::normalize(-fragmentPosition);
#if __BLINN_PHONG__
    glm::vec3 halfwayDirection = glm::normalize(lightDirection + eyeDirection);
    float spec = glm::pow(glm::max(dot(normal, halfwayDirection), 0.0f), unidata->mShininess * 2);
#else
    glm::vec3 reflectDirection = glm::reflect(-lightDirection, normal);
    float spec = glm::pow(glm::max(dot(eyeDirection, reflectDirection), 0.0f), unidata->mShininess);
#endif
    glm::vec3 result = ((unidata->mAmbientStrength + diff) * diffuseColor + spec * unidata->mSpecularStrength) * unidata->mLightColor;

    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
    {
        glm::vec4 lightClipV = fsdata->getVec4(SH_LIGHT_FRAG_POSITION);
        lightClipV = (lightClipV / lightClipV.w) * 0.5f + 0.5f;

        result *= calcShadow(lightClipV.z, lightClipV.x, lightClipV.y);
    }

    color[0] = glm::min(result[0], 1.f);
    color[1] = glm::min(result[1], 1.f);
    color[2] = glm::min(result[2], 1.f);

    return true;
}

void ColorPhongShader::getVaryingNum(size_t &v2, size_t &v3, size_t &v4)
{
    v2 = SH_VEC2_BASE_MAX;
    v3 = SH_VEC3_PHONG_MAX;
    v4 = SH_VEC4_PHONG_MAX;
}

void TextureMapPhongShader::vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index)
{
    vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
    vsdata->mVaryingVec3[SH_VIEW_FRAG_POSITION] = trGetMat4(MAT4_MODELVIEW) * glm::vec4(mesh.vertices[index], 1.0f);
    vsdata->mVaryingVec3[SH_NORMAL] = trGetMat3(MAT3_NORMAL) * mesh.normals[index];
    vsdata->mVaryingVec2[SH_TEXCOORD] = mesh.uvs[index];

    PhongUniformData *unidata = reinterpret_cast<PhongUniformData *>(trGetUniformData());

    if (trGetTexture(TEXTURE_NORMAL) != nullptr)
    {
        glm::vec3 N = glm::normalize(vsdata->mVaryingVec3[SH_NORMAL]);
        glm::vec3 T = glm::normalize(trGetMat3(MAT3_NORMAL) * mesh.tangents[index / 3]);
        T = glm::normalize(T - dot(T, N) * N);
        glm::vec3 B = glm::cross(N, T);
        // Mat3 from view space to tangent space
        glm::mat3 TBN = glm::transpose(glm::mat3(T, B, N));
        vsdata->mVaryingVec3[SH_TANGENT_FRAG_POSITION] = TBN * vsdata->mVaryingVec3[SH_VIEW_FRAG_POSITION];
        // Light Position is fixed in view space, but will changed in tangent space
        vsdata->mVaryingVec3[SH_TANGENT_LIGHT_POSITION] = TBN * unidata->mViewLightPosition;
    }

    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
        vsdata->mVaryingVec4[SH_LIGHT_FRAG_POSITION] = trGetMat4(MAT4_LIGHT_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
}

bool TextureMapPhongShader::fragment(FSInData *fsdata, float color[3])
{
    PhongUniformData *unidata = reinterpret_cast<PhongUniformData *>(trGetUniformData());

    glm::vec2 texCoord = fsdata->getVec2(SH_TEXCOORD);
    textureCoordWrap(texCoord);

    glm::vec3 fragmentPosition;
    glm::vec3 normal;
    glm::vec3 lightPosition;

    glm::vec3 diffuseColor = glm::make_vec3(texture2D(TEXTURE_DIFFUSE, texCoord.x, texCoord.y));

    if (trGetTexture(TEXTURE_NORMAL) != nullptr)
    {
        fragmentPosition = fsdata->getVec3(SH_TANGENT_FRAG_POSITION);
        normal = glm::make_vec3(texture2D(TEXTURE_NORMAL, texCoord.x, texCoord.y)) * 2.0f - 1.0f;
        lightPosition = fsdata->getVec3(SH_TANGENT_LIGHT_POSITION);
    } else {
        fragmentPosition = fsdata->getVec3(SH_VIEW_FRAG_POSITION);
        normal = fsdata->getVec3(SH_NORMAL);
        lightPosition = unidata->mViewLightPosition;
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
    float spec = glm::pow(glm::max(dot(normal, halfwayDirection), 0.0f), unidata->mShininess * 2);
#else
    glm::vec3 reflectDirection = glm::reflect(-lightDirection, normal);
    float spec = glm::pow(glm::max(dot(eyeDirection, reflectDirection), 0.0f), unidata->mShininess);
#endif
    glm::vec3 specColor(1.0f);
    if (trGetTexture(TEXTURE_SPECULAR) != nullptr)
        specColor = glm::make_vec3(texture2D(TEXTURE_SPECULAR, texCoord.x, texCoord.y));
    else
        specColor *= unidata->mSpecularStrength;

    glm::vec3 result = ((unidata->mAmbientStrength + diff) * diffuseColor + spec * specColor) * unidata->mLightColor;

    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
    {
        glm::vec4 lightClipV = fsdata->getVec4(SH_LIGHT_FRAG_POSITION);
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

void TextureMapPhongShader::getVaryingNum(size_t &v2, size_t &v3, size_t &v4)
{
    v2 = SH_VEC2_BASE_MAX;
    v3 = SH_VEC3_PHONG_MAX;
    v4 = SH_VEC4_PHONG_MAX;
}

void ShadowMapShader::vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index)
{
    vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
}

bool ShadowMapShader::fragment(FSInData *fsdata, float color[3])
{
    glm::vec4 clipV = fsdata->getPosition();
    float depth = glm::clamp((clipV.z / clipV.w) * 0.5f + 0.5f, 0.0f, 1.0f);
    color[0] = depth;
    color[1] = depth;
    color[2] = depth;

    return true;
}

void ShadowMapShader::getVaryingNum(size_t &v2, size_t &v3, size_t &v4)
{
    v2 = SH_VEC2_BASE_MAX;
    v3 = SH_VEC3_BASE_MAX;
    v4 = SH_VEC4_BASE_MAX;
}
