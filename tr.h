#ifndef __TINY_RENDER__
#define __TINY_RENDER__

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct TRTexture
{
    uint8_t *diffuse;
    int stride;
    int w;
    int h;
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
};

void trSetAmbientStrength(float v);
void trSetSpecularStrength(float v);
void trSetLightColor3f(float r, float g, float b);
void trSetLightPosition3f(float x, float y, float z);
void trMakeCurrent(TRBuffer &buffer);
void trBindTexture(TRTexture &texture);
void trClear();
void trUpdateNormalMat();
void trSetModelMat(glm::mat4 mat);
void trSetViewMat(glm::mat4 mat);
void trSetProjMat(glm::mat4 mat);
bool trCreateRenderTarget(TRBuffer &buffer, int w, int h);
void trTriangles(std::vector<glm::vec3> &vertices, std::vector<glm::vec2> &uvs, std::vector<glm::vec3> &normals);
#endif