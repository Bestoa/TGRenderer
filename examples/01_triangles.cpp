#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

#include "tr.h"
#include "utils.h"

#define WIDTH (600)
#define HEIGHT (600)

int main()
{
    TRBuffer buffer;

    trCreateRenderTarget(buffer, WIDTH, HEIGHT);
    trMakeCurrent(buffer);
    trClearColor3f(0.1, 0.1, 0.1);

    TRMeshData data;
    data.vertices.push_back(glm::vec3(-0.5f, -0.5f, 0.0f));
    data.vertices.push_back(glm::vec3(0.5f, -0.5f, 0.0f));
    data.vertices.push_back(glm::vec3(0.0f, 0.5f, 0.0f));

    trClear();
    trTriangles(data, DRAW_WITH_DEMO_COLOR);
    save_ppm("01_demo_color.ppm", buffer.data, buffer.w, buffer.h);

    data.colors.clear();
    data.colors.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
    data.colors.push_back(glm::vec3(0.5f, 0.5f, 0.5f));
    data.colors.push_back(glm::vec3(1.0f, 1.0f, 1.0f));

    trClear();
    trTriangles(data, DRAW_WITH_COLOR);
    save_ppm("01_color.ppm", buffer.data, buffer.w, buffer.h);

    trDestoryRenderTarget(buffer);
    return 0;
}
