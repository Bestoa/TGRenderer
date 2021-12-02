#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "tr.h"

// global value
TRBuffer *gCurrentBuffer = NULL;
TRMaterial gCurrentMaterial = { NULL, NULL, NULL, NULL };

glm::mat4 gModelMat = glm::mat4(1.0f);
glm::mat4 gViewMat = glm::mat4(1.0f);
// defult proj mat should swap z-order
glm::mat4 gProjMat = glm::mat4({1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
glm::mat3 gNormalMat = glm::mat3(1.0f);

float gAmbientStrength = 0.1;
int gShininess = 32;
glm::vec3 gLightColor = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 gLightPosition = glm::vec3(1.0f, 1.0f, 1.0f);

glm::vec2 empty_uvs[3];
glm::vec3 empty_normals[3];

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
        buffer.depth[i] = 1;
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
    uint8_t *color = texture->data + y * texture->stride + x * 3;
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
static void fragment_shader(glm::vec3 light_postion, glm::vec3 fragment_postion, glm::vec2 fragment_uv, glm::vec3 N, glm::vec3 T, uint8_t color[3]/* out */)
{
    float u = glm::clamp(fragment_uv.x, 0.0f, 1.0f);
    float v = glm::clamp(fragment_uv.y, 0.0f, 1.0f);

    glm::vec3 object_color = __texture_color__(u, v, gCurrentMaterial.diffuse);

    // assume ambient light is (1.0, 1.0, 1.0)
    glm::vec3 ambient = gAmbientStrength * object_color;

    glm::vec3 n;
    if (gCurrentMaterial.normal != NULL)
    {
        // Gram-Schmidt orthogonalize
        T = glm::normalize(T - dot(T, N) * N);
        glm::vec3 B = glm::cross(N, T);
        glm::mat3 TBN = glm::mat3(T, B, N);
        n = __texture_color__(u, v, gCurrentMaterial.normal) * 0.007843137f - 1.0f;
        n = TBN * n;
    }
    else
    {
        n = N;
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
#if __BLINN_PHONG__
        glm::vec3 h = glm::normalize(l + e);
        float spec = glm::pow(glm::max(dot(n, h), 0.0f), gShininess);
#else
        glm::vec3 r = glm::reflect(-l, n);
        float spec = glm::pow(glm::max(dot(e, r), 0.0f), gShininess);
#endif
        result += spec * gLightColor * __texture_color__(u, v, gCurrentMaterial.specular);
    }

    if (gCurrentMaterial.glow != NULL)
    {
        result += __texture_color__(u, v, gCurrentMaterial.glow);
    }

    color[0] = glm::clamp(int(result[0]), 0, 255);
    color[1] = glm::clamp(int(result[1]), 0, 255);
    color[2] = glm::clamp(int(result[2]), 0, 255);
}

void __plot__(int x, int y)
{
    uint8_t *base = &gCurrentBuffer->data[y * gCurrentBuffer->stride + x * BPP];
    base[0] = base[1] = base[2] = 255;
}

void __draw_line__(float x0, float y0, float x1, float y1)
{
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
        if (steep)
            __plot__(y, x);
        else
            __plot__(x, y);
        error += deltaerr;
        if (error >= 0.5)
        {
            y += ystep;
            error -= 1.0;
        }
    }
}

// tr api
// Wireframe pipeline
void triangle_pipeline(glm::vec4 v[3])
{
    glm::vec4 camera_v[3];
    glm::vec3 n[3];
    glm::vec4 clip_v[3];
    glm::vec4 ndc_v[3];
    glm::vec2 screen_v[3];

    for (int i = 0; i < 3; i++)
    {
        vertex_shader(v[i], n[i], camera_v[i], clip_v[i]);
        ndc_v[i] = clip_v[i] / clip_v[i].w;
        __convert_xy_to_buffer_size__(screen_v[i], ndc_v[i], gCurrentBuffer->w, gCurrentBuffer->h);
    }
    __draw_line__(screen_v[0].x, screen_v[0].y, screen_v[1].x, screen_v[1].y);
    __draw_line__(screen_v[1].x, screen_v[1].y, screen_v[2].x, screen_v[2].y);
    __draw_line__(screen_v[2].x, screen_v[2].y, screen_v[0].x, screen_v[0].y);
}
// Lighting pipeline
void triangle_pipeline(glm::vec4 v[3], glm::vec2 uv[3], glm::vec3 n[3], glm::vec3 light_postion /* light postion in camera space */)
{

    glm::vec4 camera_v[3];
    glm::vec4 clip_v[3];
    glm::vec4 ndc_v[3];
    glm::vec2 screen_v[3];

    glm::vec3 T;
    __compute__tangent__(v, uv, T);
    T = glm::normalize(gNormalMat * T);

    for (int i = 0; i < 3; i++)
    {
        vertex_shader(v[i], n[i], camera_v[i], clip_v[i]);
        ndc_v[i] = clip_v[i] / clip_v[i].w;
        __convert_xy_to_buffer_size__(screen_v[i], ndc_v[i], gCurrentBuffer->w, gCurrentBuffer->h);
    }

    float area = __edge__(screen_v[0], screen_v[1], screen_v[2]);
#if __CULL_FACE__
    if (area <= 0)
        return;
#endif

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
#if !__CULL_FACE__
            if (w0 <= 0 && w1 <= 0 && w2 <= 0)
                flag = -1;
#endif
            if (!flag)
                continue;

            __safe_div__(w0, area, flag);
            __safe_div__(w1, area, flag);
            __safe_div__(w2, area, flag);
            // use ndc z to calculate depth
            float depth = __interpolation__(ndc_v, 2, w0, w1, w2);
            // projection matrix will inverse z-order.
            if (gCurrentBuffer->depth[i * gCurrentBuffer->w + j] < depth) continue;
            // z in ndc of opengl should between 0.0f to 1.0f
            if (depth > 1.0f || depth < 0.0f) continue;
            gCurrentBuffer->depth[i * gCurrentBuffer->w + j] = depth;

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
            fragment_shader(light_postion, P, UV, N, T, &base[j * BPP]);
        }
    }

}

// Color pipeline
void triangle_pipeline(glm::vec4 v[3], glm::vec3 c[3])
{

    glm::vec4 clip_v[3];
    glm::vec4 ndc_v[3];
    glm::vec2 screen_v[3];

    for (int i = 0; i < 3; i++)
    {
        clip_v[i] = gProjMat * gViewMat * gModelMat * v[i];
        ndc_v[i] = clip_v[i] / clip_v[i].w;
        __convert_xy_to_buffer_size__(screen_v[i], ndc_v[i], gCurrentBuffer->w, gCurrentBuffer->h);
    }

    float area = __edge__(screen_v[0], screen_v[1], screen_v[2]);
#if __CULL_FACE__
    if (area <= 0)
        return;
#endif

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
#if !__CULL_FACE__
            if (w0 <= 0 && w1 <= 0 && w2 <= 0)
                flag = -1;
#endif
            if (!flag)
                continue;

            __safe_div__(w0, area, flag);
            __safe_div__(w1, area, flag);
            __safe_div__(w2, area, flag);
            // use ndc z to calculate depth
            float depth = __interpolation__(ndc_v, 2, w0, w1, w2);
            // projection matrix will inverse z-order.
            if (gCurrentBuffer->depth[i * gCurrentBuffer->w + j] < depth) continue;
            // z in ndc of opengl should between 0.0f to 1.0f
            if (depth > 1.0f || depth < 0.0f) continue;
            gCurrentBuffer->depth[i * gCurrentBuffer->w + j] = depth;

            base[j * BPP + 0] = int(glm::clamp(__interpolation__(c, 0, w0, w1, w2), 0.0f, 1.0f) * 255 + 0.5);
            base[j * BPP + 1] = int(glm::clamp(__interpolation__(c, 1, w0, w1, w2), 0.0f, 1.0f) * 255 + 0.5);
            base[j * BPP + 2] = int(glm::clamp(__interpolation__(c, 2, w0, w1, w2), 0.0f, 1.0f) * 255 + 0.5);
        }
    }
}

void tr_triangles_with_texture(std::vector<glm::vec3> &vertices, std::vector<glm::vec2> &uvs, std::vector<glm::vec3> &normals)
{
    size_t i = 0;
    __compute_normal_mat__();
    glm::vec3 light_postion = gViewMat * glm::vec4(gLightPosition, 1.0f);

#pragma omp parallel for
    for (i = 0; i < vertices.size(); i += 3)
    {
        glm::vec4 v[3] = { glm::vec4(vertices[i], 1.0f), glm::vec4(vertices[i+1], 1.0f), glm::vec4(vertices[i+2], 1.0f) };
        glm::vec2 uv[3] = { uvs[i], uvs[i+1], uvs[i+2] };
        glm::vec3 n[3] = { normals[i], normals[i+1], normals[i+2] };
        triangle_pipeline(v, uv, n, light_postion);
    }
}

void tr_triangles_with_color(std::vector<glm::vec3> &vertices, std::vector<glm::vec3> &colors)
{
    size_t i = 0;
#pragma omp parallel for
    for (i = 0; i < vertices.size(); i += 3)
    {
        glm::vec4 v[3] = { glm::vec4(vertices[i], 1.0f), glm::vec4(vertices[i+1], 1.0f), glm::vec4(vertices[i+2], 1.0f) };
        glm::vec3 c[3] = { colors[i], colors[i+1], colors[i+2] };
        triangle_pipeline(v, c);
    }
}

void tr_triangles_with_demo_color(std::vector<glm::vec3> &vertices)
{
    size_t i = 0;
#pragma omp parallel for
    for (i = 0; i < vertices.size(); i += 3)
    {
        glm::vec4 v[3] = { glm::vec4(vertices[i], 1.0f), glm::vec4(vertices[i+1], 1.0f), glm::vec4(vertices[i+2], 1.0f) };
        glm::vec3 c[3] = { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) };
        triangle_pipeline(v, c);
    }
}

void tr_triangles_wireframe(std::vector<glm::vec3> &vertices)
{
    size_t i = 0;
#pragma omp parallel for
    for (i = 0; i < vertices.size(); i += 3)
    {
        glm::vec4 v[3] = { glm::vec4(vertices[i], 1.0f), glm::vec4(vertices[i+1], 1.0f), glm::vec4(vertices[i+2], 1.0f) };
        triangle_pipeline(v);
    }
}

void trTriangles(TRData &data, TRDrawMode mode)
{
    switch (mode)
    {
        case DRAW_WITH_TEXTURE:
            tr_triangles_with_texture(data.vertices, data.uvs, data.normals);
            break;
        case DRAW_WITH_COLOR:
            tr_triangles_with_color(data.vertices, data.colors);
            break;
        case DRAW_WITH_DEMO_COLOR:
            tr_triangles_with_demo_color(data.vertices);
            break;
        case DRAW_WIREFRAME:
            tr_triangles_wireframe(data.vertices);
            break;
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

void trBindTexture(TRTexture *texture, TRTextureType type)
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

bool trCreateRenderTarget(TRBuffer &buffer, int w, int h, bool has_ext_buffer)
{
    buffer.data = nullptr;
    buffer.ext_buffer = has_ext_buffer;
    if (!has_ext_buffer)
    {
        buffer.data = new uint8_t[w * h * BPP];
        if (!buffer.data)
            return false;
    }
    buffer.depth = new float[w * h];
    if (!buffer.depth)
    {
        if (buffer.data)
            delete buffer.data;
        return false;
    }
    buffer.w = w;
    buffer.h = h;
    buffer.stride = w * BPP;
    buffer.bg_color[0] = 0;
    buffer.bg_color[1] = 0;
    buffer.bg_color[2] = 0;
    if (!has_ext_buffer)
        __clear_color__(buffer);
    __clear_depth__(buffer);
    return true;
}

void trSetExtBufferToRenderTarget(TRBuffer &buffer, void *addr)
{
    buffer.data = (uint8_t *)addr;
}

void trDestoryRenderTarget(TRBuffer &buffer)
{
    // ext buffer was not managered by us.
    if (!buffer.ext_buffer && buffer.data)
        delete buffer.data;

    if (buffer.depth)
        delete buffer.depth;
}

