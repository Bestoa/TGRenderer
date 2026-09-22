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
    TRTexture *texture = trGetTexture(type);
    if (texture == nullptr)
        return nullptr;
    return texture->getColor(u, v);
}

float *textureCube(const glm::vec3 &dir)
{
    TRCubeTexture *cubeTexture = trGetCubeTexture();
    if (cubeTexture == nullptr)
        return nullptr;
    return cubeTexture->sample(dir);
}

// Base reflectance of common dielectrics (glass, water, plastic) at normal
// incidence, used by the glass shading when mFresnelFactor is not set.
static constexpr float GLASS_F0 = 0.04f;

/* Double refraction through an arbitrary mesh (glmark2 recipe), tier 2.
 *
 * The back faces of the object are pre-rendered from the CURRENT view into
 * two textures by the caller (main.cpp):
 *   TEXTURE_BACK_NORMAL: back face normals, view space, packed to [0,1]
 *   TEXTURE_BACK_DEPTH : distance along the view axis (positive, view units)
 *
 * Per fragment (all in view space, where the maps live):
 * 1. refract the view ray into the glass (air -> glass)
 * 2. project a point one unit along the inner ray to find where THIS fragment
 *    would land on screen, and look up the back depth there
 * 3. chord length = backDepth / (-inner.z)  [unit conversion: the depth map
 *    stores view-axis distance, the ray direction is normalized]
 * 4. back position = fragPos + inner * chord; back normal from the normal map
 * 5. refract out of the glass (glass -> air)
 *
 * Returns false on any lookup failure; the caller falls back.
 */
static bool refractThroughMesh(const glm::vec3 &viewEyeDir, const glm::vec3 &viewNormal,
                               const glm::vec3 &viewFragPos, const glm::mat4 &projMat,
                               PhongUniformData *unidata,
                               glm::vec3 &exitOrigin, glm::vec3 &exitDirection)
{
    TRTexture *normalMap = trGetTexture(TEXTURE_BACK_NORMAL);
    TRTexture *depthMap = trGetTexture(TEXTURE_BACK_DEPTH);
    if (normalMap == nullptr || depthMap == nullptr)
        return false;


    // single refraction (air -> glass), in view space (the maps live there)
    glm::vec3 inner = glm::refract(viewEyeDir, viewNormal, 1.0f / unidata->mIOR);
    if (glm::dot(inner, inner) < 1e-8f)
        return false;

    /* Project the fragment position advanced one unit along the inner ray:
     * the screen offset between this probe and the fragment itself gives the
     * local magnification, which converts a screen-space depth delta into a
     * chord length without needing the exact projection math here. */
    glm::vec4 clipF = projMat * glm::vec4(viewFragPos, 1.0f);
    glm::vec4 clipP = projMat * glm::vec4(viewFragPos + inner, 1.0f);
    if (clipF.w <= 1e-4f || clipP.w <= 1e-4f)
        return false;
    glm::vec2 ndcF(clipF.x / clipF.w, clipF.y / clipF.w);
    glm::vec2 ndcP(clipP.x / clipP.w, clipP.y / clipP.w);
    glm::vec2 uvF(ndcF.x * 0.5f + 0.5f, ndcF.y * 0.5f + 0.5f);
    glm::vec2 dirScreen(ndcP.x - ndcF.x, ndcP.y - ndcF.y);

    // depth map stores the view-axis distance (-z, positive) of the BACK
    // surface behind this fragment. The inner ray has view-axis component
    // -inner.z, so the chord until the back surface is:
    float *d0 = depthMap->getColor(glm::clamp(uvF.x, 0.0f, 1.0f), glm::clamp(uvF.y, 0.0f, 1.0f));
    float backDepth = d0[0];
    if (backDepth <= 0.0f)
        return false;
    float viewZStep = -inner.z;
    if (viewZStep < 1e-4f)
        return false;
    float chord = backDepth / viewZStep;
    if (chord <= 0.0f || chord > 50.0f)
        return false;

    // back position and its screen lookup for the normal
    glm::vec3 backPos = viewFragPos + inner * chord;
    glm::vec4 clipB = projMat * glm::vec4(backPos, 1.0f);
    if (clipB.w <= 1e-4f)
        return false;
    glm::vec2 uvB(clipB.x / clipB.w * 0.5f + 0.5f, clipB.y / clipB.w * 0.5f + 0.5f);
    float *bn = normalMap->getColor(glm::clamp(uvB.x, 0.0f, 1.0f), glm::clamp(uvB.y, 0.0f, 1.0f));
    glm::vec3 backNormal = glm::normalize(glm::make_vec3(bn) * 2.0f - 1.0f);

    // refract out (glass -> air)
    glm::vec3 outDir = glm::refract(inner, -backNormal, unidata->mIOR);
    if (glm::dot(outDir, outDir) < 1e-8f)
    {
        // total internal reflection at the exit: mirror reflection fallback,
        // rim pixels are reflection dominated anyway
        exitOrigin = viewFragPos;
        exitDirection = glm::reflect(viewEyeDir, viewNormal);
        return true;
    }
    exitOrigin = backPos;
    exitDirection = outDir;
    return true;
}

/* Double refraction through an analytic sphere (glass ball).
 *
 * 1. refract the view ray into the glass at the fragment (air -> glass)
 * 2. find where the inner ray exits the sphere (ray/sphere, closed form)
 * 3. refract out of the glass there (glass -> air)
 *
 * Total internal reflection at the exit (grazing inner rays) falls back to
 * the outer mirror reflection direction: those rim pixels are dominated by
 * the reflection term anyway.
 */
static void refractThroughSphere(const glm::vec3 &eyeDirection, const glm::vec3 &worldNormal,
                                 const glm::vec3 &fragPos, PhongUniformData *unidata,
                                 glm::vec3 &exitOrigin, glm::vec3 &exitDirection)
{
    const glm::vec3 center(unidata->mRefractionSphere);
    const float radius = unidata->mRefractionSphere.w;

    exitOrigin = fragPos;

    // single refraction (air -> glass), zero vector means TIR at entry,
    // which can not happen for eta < 1 entering a denser medium, but stay safe
    glm::vec3 inner = glm::refract(eyeDirection, worldNormal, 1.0f / unidata->mIOR);
    if (glm::dot(inner, inner) < 1e-8f)
    {
        exitDirection = glm::reflect(eyeDirection, worldNormal);
        return;
    }

    if (radius <= 0.0f)
    {
        // no analytic sphere given, single refraction only (tier 1)
        exitDirection = inner;
        return;
    }

    // exit point Q: second intersection of (fragPos, inner) with the sphere
    glm::vec3 oc = fragPos - center;
    float b = glm::dot(oc, inner);
    float c = glm::dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc <= 0.0f)
    {
        // fragment not on the given sphere, single refraction fallback
        exitDirection = inner;
        return;
    }
    // fragPos is on the sphere, so one root is ~0: take the far root
    float chord = -b + glm::sqrt(disc);
    glm::vec3 exitPos = fragPos + chord * inner;
    glm::vec3 exitNormal = glm::normalize(exitPos - center);

    // refract out (glass -> air), eta = IOR
    glm::vec3 exitDir = glm::refract(inner, -exitNormal, unidata->mIOR);
    if (glm::dot(exitDir, exitDir) < 1e-8f)
    {
        // total internal reflection at the exit: reflect inside once more
        // and leave the sphere through the opposite side (approximation,
        // rim pixels are reflection dominated anyway)
        glm::vec3 bounced = glm::reflect(inner, -exitNormal);
        glm::vec3 oc2 = exitPos - center;
        float b2 = glm::dot(oc2, bounced);
        float c2 = glm::dot(oc2, oc2) - radius * radius;
        float disc2 = b2 * b2 - c2;
        if (disc2 > 0.0f)
        {
            glm::vec3 exit2 = exitPos + (-b2 + glm::sqrt(disc2)) * bounced;
            glm::vec3 n2 = glm::normalize(exit2 - center);
            glm::vec3 out2 = glm::refract(bounced, -n2, unidata->mIOR);
            if (glm::dot(out2, out2) > 1e-8f)
            {
                exitOrigin = exit2;
                exitDirection = out2;
                return;
            }
        }
        exitDirection = glm::reflect(eyeDirection, worldNormal);
        return;
    }
    exitOrigin = exitPos;
    exitDirection = exitDir;
}

float calcShadowFast(float depth, float x, float y)
{
    if (x <= 1.0f && x >= 0.0f && y <= 1.0f && y >= 0.0f
            && (depth > *(texture2D(TEXTURE_SHADOWMAP, x, y)) + ShadowMapShader::BIAS))
        return ShadowMapShader::FACTOR;
    else
        return 1.0f;
}

float calcShadowPCF(float depth, float x, float y)
{
    TRTexture *st = trGetTexture(TEXTURE_SHADOWMAP);
    float xstep = st->getXStep();
    float ystep = st->getYStep();
    float shadow = 0.f;

    float texcoords[][3] =
    {
        {x - xstep, y - ystep, 1.0f}, {x, y - ystep, 2.0f}, {x + xstep, y - ystep, 1.0f},
        {x - xstep, y, 2.0f}, {x, y, 4.0f}, {x + xstep, y, 2.0f},
        {x - xstep, y + ystep, 1.0f}, {x, y + ystep, 2.0f}, {x + xstep, y + ystep, 1.0f},
    };
    for (size_t i = 0; i < 9; i++)
        shadow += calcShadowFast(depth, texcoords[i][0], texcoords[i][1]) * texcoords[i][2];

    return shadow / 16.0f;
}

void ColorShader::vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index)
{
    vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
    vsdata->mVaryingVec3[SH_COLOR] = mesh.colors[index];
}

bool ColorShader::fragment(FSInData *fsdata, float color[])
{
    glm::vec3 C = fsdata->getVec3(SH_COLOR);
    for (int i = 0; i < 3; i++)
        color[i] = C[i];

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
    vsdata->mVaryingVec2[SH_TEXCOORD] = mesh.texcoords[index];
}

bool TextureMapShader::fragment(FSInData *fsdata, float color[])
{
    glm::vec2 texCoord = fsdata->getVec2(SH_TEXCOORD);
    textureCoordWrap(texCoord);

    float *C = texture2D(TEXTURE_DIFFUSE, texCoord.x, texCoord.y);
    for (int i = 0; i < 3; i++)
        color[i] = C[i];

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

bool ColorPhongShader::fragment(FSInData *fsdata, float color[])
{
    PhongUniformData *unidata = reinterpret_cast<PhongUniformData *>(trGetUniformData());

    glm::vec3 fragmentPosition = fsdata->getVec3(SH_VIEW_FRAG_POSITION);
    glm::vec3 normal = fsdata->getVec3(SH_NORMAL);
    glm::vec3 diffuseColor = fsdata->getVec3(SH_COLOR);

    normal = glm::normalize(normal);
    // from fragment to light
    glm::vec3 lightDirection = glm::normalize(unidata->mViewLightPosition - fragmentPosition);

    float diff = glm::max(glm::dot(normal, lightDirection), 0.0f);

    // in camera space, eys always in (0.0, 0.0, 0.0), from fragment to eye
    glm::vec3 eyeDirection = glm::normalize(-fragmentPosition);
#if __BLINN_PHONG__
    glm::vec3 halfwayDirection = glm::normalize(lightDirection + eyeDirection);
    float spec = glm::pow(glm::max(glm::dot(normal, halfwayDirection), 0.0f), unidata->mShininess * 2);
#else
    glm::vec3 reflectDirection = glm::reflect(-lightDirection, normal);
    float spec = glm::pow(glm::max(glm::dot(eyeDirection, reflectDirection), 0.0f), unidata->mShininess);
#endif
    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
    {
        glm::vec4 lightClipV = fsdata->getVec4(SH_LIGHT_FRAG_POSITION);
        lightClipV = (lightClipV / lightClipV.w) * 0.5f + 0.5f;
        float shadow = calcShadowPCF(lightClipV.z, lightClipV.x, lightClipV.y);
        diff *= shadow;
        spec *= shadow;
    }
    glm::vec3 result = ((unidata->mAmbientStrength + diff) * diffuseColor + spec * unidata->mSpecularStrength) * unidata->mLightColor;
    for (int i = 0; i < 3; i++)
        color[i] = glm::min(result[i], 1.f);

    return true;
}

void ColorPhongShader::getVaryingNum(size_t &v2, size_t &v3, size_t &v4)
{
    v2 = SH_VEC2_BASE_MAX;
    v3 = SH_VEC3_COLOR_PHONG_MAX;
    v4 = SH_VEC4_PHONG_MAX;
}

void TextureMapPhongShader::vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index)
{
    vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
    vsdata->mVaryingVec3[SH_VIEW_FRAG_POSITION] = trGetMat4(MAT4_MODELVIEW) * glm::vec4(mesh.vertices[index], 1.0f);
    vsdata->mVaryingVec3[SH_NORMAL] = trGetMat3(MAT3_NORMAL) * mesh.normals[index];
    // World space data for environment reflection in fragment(), the cube
    // texture (skybox) is defined in world space.
    vsdata->mVaryingVec3[SH_WORLD_FRAG_POSITION] = trGetMat4(MAT4_MODEL) * glm::vec4(mesh.vertices[index], 1.0f);
    // mat3(MODEL) is a proper normal matrix for rotation/uniform scale,
    // non-uniform scale would need the inverse transpose here.
    vsdata->mVaryingVec3[SH_WORLD_NORMAL] = glm::mat3(trGetMat4(MAT4_MODEL)) * mesh.normals[index];
    vsdata->mVaryingVec2[SH_TEXCOORD] = mesh.texcoords[index];

    PhongUniformData *unidata = reinterpret_cast<PhongUniformData *>(trGetUniformData());

    if (trGetTexture(TEXTURE_NORMAL) != nullptr)
    {
        glm::vec3 N = glm::normalize(vsdata->mVaryingVec3[SH_NORMAL]);
        glm::vec3 T = glm::normalize(trGetMat3(MAT3_NORMAL) * mesh.tangents[index / 3]);
        T = glm::normalize(T - glm::dot(T, N) * N);
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

bool TextureMapPhongShader::fragment(FSInData *fsdata, float color[])
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

    float diff = glm::max(glm::dot(normal, lightDirection), 0.0f);

    // in camera space, eys always in (0.0, 0.0, 0.0), from fragment to eye
    // even in tangent space, eys still in (0, 0, 0)
    glm::vec3 eyeDirection = glm::normalize(-fragmentPosition);
#if __BLINN_PHONG__
    glm::vec3 halfwayDirection = glm::normalize(lightDirection + eyeDirection);
    float spec = glm::pow(glm::max(glm::dot(normal, halfwayDirection), 0.0f), unidata->mShininess * 2);
#else
    glm::vec3 reflectDirection = glm::reflect(-lightDirection, normal);
    float spec = glm::pow(glm::max(glm::dot(eyeDirection, reflectDirection), 0.0f), unidata->mShininess);
#endif
    glm::vec3 specColor(1.0f);
    if (trGetTexture(TEXTURE_SPECULAR) != nullptr)
        specColor = glm::make_vec3(texture2D(TEXTURE_SPECULAR, texCoord.x, texCoord.y));
    else
        specColor *= unidata->mSpecularStrength;

    float shadow = 1.0f;
    if (trGetTexture(TEXTURE_SHADOWMAP) != nullptr)
    {
        glm::vec4 lightClipV = fsdata->getVec4(SH_LIGHT_FRAG_POSITION);
        lightClipV = (lightClipV / lightClipV.w) * 0.5f + 0.5f;
        shadow = calcShadowPCF(lightClipV.z, lightClipV.x, lightClipV.y);
        diff *= shadow;
        spec *= shadow;
    }

    glm::vec3 result = ((unidata->mAmbientStrength + diff) * diffuseColor + spec * specColor) * unidata->mLightColor;

    if (trGetCubeTexture() != nullptr)
    {
        // Environment reflection. The cube texture is defined in world space,
        // so the reflection direction must be computed in world space.
        glm::vec3 worldNormal = glm::normalize(fsdata->getVec3(SH_WORLD_NORMAL));
        // Note: with a normal map bound, lighting uses the perturbed tangent
        // space normal, but reflection uses the geometric world normal here.
        // Transforming the perturbed normal back to world space would need
        // world space TBN varyings.
        // from eye to fragment
        glm::vec3 eyeDirection = glm::normalize(fsdata->getVec3(SH_WORLD_FRAG_POSITION) - unidata->mEyeWorldPosition);
        glm::vec3 reflectDirection = glm::reflect(eyeDirection, worldNormal);
        float *envColor = textureCube(reflectDirection);
        if (envColor != nullptr)
        {
            // Schlick fresnel, used both for the reflection weight and for
            // the glass split between reflection and refraction below.
            // eyeDirection points from the eye to the fragment, so
            // 1 + dot(eyeDirection, worldNormal) is 0 head-on and 1 at
            // grazing angles. Clamp to 0: float rounding can make it
            // slightly negative head-on and pow(negative, fractional
            // power) would produce NaN colors.
            float fresnelBase = unidata->mFresnelFactor > 0.0f ? unidata->mFresnelFactor : GLASS_F0;
            float fresnel = fresnelBase
                + (1.0f - fresnelBase)
                * glm::pow(glm::max(0.0f, 1.0f + glm::dot(eyeDirection, worldNormal)), unidata->mFresnelPower);

            if (unidata->mIOR > 1.0f)
            {
                // Glass shading: the Phong body is meaningless for glass,
                // split the fragment between the reflected environment and
                // the (doubly) refracted environment by fresnel.
                bool haveExit = false;
                glm::vec3 exitOrigin, exitDirection;
                if (trGetTexture(TEXTURE_BACK_NORMAL) != nullptr
                        && trGetTexture(TEXTURE_BACK_DEPTH) != nullptr)
                {
                    // mesh refraction (glmark2 recipe): the back maps were
                    // rendered from the current view, everything runs in
                    // view space, the exit ray goes back to world space
                    glm::vec3 viewNormal = glm::normalize(fsdata->getVec3(SH_NORMAL));
                    glm::vec3 viewFragPos = fsdata->getVec3(SH_VIEW_FRAG_POSITION);
                    glm::vec3 viewEyeDir = glm::normalize(viewFragPos);
                    glm::mat3 viewMat3(trGetMat4(MAT4_VIEW));
                    glm::mat3 invView = glm::transpose(viewMat3);
                    if (refractThroughMesh(viewEyeDir, viewNormal, viewFragPos,
                                           trGetMat4(MAT4_PROJ), unidata,
                                           exitOrigin, exitDirection))
                    {
                        exitOrigin = invView * exitOrigin + unidata->mEyeWorldPosition;
                        exitDirection = invView * exitDirection;
                        haveExit = true;
                    }
                }
                else
                {
                    // analytic sphere refraction (works when mRefractionSphere
                    // describes the object)
                    refractThroughSphere(eyeDirection, worldNormal,
                                         fsdata->getVec3(SH_WORLD_FRAG_POSITION), unidata,
                                         exitOrigin, exitDirection);
                    haveExit = true;
                }

                if (haveExit)
                {
                    glm::vec3 refrColor(-1.0f);
                    // Sample the transmitted scene the way the exit ray
                    // really travels: analytically intersect the floor plane
                    // (near geometry) or sample the skybox cube (at infinity,
                    // direction sampling is exact).
                    if (exitDirection.y < -1e-4f && unidata->mRefractionFloorY < exitOrigin.y)
                    {
                        float t = (exitOrigin.y - unidata->mRefractionFloorY) / (-exitDirection.y);
                        glm::vec3 hit = exitOrigin + t * exitDirection;
                        TRTexture *floorTex = trGetTexture(TEXTURE_REFRACTION);
                        // Only inside the actual floor extent: rays escaping
                        // beyond it show the far field (skybox below-horizon),
                        // like a real table ending before the horizon
                        float half = unidata->mRefractionFloorExtent;
                        if (floorTex != nullptr
                                && glm::abs(hit.x) <= half && glm::abs(hit.z) <= half)
                        {
                            // same mapping as the visible floor mesh: uv spans
                            // [0, width) across [-extent, +extent], one tile is
                            // mRefractionFloorSize wide. Keep it positive before
                            // the wrap, negative fmod mirrors the texture.
                            float s = unidata->mRefractionFloorSize;
                            float u = (hit.x + half) / s;
                            float v = (hit.z + half) / s;
                            glm::vec2 c(u, v);
                            textureCoordWrap(c);
                            refrColor = glm::make_vec3(floorTex->getColor(c.x, c.y));
                        }
                    }
                    if (refrColor.x < 0.0f)
                    {
                        // sky (or no floor texture): skybox cube is exact here
                        float *sky = textureCube(exitDirection);
                        if (sky != nullptr)
                            refrColor = glm::make_vec3(sky);
                    }

                    if (refrColor.x >= 0.0f)
                    {
                        // fresnel is the physical reflection ratio here,
                        // mReflectivity still acts as the global dial
                        float reflWeight = glm::clamp(fresnel * glm::clamp(
                                    unidata->mReflectivity * 1.6f, 0.0f, 1.0f), 0.0f, 1.0f);
                        glm::vec3 glass = glm::mix(refrColor, glm::make_vec3(envColor), reflWeight);
                        // keep the emissive add consistent with the other paths
                        if (trGetTexture(TEXTURE_GLOW) != nullptr)
                            glass += glm::make_vec3(texture2D(TEXTURE_GLOW, texCoord.x, texCoord.y));
                        result = glass;
                        for (int i = 0; i < 3; i++)
                            color[i] = glm::min(result[i], 1.f);
                        color[3] = unidata->mOpacity;
                        return true;
                    }
                    // refraction sampling failed, fall through to plain reflection
                }
            }

            float reflectivity = unidata->mReflectivity;
            if (unidata->mFresnelFactor > 0.0f)
                reflectivity = glm::clamp(reflectivity * fresnel, 0.0f, 1.0f);
            // A mirror in shadow still reflects physically, but attenuating
            // the reflection by the shadow factor looks much more natural
            // (unlit faces no longer glow with a strong mirror image).
            if (unidata->mReflectShadowMod)
                reflectivity *= shadow;
            result = glm::mix(result, glm::make_vec3(envColor), reflectivity);
        }
    }

    if (trGetTexture(TEXTURE_GLOW) != nullptr)
        result += glm::make_vec3(texture2D(TEXTURE_GLOW, texCoord.x, texCoord.y));
    for (int i = 0; i < 3; i++)
        color[i] = glm::min(result[i], 1.f);
    color[3] = unidata->mOpacity;

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

bool ShadowMapShader::fragment(FSInData *fsdata, float color[])
{
    glm::vec4 clipV = fsdata->getPosition();
    float depth = glm::clamp((clipV.z / clipV.w) * 0.5f + 0.5f, 0.0f, 1.0f);
    for (int i = 0; i < 3; i++)
        color[i] = depth;

    return true;
}

void ShadowMapShader::getVaryingNum(size_t &v2, size_t &v3, size_t &v4)
{
    v2 = SH_VEC2_BASE_MAX;
    v3 = SH_VEC3_BASE_MAX;
    v4 = SH_VEC4_BASE_MAX;
}

void BackNormalShader::vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index)
{
    vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
    vsdata->mVaryingVec3[SH_NORMAL] = trGetMat3(MAT3_NORMAL) * mesh.normals[index];
}

bool BackNormalShader::fragment(FSInData *fsdata, float color[])
{
    glm::vec3 N = glm::normalize(fsdata->getVec3(SH_NORMAL));
    // view space normals packed to [0,1]
    for (int i = 0; i < 3; i++)
        color[i] = N[i] * 0.5f + 0.5f;

    return true;
}

void BackNormalShader::getVaryingNum(size_t &v2, size_t &v3, size_t &v4)
{
    v2 = SH_VEC2_BASE_MAX;
    v3 = SH_VEC3_BASE_MAX;
    v4 = SH_VEC4_BASE_MAX;
}

void BackDepthShader::vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index)
{
    vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
    // view space position: the fragment outputs the depth along the view axis
    vsdata->mVaryingVec3[SH_VIEW_FRAG_POSITION] = trGetMat4(MAT4_MODELVIEW) * glm::vec4(mesh.vertices[index], 1.0f);
}

bool BackDepthShader::fragment(FSInData *fsdata, float color[])
{
    // view space distance along the view axis (our view space convention
    // has the eye at the origin looking down -Z, so -z is the depth)
    glm::vec3 viewV = fsdata->getVec3(SH_VIEW_FRAG_POSITION);
    float dist = glm::max(0.0f, -viewV.z);
    for (int i = 0; i < 3; i++)
        color[i] = dist;

    return true;
}

void BackDepthShader::getVaryingNum(size_t &v2, size_t &v3, size_t &v4)
{
    v2 = SH_VEC2_BASE_MAX;
    v3 = SH_VEC3_PHONG_MAX;
    v4 = SH_VEC4_BASE_MAX;
}
