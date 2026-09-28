// Whitted-style ray-scene intersection shader for the two balls in the
// Cornell box demo. Replaces the probe-cube direction sampling for the balls:
// the reflected / refracted ray is intersected with the actual scene
// geometry (room box, panel light, two boxes, the other ball), so the balls
// see each other and the boxes with correct parallax - the single-point
// probe environment map can not do that (direction-only, no position).
//
// Scene contents (hard-coded to match the demo):
//   room box  x,y,z in [-1,1], open front (z=+1 shows the clear color)
//   albedos:  floor white, back wall white, left red, right green, ceiling gray
//   panel light at y=0.99 (emissive)
//   two boxes (OBB, white, rotated +/-20 deg about Y)
//   two balls (analytic spheres): the glass ball and the metal ball
#ifndef __CORNELL_RAY_SHADER__
#define __CORNELL_RAY_SHADER__

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "trapi.hpp"
#include "program.hpp"
#include "cornell_scene.hpp"

namespace TGRenderer
{

struct RayHit
{
    float t = 1e30f;
    glm::vec3 albedo = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    float emissive = 0.0f;      // 1 for the panel light
    int ballIndex = 0;          // 1 = glass ball, 2 = metal ball, 0 = other
    bool valid = false;
};

class CornellRayShader : public TGRenderer::Shader
{
public:
    // scene description
    glm::vec4 mBallGlass = cs::kGlassBall;     // single source: cornell_scene.hpp
    glm::vec4 mBallMirror = cs::kMirrorBall;
    glm::vec3 mLightPos = cs::kLightPos;
    glm::vec3 mLightColor = glm::vec3(1.45f);
    float mAmbient = 0.45f;
    float mAtten = 1.0f;
    float mIOR = 1.3f;
    int mSelfIndex = 0;          // which ball is being shaded (skip self)
    glm::vec3 mEyeWorld = glm::vec3(0.0f);
    // shadow map for the direct light (the metal ball's lower half reflects
    // the floor in its own shadow - it must come out dark, not lit)
    TRTexture *mShadowMap = nullptr;
    glm::mat4 mLightVP = glm::mat4(1.0f);
    float mShadowFactor = 0.2f;

    void vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index) override
    {
        vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
        vsdata->mVaryingVec3[SH_WORLD_FRAG_POSITION] = trGetMat4(MAT4_MODEL) * glm::vec4(mesh.vertices[index], 1.0f);
        vsdata->mVaryingVec3[SH_WORLD_NORMAL] = glm::mat3(trGetMat4(MAT4_MODEL)) * mesh.normals[index];
    }

    // ---- intersection helpers ----
    void hitPlane(const glm::vec3 &P, const glm::vec3 &D, float planeCoord, int axis,
                  float nSign, const glm::vec3 &albedo, RayHit &hit, float emissive = 0.0f,
                  float uMin = -2, float uMax = 2, float vMin = -2, float vMax = 2)
    {
        float d = D[axis];
        if (glm::abs(d) < 1e-6f)
            return;
        float t = (planeCoord - P[axis]) / d;
        if (t <= 1e-3f || t >= hit.t)
            return;
        glm::vec3 p = P + t * D;
        int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
        if (p[a1] < uMin || p[a1] > uMax || p[a2] < vMin || p[a2] > vMax)
            return;
        hit.t = t;
        hit.albedo = albedo;
        hit.emissive = emissive;
        hit.normal = glm::vec3(0.0f);
        hit.normal[axis] = nSign;
        hit.valid = true;
    }

    void hitSphere(const glm::vec3 &P, const glm::vec3 &D, const glm::vec4 &sph,
                   int ballIndex, RayHit &hit)
    {
        glm::vec3 oc = P - glm::vec3(sph);
        float b = glm::dot(oc, D);
        float c = glm::dot(oc, oc) - sph.w * sph.w;
        float disc = b * b - c;
        if (disc < 0.0f)
            return;
        float t = -b - sqrtf(disc);
        if (t <= 1e-3f || t >= hit.t)
            return;
        hit.t = t;
        hit.albedo = glm::vec3(0.0f);
        hit.emissive = 0.0f;
        hit.ballIndex = ballIndex;
        hit.normal = glm::normalize(P + t * D - glm::vec3(sph));
        hit.valid = true;
    }

    void hitOBB(const glm::vec3 &P, const glm::vec3 &D, const glm::vec3 &center,
                const glm::vec3 &half, float rotYdeg, const glm::vec3 &albedo, RayHit &hit)
    {
        // to box local space (rotate about Y by -rotYdeg)
        float a = glm::radians(-rotYdeg);
        float ca = cosf(a), sa = sinf(a);
        glm::mat3 R(ca, 0, sa, 0, 1, 0, -sa, 0, ca);
        glm::vec3 p = R * (P - center);
        glm::vec3 d = R * D;
        glm::vec3 t1 = (-half - p) / d;
        glm::vec3 t2 = (half - p) / d;
        glm::vec3 tmin = glm::min(t1, t2), tmax = glm::max(t1, t2);
        float tn = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
        float tf = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
        if (tn > tf || tf <= 1e-3f || tn >= hit.t)
            return;
        float t = tn > 1e-3f ? tn : tf;
        if (t <= 1e-3f || t >= hit.t)
            return;
        // local normal
        glm::vec3 lp = p + t * d;
        glm::vec3 ln(0.0f);
        int axis = 0;
        float m = 0;
        for (int i = 0; i < 3; i++)
            if (glm::abs(lp[i] / half[i]) > m) { m = glm::abs(lp[i] / half[i]); axis = i; }
        ln[axis] = lp[axis] > 0 ? 1.0f : -1.0f;
        hit.t = t;
        hit.albedo = albedo;
        hit.emissive = 0.0f;
        hit.normal = glm::normalize(glm::transpose(R) * ln);
        hit.valid = true;
    }

    RayHit trace(const glm::vec3 &P, const glm::vec3 &D)
    {
        RayHit hit;
        const glm::vec3 &white = cs::kWhite, &red = cs::kRed,
                        &green = cs::kGreen, &gray = cs::kGray;
        // room: floor, ceiling, back, left, right (front open)
        hitPlane(P, D, cs::kFloorY, 1, 1.0f, white, hit);              // floor
        hitPlane(P, D, cs::kCeilY, 1, -1.0f, gray, hit);               // ceiling
        hitPlane(P, D, cs::kPanelY, 1, -1.0f, white, hit, 1.0f,        // panel light
                 -cs::kPanelHalf, cs::kPanelHalf, -cs::kPanelHalf, cs::kPanelHalf);
        hitPlane(P, D, -cs::kRoomHalf, 2, 1.0f, white, hit);           // back wall
        hitPlane(P, D, -cs::kRoomHalf, 0, 1.0f, red, hit);             // left (red)
        hitPlane(P, D, cs::kRoomHalf, 0, -1.0f, green, hit);           // right (green)
        // boxes (OBB, white): tall left, short right
        hitOBB(P, D, cs::kTallCenter, cs::kTallHalf, cs::kTallRot, white, hit);
        hitOBB(P, D, cs::kShortCenter, cs::kShortHalf, cs::kShortRot, white, hit);
        // the other ball
        if (mSelfIndex != 1)
            hitSphere(P, D, mBallGlass, 1, hit);
        if (mSelfIndex != 2)
            hitSphere(P, D, mBallMirror, 2, hit);
        return hit;
    }

    // transmittance of the glass ball for the segment P -> light: the ball is
    // excluded from the raster shadow map (it is not an opaque occluder), but
    // physically it still shades and focuses the light it blocks — a soft dim
    // penumbra with a bright transmitted core (caustic) on the floor, like the
    // path tracer's transmission shadow. Used by directLight (all traced views).
    float glassTransmittance(const glm::vec3 &P, float &causticCore) const
    {
        causticCore = 0.0f;
        if (mSelfIndex == 1) return 1.0f;   // shading the glass ball itself
        glm::vec3 L = mLightPos - P;
        float dL = glm::length(L);
        L /= dL;
        glm::vec3 oc = glm::vec3(mBallGlass) - P;
        float s = glm::clamp(glm::dot(oc, L), 0.0f, dL);
        if (s <= 0.0f || s >= dL) return 1.0f;   // ball not between P and light
        float h = glm::length(oc - s * L);
        float r = mBallGlass.w;
        float penumbra = 1.0f - glm::smoothstep(r * 0.85f, r * 1.35f, h); // 1 blocked
        if (penumbra <= 0.0f) return 1.0f;
        causticCore = glm::smoothstep(r * 0.55f, r * 0.05f, h) * penumbra; // focus
        return 1.0f - 0.45f * penumbra;    // translucent: never fully black
    }

    // direct light at a scene point (simplified Phong, matches the demo light)
    glm::vec3 directLight(const glm::vec3 &P, const glm::vec3 &N, const glm::vec3 &albedo)
    {
        glm::vec3 L = mLightPos - P;
        float d = glm::length(L);
        L /= d;
        float diff = glm::max(glm::dot(N, L), 0.0f);
        diff /= 1.0f + mAtten * d * d;
        // glass ball transmission shadow + bright caustic core
        float core;
        float trans = glassTransmittance(P, core);
        diff *= trans;
        diff += core * 1.1f * glm::max(glm::dot(N, L), 0.0f);
        // shadow: darkens the direct diffuse only (ambient untouched - the
        // ambient-times-shadow variant made the boxes look unevenly lit).
        // Metal ball only: through the glass ball the shadowed floor under
        // the metal ball showed up as black patches (era-22 had no shadow
        // here at all); the metal ball needs it for its own floor shadow.
        if (mShadowMap != nullptr && mSelfIndex == 2)
        {
            glm::vec4 lp = mLightVP * glm::vec4(P, 1.0f);
            glm::vec3 ndc = glm::vec3(lp) / lp.w * 0.5f + 0.5f;
            if (ndc.x >= 0.0f && ndc.x <= 1.0f && ndc.y >= 0.0f && ndc.y <= 1.0f)
            {
                float depth = mShadowMap->getColor(ndc.x, ndc.y)[0];
                if (ndc.z > depth + 0.001f)
                    diff *= mShadowFactor;
            }
        }
        return (mAmbient + diff) * albedo * mLightColor;
    }

    // full shading of a ray: trace, then shade the hit. Balls recurse one
    // level (depth 0 -> 1): the glass ball transmits, the metal ball
    // reflects, so they appear as glass/metal in each other's reflections
    // instead of plain Phong spheres.
    // analytic double refraction through a sphere (rt-pathtracer semantics):
    // refract in at P, follow the chord, refract out at the far side.
    // Exit TIR: bounce once inside and leave through the opposite side (the
    // rim is reflection dominated anyway). Returns false only if even the
    // bounce fails to exit (grazing); caller falls back to plain reflection.
    // The analytic entry normal (P - center) is used for the optics so the
    // in/out geometry is exactly self-consistent (no mesh-normal noise).
    bool refractThroughBall(const glm::vec3 &P, const glm::vec3 &Vdir, const glm::vec4 &sph,
                            glm::vec3 &exitP, glm::vec3 &exitD, float &cosEntry) const
    {
        glm::vec3 c3(sph);
        glm::vec3 N = glm::normalize(P - c3);
        cosEntry = glm::clamp(-glm::dot(Vdir, N), 0.0f, 1.0f);
        glm::vec3 inner = glm::refract(Vdir, N, 1.0f / mIOR);
        if (glm::dot(inner, inner) < 1e-8f) return false;
        glm::vec3 oc = P - c3;
        float b = glm::dot(oc, inner);
        float c2 = glm::dot(oc, oc) - sph.w * sph.w;
        float disc = b * b - c2;
        if (disc <= 0.0f) return false;
        float chord = -b + glm::sqrt(disc);
        glm::vec3 exitPos = P + chord * inner;
        glm::vec3 exitN = glm::normalize(c3 - exitPos);   // inward
        glm::vec3 out = glm::refract(inner, exitN, mIOR);
        if (glm::dot(out, out) < 1e-8f) {
            glm::vec3 bounced = glm::reflect(inner, exitN);
            glm::vec3 oc2 = exitPos - c3;
            float b2 = glm::dot(oc2, bounced);
            float cb = glm::dot(oc2, oc2) - sph.w * sph.w;
            float d2 = b2 * b2 - cb;
            if (d2 > 0.0f) {
                glm::vec3 exit2 = exitPos + (-b2 + glm::sqrt(d2)) * bounced;
                glm::vec3 n2 = glm::normalize(c3 - exit2);
                glm::vec3 out2 = glm::refract(bounced, n2, mIOR);
                if (glm::dot(out2, out2) > 1e-8f) {
                    exitP = exit2;
                    exitD = out2;
                    return true;
                }
            }
            return false;
        }
        exitP = exitPos;
        exitD = out;
        return true;
    }

    glm::vec3 traceShade(const glm::vec3 &P, const glm::vec3 &D, int depth)
    {
        RayHit hit = trace(P, D);
        if (!hit.valid)
            return glm::vec3(0.02f);
        glm::vec3 hp = P + hit.t * D;
        if (hit.emissive > 0.0f)
            // HDR-ish panel emission: the direct view clamps to white either
            // way, but fresnel-scaled reflections of it (4% on glass) stay
            // visible — the path tracer keeps this sparkle because its light
            // is HDR + tonemapped; a flat 1.0 makes glass read as transparent
            return glm::vec3(6.0f);
        // ball hits recurse once to look like glass / metal
        if (depth == 0 && hit.ballIndex == 1)
        {
            // glass ball seen in a reflection: REAL double refraction through
            // the analytic sphere (same optics as the primary shading), plus a
            // fresnel reflection. (The old fixed-length 2r straight-through
            // jump made the ball a near-invisible ghost and its continuation
            // ray could start below the floor plane -> visible seam.)
            glm::vec3 exitP, exitD, through(0.02f);
            float cosE;
            if (refractThroughBall(hp, D, mBallGlass, exitP, exitD, cosE))
                through = traceShade(exitP + exitD * 1e-3f, exitD, 1);
            else
                through = traceShade(hp + hit.normal * 1e-3f, glm::reflect(D, hit.normal), 1);
            glm::vec3 R = glm::reflect(D, hit.normal);
            glm::vec3 refl = traceShade(hp + hit.normal * 1e-3f, R, 1);
            float f0 = glm::pow((mIOR - 1.0f) / (mIOR + 1.0f), 2.0f);
            float fr = f0 + (1.0f - f0) * glm::pow(1.0f - cosE, 5.0f);
            return glm::mix(through, refl, glm::clamp(fr, 0.0f, 1.0f));
        }
        if (depth == 0 && hit.ballIndex == 2)
        {
            // metal ball: mirror reflection of the environment
            glm::vec3 R = glm::reflect(D, hit.normal);
            return traceShade(hp + hit.normal * 1e-3f, R, 1);
        }
        return directLight(hp, hit.normal, hit.albedo);
    }

    bool fragment(FSInData *fsdata, float color[]) override
    {
    glm::vec3 P = fsdata->getVec3(SH_WORLD_FRAG_POSITION);
        glm::vec3 N = glm::normalize(fsdata->getVec3(SH_WORLD_NORMAL));
        glm::vec3 V = glm::normalize(P - mEyeWorld);   // incident ray

        // reflection (recurses through the other ball as glass/metal)
        glm::vec3 R = glm::reflect(V, N);
        glm::vec3 reflColor(0.0f);   // MUST init: the metal path accumulates
                                     // into this with +=, an uninitialized
                                     // vec3 leaks garbage (the purple tint)
        if (mIOR > 1.0f)
        {
            // glass keeps a sharp reflection (polished surface)
            reflColor = traceShade(P + N * 1e-3f, R, 0);
        }
        else
        {
            // metal: glossy reflection - a few jittered samples in a small
            // cone around the mirror direction (micro-surface roughness)
            const int SAMPLES = 4;
            float rough = 0.045f;
            // tangent frame around R
            glm::vec3 up = fabsf(R.z) < 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
            glm::vec3 t = glm::normalize(glm::cross(up, R));
            glm::vec3 b = glm::cross(R, t);
            float hash = glm::fract(glm::sin(glm::dot(P, glm::vec3(12.9898f, 78.233f, 45.164f))) * 43758.5453f);
            for (int i = 0; i < SAMPLES; i++)
            {
                float a = (i + hash) * 2.399963f;          // golden angle
                float r = rough * sqrtf((i + 0.5f) / SAMPLES);
                glm::vec3 dir = glm::normalize(R + (t * cosf(a) + b * sinf(a)) * r);
                reflColor += traceShade(P + N * 1e-3f, dir, 0);
            }
            reflColor /= (float)SAMPLES;
        }

        float fresnel = 1.0f;
        glm::vec3 result = reflColor;
        if (mIOR > 1.0f)
        {
            // glass: double refraction through the analytic sphere (shared
            // helper — identical optics in primary and reflected views),
            // fresnel mix. TIR at the exit bounces once inside the ball.
            glm::vec4 sph = mSelfIndex == 1 ? mBallGlass : mBallMirror;
            glm::vec3 refrColor(0.02f);
            glm::vec3 exitP, exitD;
            float cosE;
            if (refractThroughBall(P, V, sph, exitP, exitD, cosE))
                refrColor = traceShade(exitP + exitD * 1e-3f, exitD, 0);
            // fresnel (Schlick, air-side entry angle from the analytic normal)
            float f0 = glm::pow((mIOR - 1.0f) / (mIOR + 1.0f), 2.0f);
            fresnel = f0 + (1.0f - f0) * glm::pow(1.0f - cosE, 5.0f);
            fresnel = glm::clamp(fresnel, 0.0f, 1.0f);
            result = glm::mix(refrColor, reflColor, fresnel);
        }
        else
        {
            // metal: conductors reflect strongly at ALL angles (F0 ~ 0.9+,
            // unlike dielectrics whose fresnel only grows at grazing). A flat
            // high-F0 fresnel and NO diffuse base - a metal's "color" is its
            // reflection, there is no matte body underneath.
            float cosT = glm::clamp(glm::dot(-V, N), 0.0f, 1.0f);
            float F0 = 0.92f;
            fresnel = F0 + (1.0f - F0) * glm::pow(1.0f - cosT, 5.0f);
            result = reflColor * fresnel;
            // contact AO: the lower hemisphere is occluded by the floor it
            // sits on (SSAO-like darkening toward the bottom contact)
            float ao = glm::mix(0.25f, 1.0f, glm::smoothstep(-1.0f, 0.5f, N.y));
            result *= ao;
        }

        for (int i = 0; i < 3; i++)
            color[i] = glm::min(result[i], 1.0f);
        color[3] = 1.0f;
        return true;
    }

    void getVaryingNum(size_t &v2, size_t &v3, size_t &v4) override
    {
        v2 = SH_VEC2_BASE_MAX;
        v3 = SH_VEC3_PHONG_MAX;
        v4 = SH_VEC4_BASE_MAX;
    }
};

} // namespace TGRenderer
#endif
