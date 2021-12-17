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

static inline void __compute_tangent__(glm::vec3 v[3], glm::vec2 uv[3], glm::vec3 &t)
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

void compute_tangent(TRMeshData &data)
{
    std::vector<glm::vec3> &vertices = data.vertices;
    std::vector<glm::vec2> &uvs = data.uvs;
    for (size_t i = 0; i < vertices.size(); i += 3)
    {
        glm::vec3 v[3] = { vertices[i], vertices[i+1], vertices[i+2] };
        glm::vec2 uv[3] = { uvs[i], uvs[i+1], uvs[i+2] };
        glm::vec3 t;
        __compute_tangent__(v, uv, t);
        data.tangents.push_back(t);
    }
}

static inline void __compute_premultiply_mat__()
{
    __gMVMat__ =  gViewMat * gModelMat;
    __gMVPMat__ = gProjMat * __gMVMat__;
    gNormalMat = glm::mat3(glm::transpose(glm::inverse(__gMVMat__)));
}

void __plot__(TRBuffer *buffer, int x, int y)
{
    uint8_t *base = &buffer->data[y * buffer->stride + x * BPP];
    base[0] = base[1] = base[2] = 255;
}

void __draw_line__(TRBuffer *buffer, float x0, float y0, float x1, float y1)
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
            __plot__(buffer, y, x);
        else
            __plot__(buffer, x, y);
        error += deltaerr;
        if (error >= 0.5)
        {
            y += ystep;
            error -= 1.0;
        }
    }
}

// tr api
void WireframeProgram::vertexData(TRMeshData &mesh, size_t index)
{
    for (int i = 0; i < 3; i++)
    {
        vsdata[i].mV = mesh.vertices[i + index];
    }
}

void WireframeProgram::vertex(size_t i, glm::vec4 &clipV)
{
    (void)i;
    clipV = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

bool WireframeProgram::geometry()
{
    glm::vec4 clip_v[3];
    glm::vec4 ndc_v[3];
    glm::vec2 screen_v[3];

    for (int i = 0; i < 3; i++)
    {
        clip_v[i] = __gMVPMat__ * glm::vec4(vsdata[i].mV, 1.0f);
        ndc_v[i] = clip_v[i] / clip_v[i].w;
        mBuffer->viewport(screen_v[i], ndc_v[i]);
    }
    __draw_line__(mBuffer, screen_v[0].x, screen_v[0].y, screen_v[1].x, screen_v[1].y);
    __draw_line__(mBuffer, screen_v[1].x, screen_v[1].y, screen_v[2].x, screen_v[2].y);
    __draw_line__(mBuffer, screen_v[2].x, screen_v[2].y, screen_v[0].x, screen_v[0].y);

    return false;
}

bool WireframeProgram::fragment(float w0, float w1, float w2, uint8_t color[3])
{
    w0 = w1 = w2 = 0;
    (void)w0;
    color[0] = color[1] = color[2] = 0;
    return false;
}

void ColorProgram::vertexData(TRMeshData &mesh, size_t index)
{
    for (int i = 0; i < 3; i++)
    {
        vsdata[i].mV = mesh.vertices[i + index];
        vsdata[i].mColor = mesh.colors[i + index];
    }
}

void ColorProgram::vertex(size_t i, glm::vec4 &clipV)
{
    clipV = __gMVPMat__ * glm::vec4(vsdata[i].mV, 1.0f);
}

bool ColorProgram::geometry()
{
    fsdata.mColor[0] = vsdata[0].mColor;
    fsdata.mColor[1] = vsdata[1].mColor;
    fsdata.mColor[2] = vsdata[2].mColor;
    return true;
}

bool ColorProgram::fragment(float w0, float w1, float w2, uint8_t color[3])
{
    color[0] = int(glm::clamp(interpolation(fsdata.mColor, 0, w0, w1, w2), 0.0f, 1.0f) * 255 + 0.5);
    color[1] = int(glm::clamp(interpolation(fsdata.mColor, 1, w0, w1, w2), 0.0f, 1.0f) * 255 + 0.5);
    color[2] = int(glm::clamp(interpolation(fsdata.mColor, 2, w0, w1, w2), 0.0f, 1.0f) * 255 + 0.5);
    return true;
}

void TextureMapProgram::vertexData(TRMeshData &mesh, size_t index)
{
    for (int i = 0; i < 3; i++)
    {
        vsdata[i].mV = mesh.vertices[i + index];
        vsdata[i].mTexCoord = mesh.uvs[i + index];
    }
}

void TextureMapProgram::vertex(size_t i, glm::vec4 &clipV)
{
    clipV = __gMVPMat__ * glm::vec4(vsdata[i].mV, 1.0f);
}

bool TextureMapProgram::geometry()
{
    fsdata.mTexCoord[0] = vsdata[0].mTexCoord;
    fsdata.mTexCoord[1] = vsdata[1].mTexCoord;
    fsdata.mTexCoord[2] = vsdata[2].mTexCoord;
    return true;
}

bool TextureMapProgram::fragment(float w0, float w1, float w2, uint8_t color[3])
{
    float u = glm::clamp(interpolation(fsdata.mTexCoord, 0, w0, w1, w2), 0.0f, 1.0f);
    float v = glm::clamp(interpolation(fsdata.mTexCoord, 1, w0, w1, w2), 0.0f, 1.0f);

    glm::vec3 baseColor = texture_color(u, v, gCurrentMaterial.diffuse);

    color[0] = glm::clamp(int(baseColor[0]), 0, 255);
    color[1] = glm::clamp(int(baseColor[1]), 0, 255);
    color[2] = glm::clamp(int(baseColor[2]), 0, 255);
    return true;
}

void PhongProgram::vertexData(TRMeshData &mesh, size_t index)
{
    glm::vec3 viewLightPostion = gViewMat * glm::vec4(gLightPosition, 1.0f);

    for (int i = 0; i < 3; i++)
    {
        vsdata[i].mV = mesh.vertices[i + index];
        vsdata[i].mTexCoord = mesh.uvs[i + index];
        vsdata[i].mNormal = mesh.normals[i + index];
        vsdata[i].mLightPosition = viewLightPostion;
        vsdata[i].mTangent = mesh.tangents[i / 3];
    }
}

void PhongProgram::vertex(size_t i, glm::vec4 &clipV)
{
    clipV = __gMVPMat__ * glm::vec4(vsdata[i].mV, 1.0f);
    vsdata[i].mViewFragPosition = __gMVMat__ * glm::vec4(vsdata[i].mV, 1.0f);
    vsdata[i].mNormal = gNormalMat * vsdata[i].mNormal;
}

bool PhongProgram::geometry()
{
    for (int i = 0; i < 3; i++)
    {
        fsdata.mTexCoord[i] = vsdata[i].mTexCoord;
        fsdata.mViewFragPosition[i] = vsdata[i].mViewFragPosition;
        fsdata.mNormal[i] = vsdata[i].mNormal;
    }
    fsdata.mLightPosition = vsdata[0].mLightPosition;
    fsdata.mTangent = glm::normalize(gNormalMat * vsdata[0].mTangent);
    return true;
}

bool PhongProgram::fragment(float w0, float w1, float w2, uint8_t color[3])
{
    glm::vec3 viewFragPosition(
            interpolation(fsdata.mViewFragPosition, 0, w0, w1, w2),
            interpolation(fsdata.mViewFragPosition, 1, w0, w1, w2),
            interpolation(fsdata.mViewFragPosition, 2, w0, w1, w2)
            );
    glm::vec3 normal(
            interpolation(fsdata.mNormal, 0, w0, w1, w2),
            interpolation(fsdata.mNormal, 1, w0, w1, w2),
            interpolation(fsdata.mNormal, 2, w0, w1, w2)
            );

    float u = glm::clamp(interpolation(fsdata.mTexCoord, 0, w0, w1, w2), 0.0f, 1.0f);
    float v = glm::clamp(interpolation(fsdata.mTexCoord, 1, w0, w1, w2), 0.0f, 1.0f);

    glm::vec3 baseColor = texture_color(u, v, gCurrentMaterial.diffuse);

    // assume ambient light is (1.0, 1.0, 1.0)
    glm::vec3 ambient = gAmbientStrength * baseColor;

    if (gCurrentMaterial.normal != NULL)
    {
        // Gram-Schmidt orthogonalize
        glm::vec3 T = fsdata.mTangent;
        T = glm::normalize(T - dot(T, normal) * normal);
        glm::vec3 B = glm::cross(normal, T);
        glm::mat3 TBN = glm::mat3(T, B, normal);
        normal = texture_color(u, v, gCurrentMaterial.normal) * 0.007843137f - 1.0f;
        normal = TBN * normal;
    }
    normal = glm::normalize(normal);
    // from fragment to light
    glm::vec3 lightDirection = glm::normalize(fsdata.mLightPosition - viewFragPosition);

    float diff = glm::max(dot(normal, lightDirection), 0.0f);
    glm::vec3 diffuse = diff * gLightColor * baseColor;

    glm::vec3 result = ambient + diffuse;

    if (gCurrentMaterial.specular != NULL)
    {
        // in camera space, eys always in (0.0, 0.0, 0.0), from fragment to eye
        glm::vec3 eyeDirection = glm::normalize(-viewFragPosition);
#if __BLINN_PHONG__
        glm::vec3 halfwayDirection = glm::normalize(lightDirection + eyeDirection);
        float spec = glm::pow(glm::max(dot(normal, halfwayDirection), 0.0f), gShininess);
#else
        glm::vec3 reflectDirection = glm::reflect(-lightDirection, normal);
        float spec = glm::pow(glm::max(dot(eyeDirection, reflectDirection), 0.0f), gShininess);
#endif
        result += spec * gLightColor * texture_color(u, v, gCurrentMaterial.specular);
    }

    if (gCurrentMaterial.glow != NULL)
    {
        result += texture_color(u, v, gCurrentMaterial.glow);
    }

    color[0] = glm::clamp(int(result[0]), 0, 255);
    color[1] = glm::clamp(int(result[1]), 0, 255);
    color[2] = glm::clamp(int(result[2]), 0, 255);

    return true;
}

template<class Program>
void uniform_triangles_index(TRMeshData &mesh, size_t index, size_t num)
{
    size_t i = 0, j = 0;
    Program prog;

    for (i = index * 3, j = 0; j < num && i < mesh.vertices.size(); i += 3, j++)
    {
        prog.drawTriangle(gCurrentBuffer, mesh, i);
    }
}

template<class Program>
void tr_triangles_mt(TRMeshData &data)
{
    if (gThreadNum > 1)
    {
        std::vector<std::thread> thread_pool;
        size_t index_step = data.vertices.size() / 3 / gThreadNum + 1;

        for (size_t i = 0; i < gThreadNum; i++)
            thread_pool.push_back(std::thread(uniform_triangles_index<Program>, std::ref(data), i * index_step, index_step));

        for (auto &th : thread_pool)
            if (th.joinable())
                th.join();
    } else {
        uniform_triangles_index<Program>(data, 0, data.vertices.size() / 3);
    }
}

static inline void __fill_mesh_with_democolor__(TRMeshData &data)
{
    if (data.colors.size() != 0)
        return;

    size_t vnum = data.vertices.size();

    glm::vec3 demoColor[3] = { glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f) };

    for (size_t i = 0; i < vnum; i++)
    {
        data.colors.push_back(demoColor[i % 3]);
    }
}

void trTriangles(TRMeshData &mesh, TRDrawMode mode)
{
    __compute_premultiply_mat__();

    switch (mode)
    {
        case DRAW_WITH_TEXTURE:
            if (mesh.tangents.size() == 0)
                compute_tangent(mesh);
            if (gEnableLighting)
                tr_triangles_mt<PhongProgram>(mesh);
            else
                tr_triangles_mt<TextureMapProgram>(mesh);
            break;
        case DRAW_WITH_DEMO_COLOR:
            __fill_mesh_with_democolor__(mesh);
            tr_triangles_mt<ColorProgram>(mesh);
            break;
        case DRAW_WITH_COLOR:
            tr_triangles_mt<ColorProgram>(mesh);
            break;
        case DRAW_WIREFRAME:
            tr_triangles_mt<WireframeProgram>(mesh);
            break;
    }
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
