#include <vector>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "trapi.hpp"
#include "window.hpp"
#include "objs.hpp"
#include "utils.hpp"
#include "program.hpp"
#include "skybox.hpp"
#include "probe.hpp"

#define WIDTH (1280)
#define HEIGHT (720)

#define TWIDTH (1024)
#define THEIGHT (1024)

#define ENABLE_SHADOW 1
#define DRAW_FLOOR 1
#define ENABLE_SKYBOX 1

#define ENABLE_PROBE 1
#define PROBE_FACE_SIZE (256)

#if ENABLE_SHADOW
bool gNeedRedrawShadowMap = true;
#endif

// The probe content changes only when the scene (floor/light/model) changes,
// never when the eye moves.
bool gNeedRedrawProbe = true;

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

class Option
{
    public:
        int ProgramId = 3;
        bool enableSkybox = false;
        bool enableReflection = false;
        bool enableShadow = false;
        bool drawFloor = true;
        bool wireframeMode = false;
        bool glassMode = false;
        bool rotateModel = false;
        bool rotateEye = false;
        bool rotateLight = false;
        bool zoomIn = false;
        bool zoomOut = false;
        bool up = false;
        bool down = false;
        bool resetView = false;
};

Option gOption;

// Glass ball index of refraction, adjustable at runtime with [ and ]
float gGlassIOR = 1.5f;

class View
{
    public:
        float distanceInXZPlane = 2.5f;
        float Y = 0.75f;
};

View gViewDefault;
View gView;

const int endFrame = 36000;

std::string buildWindowTitle(double fps = 0.0)
{
    std::ostringstream oss;
    oss << "TopGun Renderer"
        << " | program " << gOption.ProgramId
        << " | view(" << gView.distanceInXZPlane << ", " << gView.Y << ")";

    if (gOption.enableSkybox)
        oss << " | skybox";
    if (gOption.enableReflection)
        oss << " | reflection";
    if (gOption.enableShadow)
        oss << " | shadow";
    if (gOption.drawFloor)
        oss << " | floor";
    if (gOption.wireframeMode)
        oss << " | wireframe";
    if (gOption.glassMode)
        oss << " | glass";
    if (gOption.rotateModel)
        oss << " | model-rotate";
    if (gOption.rotateEye)
        oss << " | eye-rotate";
    if (gOption.rotateLight)
        oss << " | light-rotate";
    if (fps > 0.0)
        oss << " | fps " << fps;

    return oss.str();
}

void kcb(int key)
{
    switch (key)
    {
        case SDL_SCANCODE_B:
            gOption.enableSkybox = !gOption.enableSkybox;
            break;
        case SDL_SCANCODE_T:
            gOption.glassMode = !gOption.glassMode;
            // exclusive with reflection: blending + probe reflection combos
            // produce hard-to-explain renders in this demo
            if (gOption.glassMode)
                gOption.enableReflection = false;
            break;
        case SDL_SCANCODE_LEFTBRACKET:
            // glass IOR dial, watch the image flip as it passes ~1.25
            gGlassIOR = glm::max(1.05f, gGlassIOR - 0.05f);
            std::cout << "IOR = " << gGlassIOR << std::endl;
            break;
        case SDL_SCANCODE_RIGHTBRACKET:
            gGlassIOR = glm::min(2.5f, gGlassIOR + 0.05f);
            std::cout << "IOR = " << gGlassIOR << std::endl;
            break;
        case SDL_SCANCODE_N:
            gOption.enableReflection = !gOption.enableReflection;
            if (gOption.enableReflection)
                gOption.glassMode = false;
            break;
        case SDL_SCANCODE_S:
            gOption.enableShadow = !gOption.enableShadow;
            break;
        case SDL_SCANCODE_F:
            gOption.drawFloor = !gOption.drawFloor;
            gNeedRedrawProbe = true;
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
        case SDL_SCANCODE_U:
            gOption.up = true;
            break;
        case SDL_SCANCODE_D:
            gOption.down = true;
            break;
        case SDL_SCANCODE_R:
            gOption.resetView = true;
            break;
        case SDL_SCANCODE_C:
            gOption.ProgramId++;
            if (gOption.ProgramId == 4)
                gOption.ProgramId = 0;
            break;
    }
}

void reCalcMat(glm::mat4 &modelMat, glm::mat4 &eyeViewMat
#if ENABLE_SHADOW
        ,glm::mat4 &lightViewMat
#endif
        )
{
    static int rotateM = 0, rotateE = 0, rotateL = 0;

    bool reCalcViewMat = false;
    if (gOption.rotateModel)
    {
        rotateM++;
        modelMat = glm::rotate(glm::mat4(1.0f), glm::radians(1.0f * rotateM), glm::vec3(0.0f, 1.0f, 0.0f));
#if ENABLE_SHADOW
        gNeedRedrawShadowMap = true;
#endif
        gNeedRedrawProbe = true;
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
        gView.distanceInXZPlane -= 0.1f;
    }

    if (gOption.zoomOut)
    {
        gOption.zoomOut = false;
        reCalcViewMat = true;
        gView.distanceInXZPlane += 0.1f;
    }

    if (gOption.up)
    {
        gOption.up = false;
        reCalcViewMat = true;
        gView.Y += 0.1f;
    }

    if (gOption.down)
    {
        gOption.down = false;
        reCalcViewMat = true;
        gView.Y -= 0.1f;
    }

    if (gOption.resetView)
    {
        gOption.resetView = false;
        gView = gViewDefault;
        reCalcViewMat = true;;
    }

    if (reCalcViewMat)
    {
        float degree = glm::radians(1.0f * rotateE);
        eyeViewMat = glm::lookAt(
                glm::vec3(gView.distanceInXZPlane * glm::sin(degree), gView.Y, gView.distanceInXZPlane * glm::cos(degree)),
                glm::vec3(0,0,0),
                glm::vec3(0,1,0));
    }

    if (gOption.rotateLight)
    {
        rotateL++;
        float degree = glm::radians(1.0f * rotateL);
#if ENABLE_SHADOW
        lightViewMat = glm::lookAt(
                glm::vec3(glm::sin(degree), 1, glm::cos(degree)),
                glm::vec3(0,0,0),
                glm::vec3(0,1,0));
        gNeedRedrawShadowMap = true;
#endif
        unidata.mLightPosition = glm::vec3(glm::sin(degree), 1.0f, glm::cos(degree));
        // the floor shading inside the probe follows the light
        gNeedRedrawProbe = true;
    }
}

void dumpInfo()
{
    std::cout << "Mode: ";
    std::cout << "program id = " << gOption.ProgramId << " ";
    if (gOption.enableSkybox)
        std::cout << "skybox ";
    if (gOption.enableReflection)
        std::cout << "reflection ";
    if (gOption.enableShadow)
        std::cout << "shadow ";
    if (gOption.drawFloor)
        std::cout << "floor ";
    if (gOption.wireframeMode)
        std::cout << "wireframe ";
    if (gOption.glassMode)
        std::cout << "glass ";
    if (gOption.rotateModel)
        std::cout << "model-rotate ";
    if (gOption.rotateEye)
        std::cout << "eye-rotate ";
    if (gOption.rotateLight)
        std::cout << "light-rotate ";
    std::cout << std::endl;
}

int main(int argc, char *argv[])
{
    // Default demo: a reflective sphere in front of the skybox
    const char *configFiles[16];
    int configFileNum = 0;
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " obj_config..." << std::endl;
        std::cout << "No config given, using the default sphere demo." << std::endl;
        configFiles[configFileNum++] = "res/conf/sphere.conf";
    } else {
        for (int i = 1; i < argc && configFileNum < 16; i++)
            configFiles[configFileNum++] = argv[i];
    }
    TRWindow w(WIDTH, HEIGHT);
    if (!w.OK())
        return 1;

    w.registerKeyEventCb(kcb);
    w.setTitle(buildWindowTitle());

#if ENABLE_SHADOW
    TRBuffer *windowBuffer = trGetRenderTarget();
    TRTextureBuffer *shadowBuffer = new TRTextureBuffer(TWIDTH, THEIGHT);
#endif

    std::vector<std::shared_ptr <TRObj>> objs;
    for (int i = 0; i < configFileNum; i++)
    {
        std::shared_ptr<TRObj> obj(new TRObj(configFiles[i]));
        if (obj->OK())
            objs.push_back(obj);
    }

    if (objs.size() == 0)
        abort();

    glm::mat4 eyeViewMat = glm::lookAt(
            glm::vec3(0, gView.Y, gView.distanceInXZPlane), // Camera is at (0,0.75,1.5), in World Space
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
    // Table-sized floor (not an infinite world plane): through the glass
    // ball the far field beyond the table edge shows the skybox horizon,
    // which matches the real-life look of a ball on a table
    truCreateFloorPlane(floorMesh, floorHeight, 3.0f);
    TRTexture floorTex("res/tex/floor_diffuse.tga");
    if (!floorTex.OK())
        abort();
#endif

#if ENABLE_SKYBOX
    TRSkyBox *pSkybox = nullptr;
#endif
#if ENABLE_PROBE
    TRReflectionProbe *pProbe = nullptr;
#endif

    glm::mat4 modelMat(1.0f);
    unidata.mLightPosition = glm::vec3(0.0f, 1.0f, 1.0f);
    unidata.mReflectivity = 0.8f;
    // Fresnel: weaker reflection head-on, stronger at grazing angles
    unidata.mFresnelFactor = 0.25f;
    // Faces the light can not reach should not show a strong mirror image
    unidata.mReflectShadowMod = true;

    int frame = 0;
    int frame_fps = 0;
    truTimerBegin();
    double titleTime = truTimerGetSecondsFromBegin();
    while (!w.shouldStop() && frame++ < endFrame)
    {
        reCalcMat(modelMat, eyeViewMat
#if ENABLE_SHADOW
                ,lightViewMat
#endif
                );
        unidata.mViewLightPosition = eyeViewMat * glm::vec4(unidata.mLightPosition, 1.0f);
        // Eye position in world space for the environment reflection
        unidata.mEyeWorldPosition = glm::vec3(glm::inverse(eyeViewMat) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        trSetUniformData(&unidata);
#if ENABLE_SHADOW
        if (gOption.enableShadow && gNeedRedrawShadowMap)
        {
            gNeedRedrawShadowMap = false;
            // Get window buffer again since we enable resize event
            windowBuffer = trGetRenderTarget();
            trSetRenderTarget(shadowBuffer);
            trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
            trSetMat4(modelMat, MAT4_MODEL);
            trSetMat4(lightViewMat, MAT4_VIEW);
            trSetMat4(lightProjMat, MAT4_PROJ);
            for (auto obj : objs)
                obj->drawShadowMap();
            /* Skip floor in shadow map to speedup */

            trSetRenderTarget(windowBuffer);
        }
        if (gOption.enableShadow)
        {
            // We must reset the light mvp here
            trSetMat4(lightProjMat * lightViewMat * modelMat, MAT4_LIGHT_MVP);
            trBindTexture(shadowBuffer->getTexture(), TEXTURE_SHADOWMAP);
        }
#endif
#if ENABLE_PROBE
        // Reflection probe: render the scene (floor + skybox, the reflective
        // objects themselves are skipped) into 6 cube faces from the model
        // center, so the reflection picks up scene geometry too. The glass
        // ball does not use the probe: its far field is the static skybox
        // and its floor is the analytic plane, so no probe work for T mode.
        if (gOption.enableReflection && gNeedRedrawProbe
                && !objs.empty() && objs.front()->OK()
#if ENABLE_SKYBOX
                && pSkybox && pSkybox->OK()
#endif
                )
        {
            gNeedRedrawProbe = false;
            if (!pProbe)
                pProbe = new TRReflectionProbe(glm::vec3(0.0f), PROBE_FACE_SIZE);
            if (pProbe->OK())
            {
                // no environment reflection / shadow inside the probe
                trBindCubeTexture(nullptr);
#if ENABLE_SHADOW
                trBindTexture(nullptr, TEXTURE_SHADOWMAP);
#endif
                // The Phong light position uniform is precomputed in view
                // space, recompute it with the probe face view matrix or the
                // floor inside the probe gets lit from a wrong direction
                // (e.g. fully black when it ends up backlit).
                glm::vec3 viewLightBackup = unidata.mViewLightPosition;
                for (size_t i = 0; i < 6; i++)
                {
                    trSetRenderTarget(pProbe->getFaceBuffer(i));
                    trClearColor3f(0.1f, 0.1f, 0.1f);
                    trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
                    trSetMat4(glm::mat4(1.0f), MAT4_MODEL);
                    trSetMat4(pProbe->getFaceViewMat(i), MAT4_VIEW);
                    trSetMat4(pProbe->getFaceProjMat(), MAT4_PROJ);
                    unidata.mViewLightPosition = pProbe->getFaceViewMat(i) * glm::vec4(unidata.mLightPosition, 1.0f);
#if DRAW_FLOOR
                    if (gOption.drawFloor)
                    {
                        trBindTexture(&floorTex, TEXTURE_DIFFUSE);
                        trDrawArrays(TR_TRIANGLES, floorMesh, &floorShader);
                    }
#endif
#if ENABLE_SKYBOX
                    // the skybox part of the probe stays exact: it is at
                    // infinity, independent of the probe position
                    pSkybox->draw();
#endif
                }
                unidata.mViewLightPosition = viewLightBackup;
                // back to the window buffer
                trSetRenderTarget(windowBuffer);
#if ENABLE_SHADOW
                // the probe pass unbound the shadow map above, but the objs
                // below still need it: without this rebind they would lose
                // their shadows on every probe re-render frame
                if (gOption.enableShadow)
                    trBindTexture(shadowBuffer->getTexture(), TEXTURE_SHADOWMAP);
#endif
            }
        }
#endif
        // do clear color again since we enable resize event
        trClearColor3f(0.1, 0.1, 0.1);
        trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
        trSetMat4(modelMat, MAT4_MODEL);
        trSetMat4(eyeViewMat, MAT4_VIEW);
        trSetMat4(eyeProjMat, MAT4_PROJ);
#if ENABLE_SKYBOX
        // Reflection needs the skybox cube textures even when the skybox
        // itself is not drawn.
        if (!pSkybox && (gOption.enableSkybox || gOption.enableReflection || gOption.glassMode))
            pSkybox = new TRSkyBox(gCubeTextureNames);
#endif

        if (gOption.glassMode && gGlassIOR > 1.0f && !objs.empty())
        {
            // Back face maps for the mesh refraction (glmark2 style):
            // render the objects with front faces culled from the CURRENT
            // view into a normal map and a view-axis depth map. The glass
            // shader then looks these up per fragment.
            static BackNormalShader backNormalShader;
            static BackDepthShader backDepthShader;
            static TRTextureBuffer *backNormalTarget = nullptr;
            static TRTextureBuffer *backDepthTarget = nullptr;
            static int backMapW = 0, backMapH = 0;
            int bw = WIDTH / 2, bh = HEIGHT / 2;
            if (backMapW != bw || backMapH != bh)
            {
                if (backNormalTarget) delete backNormalTarget;
                if (backDepthTarget) delete backDepthTarget;
                backNormalTarget = new TRTextureBuffer(bw, bh);
                backDepthTarget = new TRTextureBuffer(bw, bh);
                backMapW = bw;
                backMapH = bh;
            }
            TRBuffer *current = trGetRenderTarget();
            for (int pass = 0; pass < 2; pass++)
            {
                trSetRenderTarget(pass == 0 ? backNormalTarget : backDepthTarget);
                trClearColor3f(0.0f, 0.0f, 0.0f);
                trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
                trCullFaceMode(TR_CW);   // cull front faces, keep back faces
                for (auto obj : objs)
                    obj->drawRaw(pass == 0
                            ? static_cast<TGRenderer::Shader *>(&backNormalShader)
                            : static_cast<TGRenderer::Shader *>(&backDepthShader));
                trCullFaceMode(TR_NONE);
            }
            trSetRenderTarget(current);
            trBindTexture(backNormalTarget->getTexture(), TEXTURE_BACK_NORMAL);
            trBindTexture(backDepthTarget->getTexture(), TEXTURE_BACK_DEPTH);
        }

#if ENABLE_PROBE
        if ((gOption.enableReflection || gOption.glassMode) && pProbe && pProbe->OK())
            // dynamic environment: floor + skybox
            trBindCubeTexture(pProbe->getCubeTexture());
        else
#endif
#if ENABLE_SKYBOX
        if ((gOption.enableReflection || gOption.glassMode) && pSkybox && pSkybox->OK())
            // fallback: static skybox only
            trBindCubeTexture(pSkybox->getCubeTexture());
        else
            trBindCubeTexture(nullptr);
#endif
#if ENABLE_SKYBOX
        // Draw the skybox first as the background: transparent geometry does
        // not write depth, so anything drawn after it would show through the
        // glass ball only if the skybox is already there to be blended with.
        if (gOption.enableSkybox && pSkybox && pSkybox->OK())
            pSkybox->draw();
#endif
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
        // trUnbindTextureAll() above cleared the cube binding too, but the
        // sphere is drawn after the floor now: rebind or the reflection /
        // refraction would silently disappear.
        if ((gOption.enableReflection || gOption.glassMode) && pProbe && pProbe->OK())
            trBindCubeTexture(pProbe->getCubeTexture());
        else if ((gOption.enableReflection || gOption.glassMode) && pSkybox && pSkybox->OK())
            trBindCubeTexture(pSkybox->getCubeTexture());
#endif
        if (gOption.glassMode && gGlassIOR > 1.0f)
        {
            // Refraction glass ball, drawn last, after all opaque geometry.
            // The doubly refracted environment replaces the Phong body (see
            // TextureMapPhongShader), so no alpha blending is involved: the
            // "transparency" comes from sampling the environment along the
            // refracted exit ray. Single pass, no culling.
            float dialBackup = unidata.mReflectivity;
            float f0Backup = unidata.mFresnelFactor;
            unidata.mReflectivity = 0.9f;
            unidata.mFresnelFactor = 0.08f;
            unidata.mIOR = gGlassIOR;
            // analytic refraction scene: follow the floor visibility toggle
            // (F), no invisible floor in the transmitted image
            if (gOption.drawFloor)
            {
                unidata.mRefractionFloorY = floorHeight;
                trBindTexture(&floorTex, TEXTURE_REFRACTION);
            }
            else
            {
                unidata.mRefractionFloorY = 0.0f;
                trBindTexture(nullptr, TEXTURE_REFRACTION);
            }
            unidata.mRefractionFloorSize = 2.0f;
            unidata.mRefractionFloorExtent = 3.0f;   // matches the table floor mesh (width 3)
            for (auto obj : objs)
                obj->draw(gOption.ProgramId);
            unidata.mIOR = 1.0f;
            unidata.mRefractionFloorY = 0.0f;
            trBindTexture(nullptr, TEXTURE_REFRACTION);
            unidata.mReflectivity = dialBackup;
            unidata.mFresnelFactor = f0Backup;
        } else {
            for (auto obj : objs)
                obj->draw(gOption.ProgramId);
        }
        w.swapBuffer();

        double current = truTimerGetSecondsFromClick();
        double now = truTimerGetSecondsFromBegin();
        frame_fps++;
        if (now - titleTime > 0.2)
        {
            w.setTitle(buildWindowTitle(current > 0.0 ? frame_fps / current : 0.0));
            titleTime = now;
        }
        if (current > 5.0f)
        {
            std::cout << "Current fps in last 5s: " << frame_fps / current << std::endl;
            dumpInfo();
            frame_fps = 0;
            truTimerClick();
            w.setTitle(buildWindowTitle());
            titleTime = truTimerGetSecondsFromBegin();
        }

        w.pollEvent();
    }
    double fps = frame / truTimerGetSecondsFromBegin();
    std::cout << "Fps: " << fps << std::endl;

#if ENABLE_SKYBOX
    if (pSkybox)
        delete pSkybox;
#endif
#if ENABLE_PROBE
    if (pProbe)
        delete pProbe;
#endif
#if ENABLE_SHADOW
    delete shadowBuffer;
#endif
    return 0;
}
