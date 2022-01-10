#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "trapi.hpp"
#include "program.hpp"
#include "window.hpp"
#include "utils.hpp"

constexpr int WIDTH = 1280;
constexpr int HEIGHT = 800;
constexpr float PI = 3.1415926;

using namespace TGRenderer;

class TextureMapExShader : public TextureMapShader
{
    private:
        bool fragment(FSInData *fsdata, float color[])
        {
            int frame = *reinterpret_cast<int *>(trGetUniformData());
            glm::vec2 texCoord = fsdata->getVec2(SH_TEXCOORD);
            texCoord.x -= frame * 0.001f;
            textureCoordWrap(texCoord);
            float *c = texture2D(TEXTURE_DIFFUSE, texCoord.x, texCoord.y);
            color[0] = c[0];
            color[1] = c[1];
            color[2] = c[2];
            return true;
        }
};

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

int main()
{
    TRWindow w(WIDTH, HEIGHT, "Sphere demo");
    if (!w.isRunning())
        return 1;

    TRMeshData sphere;
    create_sphere(sphere.vertices, sphere.uvs, sphere.normals, 30, 30);
    sphere.fillSpriteColor();
    sphere.computeTangent();

    glm::mat4 viewMat = glm::lookAt(
                glm::vec3(2,1,2),
                glm::vec3(0,0,0),
                glm::vec3(0,1,0));

    glm::mat4 projMat = glm::perspective(glm::radians(75.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

    TRTexture earthTex("examples/res/earth.tga");
    if (!earthTex.isValid())
    {
        std::cout << "Load earthTexture failed." << std::endl;
        abort();
    }

    int frame = 0;
    TextureMapPhongShader shader;

    PhongUniformData unidata;
    unidata.mLightPosition = glm::vec3(100.0f, 0.0f, 0.0f);
    unidata.mViewLightPosition = trGetMat4(MAT4_VIEW) * glm::vec4(unidata.mLightPosition, 1.0f);

    TRMeshData BGPlane;
    float white[] = { 1.0f, 1.0f, 1.0f };
    truCreateQuadPlane(BGPlane, white);
    TextureMapExShader texShader;
    TRTexture bgTex("examples/res/stars.tga");
    if (!bgTex.isValid())
    {
        std::cout << "Load background texture failed." << std::endl;
        abort();
    }

    glm::mat4 self = glm::rotate(glm::mat4(1.0f), glm::radians(-23.5f), glm::vec3(0.0f, 0.0f, 1.0f));
    while (!w.shouldStop()) {
        frame++;
        trSetMat4(glm::rotate(self, glm::radians(1.0f * frame), glm::vec3(0.0f, 1.0f, 0.0f)), MAT4_MODEL);
        trSetMat4(viewMat, MAT4_VIEW);
        trSetMat4(projMat, MAT4_PROJ);

        trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT | TR_CLEAR_STENCIL_BIT);
        trEnableStencilWrite(true);
        trBindTexture(&earthTex, TEXTURE_DIFFUSE);
        trSetUniformData(&unidata);
        trTriangles(sphere, &shader);

        trEnableDepthTest(false);
        trEnableStencilTest(true);
        trEnableStencilWrite(false);
        trResetMat4(MAT4_MODEL);
        trResetMat4(MAT4_VIEW);
        trResetMat4(MAT4_PROJ);
        trBindTexture(&bgTex, TEXTURE_DIFFUSE);
        trSetUniformData(&frame);
        trTriangles(BGPlane, &texShader);
        trEnableDepthTest(true);
        trEnableStencilTest(false);

        w.swapBuffer();
        w.pollEvent();
    }
    std::cout << "frames = " << frame << std::endl;
    return 0;
}
