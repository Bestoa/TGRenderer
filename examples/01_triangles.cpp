#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

#include "tr.hpp"
#include "program.hpp"
#include "utils.hpp"

#define WIDTH (600)
#define HEIGHT (600)

using namespace TGRenderer;

int main()
{
    TRBuffer *buffer;

    buffer = trCreateRenderTarget(WIDTH, HEIGHT);
    trSetCurrentRenderTarget(buffer);

    trClearColor3f(0.1, 0.1, 0.1);

    float vertex[] =
    {
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.0f, 0.5f, 0.5f, 0.5f,
        0.0f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f,
    };

    TRMeshData mesh;
    truLoadVec3(vertex, 0, 3, 0, 6, mesh.vertices);

    ColorProgram prog;

    trClear();
    mesh.fillSpriteColor();
    trTriangles(mesh, &prog);
    truSavePPM("01_sprite_color.ppm", buffer->mData, buffer->mW, buffer->mH);

    mesh.colors.clear();
    truLoadVec3(vertex, 0, 3, 3, 6, mesh.colors);

    trClear();
    trTriangles(mesh, &prog);
    truSavePPM("01_color.ppm", buffer->mData, buffer->mW, buffer->mH);

    delete buffer;

    return 0;
}
