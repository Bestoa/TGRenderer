#ifndef __TR_PROBE_H__
#define __TR_PROBE_H__

#include <glm/glm.hpp>
#include "trapi.hpp"

// Reflection probe: renders the scene into 6 cube faces from a fixed world
// space point and exposes the result as a TRCubeTexture, so the environment
// reflection can pick up scene geometry (floor, sky, other objects...) in
// addition to the static skybox images.
//
// The probe knows nothing about the scene. Typical use:
//   for (size_t i = 0; i < 6; i++)
//   {
//       trSetRenderTarget(probe->getFaceBuffer(i));
//       trSetMat4(probe->getFaceViewMat(i), MAT4_VIEW);
//       trSetMat4(probe->getFaceProjMat(), MAT4_PROJ);
//       ... draw the scene here, skip the reflective object itself ...
//   }
//   trBindCubeTexture(probe->getCubeTexture());
//
// The face textures are updated in place while rendering, no extra update
// call is needed. Re-render the faces only when the scene changed: the probe
// content never depends on the eye position.
class TRReflectionProbe
{
    public:
        TRReflectionProbe(const glm::vec3 &position, int faceSize);
        TRReflectionProbe(const TRReflectionProbe &&) = delete;
        ~TRReflectionProbe();

        bool OK();

        // Face order matches TRCubeTexture: bottom, top, front, back, left, right
        TGRenderer::TRTextureBuffer *getFaceBuffer(size_t face);
        const glm::mat4 &getFaceViewMat(size_t face);
        const glm::mat4 &getFaceProjMat();

        // Aggregated cube texture for trBindCubeTexture()
        TGRenderer::TRCubeTexture *getCubeTexture();

    private:
        glm::vec3 mPosition;
        TGRenderer::TRTextureBuffer *mFaceBuffer[6] = { nullptr };
        glm::mat4 mFaceViewMat[6];
        glm::mat4 mProjMat;
        TGRenderer::TRCubeTexture *mCubeTextureAgg = nullptr;
        bool mOK = false;
};

#endif
