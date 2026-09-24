// Screen-space ambient occlusion - application level.
// Route B: the AO map modulates only the ambient/indirect terms inside the
// Phong shader (TEXTURE_AO slot), never direct light or emissive glow.
//
// Normals come from a dedicated normal-buffer pass (ViewNormalShader below):
// exact per-pixel view-space normals from the geometry itself. Reconstructing
// normals from depth derivatives was the root cause of the dirty edges: any
// depth silhouette tilts the differenced normal, the tangent frame skews and
// same-surface samples start counting as occluders.
//
// Conventions (matching trcore):
//   - render coords are y-up, screen = (mVW-1)*(ndc/2+0.5)
//   - stored depth = ndc.z/2 + 0.5, depth test passes when stored < incoming,
//     so larger depth value = closer to the camera
#ifndef __CORNELL_SSAO__
#define __CORNELL_SSAO__

#include <vector>
#include <thread>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "buffer.hpp"
#include "texture.hpp"
#include "program.hpp"

namespace TGRenderer
{

static inline void ssaoBlur(std::vector<float> &ao, int w, int h);

// Writes the view-space normal (packed to [0,1]) into the color buffer.
// Render the scene with this into a TRTextureBuffer at window resolution to
// get exact per-pixel normals for the AO pass.
class ViewNormalShader : public TGRenderer::Shader
{
    void vertex(TRMeshData &mesh, VSOutData *vsdata, size_t index) override
    {
        vsdata->tr_Position = trGetMat4(MAT4_MVP) * glm::vec4(mesh.vertices[index], 1.0f);
        vsdata->mVaryingVec3[SH_NORMAL] = trGetMat3(MAT3_NORMAL) * mesh.normals[index];
    }
    bool fragment(FSInData *fsdata, float color[]) override
    {
        glm::vec3 n = glm::normalize(fsdata->getVec3(SH_NORMAL)) * 0.5f + 0.5f;
        color[0] = n.x;
        color[1] = n.y;
        color[2] = n.z;
        return true;
    }
    void getVaryingNum(size_t &v2, size_t &v3, size_t &v4) override
    {
        v2 = SH_VEC2_BASE_MAX;
        v3 = SH_VEC3_BASE_MAX;
        v4 = SH_VEC4_BASE_MAX;
    }
};

// Reconstruct the view-space position of a render-space pixel from depth.
static inline glm::vec3 ssaoViewPos(int px, int py, int w, int h,
                                    TRBuffer *buf, const glm::mat4 &invProj)
{
    float nx = 2.0f * px / (w - 1) - 1.0f;
    float ny = 2.0f * py / (h - 1) - 1.0f;
    float ndcZ = buf->getDepth(buf->getOffset(px, py)) * 2.0f - 1.0f;
    glm::vec4 v = invProj * glm::vec4(nx, ny, ndcZ, 1.0f);
    return glm::vec3(v) / v.w;
}

// Compute AO (0..1, 1 = unoccluded) at window resolution from a 2x-sized
// depth buffer and a window-sized normal buffer. The occlusion itself is
// evaluated at HALF resolution and bilinearly upsampled.
static inline void ssaoCompute(TRBuffer *depthBuf, TRTexture *normalTex,
                               const glm::mat4 &proj,
                               std::vector<float> &ao, int w, int h,
                               float radius, float bias, float intensity,
                               float floor_ = 0.45f)
{
    int sw = depthBuf->getW(), sh = depthBuf->getH();
    int hw = w / 2, hh = h / 2;
    std::vector<float> hao(hw * hh, 1.0f);
    ao.resize(w * h);
    glm::mat4 invProj = glm::inverse(proj);

    // fixed hemisphere kernel (tangent space, z up). No per-pixel rotation:
    // a rotated sparse kernel makes the occluded fraction flip randomly
    // between neighboring pixels along edges (the "caterpillar" banding).
    const int KERNEL = 32;
    glm::vec3 kernel[KERNEL];
    for (int i = 0; i < KERNEL; i++)
    {
        float a = i * 2.399963f;             // golden angle spiral
        float r = (i + 0.5f) / KERNEL;
        kernel[i] = glm::normalize(glm::vec3(cos(a) * r, sin(a) * r,
                                             0.35f + 0.65f * r));
    }

    auto worker = [&](int y0, int y1)
    {
        for (int y = y0; y < y1; y++)      // half-res image rows (top-down)
        {
            int ry = hh - 1 - y;           // render coords are y-up
            for (int x = 0; x < hw; x++)
            {
                // one AO tap per half-res pixel: center of its 4x4 msaa block
                int sx = x * 4 + 2;
                int sy = ry * 4 + 2;
                float d = depthBuf->getDepth(depthBuf->getOffset(sx, sy));
                if (d >= 0.9999f)
                {
                    hao[y * hw + x] = 1.0f;    // background
                    continue;
                }
                glm::vec3 p = ssaoViewPos(sx, sy, sw, sh, depthBuf, invProj);

                // exact view-space normal from the normal buffer, sampled at
                // the SAME render pixel as the depth tap (sx, sy) so a
                // half-res pixel never mixes a depth from one side of an
                // edge with a normal from the other side. The normal buffer
                // is 2x-sized like the depth buffer. NOTE: TRTextureBuffer
                // does not flip Y, its rows are render coords (y-up).
                float *np = normalTex->getColor(sx / (float)(sw - 1),
                                                sy / (float)(sh - 1));
                glm::vec3 n = glm::normalize(glm::make_vec3(np) * 2.0f - 1.0f);

                // tangent frame
                glm::vec3 up = fabsf(n.z) < 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
                glm::vec3 t = glm::normalize(glm::cross(up, n));
                glm::vec3 b = glm::cross(n, t);

                float occ = 0.0f;
                int valid = 0;
                for (int i = 0; i < KERNEL; i++)
                {
                    glm::vec3 s = p + (t * kernel[i].x + b * kernel[i].y
                                       + n * kernel[i].z) * radius;

                    // project the sample point to screen
                    glm::vec4 clip = proj * glm::vec4(s, 1.0f);
                    if (clip.w <= 0.0f)
                        continue;
                    int qx = int((clip.x / clip.w / 2.0f + 0.5f) * (sw - 1));
                    int qy = int((clip.y / clip.w / 2.0f + 0.5f) * (sh - 1));
                    if (qx < 0 || qx >= sw || qy < 0 || qy >= sh)
                        continue;
                    float qd = depthBuf->getDepth(depthBuf->getOffset(qx, qy));
                    if (qd >= 0.9999f)
                        continue;
                    valid++;
                    glm::vec3 qp = ssaoViewPos(qx, qy, sw, sh, depthBuf, invProj);

                    // Occlusion test along the normal: how much higher than
                    // the tangent plane the blocker sits. On a flat surface
                    // this is exactly 0 (exact normals), so flat areas stay
                    // perfectly clean. The weight also scales with the
                    // sample's z (cosine-ish solid angle).
                    glm::vec3 v = qp - p;
                    float nh = glm::dot(v, n);
                    float dist = glm::length(v);
                    if (dist < radius * 2.0f && nh > 0.0f)
                    {
                        float wgt = glm::clamp((nh - 0.012f) / 0.05f, 0.0f, 1.0f)
                                  * kernel[i].z
                                  * (1.0f - glm::clamp(dist / (radius * 2.0f),
                                                       0.0f, 1.0f) * 0.5f);
                        occ += wgt;
                    }
                }
                // normalize by the VALID sample count: dropped samples (off
                // screen / background) are neither occluders nor empty sky
                float val = valid ? 1.0f - intensity * occ / valid * 1.6f : 1.0f;
                hao[y * hw + x] = glm::clamp(val, floor_, 1.0f);
            }
        }
    };

    // half-res bands for the threads
    const int THREADS = 4;
    std::thread th[THREADS];
    int band = (hh + THREADS - 1) / THREADS;
    for (int i = 0; i < THREADS; i++)
    {
        int y0 = i * band;
        int y1 = glm::min(y0 + band, hh);
        th[i] = std::thread(worker, y0, y1);
    }
    for (int i = 0; i < THREADS; i++)
        th[i].join();

    // smooth at half res, then bilinear-upsample to the window resolution
    ssaoBlur(hao, hw, hh);
    ssaoBlur(hao, hw, hh);
    for (int y = 0; y < h; y++)
    {
        float fy = glm::min((y + 0.5f) / 2.0f - 0.5f, hh - 1.0f);
        int y0 = glm::clamp((int)fy, 0, hh - 1);
        int y1 = glm::min(y0 + 1, hh - 1);
        float ty = glm::clamp(fy - y0, 0.0f, 1.0f);
        for (int x = 0; x < w; x++)
        {
            float fx = glm::min((x + 0.5f) / 2.0f - 0.5f, hw - 1.0f);
            int x0 = glm::clamp((int)fx, 0, hw - 1);
            int x1 = glm::min(x0 + 1, hw - 1);
            float tx = glm::clamp(fx - x0, 0.0f, 1.0f);
            float v = (hao[y0 * hw + x0] * (1 - tx) + hao[y0 * hw + x1] * tx) * (1 - ty)
                    + (hao[y1 * hw + x0] * (1 - tx) + hao[y1 * hw + x1] * tx) * ty;
            ao[y * w + x] = glm::clamp(v, 0.0f, 1.0f);
        }
    }
}

// Separable 3x3 blur; invalid samples (-1, silhouette edges) are skipped so
// they get interpolated from valid neighbors instead of punching bright
// holes into dark bands.
static inline void ssaoBlur(std::vector<float> &ao, int w, int h)
{
    std::vector<float> tmp(ao.size());
    auto acc = [&](std::vector<float> &src, std::vector<float> &dstv,
                   int x, int y, int dx, int dy)
    {
        float s = 0;
        int c = 0;
        for (int i = -1; i <= 1; i++)
        {
            int xx = glm::clamp(x + dx * i, 0, w - 1);
            int yy = glm::clamp(y + dy * i, 0, h - 1);
            float v = src[yy * w + xx];
            if (v >= 0.0f)
            {
                s += v;
                c++;
            }
        }
        dstv[y * w + x] = c ? s / c : 1.0f;
    };
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            acc(ao, tmp, x, y, 1, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            acc(tmp, ao, x, y, 0, 1);
}

} // namespace TGRenderer
#endif
