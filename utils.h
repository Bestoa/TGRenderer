#ifndef __TR_UTILS__
#define __TR_UTILS__
#include "tr.h"
void save_ppm(const char *name, uint8_t *buffer, int width, int height);
bool load_ppm_texture(
        const char *path,
        TRTexture &tex
        );
bool load_tga_texture(
        const char *path,
        TRTexture &tex
        );
bool load_obj(
        const char * path, 
        std::vector<glm::vec3> & out_vertices, 
        std::vector<glm::vec2> & out_uvs,
        std::vector<glm::vec3> & out_normals
        );
#endif
