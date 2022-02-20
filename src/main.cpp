#include <vector>
#include <iostream>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "trapi.hpp"
#include "window.hpp"
#include "objs.hpp"
#include "utils.hpp"
#include "program.hpp"
#include "skybox.hpp"

#define WIDTH (1280)
#define HEIGHT (720)

#define TWIDTH (1024)
#define THEIGHT (1024)

#define ENABLE_SHADOW 1
#define DRAW_FLOOR 1
#define ENABLE_SKYBOX 1

#if ENABLE_SKYBOX
std::string gCubeTextureNames[] =
{
    "res/tex/sb/bottom.jpg",
    "res/tex/sb/top.jpg",
    "res/tex/sb/front.jpg",
    "res/tex/sb/back.jpg",
    "res/tex/sb/left.jpg",
    "res/tex/sb/right.jpg",
};
#endif

using namespace TGRenderer;

PhongUniformData unidata;
int ProgramId = 3;

class Option
{
    public:
        bool enableSkybox = false;
        bool enableShadow = false;
        bool drawFloor = false;
        bool wireframeMode = false;
        bool rotateModel = false;
        bool rotateEye = false;
        bool rotateLight = false;
        bool zoomIn = false;
        bool zoomOut = false;
};

Option gOption;

const int endFrame = 36000;

void kcb(int key)
{
    switch (key)
    {
        case SDL_SCANCODE_B:
            gOption.enableSkybox = !gOption.enableSkybox;
            break;
        case SDL_SCANCODE_S:
            gOption.enableShadow = !gOption.enableShadow;
            break;
        case SDL_SCANCODE_F:
            gOption.drawFloor = !gOption.drawFloor;
            break;
        case SDL_SCANCODE_W:
            gOption.wireframeMode = !gOption.wireframeMode;
            if (gOption.wireframeMode)
                trPolygonMode(TR_LINE);
            else
                trPolygonMode(TR_FILL);
            break;
        case SDL_SCANCODE_M:
            gOption.rotateModel = !gOption.rotateModel;
            // avoid chaos
            gOption.rotateEye = false;
            gOption.rotateLight = false;
            break;
        case SDL_SCANCODE_E:
            gOption.rotateEye = !gOption.rotateEye;
            gOption.rotateModel = false;
            gOption.rotateLight = false;
            break;
        case SDL_SCANCODE_L:
            gOption.rotateLight = !gOption.rotateLight;
            gOption.rotateModel = false;
            gOption.rotateEye = false;
            break;
        case SDL_SCANCODE_I:
            gOption.zoomIn = true;
            break;
        case SDL_SCANCODE_O:
            gOption.zoomOut = true;
            break;
        case SDL_SCANCODE_C:
            ProgramId++;
            if (ProgramId == 4)
                ProgramId = 0;
            break;
    }
}

void reCalcMat(glm::mat4 &modelMat, glm::mat4 &eyeViewMat, glm::mat4 &lightViewMat)
{
    static int rotateM = 0, rotateE = 0, rotateL = 0;
    static float eyeStartDistance = 1.5f;

    bool reCalcViewMat = false;
    if (gOption.rotateModel)
    {
        rotateM++;
        modelMat = glm::rotate(glm::mat4(1.0f), glm::radians(1.0f * rotateM), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    if (gOption.rotateEye)
    {
        rotateE++;
        reCalcViewMat = true;
    }

    if (gOption.zoomIn)
    {
        gOption.zoomIn = false;
        reCalcViewMat = true;
        eyeStartDistance -= 0.1f;
    }

    if (gOption.zoomOut)
    {
        gOption.zoomOut = false;
        reCalcViewMat = true;
        eyeStartDistance += 0.1f;
    }

    if (reCalcViewMat)
    {
        float degree = glm::radians(1.0f * rotateE);
        eyeViewMat = glm::lookAt(
                glm::vec3(eyeStartDistance * glm::sin(degree), eyeStartDistance / 2, eyeStartDistance * glm::cos(degree)),
                glm::vec3(0,0,0),
                glm::vec3(0,1,0));
    }

    if (gOption.rotateLight)
    {
        rotateL++;
        float degree = glm::radians(1.0f * rotateL);
        lightViewMat = glm::lookAt(
                glm::vec3(glm::sin(degree), 1, glm::cos(degree)),
                glm::vec3(0,0,0),
                glm::vec3(0,1,0));
        unidata.mLightPosition = glm::vec3(glm::sin(degree), 1.0f, glm::cos(degree));
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " obj_config..." << std::endl;
        return 0;
    }
    TRWindow w(WIDTH, HEIGHT);
    if (!w.OK())
        return 1;

    w.registerKeyEventCb(kcb);

#if ENABLE_SHADOW
    TRBuffer *windowBuffer = trGetRenderTarget();
    TRTextureBuffer *shadowBuffer = new TRTextureBuffer(TWIDTH, THEIGHT);
#endif

    trClearColor3f(0.1, 0.1, 0.1);

    std::vector<std::shared_ptr <TRObj>> objs;
    for (int i = 1; i < argc; i++)
    {
        std::shared_ptr<TRObj> obj(new TRObj(argv[i]));
        if (obj->OK())
            objs.push_back(obj);
    }

    if (objs.size() == 0)
        abort();

    float eyeStartDistance = 1.5f;
    glm::mat4 eyeViewMat = glm::lookAt(
            glm::vec3(0,eyeStartDistance / 2 ,eyeStartDistance), // Camera is at (0,0.75,1.5), in World Space
            glm::vec3(0,0,0), // and looks at the origin
            glm::vec3(0,1,0));  // Head is up (set to 0,-1,0 to look upside-down)

    // Projection matrix : xx Field of View, w:h ratio, display range : 0.1 unit <-> 100 units
    glm::mat4 eyeProjMat = glm::perspective(glm::radians(75.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

#if ENABLE_SHADOW
    glm::mat4 lightViewMat = glm::lookAt(
            glm::vec3(0,1,1),
            glm::vec3(0,0,0),
            glm::vec3(0,1,0));

    glm::mat4 lightProjMat = glm::ortho(-2.0f, 2.0f, -2.0f, 2.0f, 0.1f, 100.0f);

    trSetRenderTarget(shadowBuffer);
    trClearColor3f(1, 1, 1);
    trSetRenderTarget(windowBuffer);
#endif

#if DRAW_FLOOR
    TextureMapPhongShader floorShader;
    TRMeshData floorMesh;
    float floorHeight = 10.0f;
    for (auto obj : objs)
    {
        float currentFloorHeight = obj->getFloorYAxis();
        if (currentFloorHeight < floorHeight)
            floorHeight = currentFloorHeight;
    }
    truCreateFloorPlane(floorMesh, floorHeight);
    TRTexture floorTex("res/tex/floor_diffuse.tga");
    if (!floorTex.OK())
        abort();
#endif

#if ENABLE_SKYBOX
    TRSkyBox skybox(gCubeTextureNames);
#endif

    glm::mat4 modelMat(1.0f);
    unidata.mLightPosition = glm::vec3(0.0f, 1.0f, 1.0f);

    int frame = 0;
    truTimerBegin();
    while (!w.shouldStop() && frame++ < endFrame)
    {
        reCalcMat(modelMat, eyeViewMat, lightViewMat);
        unidata.mViewLightPosition = eyeViewMat * glm::vec4(unidata.mLightPosition, 1.0f);
        trSetUniformData(&unidata);
#if ENABLE_SHADOW
        if (gOption.enableShadow)
        {
            trSetRenderTarget(shadowBuffer);
            trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
            trSetMat4(modelMat, MAT4_MODEL);
            trSetMat4(lightViewMat, MAT4_VIEW);
            trSetMat4(lightProjMat, MAT4_PROJ);
            for (auto obj : objs)
                obj->drawShadowMap();
            /* Skip floor in shadow map to speedup */

            trSetMat4(lightProjMat * lightViewMat * modelMat, MAT4_LIGHT_MVP);
            trBindTexture(shadowBuffer->getTexture(), TEXTURE_SHADOWMAP);
            trSetRenderTarget(windowBuffer);
        }
#endif
        trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
        trSetMat4(modelMat, MAT4_MODEL);
        trSetMat4(eyeViewMat, MAT4_VIEW);
        trSetMat4(eyeProjMat, MAT4_PROJ);
        for (auto obj : objs)
            obj->draw(ProgramId);

#if DRAW_FLOOR
        if (gOption.drawFloor)
        {
            trUnbindTextureAll();
#if ENABLE_SHADOW
            if (gOption.enableShadow)
            {
                trSetMat4(lightProjMat * lightViewMat, MAT4_LIGHT_MVP);
                trBindTexture(shadowBuffer->getTexture(), TEXTURE_SHADOWMAP);
            }
#endif
            trSetMat4(glm::mat4(1.0f), MAT4_MODEL);
            trBindTexture(&floorTex, TEXTURE_DIFFUSE);
            trDrawArrays(TR_TRIANGLES, floorMesh, &floorShader);
        }
#endif
#if ENABLE_SHADOW
        if (gOption.enableShadow)
            trBindTexture(nullptr, TEXTURE_SHADOWMAP);
#endif
#if ENABLE_SKYBOX
        if (gOption.enableSkybox)
            skybox.draw();
#endif
        w.swapBuffer();
        w.pollEvent();
        if (frame % 360 == 0)
        {
            double fps = 360 / truTimerGetSecondsFromClick();
            std::cout << "Current fps: " << fps << std::endl;
            truTimerClick();
        }
    }
    double fps = frame / truTimerGetSecondsFromBegin();
    std::cout << "Fps: " << fps << std::endl;

#if ENABLE_SHADOW
    delete shadowBuffer;
#endif
    return 0;
}
