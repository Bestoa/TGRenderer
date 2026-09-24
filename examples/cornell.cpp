// Cornell box demo: classic layout - 2 boxes in the back, a glass ball and a
// mirror ball in the front. One-bounce indirect light (color bleeding) via
// the reflection probe.
//
// Keys: I/O zoom, U/D camera height, [ ] indirect strength, A SSAO toggle,
// S screenshot, ESC quit.
#include <string>
#include <vector>
#include <cstdio>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "trapi.hpp"
#include "program.hpp"
#include "probe.hpp"
#include "window.hpp"
#include "objs.hpp"
#include "buffer.hpp"
#include "ssao.hpp"
#include "cornell_ray.hpp"
#include "utils.hpp"

#define WIDTH (900)
#define HEIGHT (700)

using namespace TGRenderer;

static TRObj *loadConf(const char *path)
{
    TRObj *o = new TRObj(path);
    if (!o->OK())
    {
        printf("load fail: %s\n", path);
        delete o;
        return nullptr;
    }
    return o;
}

static float gEyeZ = 2.6f;
static float gEyeY = 0.0f;
static float gIndirect = 0.4f;
static bool gNeedProbe = true;
static bool gNeedAO = true;          // screen-space AO follows the camera
static bool gSSAO = true;            // A toggles

static void kcb(int key)
{
    switch (key)
    {
        case SDL_SCANCODE_I: gEyeZ = glm::max(1.4f, gEyeZ - 0.1f); gNeedProbe = true; gNeedAO = true; break;
        case SDL_SCANCODE_O: gEyeZ = glm::min(4.0f, gEyeZ + 0.1f); gNeedProbe = true; gNeedAO = true; break;
        case SDL_SCANCODE_U: gEyeY = glm::min(0.9f, gEyeY + 0.1f); gNeedProbe = true; gNeedAO = true; break;
        case SDL_SCANCODE_D: gEyeY = glm::max(-0.9f, gEyeY - 0.1f); gNeedProbe = true; gNeedAO = true; break;
        case SDL_SCANCODE_LEFTBRACKET:
            gIndirect = glm::max(0.0f, gIndirect - 0.1f);
            printf("indirect = %.2f\n", gIndirect);
            break;
        case SDL_SCANCODE_RIGHTBRACKET:
            gIndirect = glm::min(2.0f, gIndirect + 0.1f);
            printf("indirect = %.2f\n", gIndirect);
            break;
        case SDL_SCANCODE_A:
            gSSAO = !gSSAO;
            gNeedAO = true;
            printf("SSAO %s\n", gSSAO ? "on" : "off");
            break;
        case SDL_SCANCODE_S:
        {
            mkdir("screenshots", 0755);
            char path[128];
            snprintf(path, sizeof(path), "screenshots/cornell_%lld.png",
                     (long long)time(nullptr));
            if (truSavePNG(path, trGetRenderTarget()))
                printf("saved %s\n", path);
            else
                printf("save failed: %s\n", path);
            break;
        }
    }
}

int main()
{
    TRWindow w(WIDTH, HEIGHT);
    if (!w.OK())
        return 1;
    w.setTitle("Cornell Box - TGRenderer");
    w.registerKeyEventCb(kcb);

    // room: walls, light | boxes (probe content) | balls (probe-excluded)
    // room | boxes (probe content) | balls (probe-excluded, ray-traced)
    TRObj *objs[] = {
        loadConf("res/conf/cornell_room.conf"),        // 0
        loadConf("res/conf/cornell_red.conf"),         // 1
        loadConf("res/conf/cornell_green.conf"),       // 2
        loadConf("res/conf/cornell_light.conf"),       // 3
        loadConf("res/conf/cornell_ceiling.conf"),     // 4
        loadConf("res/conf/cornell_box_tall.conf"),    // 5
        loadConf("res/conf/cornell_box_short.conf"),   // 6
    };
    const int ROOM_NUM = 5;
    const int BOX_NUM = 2;                     // boxes join the probe (indirect sources)
    const int OBJ_NUM = ROOM_NUM + BOX_NUM;
    for (int i = 0; i < OBJ_NUM; i++)
        if (!objs[i])
            return 1;
    // glass ball: probe/shadow/AO-excluded, drawn last with glass uniforms
    TRObj *ballGlass = loadConf("res/conf/cornell_ball_glass.conf");
    if (!ballGlass)
        return 1;
    // mirror (metal) ball: probe/AO-excluded like the glass ball, but it is
    // opaque - it joins the MAIN shadow map (deep shadow)
    TRObj *ballMirror = loadConf("res/conf/cornell_ball_mirror.conf");
    if (!ballMirror)
        return 1;

    TextureMapPhongShader shader;
    PhongUniformData unidata;
    unidata.mLightPosition = glm::vec3(0.0f, 0.9f, 0.0f);   // at the panel
    unidata.mAmbientStrength = 0.22f;
    unidata.mLightColor = glm::vec3(1.45f);
    unidata.mLightAttenuation = 1.0f;
    unidata.mIndirectStrength = 0.0f;                        // base value; the loop reapplies gIndirect
    unidata.mReflectivity = 0.0f;
    unidata.mProbePosition = glm::vec3(0.0f, -0.76f, 0.32f); // matches the probe below
    trSetUniformData(&unidata);

    glm::vec3 eye(0.0f, gEyeY, gEyeZ);
    glm::mat4 projMat = glm::perspective(glm::radians(55.0f),
            (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

    // ---- shadow map: depth from the light, looking down (fov 90 covers the
    // whole floor incl. corners; walls are beyond the frustum and stay lit) ----
    TRBuffer *windowBuffer0 = trGetRenderTarget();
    TRTextureBuffer *shadowBuffer = new TRTextureBuffer(1024, 1024);
    glm::mat4 lightViewMat = glm::lookAt(unidata.mLightPosition,
            glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::mat4 lightProjMat = glm::perspective(glm::radians(90.0f), 1.0f, 0.05f, 4.5f);
    {
        trSetRenderTarget(shadowBuffer);
        trClearColor3f(1.0f, 1.0f, 1.0f);    // out of frustum = lit
        trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
        trSetMat4(glm::mat4(1.0f), MAT4_MODEL);
        trSetMat4(lightViewMat, MAT4_VIEW);
        trSetMat4(lightProjMat, MAT4_PROJ);
        for (int o = 0; o < OBJ_NUM; o++)
            objs[o]->drawShadowMap();
        ballMirror->drawShadowMap();         // opaque: deep shadow in the main map
        trSetRenderTarget(windowBuffer0);    // restore: the loop expects the window
    }
    trSetMat4(lightProjMat * lightViewMat, MAT4_LIGHT_MVP);

    // ---- glass shadow map: the ball alone, from the light. Translucent
    // occluder: applied with a weaker factor so light passes through. ----
    TRTextureBuffer *glassShadowBuffer = new TRTextureBuffer(1024, 1024);
    {
        trSetRenderTarget(glassShadowBuffer);
        trClearColor3f(1.0f, 1.0f, 1.0f);
        trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
        trSetMat4(glm::mat4(1.0f), MAT4_MODEL);
        trSetMat4(lightViewMat, MAT4_VIEW);
        trSetMat4(lightProjMat, MAT4_PROJ);
        ballGlass->drawShadowMap();
        trSetRenderTarget(windowBuffer0);
    }

    // ---- glass caustics texture: the ball acts as a lens and focuses the
    // transmitted light. Procedural approximation in the shadow map's light
    // space: a bright spot under the ball plus a发散 ring (the floor sits
    // in front of the focal point, so the caustic is spot + ring). ----
    TRTexture *causticTex = new TRTexture(1024, 1024);
    {
        // caustic center: the floor point right under the ball, in light uv
        glm::vec4 cc = lightProjMat * lightViewMat * glm::vec4(-0.15f, -1.0f, 0.3f, 1.0f);
        float cu = cc.x / cc.w / 2.0f + 0.5f;
        float cv = cc.y / cc.w / 2.0f + 0.5f;
        // light-space scale: how many uv units per world unit at the floor
        // (perspective at distance ~1.9, fov 90: half-extent = 1.9 world)
        float uvPerWorld = 1.0f / (2.0f * 1.9f);
        float spotSigma = 0.055f * uvPerWorld * 1024.0f;   // in texels
        float ringR = 0.16f * uvPerWorld * 1024.0f;
        float ringW = 0.045f * uvPerWorld * 1024.0f;
        float *b = causticTex->getBuffer();
        for (int y = 0; y < 1024; y++)
        {
            for (int x = 0; x < 1024; x++)
            {
                float dx = x / 1023.0f - cu;
                float dy = y / 1023.0f - cv;
                float d = sqrtf(dx * dx + dy * dy) * 1024.0f;   // texels
                // central focused spot
                float spot = expf(-d * d / (2.0f * spotSigma * spotSigma)) * 0.85f;
                //发散 ring where the rim rays land
                float dr = (d - ringR) / ringW;
                float ring = expf(-dr * dr / 2.0f) * 0.35f;
                float v = glm::min(spot + ring, 1.0f);
                b[(y * 1024 + x) * 3] = v;
                b[(y * 1024 + x) * 3 + 1] = v;
                b[(y * 1024 + x) * 3 + 2] = v;
            }
        }
    }

    // ---- 2x supersampling: render into msaa buffer, box-filter down ----
    TRBuffer *msaaBuffer = new TRBuffer(WIDTH * 2, HEIGHT * 2);

    // ---- SSAO texture (route B): the shader multiplies the ambient and
    // indirect terms by this map, sampled at the fragment screen position.
    // Filled from the previous frame's depth (1-frame lag, static scene).
    TRTexture *aoTex = new TRTexture(WIDTH, HEIGHT);
    {
        float *b = aoTex->getBuffer();
        for (int i = 0; i < WIDTH * HEIGHT * 3; i++)
            b[i] = 1.0f;                     // no occlusion until computed
    }

    TRReflectionProbe *probe = nullptr;

    int frame = 0;
    truTimerBegin();
    while (!w.shouldStop() && frame++ < 100000)
    {
        glm::vec3 eye(0.0f, gEyeY, gEyeZ);
        glm::mat4 viewMat = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
        unidata.mViewLightPosition = viewMat * glm::vec4(unidata.mLightPosition, 1.0f);
        unidata.mEyeWorldPosition = eye;
        unidata.mIndirectStrength = gIndirect;
        trSetUniformData(&unidata);

        // grab the window buffer BEFORE the probe pass switches targets
        TRBuffer *windowBuffer = trGetRenderTarget();

        // ---- probe pass: walls + light + boxes (indirect sources), balls excluded ----
        if (!probe)
            probe = new TRReflectionProbe(glm::vec3(0.0f, -0.76f, 0.32f), 256);
        if (gNeedProbe && probe->OK())
        {
            gNeedProbe = false;
            trBindCubeTexture(nullptr);
            trBindTexture(nullptr, TEXTURE_SHADOWMAP);   // no shadows inside the probe
            trBindTexture(nullptr, TEXTURE_SHADOWMAP_GLASS);
            trBindTexture(nullptr, TEXTURE_CAUSTIC);
            trBindTexture(nullptr, TEXTURE_AO);          // no AO inside the probe
            for (size_t i = 0; i < 6; i++)
            {
                trSetRenderTarget(probe->getFaceBuffer(i));
                trClearColor3f(0.0f, 0.0f, 0.0f);
                trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
                trSetMat4(glm::mat4(1.0f), MAT4_MODEL);
                trSetMat4(probe->getFaceViewMat(i), MAT4_VIEW);
                trSetMat4(probe->getFaceProjMat(), MAT4_PROJ);
                unidata.mViewLightPosition = probe->getFaceViewMat(i) * glm::vec4(unidata.mLightPosition, 1.0f);
                trSetUniformData(&unidata);
                for (int o = 0; o < OBJ_NUM; o++)
                {
                    // skip the light panel (o==3): its emissive white would
                    // flood the up-face, and upward indirect samples (the
                    // floor) blow out. The full ceiling quad sits right
                    // behind it, so no hole is left.
                    if (o == 3)
                        continue;
                    objs[o]->draw(3);
                }
            }
            probe->updateIrradiance();
        }

        // ---- main pass: supersampled ----
        trSetRenderTarget(msaaBuffer);
        trSetMat4(glm::mat4(1.0f), MAT4_MODEL);
        trSetMat4(viewMat, MAT4_VIEW);
        trSetMat4(projMat, MAT4_PROJ);
        unidata.mViewLightPosition = viewMat * glm::vec4(unidata.mLightPosition, 1.0f);
        unidata.mIndirectStrength = gIndirect;
        trSetUniformData(&unidata);
        // irradiance (blurred) cube: only the indirect term samples the cube
        // in this scene (mReflectivity = 0), so bind the low-frequency one
        trBindCubeTexture(probe->OK() ? probe->getIrradianceTexture() : nullptr);
        trBindTexture(shadowBuffer->getTexture(), TEXTURE_SHADOWMAP);
        trBindTexture(glassShadowBuffer->getTexture(), TEXTURE_SHADOWMAP_GLASS);
        trBindTexture(causticTex, TEXTURE_CAUSTIC);
        trBindTexture(gSSAO ? aoTex : nullptr, TEXTURE_AO);

        trClearColor3f(0.02f, 0.02f, 0.02f);
        trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
        for (int o = 0; o < OBJ_NUM; o++)
            objs[o]->draw(3);

        // ---- SSAO (route B): compute from this frame's depth, upload to the
        // AO texture for the NEXT frame (1-frame lag, static scene) ----
        static std::vector<float> ao;
        static TRTextureBuffer *normalBuf = nullptr;
        static ViewNormalShader normalShader;
        if (gSSAO && gNeedAO)
        {
            gNeedAO = false;
            if (!normalBuf)
                normalBuf = new TRTextureBuffer(WIDTH * 2, HEIGHT * 2);
            // exact view-space normals for the AO pass
            trSetRenderTarget(normalBuf);
            trClearColor3f(0.0f, 0.0f, 0.0f);
            trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT);
            trSetMat4(glm::mat4(1.0f), MAT4_MODEL);
            trSetMat4(viewMat, MAT4_VIEW);
            trSetMat4(projMat, MAT4_PROJ);
            for (int o = 0; o < OBJ_NUM; o++)
                objs[o]->drawRaw(&normalShader);
            trSetRenderTarget(msaaBuffer);   // restore for the next main pass

            ssaoCompute(msaaBuffer, normalBuf->getTexture(), projMat, ao,
                        WIDTH, HEIGHT,
                        0.18f /*radius*/, 0.015f /*bias*/, 0.3f /*intensity*/, 0.72f /*floor*/);
            // upload, flipping rows: texture v=1 is the screen top while the
            // ao vector is indexed by image rows (top-down)
            float *b = aoTex->getBuffer();
            for (int y = 0; y < HEIGHT; y++)
            {
                float *dst = b + (HEIGHT - 1 - y) * WIDTH * 3;
                for (int x = 0; x < WIDTH; x++)
                {
                    float v = ao[y * WIDTH + x];
                    dst[x * 3] = dst[x * 3 + 1] = dst[x * 3 + 2] = v;
                }
            }
        }

        // ---- the two balls: Whitted-style ray-scene intersection
        // (CornellRayShader). The reflected / refracted rays are intersected
        // with the real scene geometry (room, boxes, the other ball), so the
        // balls see each other and the boxes with correct parallax - no
        // probe-cube direction sampling, no back-face maps needed. Drawn
        // after the SSAO block so the AO depth does not contain them. ----
        {
            static CornellRayShader rayShader;
            rayShader.mLightPos = unidata.mLightPosition;
            rayShader.mLightColor = unidata.mLightColor;
            rayShader.mAmbient = 0.45f;
            rayShader.mAtten = unidata.mLightAttenuation;
            rayShader.mEyeWorld = unidata.mEyeWorldPosition;
            rayShader.mShadowMap = shadowBuffer->getTexture();
            rayShader.mLightVP = lightProjMat * lightViewMat;

            trSetMat4(glm::mat4(1.0f), MAT4_MODEL);
            trSetMat4(viewMat, MAT4_VIEW);
            trSetMat4(projMat, MAT4_PROJ);

            // glass ball: refraction + reflection. IOR 1.3: the ball acts as
            // a real lens (inverted mini-image), so the floor/back-wall
            // junction shows inside the ball; at 1.1 (magnifier regime) the
            // straight-through footprint is too narrow to contain it.
            rayShader.mIOR = 1.5f;
            rayShader.mSelfIndex = 1;
            ballGlass->drawRaw(&rayShader);
            // metal ball: pure mirror reflection
            rayShader.mIOR = 1.0f;
            rayShader.mSelfIndex = 2;
            ballMirror->drawRaw(&rayShader);
        }

        // ---- downsample 2x2 -> window buffer ----
        {
            uint8_t *src = (uint8_t *)msaaBuffer->getRawData();
            uint8_t *dst = (uint8_t *)windowBuffer->getRawData();
            if (!src || !dst)
            {
                printf("downsample skip: src=%p dst=%p (frame %d)\n", src, dst, frame);
                w.swapBuffer();
                continue;
            }
            int sw = WIDTH * 2;
            for (int y = 0; y < HEIGHT; y++)
            {
                for (int x = 0; x < WIDTH; x++)
                {
                    uint8_t *s0 = src + ((y * 2) * sw + x * 2) * 4;
                    uint8_t *s1 = s0 + 4;
                    uint8_t *s2 = s0 + sw * 4;
                    uint8_t *s3 = s2 + 4;
                    uint8_t *d = dst + (y * WIDTH + x) * 4;
                    d[0] = (s0[0] + s1[0] + s2[0] + s3[0] + 2) / 4;
                    d[1] = (s0[1] + s1[1] + s2[1] + s3[1] + 2) / 4;
                    d[2] = (s0[2] + s1[2] + s2[2] + s3[2] + 2) / 4;
                    d[3] = 255;
                }
            }
        }

        // back to the window target so the next frame (and the S screenshot)
        // sees the window buffer, not the msaa buffer
        trSetRenderTarget(windowBuffer);

        w.swapBuffer();
        w.pollEvent();

        // real-time FPS: rolling 0.5s window, printed to stdout and shown
        // in the window title
        static Uint32 fpsT0 = SDL_GetTicks();
        static int fpsFrames = 0;
        fpsFrames++;
        Uint32 now = SDL_GetTicks();
        if (now - fpsT0 >= 500)
        {
            double fps = fpsFrames * 1000.0 / (now - fpsT0);
            char title[128];
            snprintf(title, sizeof(title), "Cornell Box - TGRenderer | %.1f FPS", fps);
            w.setTitle(title);
            printf("FPS: %.1f\n", fps);
            fpsT0 = now;
            fpsFrames = 0;
        }
    }

    for (int i = 0; i < OBJ_NUM; i++)
        delete objs[i];
    delete probe;
    delete shadowBuffer;
    delete glassShadowBuffer;
    delete causticTex;
    delete msaaBuffer;
    delete ballGlass;
    delete ballMirror;
    delete aoTex;
    return 0;
}
