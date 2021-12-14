#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include "tr.h"
#include "utils.h"
#include "window.h"
#include "objs.h"

#define __ON_SCREEN__ 1
#if __ON_SCREEN__
#define WIDTH (800)
#define HEIGHT (600)
#else
#define WIDTH (4000)
#define HEIGHT (4000)
#endif

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s obj_config...\n", argv[0]);
        return 0;
    }
    TRBuffer buffer;
    ZERO(buffer);
#if __ON_SCREEN__
    TRWindow w(WIDTH, HEIGHT);
    if (!w.isRunning())
        return 1;
    w.createSurfaceRenderTarget(buffer, WIDTH, HEIGHT);
#else
    trCreateRenderTarget(buffer, WIDTH, HEIGHT);
#endif
    trMakeCurrent(buffer);

    trEnableLighting(true);
    trClearColor3f(0.1, 0.1, 0.1);

    std::vector<std::shared_ptr <TRObj>> objs;
    for (int i = 1; i < argc; i++)
    {
        std::shared_ptr<TRObj> obj(new TRObj());
        if (obj->load(argv[i]))
            objs.push_back(obj);
    }

    trSetViewMat(glm::lookAt(
                glm::vec3(0,0.75,1.5), // Camera is at (0,0.75,1.5), in World Space
                glm::vec3(0,0,0), // and looks at the origin
                glm::vec3(0,1,0)  // Head is up (set to 0,-1,0 to look upside-down)
                ));

    // Projection matrix : xx Field of View, w:h ratio, display range : 0.1 unit <-> 100 units
    trSetProjMat(glm::perspective(glm::radians(75.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f));

#if __ON_SCREEN__
    int frame = 0;
    auto start = std::chrono::system_clock::now();
    while (!w.shouldStop() && frame < 720) {
        frame++;
        trSetModelMat(glm::rotate(glm::mat4(1.0f), glm::radians(1.0f * frame), glm::vec3(0.0f, 1.0f, 0.0f)));
#endif
        trClear();
        for (auto obj : objs)
            obj->draw();
#if __ON_SCREEN__
        w.swapBuffer(buffer);
    }
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double fps = double(frame) / (double(duration.count()) * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den);
    printf("fps = %lf\n", fps);
#endif

    printf("Rendering done.\n");
#if !__ON_SCREEN__
    save_ppm("out.ppm", buffer.data, buffer.w, buffer.h);
#endif
    trDestoryRenderTarget(buffer);
    return 0;
}
