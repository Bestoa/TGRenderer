#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "tr.h"


// global value
TRBuffer *gCurrentBuffer = NULL;
TRMaterial gCurrentMaterial = { NULL, NULL, NULL, NULL };

glm::mat4 gModelMat = glm::mat4(1.0f);
glm::mat4 gViewMat = glm::mat4(1.0f);
glm::mat4 gProjMat = glm::mat4(1.0f);
glm::mat3 gNormalMat = glm::mat3(1.0f);

float gAmbientStrength = 0.1;
int gShininess = 32;
glm::vec3 gLightColor = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 gLightPosition = glm::vec3(1.0f, 1.0f, 1.0f);

// internal function
static inline void __clear_color__(TRBuffer &buffer)
{
    for (int i = 0; i < buffer.h; i++)
    {
        uint8_t *base = &buffer.data[i * buffer.stride];
        for (int j = 0; j < buffer.w; j++)
        {
            base[j * BPP + 0] = buffer.bg_color[0];
            base[j * BPP + 1] = buffer.bg_color[1];
            base[j * BPP + 2] = buffer.bg_color[2];
        }
    }
}

static inline void __clear_depth__(TRBuffer &buffer)
{
    for (int i = 0; i < buffer.w * buffer.h; i++)
    {
        buffer.depth[i] = 1e6;
    }
}

static inline float __edge__(glm::vec2 &a, glm::vec2 &b, glm::vec2 &c)
{
    return (c.x - a.x)*(b.y - a.y) - (c.y - a.y)*(b.x - a.x);
}

static inline void __convert_xy_to_buffer_size__(glm::vec2 &v, glm::vec4 &v1, int w, int h)
{
    v.x = (w - 1) * (v1.x / 2 + 0.5);
    // inverse y here
    v.y = (h - 1) - (h - 1) * (v1.y / 2 + 0.5);
}

static inline void __safe_div__(float &w1, float w2, int flag)
{
    if (glm::abs(w2) < 1e-6)
    {
        w2 = 1e-6 * flag;
    }
    w1 /= w2;
}

static inline glm::vec3 __texture_color__(float u, float v, TRTexture *texture)
{
    int x = int(u * (texture->w - 1) + 0.5);
    // inverse y here
    int y = int(texture->h - 1 - v * (texture->h - 1) + 0.5);
    // Only support RGB texture
    float *color = texture->data + y * texture->stride + x * 3;
    return glm::vec3(color[0], color[1], color[2]);
}

template <class T> float __interpolation__(T v[3], int i, float w0, float w1, float w2)
{
    return v[0][i] * w0 + v[1][i] * w1 + v[2][i] * w2;
}

static inline void __compute__tangent__(glm::vec4 v[3], glm::vec2 uv[3], glm::vec3 &t)
{
    // Edges of the triangle : postion delta
    glm::vec3 deltaPos1 = v[1]-v[0];
    glm::vec3 deltaPos2 = v[2]-v[0];

    // UV delta
    glm::vec2 deltaUV1 = uv[1]-uv[0];
    glm::vec2 deltaUV2 = uv[2]-uv[0];

    float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
    t = (deltaPos1 * deltaUV2.y   - deltaPos2 * deltaUV1.y) * r;
}

static inline void __compute_normal_mat__()
{
    gNormalMat = glm::mat3(glm::transpose(glm::inverse(gViewMat * gModelMat)));
}

// vertex shader
static void vertex_shader(
        glm::vec3 v,
        glm::vec3 &n/* out, for lighting */,
        glm::vec4 &camera_v/* out, for lighting */,
        glm::vec4 &clip_v/* out, for raster */)
{
    glm::vec4 world_v;

    world_v = gModelMat * glm::vec4(v, 1.0f);
    camera_v = gViewMat * world_v;
    clip_v = gProjMat * camera_v;
    n = gNormalMat * n;
}

// fragment shader
static void fragment_shader(glm::vec3 light_postion, glm::vec3 fragment_postion, glm::vec2 fragment_uv, glm::vec3 normal, glm::mat3 TBN, uint8_t color[3]/* out */)
{
    float u = glm::clamp(fragment_uv.x, 0.0f, 1.0f);
    float v = glm::clamp(fragment_uv.y, 0.0f, 1.0f);

    glm::vec3 object_color = __texture_color__(u, v, gCurrentMaterial.diffuse);

    // assume ambient light is (1.0, 1.0, 1.0)
    glm::vec3 ambient = gAmbientStrength * object_color;

    glm::vec3 n;
    if (gCurrentMaterial.normal != NULL)
    {
        n = __texture_color__(u, v, gCurrentMaterial.normal) * 2.0f - 1.0f;
        n = TBN * n;
    }
    else
    {
        n = normal;
    }
    n = glm::normalize(n);
    // from fragment to light
    glm::vec3 l = glm::normalize(light_postion - fragment_postion);

    float diff = glm::max(dot(n, l), 0.0f);
    glm::vec3 diffuse = diff * gLightColor * object_color;

    glm::vec3 result = ambient + diffuse;

    if (gCurrentMaterial.specular != NULL)
    {
        // in camera space, eys always in (0.0, 0.0, 0.0), from fragment to eye
        glm::vec3 e = glm::normalize(-fragment_postion);
        glm::vec3 r = glm::reflect(-l, n);
        float spec = glm::pow(glm::max(dot(e, r), 0.0f), gShininess);
        result += spec * gLightColor * __texture_color__(u, v, gCurrentMaterial.specular);
    }

    if (gCurrentMaterial.glow != NULL)
    {
        result += __texture_color__(u, v, gCurrentMaterial.glow);
    }

    color[0] = glm::clamp(int(result[0] * 255), 0, 255);
    color[1] = glm::clamp(int(result[1] * 255), 0, 255);
    color[2] = glm::clamp(int(result[2] * 255), 0, 255);
}

// tr api
#define __USE_DEMO_COLOR__ 0
void triangle_pipeline(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
        glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3,
        glm::vec3 normal1, glm::vec3 normal2, glm::vec3 normal3)
{

    glm::vec4 v[3] = { glm::vec4(v1, 1.0f), glm::vec4(v2, 1.0f), glm::vec4(v3, 1.0f) };
    glm::vec2 uv[3] = { uv1, uv2, uv3 };
    glm::vec3 n[3] = { normal1, normal2, normal3 };

    glm::vec4 camera_v[3];
    glm::vec4 clip_v[3];
    glm::vec4 ndc_v[3];
    glm::vec2 screen_v[3];

    glm::vec3 light_postion = gViewMat * glm::vec4(gLightPosition, 1.0f);

    glm::vec3 t;
    __compute__tangent__(v, uv, t);
    t = glm::normalize(gNormalMat * t);

    for (int i = 0; i < 3; i++)
    {
        vertex_shader(v[i], n[i], camera_v[i], clip_v[i]);
        ndc_v[i] = clip_v[i] / clip_v[i].w;
        __convert_xy_to_buffer_size__(screen_v[i], ndc_v[i], gCurrentBuffer->w, gCurrentBuffer->h);
    }

    float area = __edge__(screen_v[0], screen_v[1], screen_v[2]);

    glm::vec2 left_up, right_down;
    left_up.x = glm::max(0.0f, glm::min(glm::min(screen_v[0].x, screen_v[1].x), screen_v[2].x)) + 0.5;
    left_up.y = glm::max(0.0f, glm::min(glm::min(screen_v[0].y, screen_v[1].y), screen_v[2].y)) + 0.5;
    right_down.x = glm::min(float(gCurrentBuffer->w - 1), glm::max(glm::max(screen_v[0].x, screen_v[1].x), screen_v[2].x)) + 0.5;
    right_down.y = glm::min(float(gCurrentBuffer->h - 1), glm::max(glm::max(screen_v[0].y, screen_v[1].y), screen_v[2].y)) + 0.5;

    for (int i = left_up.y; i < right_down.y; i++)
    {
        uint8_t *base = &gCurrentBuffer->data[i * gCurrentBuffer->stride];
        for (int j = left_up.x; j < right_down.x; j++)
        {
            glm::vec2 v(j, i);
            float w0 = __edge__(screen_v[1], screen_v[2], v);
            float w1 = __edge__(screen_v[2], screen_v[0], v);
            float w2 = __edge__(screen_v[0], screen_v[1], v);
            int flag = 0;
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                flag = 1;
            if (w0 <= 0 && w1 <= 0 && w2 <= 0)
                flag = -1;
            if (flag)
            {
                __safe_div__(w0, area, flag);
                __safe_div__(w1, area, flag);
                __safe_div__(w2, area, flag);
                // use ndc z to calculate depth
                float depth = __interpolation__(ndc_v, 2, w0, w1, w2);
                // projection matrix will inverse z-order.
                if (gCurrentBuffer->depth[i * gCurrentBuffer->w + j] < depth) continue;
                // z in ndc of opengl should between -1.0f to 1.0f
                if (depth > 1.0f || depth < -1.0f) continue;
                gCurrentBuffer->depth[i * gCurrentBuffer->w + j] = depth;
#if __USE_DEMO_COLOR__
                float r = w0;
                float g = w1;
                float b = w2;
                base[j * BPP + 0] = r * 255;
                base[j * BPP + 1] = g * 255;
                base[j * BPP + 2] = b * 255;
#else
                glm::vec2 UV(
                        __interpolation__(uv, 0, w0, w1, w2),
                        __interpolation__(uv, 1, w0, w1, w2)
                        );
                glm::vec3 P(
                        __interpolation__(camera_v, 0, w0, w1, w2),
                        __interpolation__(camera_v, 1, w0, w1, w2),
                        __interpolation__(camera_v, 2, w0, w1, w2)
                        );
                glm::vec3 N = glm::normalize(glm::vec3(
                        __interpolation__(n, 0, w0, w1, w2),
                        __interpolation__(n, 1, w0, w1, w2),
                        __interpolation__(n, 2, w0, w1, w2)
                        ));
                // Gram-Schmidt orthogonalize
                glm::vec3 T = glm::normalize(t - dot(t, N) * N);
                glm::vec3 B = glm::cross(N, T);
                glm::mat3 TBN = glm::mat3(T, B, N);
                fragment_shader(light_postion, P, UV, N, TBN, &base[j * BPP]);
#endif
            }

        }
    }

}

void trTriangles(std::vector<glm::vec3> &vertices, std::vector<glm::vec2> &uvs, std::vector<glm::vec3> &normals)
{
    size_t i = 0;
    __compute_normal_mat__();
#pragma omp parallel for
    for (i = 0; i < vertices.size(); i += 3)
    {
        triangle_pipeline(vertices[i], vertices[i+1], vertices[i+2], uvs[i], uvs[i+1], uvs[i+2], normals[i], normals[i+1], normals[i+2]);
    }
}

void trSetAmbientStrength(float v)
{
    gAmbientStrength = v;
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

void trMakeCurrent(TRBuffer &buffer)
{
    gCurrentBuffer = &buffer;
}

void trBindTexture(TRTexture *texture, int type)
{
    switch (type)
    {
        case TEXTURE_DIFFUSE:
            gCurrentMaterial.diffuse = texture;
            break;
        case TEXTURE_SPECULAR:
            gCurrentMaterial.specular = texture;
            break;
        case TEXTURE_GLOW:
            gCurrentMaterial.glow = texture;
            break;
        case TEXTURE_NORMAL:
            gCurrentMaterial.normal = texture;
            break;
        default:
            printf("Unknow texture type!\n");
    }
}

void trClear()
{
    __clear_color__(*gCurrentBuffer);
    __clear_depth__(*gCurrentBuffer);
}

void trSetModelMat(glm::mat4 mat)
{
    gModelMat = mat;
}

void trSetViewMat(glm::mat4 mat)
{
    gViewMat = mat;
}

void trSetProjMat(glm::mat4 mat)
{
    gProjMat = mat;
}

bool trCreateRenderTarget(TRBuffer &buffer, int w, int h)
{
    buffer.data = new uint8_t[w * h * BPP];
    if (!buffer.data)
        abort();
    buffer.depth = new float[w * h];
    if (!buffer.depth)
        abort();
    buffer.w = w;
    buffer.h = h;
    buffer.stride = w * BPP;
    buffer.bg_color[0] = 0;
    buffer.bg_color[1] = 0;
    buffer.bg_color[2] = 0;
    __clear_color__(buffer);
    __clear_depth__(buffer);
    return true;
}

