#ifndef __TR_UTILS__
#define __TR_UTILS__
#include <cstring>
#include "trapi.hpp"

void truSavePPM(const char *name, uint8_t *buffer, int width, int height);
void truLoadVec2(float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec2> &out);
void truLoadVec3(float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec3> &out);
void truLoadVec4(float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec3> &out);
bool truLoadObj(
        const char * path,
        std::vector<glm::vec3> & out_vertices,
        std::vector<glm::vec2> & out_uvs,
        std::vector<glm::vec3> & out_normals
        );
void truCreateFloor(TGRenderer::TRMeshData &mesh, float height, float color[3]);
void truCreateQuadPlane(TGRenderer::TRMeshData &mesh, float color[3]);
#endif
