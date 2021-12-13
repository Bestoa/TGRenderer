#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
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

glm::mat4 __gMVMat__ = glm::mat4(1.0f);
glm::mat4 __gMVPMat__ = glm::mat4(1.0f);

bool gEnableLighting = false;
float gAmbientStrength = 0.1;
int gShininess = 32;
glm::vec3 gLightColor = glm::vec3(1.0f, 1.0f, 1.0f);
glm::vec3 gLightPosition = glm::vec3(1.0f, 1.0f, 1.0f);

size_t gThreadNum = 4;
std::mutex gDepthMutex;

// internal function
static inline void __clear_color__(TRBuffer &buffer)
{
    for (uint32_t i = 0; i < buffer.h; i++)
    {
        uint8_t *base = &buffer.data[i * buffer.stride];
        for (uint32_t j = 0; j < buffer.w; j++)
        {
            base[j * BPP + 0] = buffer.bg_color[0];
            base[j * BPP + 1] = buffer.bg_color[1];
            base[j * BPP + 2] = buffer.bg_color[2];
        }
    }
}

static inline void __clear_depth__(TRBuffer &buffer)
{
    for (uint32_t i = 0; i < buffer.w * buffer.h; i++)
    {
        buffer.depth[i] = 1;
    }
}

static inline float __edge__(glm::vec2 &a, glm::vec2 &b, glm::vec2 &c)
{
    return (c.x - a.x)*(b.y - a.y) - (c.y - a.y)*(b.x - a.x);
}

static inline void __tr_viewport__(glm::vec2 &screen_v, glm::vec4 &ndc_v, int vx, int vy, int w, int h)
{
    screen_v.x = vx + (w - 1) * (ndc_v.x / 2 + 0.5);
    // inverse y here
    screen_v.y = vy + (h - 1) - (h - 1) * (ndc_v.y / 2 + 0.5);
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

static inline void __compute_premultiply_mat__()
{
    __gMVMat__ =  gViewMat * gModelMat;
    __gMVPMat__ = gProjMat * __gMVMat__;
    gNormalMat = glm::mat3(glm::transpose(glm::inverse(__gMVMat__)));
}

// lighting vertex shader
static void lighting_vertex_shader(
        glm::vec3 v,
        glm::vec3 &n/* out, for lighting */,
        glm::vec4 &camera_v/* out, for lighting */,
        glm::vec4 &clip_v/* out, for raster */)
{
    camera_v = __gMVMat__ * glm::vec4(v, 1.0f);
    clip_v = gProjMat * camera_v;
    n = gNormalMat * n;
}

// vertex shader
static void vertex_shader(
        glm::vec3 v,
        glm::vec4 &clip_v/* out, for raster */)
{
    clip_v = __gMVPMat__ * glm::vec4(v, 1.0f);
}

// lighting fragment shader
static void lighting_fragment_shader(glm::vec3 light_postion, glm::vec3 fragment_postion, glm::vec2 fragment_uv, glm::vec3 N, glm::vec3 T, uint8_t color[3]/* out */)
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

// fragment shader
static void fragment_shader(glm::vec2 fragment_uv, uint8_t color[3]/* out */)
{
    float u = glm::clamp(fragment_uv.x, 0.0f, 1.0f);
    float v = glm::clamp(fragment_uv.y, 0.0f, 1.0f);

    glm::vec3 object_color = __texture_color__(u, v, gCurrentMaterial.diffuse);

    color[0] = glm::clamp(int(object_color[0]), 0, 255);
    color[1] = glm::clamp(int(object_color[1]), 0, 255);
    color[2] = glm::clamp(int(object_color[2]), 0, 255);
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
    glm::vec4 clip_v[3];
    glm::vec4 ndc_v[3];
    glm::vec2 screen_v[3];

    for (int i = 0; i < 3; i++)
    {
        vertex_shader(v[i], clip_v[i]);
        ndc_v[i] = clip_v[i] / clip_v[i].w;
        __tr_viewport__(screen_v[i], ndc_v[i], gCurrentBuffer->vx, gCurrentBuffer->vy, gCurrentBuffer->vw, gCurrentBuffer->vh);
    }
    __draw_line__(screen_v[0].x, screen_v[0].y, screen_v[1].x, screen_v[1].y);
    __draw_line__(screen_v[1].x, screen_v[1].y, screen_v[2].x, screen_v[2].y);
    __draw_line__(screen_v[2].x, screen_v[2].y, screen_v[0].x, screen_v[0].y);
}

static inline bool __depth_test__(TRBuffer *buffer, int depth_offset, float depth)
{
    /* projection matrix will inverse z-order. */
    if (buffer->depth[depth_offset] < depth)
        return false;
    else
        buffer->depth[depth_offset] = depth;
    return true;
}

void __tr_rasterization__(glm::vec4 clip_v[3], TRBuffer *buffer, std::vector<TRFragData> &frags)
{
    glm::vec4 ndc_v[3];
    glm::vec2 screen_v[3];

    for (int i = 0; i < 3; i++)
    {
        ndc_v[i] = clip_v[i] / clip_v[i].w;
        __tr_viewport__(screen_v[i], ndc_v[i], buffer->vx, buffer->vy, buffer->vw, buffer->vh);
    }
    float area = __edge__(screen_v[0], screen_v[1], screen_v[2]);
    if (area <= 0) return;
    glm::vec2 left_up, right_down;
    left_up.x = glm::max(0.0f, glm::min(glm::min(screen_v[0].x, screen_v[1].x), screen_v[2].x)) + 0.5;
    left_up.y = glm::max(0.0f, glm::min(glm::min(screen_v[0].y, screen_v[1].y), screen_v[2].y)) + 0.5;
    right_down.x = glm::min(float(buffer->w - 1), glm::max(glm::max(screen_v[0].x, screen_v[1].x), screen_v[2].x)) + 0.5;
    right_down.y = glm::min(float(buffer->h - 1), glm::max(glm::max(screen_v[0].y, screen_v[1].y), screen_v[2].y)) + 0.5;
    for (int i = left_up.y; i < right_down.y; i++) {
        int offset_base = i * buffer->w;
        for (int j = left_up.x; j < right_down.x; j++) {
            glm::vec2 v(j, i);
            float w0 = __edge__(screen_v[1], screen_v[2], v);
            float w1 = __edge__(screen_v[2], screen_v[0], v);
            float w2 = __edge__(screen_v[0], screen_v[1], v);
            int flag = 0;
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                flag = 1;
            if (!flag)
                continue;
            w0 /= (area + 1e-6);
            w1 /= (area + 1e-6);
            w2 /= (area + 1e-6);

            /* use ndc z to calculate depth */
            float depth = __interpolation__(ndc_v, 2, w0, w1, w2);
            /* z in ndc of opengl should between 0.0f to 1.0f */
            if (depth > 1.0f || depth < 0.0f)
                continue;

            int offset = offset_base + j;
            /* easy-z */
            /* Do not use mutex here to speed up */
            if (!__depth_test__(buffer, offset, depth))
                continue;

            TRFragData fdat = { j, i, w0, w1, w2, depth, &buffer->data[offset * BPP] };
            frags.push_back(fdat);
        }
    }
}

// Texture with lighting pipeline
void triangle_pipeline(glm::vec4 v[3], glm::vec2 uv[3], glm::vec3 n[3], glm::vec3 light_postion /* light postion in camera space */)
{

    glm::vec4 camera_v[3];
    glm::vec4 clip_v[3];

    glm::vec3 T;
    __compute__tangent__(v, uv, T);
    T = glm::normalize(gNormalMat * T);

    for (int i = 0; i < 3; i++)
        lighting_vertex_shader(v[i], n[i], camera_v[i], clip_v[i]);

    std::vector<TRFragData> frags;
    __tr_rasterization__(clip_v, gCurrentBuffer, frags);

    for (auto &frag : frags)
    {
        float w0 = frag.w[0];
        float w1 = frag.w[1];
        float w2 = frag.w[2];
        int x = frag.x;
        int y = frag.y;

        int offset = y * gCurrentBuffer->w + x;

        glm::vec2 UV(
                __interpolation__(uv, 0, w0, w1, w2),
                __interpolation__(uv, 1, w0, w1, w2)
                );
        glm::vec3 P(
                __interpolation__(camera_v, 0, w0, w1, w2),
                __interpolation__(camera_v, 1, w0, w1, w2),
                __interpolation__(camera_v, 2, w0, w1, w2)
                );
        glm::vec3 N(
                __interpolation__(n, 0, w0, w1, w2),
                __interpolation__(n, 1, w0, w1, w2),
                __interpolation__(n, 2, w0, w1, w2)
                );

        uint8_t c[3];
        lighting_fragment_shader(light_postion, P, UV, N, T, c);

        /* depth test */
        {
            std::lock_guard<std::mutex> lck(gDepthMutex);
            if (!__depth_test__(gCurrentBuffer, offset, frag.depth))
                continue;

            frag.addr[0] = c[0];
            frag.addr[1] = c[1];
            frag.addr[2] = c[2];
        }
    }
}

// Texture without lighting pipeline
void triangle_pipeline(glm::vec4 v[3], glm::vec2 uv[3])
{

    glm::vec4 clip_v[3];

    for (int i = 0; i < 3; i++)
        vertex_shader(v[i], clip_v[i]);

    std::vector<TRFragData> frags;
    __tr_rasterization__(clip_v, gCurrentBuffer, frags);

    for (auto &frag : frags)
    {
        float w0 = frag.w[0];
        float w1 = frag.w[1];
        float w2 = frag.w[2];
        int x = frag.x;
        int y = frag.y;

        int offset = y * gCurrentBuffer->w + x;

        glm::vec2 UV(
                __interpolation__(uv, 0, w0, w1, w2),
                __interpolation__(uv, 1, w0, w1, w2)
                );

        uint8_t c[3];
        fragment_shader(UV, c);
        /* depth test */
        {
            std::lock_guard<std::mutex> lck(gDepthMutex);
            if (!__depth_test__(gCurrentBuffer, offset, frag.depth))
                continue;

            frag.addr[0] = c[0];
            frag.addr[1] = c[1];
            frag.addr[2] = c[2];
        }
    }
}

// Color pipeline
void triangle_pipeline(glm::vec4 v[3], glm::vec3 c[3])
{

    glm::vec4 clip_v[3];

    for (int i = 0; i < 3; i++)
        vertex_shader(v[i], clip_v[i]);

    std::vector<TRFragData> frags;
    __tr_rasterization__(clip_v, gCurrentBuffer, frags);

    for (auto &frag : frags)
    {
        float w0 = frag.w[0];
        float w1 = frag.w[1];
        float w2 = frag.w[2];
        int x = frag.x;
        int y = frag.y;

        int offset = y * gCurrentBuffer->w + x;

        /* depth test */
        {
            std::lock_guard<std::mutex> lck(gDepthMutex);
            if (!__depth_test__(gCurrentBuffer, offset, frag.depth))
                continue;

            frag.addr[0] = int(glm::clamp(__interpolation__(c, 0, w0, w1, w2), 0.0f, 1.0f) * 255 + 0.5);
            frag.addr[1] = int(glm::clamp(__interpolation__(c, 1, w0, w1, w2), 0.0f, 1.0f) * 255 + 0.5);
            frag.addr[2] = int(glm::clamp(__interpolation__(c, 2, w0, w1, w2), 0.0f, 1.0f) * 255 + 0.5);
        }
    }
}

typedef void (*RenderFunc)(TRMeshData &, size_t, size_t);

void tr_triangles_with_texture_index(TRMeshData &data, size_t index, size_t num)
{
    size_t i = 0, j = 0;
    glm::vec3 light_postion = gViewMat * glm::vec4(gLightPosition, 1.0f);
    std::vector<glm::vec3> &vertices = data.vertices;
    std::vector<glm::vec2> &uvs = data.uvs;
    std::vector<glm::vec3> &normals = data.normals;

    for (i = index * 3, j = 0; j < num && i < vertices.size(); i += 3, j++)
    {
        glm::vec4 v[3] = { glm::vec4(vertices[i], 1.0f), glm::vec4(vertices[i+1], 1.0f), glm::vec4(vertices[i+2], 1.0f) };
        glm::vec2 uv[3] = { uvs[i], uvs[i+1], uvs[i+2] };
        glm::vec3 n[3] = { normals[i], normals[i+1], normals[i+2] };
        if (gEnableLighting)
            triangle_pipeline(v, uv, n, light_postion);
        else
            triangle_pipeline(v, uv);
    }
}

void tr_triangles_with_color_index(TRMeshData &data, size_t index, size_t num)
{
    size_t i = 0, j = 0;
    std::vector<glm::vec3> &vertices = data.vertices;
    std::vector<glm::vec3> &colors = data.colors;

    for (i = index * 3, j = 0; j < num && i < vertices.size(); i += 3, j++)
    {
        glm::vec4 v[3] = { glm::vec4(vertices[i], 1.0f), glm::vec4(vertices[i+1], 1.0f), glm::vec4(vertices[i+2], 1.0f) };
        glm::vec3 c[3] = { colors[i], colors[i+1], colors[i+2] };
        triangle_pipeline(v, c);
    }
}

void tr_triangles_with_demo_color_index(TRMeshData &data, size_t index, size_t num)
{
    size_t i = 0, j = 0;
    std::vector<glm::vec3> &vertices = data.vertices;

    for (i = index * 3, j = 0; j < num && i < vertices.size(); i += 3, j++)
    {
        glm::vec4 v[3] = { glm::vec4(vertices[i], 1.0f), glm::vec4(vertices[i+1], 1.0f), glm::vec4(vertices[i+2], 1.0f) };
        glm::vec3 c[3] = { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) };
        triangle_pipeline(v, c);
    }
}

void tr_triangles_wireframe_index(TRMeshData &data, size_t index, size_t num)
{
    size_t i = 0, j = 0;
    std::vector<glm::vec3> &vertices = data.vertices;

    for (i = index * 3, j = 0; j < num && i < vertices.size(); i += 3, j++)
    {
        glm::vec4 v[3] = { glm::vec4(vertices[i], 1.0f), glm::vec4(vertices[i+1], 1.0f), glm::vec4(vertices[i+2], 1.0f) };
        triangle_pipeline(v);
    }
}

void tr_triangles_mt(TRMeshData &data, RenderFunc render)
{
    if (gThreadNum > 1)
    {
        std::vector<std::thread> thread_pool;
        size_t index_step = data.vertices.size() / 3 / gThreadNum + 1;

        for (size_t i = 0; i < gThreadNum; i++)
            thread_pool.push_back(std::thread(render, std::ref(data), i * index_step, index_step));

        for (auto &th : thread_pool)
            if (th.joinable())
                th.join();
    } else {
        render(data, 0, data.vertices.size() / 3);
    }
}


void trTriangles(TRMeshData &data, TRDrawMode mode)
{
    __compute_premultiply_mat__();
    RenderFunc render = nullptr;

    switch (mode)
    {
        case DRAW_WITH_TEXTURE:
            render = tr_triangles_with_texture_index;
            break;
        case DRAW_WITH_COLOR:
            render = tr_triangles_with_color_index;
            break;
        case DRAW_WITH_DEMO_COLOR:
            render = tr_triangles_with_demo_color_index;
            break;
        case DRAW_WIREFRAME:
            render = tr_triangles_wireframe_index;
            break;
    }
    if (render)
        tr_triangles_mt(data, render);
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
    gCurrentBuffer->vx = x;
    gCurrentBuffer->vy = y;
    gCurrentBuffer->vw = w;
    gCurrentBuffer->vh = h;
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

void trClearColor3f(float r, float g, float b)
{
    gCurrentBuffer->bg_color[0] = glm::clamp(int(r * 255 + 0.5), 0, 255);
    gCurrentBuffer->bg_color[1] = glm::clamp(int(g * 255 + 0.5), 0, 255);
    gCurrentBuffer->bg_color[2] = glm::clamp(int(b * 255 + 0.5), 0, 255);
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
    buffer.vx = 0;
    buffer.vy = 0;
    buffer.vw = w;
    buffer.vh = h;
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

