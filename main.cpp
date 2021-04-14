#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "tr.h"
#include "utils.h"
#include "window.h"

#define __ON_SCREEN__ 1
#if __ON_SCREEN__
#define WIDTH (1280)
#define HEIGHT (720)
#else
#define WIDTH (4000)
#define HEIGHT (4000)
#endif

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: %s objfile texturefile...\n", argv[0]);
        return 0;
    }
    TRBuffer buffer;
    trCreateRenderTarget(buffer, WIDTH, HEIGHT);
    trMakeCurrent(buffer);

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> normals;

    load_obj(argv[1], vertices, uvs, normals);

    TRTexture diffuse;
    load_ppm_texture(argv[2], diffuse);
    trBindTexture(&diffuse, TEXTURE_DIFFUSE);

    TRTexture specular;
    if (argc > 3)
    {
        load_ppm_texture(argv[3], specular);
        trBindTexture(&specular, TEXTURE_SPECULAR);
    }
    TRTexture glow;
    if (argc > 4)
    {
        load_ppm_texture(argv[4], glow);
        trBindTexture(&glow, TEXTURE_GLOW);
    }
    TRTexture normal;
    if (argc > 5)
    {
        load_ppm_texture(argv[5], normal);
        trBindTexture(&normal, TEXTURE_NORMAL);
    }

    trSetViewMat(glm::lookAt(
                glm::vec3(0,1,2), // Camera is at (0,1,2), in World Space
                glm::vec3(0,0,0), // and looks at the origin
                glm::vec3(0,1,0)  // Head is up (set to 0,-1,0 to look upside-down)
                ));

    // Projection matrix : xx Field of View, w:h ratio, display range : 0.1 unit <-> 100 units
    trSetProjMat(glm::perspective(glm::radians(75.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f));

#if __ON_SCREEN__
    window *w = window_create(WIDTH, HEIGHT);
    int frame = 0;
    while (!window_should_exit() && frame < 1000) {
#endif
        trClear();
#if __ON_SCREEN__
        frame++;
        trSetModelMat(glm::rotate(glm::mat4(1.0f), glm::radians(1.0f * frame), glm::vec3(0.0f, 1.0f, 0.0f)));
#endif
        trTriangles(vertices, uvs, normals);
#if __ON_SCREEN__
        window_update(w, buffer.data, buffer.h * buffer.stride);
    }
    window_destory(w);
    printf("frames = %d\n", frame);
#endif

    printf("Rendering done.\n");
#if !__ON_SCREEN__
    save_ppm("out.ppm", buffer.data, buffer.w, buffer.h);
#endif
    return 0;
}
