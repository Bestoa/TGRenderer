#include <glm/gtc/matrix_transform.hpp>

#include "trapi.hpp"
#include "probe.hpp"

using namespace TGRenderer;

TRReflectionProbe::TRReflectionProbe(const glm::vec3 &position, int faceSize)
{
    mPosition = position;

    /* Forward/up pairs are derived from the cube texture UV table in
     * TRCubeTexture::sample(): for each face the camera up vector is the
     * direction in which the face v coordinate increases, so the rendered
     * face content and the sampler agree by construction. A 90 degree FOV
     * per face tiles the full sphere seamlessly.
     *
     * face        forward        up
     * bottom      (0, -1, 0)     (0, 0, -1)
     * top         (0,  1, 0)     (0, 0,  1)
     * front       (0, 0, -1)     (0, 1,  0)
     * back        (0, 0,  1)     (0, 1,  0)
     * left        (-1, 0, 0)     (0, 1,  0)
     * right       (1, 0,  0)     (0, 1,  0)
     */
    glm::vec3 axis[6][2] =
    {
        { glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f) },
        { glm::vec3(0.0f,  1.0f, 0.0f), glm::vec3(0.0f, 0.0f,  1.0f) },
        { glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, 1.0f,  0.0f) },
        { glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, 1.0f,  0.0f) },
        { glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f,  0.0f) },
        { glm::vec3( 1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f,  0.0f) },
    };

    for (size_t i = 0; i < 6; i++)
    {
        mFaceViewMat[i] = glm::lookAt(position, position + axis[i][0], axis[i][1]);
        mFaceBuffer[i] = new TRTextureBuffer(faceSize, faceSize);
        if (!mFaceBuffer[i]->OK())
            return;
    }

    /* 94 degrees instead of 90: the outermost pixel rows of each face would
     * otherwise be contaminated by the neighboring skybox cube face edges
     * (they project exactly onto the NDC +/-1 border at 45 degrees), which
     * shows up as a thin seam along the cube face boundaries in reflections.
     * With the overlap the nominal 90 degree pyramid maps strictly inside
     * the face image and the contaminated border is never sampled. */
    mProjMat = glm::perspective(glm::radians(94.0f), 1.0f, 0.1f, 100.0f);

    /* The face textures are updated in place while rendering into the face
     * buffers, aggregating once is enough. */
    TRTexture *faces[6];
    for (size_t i = 0; i < 6; i++)
        faces[i] = mFaceBuffer[i]->getTexture();
    mCubeTextureAgg = new TRCubeTexture(faces);
    mOK = true;
}

TRReflectionProbe::~TRReflectionProbe()
{
    for (size_t i = 0; i < 6; i++)
        if (mFaceBuffer[i])
            delete mFaceBuffer[i];
    if (mCubeTextureAgg)
        delete mCubeTextureAgg;
}

bool TRReflectionProbe::OK()
{
    return mOK;
}

TGRenderer::TRTextureBuffer *TRReflectionProbe::getFaceBuffer(size_t face)
{
    if (!mOK || face >= 6)
        return nullptr;
    return mFaceBuffer[face];
}

const glm::mat4 &TRReflectionProbe::getFaceViewMat(size_t face)
{
    return mFaceViewMat[face];
}

const glm::mat4 &TRReflectionProbe::getFaceProjMat()
{
    return mProjMat;
}

TGRenderer::TRCubeTexture *TRReflectionProbe::getCubeTexture()
{
    if (!mOK)
        return nullptr;
    return mCubeTextureAgg;
}
