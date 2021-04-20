#ifndef __TINY_RENDER__
#define __TINY_RENDER__

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct TRTexture
{
    float *data;
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

enum texture_type
{
    TEXTURE_DIFFUSE,
    TEXTURE_SPECULAR,
    TEXTURE_GLOW,
    TEXTURE_NORMAL,
};

#define BPP (3)

struct TRBuffer
{
    uint8_t *data;
    float *depth;
    int w;
    int h;
    int stride;
    uint8_t bg_color[BPP];
    // data was not allocated by us.
    bool ext_buffer;
};

#ifndef __BLINN_PHONG__
#define __BLINN_PHONG__ 1
#endif

#ifndef __CULL_FACE__
#define __CULL_FACE__ 1
#endif

void trSetAmbientStrength(float v);
void trSetLightColor3f(float r, float g, float b);
void trSetLightPosition3f(float x, float y, float z);
void trMakeCurrent(TRBuffer &buffer);
void trBindTexture(TRTexture *texture, int type);
void trClear();
void trSetModelMat(glm::mat4 mat);
void trSetViewMat(glm::mat4 mat);
void trSetProjMat(glm::mat4 mat);
bool trCreateRenderTarget(TRBuffer &buffer, int w, int h, bool has_ext_buffer = false);
void trSetExtBufferToRenderTarget(TRBuffer &buffer, void *addr);
void trDestoryRenderTarget(TRBuffer &buffer);
void trTriangles(std::vector<glm::vec3> &vertices, std::vector<glm::vec2> &uvs, std::vector<glm::vec3> &normals, bool demo_color = false);
void trTriangles(std::vector<glm::vec3> &vertices, std::vector<glm::vec3> &colors);
void trTrianglesWireframe(std::vector<glm::vec3> &vertices);
#endif
