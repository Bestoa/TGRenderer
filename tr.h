#ifndef __TINY_RENDER__
#define __TINY_RENDER__

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct TRTexture
{
    uint8_t *data;
    int stride;
    int w;
    int h;
};

struct TRMaterial
{
    TRTexture *diffuse;
    TRTexture *specular;
    TRTexture *glow;
    TRTexture *normal;
};

enum TRTextureType
{
    TEXTURE_DIFFUSE,
    TEXTURE_SPECULAR,
    TEXTURE_GLOW,
    TEXTURE_NORMAL,
};

struct TRMeshData
{
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> colors;
    std::vector<glm::vec3> tangents;
};

enum TRDrawMode
{
    DRAW_WITH_TEXTURE,
    DRAW_WITH_COLOR,
    DRAW_WITH_DEMO_COLOR,
    DRAW_WIREFRAME,
};

#define BPP (3)

struct TRFragData
{
    int x, y;
    float w[3];
    float depth;
    uint8_t *addr;
};

struct TRBuffer
{
    uint8_t *data;
    float *depth;

    uint32_t w;
    uint32_t h;

    uint32_t vx;
    uint32_t vy;
    uint32_t vw;
    uint32_t vh;

    uint32_t stride;
    uint8_t bg_color[BPP];
    // data was not allocated by us.
    bool ext_buffer;
};

#ifndef __BLINN_PHONG__
#define __BLINN_PHONG__ 1
#endif

void trSetRenderThreadNum(size_t num);
void trEnableLighting(bool enable);
void trSetAmbientStrength(float v);
void trSetLightColor3f(float r, float g, float b);
void trSetLightPosition3f(float x, float y, float z);
void trViewport(int x, int y, int w, int h);
void trMakeCurrent(TRBuffer &buffer);
void trBindTexture(TRTexture *texture, TRTextureType type);
void trClear();
void trClearColor3f(float r, float g, float b);
void trSetModelMat(glm::mat4 mat);
void trSetViewMat(glm::mat4 mat);
void trSetProjMat(glm::mat4 mat);
bool trCreateRenderTarget(TRBuffer &buffer, int w, int h, bool has_ext_buffer = false);
void trSetExtBufferToRenderTarget(TRBuffer &buffer, void *addr);
void trDestoryRenderTarget(TRBuffer &buffer);
void trTriangles(TRMeshData &data, TRDrawMode mode);
#endif
