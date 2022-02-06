#ifndef __TR_UTILS__
#define __TR_UTILS__
#include <cstring>
#include "trapi.hpp"

constexpr float WHITE[3] = { 1.0f, 1.0f, 1.0f };
constexpr float BLACK[3] = { 0.0f, 0.0f, 0.0f };
constexpr float GREY[3] = { 0.5f, 0.5f, 0.5f };
constexpr float RED[3] = { 1.0f, 0.0f, 0.0f };
constexpr float GREEN[3] = { 0.0f, 1.0f, 0.0f };
constexpr float BLUE[3] = { 0.0f, 0.0f, 1.0f };

bool truSavePNG(const char *name, TGRenderer::TRBuffer *buffer);
void truLoadVec2(const float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec2> &out);
void truLoadVec3(const float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec3> &out);
void truLoadVec4(const float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec3> &out);
void truTimerBegin();
void truTimerClick();
double truTimerGetSecondsFromClick();
double truTimerGetSecondsFromBegin();
bool truLoadObj(
        const char * path,
        std::vector<glm::vec3> & out_vertices,
        std::vector<glm::vec2> & out_texcoords,
        std::vector<glm::vec3> & out_normals
        );
void truCreateFloorPlane(TGRenderer::TRMeshData &mesh, float height, float width = 4.0f, const float *color = &WHITE[0]);
void truCreateQuadPlane(TGRenderer::TRMeshData &mesh);
void truCreateSphere(TGRenderer::TRMeshData &mesh, int uStepNum, int vStepNum, const float *color = &WHITE[0]);
#endif
