#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.hpp"
#include "trapi.hpp"
#include "program.hpp"
#include "utils.hpp"
#include "window.hpp"

using namespace TGRenderer;

namespace
{
    constexpr int WIDTH = 1280;
    constexpr int HEIGHT = 800;
    constexpr float PI = 3.14159265358979323846f;

    struct Body
    {
        const char *name;
        float orbitRadius;
        float orbitEccentricity;
        float orbitInclinationDeg;
        float orbitAscendingNodeDeg;
        float orbitPeriapsisDeg;
        float radius;
        float orbitDays;
        float spinDays;
        float axialTiltDeg;
        float phaseDeg;
        const char *texturePath;
        bool hasRing;
        float ringInnerRadius;
        float ringOuterRadius;
        glm::vec3 ringColor;
    };

    struct SimState
    {
        float days = 0.0f;
        float speed = 3.0f;
        float spinScale = 0.08f;
        bool paused = false;
    };

    class BackgroundShader : public TextureMapShader
    {
        public:
            bool fragment(FSInData *fsdata, float color[]) override
            {
                glm::vec2 texCoord = fsdata->getVec2(SH_TEXCOORD);
                textureCoordWrap(texCoord);
                float *sample = texture2D(TEXTURE_DIFFUSE, texCoord.x, texCoord.y);
                color[0] = glm::min(sample[0] * 0.65f, 1.0f);
                color[1] = glm::min(sample[1] * 0.65f, 1.0f);
                color[2] = glm::min(sample[2] * 0.8f, 1.0f);
                return true;
            }
    };

    const std::array<Body, 9> gBodies =
    {{
        { "Sun",     0.0f,  0.0f,   0.00f,   0.00f,   0.00f,  2.30f,      1.0f,   25.0f,  7.25f,   0.0f, "res/tex/solar_system/sun.jpg",     false, 0.0f, 0.0f, glm::vec3(0.0f) },
        { "Mercury", 4.0f,  0.2056f, 7.00f,  48.33f,  29.12f,  0.30f,     88.0f,   58.6f,  0.03f,  10.0f, "res/tex/solar_system/mercury.jpg", false, 0.0f, 0.0f, glm::vec3(0.0f) },
        { "Venus",   5.8f,  0.0068f, 3.39f,  76.68f,  54.88f,  0.48f,    225.0f, -243.0f, 177.0f,  75.0f, "res/tex/solar_system/venus.jpg",   false, 0.0f, 0.0f, glm::vec3(0.0f) },
        { "Earth",   8.0f,  0.0167f, 0.00f, -11.26f, 114.21f,  0.52f,    365.0f,    1.0f, 23.50f, 135.0f, "res/tex/solar_system/earth.jpg",   false, 0.0f, 0.0f, glm::vec3(0.0f) },
        { "Mars",   10.5f,  0.0934f, 1.85f,  49.58f, 286.50f,  0.40f,    687.0f,    1.03f, 25.0f, 195.0f, "res/tex/solar_system/mars.jpg",    false, 0.0f, 0.0f, glm::vec3(0.0f) },
        { "Jupiter",14.5f,  0.0489f, 1.30f, 100.46f, 273.87f,  1.15f,   4333.0f,    0.41f,  3.1f, 250.0f, "res/tex/solar_system/jupiter.jpg", false, 0.0f, 0.0f, glm::vec3(0.0f) },
        { "Saturn", 19.0f,  0.0565f, 2.49f, 113.67f, 339.39f,  0.98f,  10759.0f,    0.45f, 26.7f, 305.0f, "res/tex/solar_system/saturn.jpg",   true, 1.45f, 2.20f, glm::vec3(0.85f, 0.80f, 0.62f) },
        { "Uranus", 23.0f,  0.0472f, 0.77f,  74.01f,  96.73f,  0.72f,  30687.0f,   -0.72f, 97.8f,  15.0f, "res/tex/solar_system/uranus.jpg",  false, 0.0f, 0.0f, glm::vec3(0.0f) },
        { "Neptune",27.5f,  0.0086f, 1.77f, 131.78f, 273.19f,  0.70f,  60190.0f,    0.67f, 28.3f, 100.0f, "res/tex/solar_system/neptune.jpg", false, 0.0f, 0.0f, glm::vec3(0.0f) },
    }};

    TRCamera gCamera;
    SimState gSim;
    bool gLockTarget = false;
    int gFocusIndex = 0;

    float deg2rad(float deg)
    {
        return glm::radians(deg);
    }

    // Solve Kepler's equation M = E - e sin(E) with a few Newton iterations.
    float solveEccentricAnomaly(float meanAnomaly, float eccentricity)
    {
        float eccentricAnomaly = eccentricity < 0.8f ? meanAnomaly : PI;
        for (int i = 0; i < 6; i++)
        {
            float sinE = std::sin(eccentricAnomaly);
            float cosE = std::cos(eccentricAnomaly);
            float f = eccentricAnomaly - eccentricity * sinE - meanAnomaly;
            float fp = 1.0f - eccentricity * cosE;
            eccentricAnomaly -= f / glm::max(fp, 1e-4f);
        }
        return eccentricAnomaly;
    }

    // Rotate a point from the orbital plane into world space using standard orbital elements.
    glm::vec3 orbitToWorld(const Body &body, float radius, float trueAnomaly)
    {
        float u = deg2rad(body.orbitPeriapsisDeg) + trueAnomaly;
        float node = deg2rad(body.orbitAscendingNodeDeg);
        float inclination = deg2rad(body.orbitInclinationDeg);

        float cosU = std::cos(u);
        float sinU = std::sin(u);
        float cosNode = std::cos(node);
        float sinNode = std::sin(node);
        float cosI = std::cos(inclination);
        float sinI = std::sin(inclination);

        return glm::vec3(radius * (cosNode * cosU - sinNode * sinU * cosI),
                         radius * (sinU * sinI),
                         radius * (sinNode * cosU + cosNode * sinU * cosI));
    }

    std::string buildWindowTitle()
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2)
            << "TGRenderer Solar System"
            << " | focus: " << gBodies[gFocusIndex].name
            << " | orbit x" << gSim.speed
            << " | spin x" << gSim.spinScale
            << " | day " << gSim.days;

        if (gLockTarget)
            oss << " | lock";
        if (gSim.paused)
            oss << " | paused";

        return oss.str();
    }

    // Advance the body along its ellipse and return the current world-space position.
    glm::vec3 getBodyPosition(const Body &body, float simDays)
    {
        if (body.orbitRadius <= 0.0f)
            return glm::vec3(0.0f);

        float meanAnomaly = deg2rad(body.phaseDeg + simDays / body.orbitDays * 360.0f);
        float eccentricAnomaly = solveEccentricAnomaly(meanAnomaly, body.orbitEccentricity);
        float cosE = std::cos(eccentricAnomaly);
        float sinE = std::sin(eccentricAnomaly);
        float radius = body.orbitRadius * (1.0f - body.orbitEccentricity * cosE);
        float trueAnomaly = std::atan2(std::sqrt(1.0f - body.orbitEccentricity * body.orbitEccentricity) * sinE,
                                       cosE - body.orbitEccentricity);
        return orbitToWorld(body, radius, trueAnomaly);
    }

    glm::mat4 getBodyModel(const Body &body, const glm::vec3 &position, float simDays, float spinScale)
    {
        float spinAngle = 0.0f;
        if (std::abs(body.spinDays) > 1e-4f)
            spinAngle = simDays * spinScale / body.spinDays * 360.0f;

        glm::mat4 model(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, deg2rad(body.axialTiltDeg), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::rotate(model, deg2rad(spinAngle), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(body.radius));
        return model;
    }

    // Build a polyline approximation of the same ellipse used by getBodyPosition().
    void appendOrbitMesh(TRMeshData &mesh, const Body &body, const glm::vec3 &color, int segments = 320)
    {
        if (body.orbitRadius <= 0.0f)
            return;

        for (int i = 0; i < segments; i++)
        {
            float a0 = (float)i / (float)segments * 2.0f * PI;
            float a1 = (float)(i + 1) / (float)segments * 2.0f * PI;

            float r0 = body.orbitRadius * (1.0f - body.orbitEccentricity * body.orbitEccentricity) /
                       (1.0f + body.orbitEccentricity * std::cos(a0));
            float r1 = body.orbitRadius * (1.0f - body.orbitEccentricity * body.orbitEccentricity) /
                       (1.0f + body.orbitEccentricity * std::cos(a1));

            mesh.vertices.push_back(orbitToWorld(body, r0, a0));
            mesh.vertices.push_back(orbitToWorld(body, r1, a1));
            mesh.colors.push_back(color);
            mesh.colors.push_back(color);
        }
    }

    void createRingMesh(TRMeshData &mesh, float innerRadius, float outerRadius, const glm::vec3 &color, int segments = 96)
    {
        for (int i = 0; i < segments; i++)
        {
            float a0 = (float)i / (float)segments * 2.0f * PI;
            float a1 = (float)(i + 1) / (float)segments * 2.0f * PI;

            glm::vec3 i0(std::cos(a0) * innerRadius, 0.0f, std::sin(a0) * innerRadius);
            glm::vec3 i1(std::cos(a1) * innerRadius, 0.0f, std::sin(a1) * innerRadius);
            glm::vec3 o0(std::cos(a0) * outerRadius, 0.0f, std::sin(a0) * outerRadius);
            glm::vec3 o1(std::cos(a1) * outerRadius, 0.0f, std::sin(a1) * outerRadius);

            mesh.vertices.push_back(i0);
            mesh.vertices.push_back(o0);
            mesh.vertices.push_back(o1);

            mesh.vertices.push_back(i0);
            mesh.vertices.push_back(o1);
            mesh.vertices.push_back(i1);

            for (int v = 0; v < 6; v++)
                mesh.colors.push_back(color);
        }
    }

    void onMouseMove(int dx, int dy)
    {
        gCamera.handleMouseDelta((float)dx, (float)dy);
    }

    void onKey(int key)
    {
        switch (key)
        {
            case SDL_SCANCODE_L:
                gLockTarget = !gLockTarget;
                break;
            case SDL_SCANCODE_SPACE:
                gSim.paused = !gSim.paused;
                break;
            case SDL_SCANCODE_MINUS:
            case SDL_SCANCODE_KP_MINUS:
                gSim.speed = glm::max(0.25f, gSim.speed * 0.5f);
                break;
            case SDL_SCANCODE_EQUALS:
            case SDL_SCANCODE_KP_PLUS:
                gSim.speed = glm::min(64.0f, gSim.speed * 2.0f);
                break;
            case SDL_SCANCODE_LEFTBRACKET:
                gSim.spinScale = glm::max(0.01f, gSim.spinScale * 0.5f);
                break;
            case SDL_SCANCODE_RIGHTBRACKET:
                gSim.spinScale = glm::min(1.0f, gSim.spinScale * 2.0f);
                break;
            case SDL_SCANCODE_0:
                gFocusIndex = 0;
                break;
            case SDL_SCANCODE_1:
            case SDL_SCANCODE_2:
            case SDL_SCANCODE_3:
            case SDL_SCANCODE_4:
            case SDL_SCANCODE_5:
            case SDL_SCANCODE_6:
            case SDL_SCANCODE_7:
            case SDL_SCANCODE_8:
                gFocusIndex = 1 + key - SDL_SCANCODE_1;
                break;
        }
    }

    void printHelp()
    {
        std::cout << "Controls:\n"
                  << "  Move mouse/trackpad to look around\n"
                  << "  WASD move\n"
                  << "  L toggle target lock, 0-8 focus Sun/planets\n"
                  << "  +/- change orbit speed, [/] change spin speed, Space pause, Esc quit\n";
    }
}

int main()
{
    TRWindow window(WIDTH, HEIGHT, "TGRenderer Solar System");
    if (!window.OK())
        return 1;

    printHelp();
    window.registerKeyEventCb(onKey);
    window.registerMouseMotionEventCb(onMouseMove);
    window.setRelativeMouseMode(true);

    gCamera.setMoveKeyBinding({
        SDL_SCANCODE_W,
        SDL_SCANCODE_S,
        SDL_SCANCODE_A,
        SDL_SCANCODE_D
    });
    gCamera.setPosition(glm::vec3(0.0f, 8.0f, 38.0f));
    gCamera.setYawPitch(-90.0f, -10.0f);
    gCamera.setMoveSpeed(10.0f);
    gCamera.setLookSensitivity(0.14f);
    gCamera.setPerspective(60.0f, (float)WIDTH / (float)HEIGHT, 0.1f, 200.0f);
    window.setTitle(buildWindowTitle());

    truTimerBegin();
    double prevTime = truTimerGetSecondsFromBegin();
    double titleTime = prevTime;

    trClearColor3f(0.01f, 0.01f, 0.03f);

    TRMeshData sphereMesh;
    truCreateSphere(sphereMesh, 48, 32);

    TRMeshData orbitMesh;
    for (size_t i = 1; i < gBodies.size(); i++)
        appendOrbitMesh(orbitMesh, gBodies[i], glm::vec3(0.25f, 0.35f, 0.55f));

    TRMeshData saturnRingMesh;
    createRingMesh(saturnRingMesh,
                   gBodies[6].ringInnerRadius,
                   gBodies[6].ringOuterRadius,
                   gBodies[6].ringColor);

    TRMeshData backgroundMesh;
    truCreateQuadPlane(backgroundMesh);

    std::array<std::unique_ptr<TRTexture>, gBodies.size()> bodyTextures;
    for (size_t i = 0; i < gBodies.size(); i++)
    {
        bodyTextures[i].reset(new TRTexture(gBodies[i].texturePath));
        if (!bodyTextures[i]->OK())
        {
            std::cout << "Load texture failed: " << gBodies[i].texturePath << std::endl;
            return 1;
        }
    }

    TRTexture backgroundTexture("examples/res/stars.tga");
    if (!backgroundTexture.OK())
    {
        std::cout << "Load background texture failed." << std::endl;
        return 1;
    }

    TextureMapShader unlitTextureShader;
    TextureMapPhongShader litTextureShader;
    ColorShader colorShader;
    BackgroundShader backgroundShader;

    PhongUniformData lightData;
    lightData.mAmbientStrength = 0.10f;
    lightData.mSpecularStrength = 0.18f;
    lightData.mLightColor = glm::vec3(1.0f, 0.96f, 0.88f);

    while (!window.shouldStop())
    {
        window.pollEvent();

        double now = truTimerGetSecondsFromBegin();
        float dt = (float)(now - prevTime);
        prevTime = now;

        if (!gSim.paused)
            gSim.days += gSim.speed * glm::max(dt * 60.0f, 0.0f);

        std::array<glm::vec3, gBodies.size()> positions;
        for (size_t i = 0; i < gBodies.size(); i++)
            positions[i] = getBodyPosition(gBodies[i], gSim.days);

        gCamera.updateFromWindow(window, dt);
        if (gLockTarget)
            gCamera.lookAt(positions[gFocusIndex]);

        if (now - titleTime >= 0.2)
        {
            window.setTitle(buildWindowTitle());
            titleTime = now;
        }

        glm::mat4 viewMat = gCamera.getViewMatrix();
        glm::mat4 projMat = gCamera.getProjectionMatrix();

        lightData.mLightPosition = glm::vec3(0.0f);
        lightData.mViewLightPosition = viewMat * glm::vec4(lightData.mLightPosition, 1.0f);

        trSetMat4(glm::mat4(1.0f), MAT4_MODEL);
        trSetMat4(viewMat, MAT4_VIEW);
        trSetMat4(projMat, MAT4_PROJ);

        trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT | TR_CLEAR_STENCIL_BIT);
        trEnableStencilWrite(true);
        trEnableStencilTest(false);
        trEnableDepthTest(true);
        trUnbindTextureAll();
        trSetUniformData(&lightData);
        trDrawArrays(TR_LINES, orbitMesh, &colorShader);

        for (size_t i = 0; i < gBodies.size(); i++)
        {
            trSetMat4(getBodyModel(gBodies[i], positions[i], gSim.days, gSim.spinScale), MAT4_MODEL);
            trBindTexture(bodyTextures[i].get(), TEXTURE_DIFFUSE);
            if (i == 0)
                trDrawArrays(TR_TRIANGLES, sphereMesh, &unlitTextureShader);
            else
                trDrawArrays(TR_TRIANGLES, sphereMesh, &litTextureShader);
        }

        glm::mat4 saturnRingModel(1.0f);
        saturnRingModel = glm::translate(saturnRingModel, positions[6]);
        saturnRingModel = glm::rotate(saturnRingModel, deg2rad(gBodies[6].axialTiltDeg), glm::vec3(0.0f, 0.0f, 1.0f));
        trSetMat4(saturnRingModel, MAT4_MODEL);
        trUnbindTextureAll();
        trDrawArrays(TR_TRIANGLES, saturnRingMesh, &colorShader);

        trEnableDepthTest(false);
        trEnableStencilTest(true);
        trEnableStencilWrite(false);
        trResetMat4(MAT4_MODEL);
        trResetMat4(MAT4_VIEW);
        trResetMat4(MAT4_PROJ);
        trBindTexture(&backgroundTexture, TEXTURE_DIFFUSE);
        trDrawArrays(TR_TRIANGLES, backgroundMesh, &backgroundShader);

        trEnableDepthTest(true);
        trEnableStencilTest(false);
        trUnbindTextureAll();

        window.swapBuffer();
    }

    return 0;
}
