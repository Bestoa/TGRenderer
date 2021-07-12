#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "tr.h"
#include "utils.h"
#include "window.h"

#define WIDTH (800)
#define HEIGHT (800)
#define PI (3.1415926)

glm::vec3 get_point(float u, float v)
{
    float a1 = PI * v, a2 = PI * u * 2;
    float x = sin(a1) * cos(a2);
    float y = cos(a1);
    float z = sin(a1) * sin(a2);
    return glm::vec3(x, y, z);
}

void create_sphere(std::vector<glm::vec3> &vs, std::vector<glm::vec2> &uvs, std::vector<glm::vec3> &ns, int uStepNum, int vStepNum)
{

    float ustep = 1/(float)uStepNum, vstep = 1/(float)vStepNum;
    float u = 0, v = 0;
    std::vector<glm::vec3> points;

    for (int i = 0; i < uStepNum; i++)
    {
        glm::vec3 p1 = get_point(0, 0);
        glm::vec3 p2 = get_point(u + ustep, vstep);
        glm::vec3 p3 = get_point(u, vstep);
        // counter-clockwise
        vs.push_back(p1);
        vs.push_back(p2);
        vs.push_back(p3);

        uvs.push_back(glm::vec2(1, 1));
        uvs.push_back(glm::vec2(1 - (u + ustep), 1 - vstep));
        uvs.push_back(glm::vec2(1 - u, 1 - vstep));

        ns.push_back(p1);
        ns.push_back(p2);
        ns.push_back(p3);
        u += ustep;
    };

    u = 0;
    v = vstep;
    for (int i = 1; i < vStepNum - 1; i++)
    {
        u = 0;
        for (int j = 0; j < uStepNum; j++)
        {
            // counter-clockwise
            /*
             *   p4---p1
             *   |   / |
             *   |  /  |
             *   | /   |
             *   p2---p3
             */
            glm::vec3 p1 = get_point(u, v);
            glm::vec3 p2 = get_point(u + ustep, v + vstep);
            glm::vec3 p3 = get_point(u, v + vstep);
            glm::vec3 p4 = get_point(u + ustep, v);

            vs.push_back(p1);
            vs.push_back(p4);
            vs.push_back(p2);

            uvs.push_back(glm::vec2(1 - u, 1 - v));
            uvs.push_back(glm::vec2(1 - (u + ustep), 1 -v));
            uvs.push_back(glm::vec2(1 - (u + ustep), 1 - (v + vstep)));

            ns.push_back(p1);
            ns.push_back(p4);
            ns.push_back(p2);

            vs.push_back(p1);
            vs.push_back(p2);
            vs.push_back(p3);

            uvs.push_back(glm::vec2(1 - u, 1 - v));
            uvs.push_back(glm::vec2(1 - (u + ustep), 1 - (v + vstep)));
            uvs.push_back(glm::vec2(1 - u, 1 - (v + vstep)));

            ns.push_back(p1);
            ns.push_back(p2);
            ns.push_back(p3);

            u += ustep;
        }
        v += vstep;
    }

    u = 0;
    for (int i = 0; i < uStepNum; i++)
    {
        glm::vec3 p1 = get_point(0, 1);
        glm::vec3 p2 = get_point(u, 1 - vstep);
        glm::vec3 p3 = get_point(u + ustep, 1 - vstep);

        // counter-clockwise
        vs.push_back(p1);
        vs.push_back(p2);
        vs.push_back(p3);

        uvs.push_back(glm::vec2(1, 0));
        uvs.push_back(glm::vec2(1 - u, vstep));
        uvs.push_back(glm::vec2(1 - (u + ustep), vstep));

        ns.push_back(p1);
        ns.push_back(p2);
        ns.push_back(p3);

        u += ustep;
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " texture_file" << std::endl;
        abort();
    }
    TRBuffer buffer;
    ZERO(buffer);
    trCreateRenderTarget(buffer, WIDTH, HEIGHT, true);
    trMakeCurrent(buffer);

    std::vector<glm::vec3> vs;
    std::vector<glm::vec2> uvs;
    std::vector<glm::vec3> ns;
    create_sphere(vs, uvs, ns, 30, 30);

    trSetViewMat(glm::lookAt(
                glm::vec3(2,1,2),
                glm::vec3(0,0,0),
                glm::vec3(0,1,0)
                ));

    trSetProjMat(glm::perspective(glm::radians(75.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f));

    trSetLightPosition3f(100.0f, 0.0f, 0.0f);

    TRTexture tex;
    ZERO(tex);
    if (!load_texture(argv[1], tex))
    {
        std::cout << "Load texture failed." << std::endl;
        abort();
    }
    trBindTexture(&tex, TEXTURE_DIFFUSE);

    TRWindow *w = new TRWindow(WIDTH, HEIGHT);
    if (w->fail())
    {
        std::cout << "Create window failed." << std::endl;
        abort();
    }
    int frame = 0;
    void *addr;

    glm::mat4 self = glm::rotate(glm::mat4(1.0f), glm::radians(-23.5f), glm::vec3(0.0f, 0.0f, 1.0f));
    while (!w->should_exit()) {
        frame++;
        trSetModelMat(glm::rotate(self, glm::radians(1.0f * frame), glm::vec3(0.0f, 1.0f, 0.0f)));
        w->lock(&addr);
        trSetExtBufferToRenderTarget(buffer, addr);

        trClear();
        trTrianglesWithTexture(vs, uvs, ns);
        w->unlock();
    }
    delete w;
    std::cout << "frames = " << frame << std::endl;
    trDestoryRenderTarget(buffer);
    return 0;
}
