#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

#include "tr.hpp"
#include "utils.hpp"

#define WIDTH (600)
#define HEIGHT (600)

int main()
{
    TRBuffer *buffer;

    buffer = trCreateRenderTarget(WIDTH, HEIGHT);
    trSetCurrentRenderTarget(buffer);

    trClearColor3f(0.1, 0.1, 0.1);

    TRMeshData data;
    data.vertices.push_back(glm::vec3(-0.5f, -0.5f, 0.0f));
    data.vertices.push_back(glm::vec3(0.5f, -0.5f, 0.0f));
    data.vertices.push_back(glm::vec3(0.0f, 0.5f, 0.0f));

    trClear();
    trTriangles(data, DRAW_WITH_DEMO_COLOR);
    save_ppm("01_demo_color.ppm", buffer->mData, buffer->mW, buffer->mH);

    data.colors.clear();
    data.colors.push_back(glm::vec3(0.0f, 0.0f, 0.0f));
    data.colors.push_back(glm::vec3(0.5f, 0.5f, 0.5f));
    data.colors.push_back(glm::vec3(1.0f, 1.0f, 1.0f));

    trClear();
    trTriangles(data, DRAW_WITH_COLOR);
    save_ppm("01_color.ppm", buffer->mData, buffer->mW, buffer->mH);

    delete buffer;

    return 0;
}
