#include <glm/gtc/matrix_transform.hpp>
#include <vector>

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
    /* The faces were rendered at 94 degrees: the nominal 90 degree cube
     * direction maps to the central tan(45)/tan(47) sub-region, keep the
     * sampler out of the contaminated border the overlap was meant to avoid. */
    mCubeTextureAgg->setSampleScale(glm::tan(glm::radians(45.0f)) / glm::tan(glm::radians(47.0f)));

    /* Irradiance cube: same faces, heavily blurred. Aggregation references
     * the textures, so it is done once here; content is (re)built by
     * updateIrradiance() after each face re-render. */
    mFaceSize = faceSize;
    TRTexture *irrFaces[6];
    for (size_t i = 0; i < 6; i++)
    {
        mIrrFace[i] = new TRTexture(faceSize, faceSize);
        irrFaces[i] = mIrrFace[i];
    }
    mIrrAgg = new TRCubeTexture(irrFaces);
    mIrrAgg->setSampleScale(glm::tan(glm::radians(45.0f)) / glm::tan(glm::radians(47.0f)));
    mOK = true;
}

TRReflectionProbe::~TRReflectionProbe()
{
    for (size_t i = 0; i < 6; i++)
        if (mFaceBuffer[i])
            delete mFaceBuffer[i];
    if (mCubeTextureAgg)
        delete mCubeTextureAgg;
    for (size_t i = 0; i < 6; i++)
        if (mIrrFace[i])
            delete mIrrFace[i];
    if (mIrrAgg)
        delete mIrrAgg;
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

TGRenderer::TRCubeTexture *TRReflectionProbe::getIrradianceTexture()
{
    if (!mOK)
        return nullptr;
    return mIrrAgg;
}

void TRReflectionProbe::updateIrradiance()
{
    if (!mOK)
        return;

    /* Separable box blur, two iterations, radius faceSize/8: wide enough
     * that face-local hotspots dissolve into their surroundings. Edge texels
     * are clamped (no cross-face filtering) - invisible for a term that is
     * only multiplied by albedo and a modest strength. */
    const int n = mFaceSize;
    const int radius = n / 8;
    std::vector<float> tmp(n * n * TEXTURE_CHANNEL);

    for (int f = 0; f < 6; f++)
    {
        float *src = mFaceBuffer[f]->getTexture()->getBuffer();
        float *dst = mIrrFace[f]->getBuffer();
        float *from = src;
        for (int iter = 0; iter < 2; iter++)
        {
            // horizontal: from -> tmp
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++)
                    for (int c = 0; c < TEXTURE_CHANNEL; c++)
                    {
                        float sum = 0.0f;
                        for (int k = -radius; k <= radius; k++)
                        {
                            int xx = glm::clamp(x + k, 0, n - 1);
                            sum += from[(y * n + xx) * TEXTURE_CHANNEL + c];
                        }
                        tmp[(y * n + x) * TEXTURE_CHANNEL + c] = sum / (2 * radius + 1);
                    }
            // vertical: tmp -> dst
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++)
                    for (int c = 0; c < TEXTURE_CHANNEL; c++)
                    {
                        float sum = 0.0f;
                        for (int k = -radius; k <= radius; k++)
                        {
                            int yy = glm::clamp(y + k, 0, n - 1);
                            sum += tmp[(yy * n + x) * TEXTURE_CHANNEL + c];
                        }
                        dst[(y * n + x) * TEXTURE_CHANNEL + c] = sum / (2 * radius + 1);
                    }
            from = dst;
        }

        /* Cap the irradiance sources: the demo's point light sits 0.1 below
         * the ceiling and blasts it to near saturation in the probe. A real
         * panel light emits downward only (dark ceiling). Uncapped, upward
         * taps of the hemisphere gather wash out the walls. 0.55 keeps the
         * colored walls (<= ~0.7) nearly intact while taming the ceiling. */
        const float cap = 0.55f;
        for (int i = 0; i < n * n * TEXTURE_CHANNEL; i++)
            dst[i] = glm::min(dst[i], cap);
    }
}
