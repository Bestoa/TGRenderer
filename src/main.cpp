#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include "tr.hpp"
#include "window.hpp"
#include "objs.hpp"
#include "utils.hpp"

#define WIDTH (1280)
#define HEIGHT (720)

#define TWIDTH (1024)
#define THEIGHT (1024)

#define ENABLE_SHADOW 0

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s obj_config...\n", argv[0]);
        return 0;
    }
    TRWindow w(WIDTH, HEIGHT);
    if (!w.isRunning())
        return 1;

    LightInfo &light = trGetLightInfo();
    light.mPosition = glm::vec3(1.0f, 1.0f, 1.0f);
#if ENABLE_SHADOW
    TRBuffer *windowBuffer = w.getBuffer();
    TRTextureBuffer *shadowBuffer = new TRTextureBuffer(TWIDTH, THEIGHT);
#endif

    trClearColor3f(0.1, 0.1, 0.1);

    std::vector<std::shared_ptr <TRObj>> objs;
    for (int i = 1; i < argc; i++)
    {
        std::shared_ptr<TRObj> obj(new TRObj(argv[i]));
        if (obj->isValid())
            objs.push_back(obj);
    }

    if (objs.size() == 0)
        abort();

    glm::mat4 eyeViewMat = glm::lookAt(
            glm::vec3(0,0.75,1.5), // Camera is at (0,0.75,1.5), in World Space
            glm::vec3(0,0,0), // and looks at the origin
            glm::vec3(0,1,0));  // Head is up (set to 0,-1,0 to look upside-down)

    // Projection matrix : xx Field of View, w:h ratio, display range : 0.1 unit <-> 100 units
    glm::mat4 eyeProjMat = glm::perspective(glm::radians(75.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

#if ENABLE_SHADOW
    glm::mat4 lightViewMat = glm::lookAt(
            glm::vec3(1,1,1),
            glm::vec3(0,0,0),
            glm::vec3(0,1,0));

    glm::mat4 lightProjMat = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 100.0f);

    trSetCurrentRenderTarget(shadowBuffer);
    trClearColor3f(1, 1, 1);

    TRMeshData floorMesh;
    float floorColor[] = {0.5f, 0.5f, 0.5f};
    truCreateFloor(floorMesh, -1.0f, floorColor);

    ColorPhongProgram prog;
#endif

    int frame = 0;
    auto start = std::chrono::system_clock::now();
    while (!w.shouldStop() && frame < 360) {
        frame++;
        glm::mat4 modelMat = glm::rotate(glm::mat4(1.0f), glm::radians(1.0f * frame), glm::vec3(0.0f, 1.0f, 0.0f));
        trSetMat4(modelMat, MAT4_MODEL);

#if ENABLE_SHADOW
        trSetCurrentRenderTarget(shadowBuffer);
        trClear();
        trSetMat4(lightViewMat, MAT4_VIEW);
        trSetMat4(lightProjMat, MAT4_PROJ);
        for (auto obj : objs)
            obj->drawShadowMap();
        /* Skip floor in shadow map to speedup */

        trSetMat4(lightProjMat * lightViewMat * modelMat, MAT4_LIGHT_MVP);
        trBindTexture(shadowBuffer->getTexture(), TEXTURE_SHADOWMAP);
        trSetCurrentRenderTarget(windowBuffer);
#endif
        trClear();
        trSetMat4(eyeViewMat, MAT4_VIEW);
        trSetMat4(eyeProjMat, MAT4_PROJ);
        for (auto obj : objs)
            obj->draw();

#if ENABLE_SHADOW
        trSetMat4(glm::mat4(1.0f), MAT4_MODEL);
        trSetMat4(lightProjMat * lightViewMat, MAT4_LIGHT_MVP);
        trTriangles(floorMesh, &prog);
        trBindTexture(nullptr, TEXTURE_SHADOWMAP);
#endif
        w.swapBuffer();
    }
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double fps = double(frame) / (double(duration.count()) * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den);
    printf("fps = %lf\n", fps);

#if ENABLE_SHADOW
    delete shadowBuffer;
#endif

    printf("Rendering done.\n");
    return 0;
}
