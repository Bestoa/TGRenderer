#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

#include "tr.hpp"
#include "window.hpp"
#include "objs.hpp"

#define WIDTH (800)
#define HEIGHT (600)

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
    trMakeCurrent(w.getBuffer());

    trEnableLighting(true);
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

    trSetViewMat(glm::lookAt(
                glm::vec3(0,0.75,1.5), // Camera is at (0,0.75,1.5), in World Space
                glm::vec3(0,0,0), // and looks at the origin
                glm::vec3(0,1,0)  // Head is up (set to 0,-1,0 to look upside-down)
                ));

    // Projection matrix : xx Field of View, w:h ratio, display range : 0.1 unit <-> 100 units
    trSetProjMat(glm::perspective(glm::radians(75.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f));

    int frame = 0;
    auto start = std::chrono::system_clock::now();
    while (!w.shouldStop() && frame < 360) {
        frame++;
        trSetModelMat(glm::rotate(glm::mat4(1.0f), glm::radians(1.0f * frame), glm::vec3(0.0f, 1.0f, 0.0f)));
        trClear();
        for (auto obj : objs)
            obj->draw();
        w.swapBuffer();
    }
    auto end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double fps = double(frame) / (double(duration.count()) * std::chrono::microseconds::period::num / std::chrono::microseconds::period::den);
    printf("fps = %lf\n", fps);

    printf("Rendering done.\n");
    return 0;
}
