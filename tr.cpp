#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "tr.h"


// global value
TRBuffer *gCurrentBuffer = NULL;
TRTexture *gCurrentTexutre = NULL;

glm::mat4 gModelMat = glm::mat4(1.0f);
glm::mat4 gViewMat = glm::mat4(1.0f);
glm::mat4 gProjMat = glm::mat4(1.0f);
glm::mat3 gNormalMat = glm::mat3(1.0f);

float gAmbientStrength = 0.1;
float gSpecularStrength = 0.5;
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

// fragment shader
static void fragment_shader(glm::vec3 frag_pos, float u, float v, glm::vec3 normal, uint8_t color[3]/* out */)
{
    u = glm::clamp(u, 0.0f, 1.0f);
    v = glm::clamp(v, 0.0f, 1.0f);
    int texture_x = int(u * (gCurrentTexutre->w - 1) + 0.5);
    // inverse y here
    int texture_y = int(gCurrentTexutre->h - 1 - v * (gCurrentTexutre->h - 1) + 0.5);

    uint8_t *texture_color = gCurrentTexutre->diffuse + texture_y * gCurrentTexutre->stride + texture_x * 3;

    glm::vec3 object_color = glm::vec3(texture_color[0], texture_color[1], texture_color[2]);
    glm::vec3 ambient = gAmbientStrength * gLightColor;

    glm::vec3 camera_light_position = gViewMat * glm::vec4(gLightPosition, 1.0f);
    glm::vec3 n_normal = glm::normalize(normal);
    glm::vec3 n_light_dir = glm::normalize(camera_light_position - frag_pos);

    float diff = glm::max(dot(n_normal, n_light_dir), 0.0f);
    glm::vec3 diffuse = diff * gLightColor;

    // in camera space, camera always in (0, 0, 0)
    glm::vec3 n_view_dir = glm::normalize(-frag_pos);
    glm::vec3 reflect_dir = glm::reflect(-n_light_dir, n_normal);
    float spec = glm::pow(glm::max(dot(n_view_dir, reflect_dir), 0.0f), gShininess);
    glm::vec3 specular = gSpecularStrength * spec * gLightColor;

    glm::vec3 result = (ambient + diffuse + specular) * object_color;

    color[0] = glm::clamp(int(result[0]), 0, 255);
    color[1] = glm::clamp(int(result[1]), 0, 255);
    color[2] = glm::clamp(int(result[2]), 0, 255);
}

// vertex shader
static void vertex_shader(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
        glm::vec4 camera_v[3]/* out, for fragment lighting */, glm::vec4 clip_v[3]/* out, for raster */)
{
    glm::vec4 world_v[3];

    world_v[0] = gModelMat * glm::vec4(v1, 1.0f);
    world_v[1] = gModelMat * glm::vec4(v2, 1.0f);
    world_v[2] = gModelMat * glm::vec4(v3, 1.0f);

    for (int i = 0; i < 3; i++)
    {
        camera_v[i] = gViewMat * world_v[i];
        clip_v[i] = gProjMat * camera_v[i];
    }

}

// tr api
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

void trMakeCurrent(TRBuffer &buffer)
{
    gCurrentBuffer = &buffer;
}

void trBindTexture(TRTexture &texture)
{
    gCurrentTexutre = &texture;
}

void trClear()
{
    __clear_color__(*gCurrentBuffer);
    __clear_depth__(*gCurrentBuffer);
}

void trUpdateNormalMat()
{
    gNormalMat = glm::mat3(glm::transpose(glm::inverse(gViewMat * gModelMat)));
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

#define __USE_DEMO_COLOR__ 0

void trTriangle(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
        glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3,
        glm::vec3 normal1, glm::vec3 normal2, glm::vec3 normal3)
{
    glm::vec4 camera_v[3];
    glm::vec4 clip_v[3];
    glm::vec4 ndc_v[3];
    glm::vec2 screen_v[3];

    vertex_shader(v1, v2, v3, camera_v, clip_v);

    for (int i = 0; i < 3; i++)
    {
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
                float depth = (w0 * ndc_v[0].z + w1 * ndc_v[1].z + w2 * ndc_v[2].z);
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
                float u = w0 * uv1.x + w1 * uv2.x + w2 * uv3.x;
                float v = w0 * uv1.y + w1 * uv2.y + w2 * uv3.y;
                glm::vec3 frag_pos(
                        camera_v[0].x * w0 + camera_v[1].x * w1 + camera_v[2].x * w2,
                        camera_v[0].y * w0 + camera_v[1].y * w1 + camera_v[2].y * w2,
                        camera_v[0].z * w0 + camera_v[1].z * w1 + camera_v[2].z * w2
                        );
                glm::vec3 frag_normal = gNormalMat * glm::vec3(
                        normal1.x * w0 + normal2.x * w1 + normal3.x * w2,
                        normal1.y * w0 + normal2.y * w1 + normal3.y * w2,
                        normal1.z * w0 + normal2.z * w1 + normal3.z * w2
                        );
                fragment_shader(frag_pos, u, v, frag_normal, &base[j * BPP]);
#endif
            }

        }
    }

}

void trTriangles(std::vector<glm::vec3> &vertices, std::vector<glm::vec2> &uvs, std::vector<glm::vec3> &normals)
{
    size_t i = 0;
#pragma omp parallel for
    for (i = 0; i < vertices.size(); i += 3)
    {
        trTriangle(vertices[i], vertices[i+1], vertices[i+2], uvs[i], uvs[i+1], uvs[i+2], normals[i], normals[i+1], normals[i+2]);
    }
}

