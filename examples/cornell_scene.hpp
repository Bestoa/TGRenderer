// Single source of truth for the Cornell scene data used by BOTH the raster
// pass (cornell.cpp) and the ray shader (cornell_ray.hpp). Everything that is
// scene-coupled — balls, light, boxes, room, caustic foot — derives from here,
// so moving an object updates the probe position, the caustic projection and
// the ray-traced world at once.
//
// The world-space assets under res/obj/cornell and res/tex must match these
// values (positions/albedos are baked into them); change both together.
#ifndef __CORNELL_SCENE_H__
#define __CORNELL_SCENE_H__

#include <glm/glm.hpp>

namespace cs {

// room interior [-1,1]^3, open front (+z), floor at y = -1
static const float kRoomHalf = 1.0f;
static const float kFloorY   = -1.0f;
static const float kCeilY    =  1.0f;

// ceiling area-light panel
static const float kPanelY    = 0.992308f;   // rt-mapped: (5.18 - 2.6) / 2.6
static const float kPanelHalf = 0.25f;       // rt-mapped: 0.65 / 2.6

// the Phong point light sits slightly below the panel (hand approximation of
// the area light; shadows/shading key off this single definition now)
static const glm::vec3 kLightPos = glm::vec3(0.0f, 0.9f, 0.0f);

// balls: xyz = center, w = radius (ball_glass.obj / ball_mirror.obj match)
static const glm::vec4 kGlassBall  = glm::vec4(0.0f, -0.738462f, 0.653846f, 0.261538f);
static const glm::vec4 kMirrorBall = glm::vec4(0.576923f, -0.807692f, 0.615385f, 0.192308f);

// boxes: center + half extents + Y rotation (box_tall.obj / box_short.obj match)
static const glm::vec3 kTallCenter  = glm::vec3(-0.415385f, -0.619231f, -0.134615f);
static const glm::vec3 kTallHalf    = glm::vec3(0.203846f, 0.380769f, 0.203846f);
static const float     kTallRot     = 18.0f;
static const glm::vec3 kShortCenter = glm::vec3(0.480769f, -0.796154f, 0.134615f);
static const glm::vec3 kShortHalf   = glm::vec3(0.203846f, 0.203846f, 0.203846f);
static const float     kShortRot    = -15.0f;

// wall albedos (must match res/tex/cornell_*.ppm)
static const glm::vec3 kWhite(0.73f);
static const glm::vec3 kRed  (0.63f, 0.06f, 0.05f);
static const glm::vec3 kGreen(0.14f, 0.45f, 0.09f);
static const glm::vec3 kGray (0.73f);

// Floor point where light focused through the glass ball lands: extend the
// light->ball-center ray to the floor. Derived, not hand-placed — it tracks
// kLightPos / kGlassBall / kFloorY automatically.
inline glm::vec3 causticFoot()
{
    glm::vec3 c(kGlassBall);
    glm::vec3 d = c - kLightPos;
    float t = (kFloorY - c.y) / d.y;
    return c + t * d;
}

} // namespace cs

#endif
