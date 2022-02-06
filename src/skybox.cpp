#include "trapi.hpp"
#include "texture.hpp"
#include "skybox.hpp"
#include "utils.hpp"

using namespace TGRenderer;

static constexpr float cube[] =
{
    // bottom
    -1.0, -1.0, -1.0,   0.0, 1.0,
    -1.0, -1.0, 1.0,    0.0, 0.0,
    1.0, -1.0, 1.0,     1.0, 0.0,
    1.0, -1.0, 1.0,     1.0, 0.0,
    1.0, -1.0, -1.0,    1.0, 1.0,
    -1.0, -1.0, -1.0,   0.0, 1.0,
    // top
    -1.0, 1.0, -1.0,   0.0, 0.0,
    1.0, 1.0, 1.0,     1.0, 1.0,
    -1.0, 1.0, 1.0,    0.0, 1.0,
    1.0, 1.0, 1.0,     1.0, 1.0,
    -1.0, 1.0, -1.0,   0.0, 0.0,
    1.0, 1.0, -1.0,    1.0, 0.0,
    // front
    -1.0, 1.0, -1.0,    0.0, 1.0,
    -1.0, -1.0, -1.0,   0.0, 0.0,
    1.0, -1.0, -1.0,    1.0, 0.0,
    1.0, -1.0, -1.0,    1.0, 0.0,
    1.0, 1.0, -1.0,     1.0, 1.0,
    -1.0, 1.0, -1.0,    0.0, 1.0,
    // back
    -1.0, 1.0, 1.0,    1.0, 1.0,
    1.0, -1.0, 1.0,    0.0, 0.0,
    -1.0, -1.0, 1.0,   1.0, 0.0,
    1.0, -1.0, 1.0,    0.0, 0.0,
    -1.0, 1.0, 1.0,    1.0, 1.0,
    1.0, 1.0, 1.0,     0.0, 1.0,
    // left
    -1.0, 1.0, 1.0,     0.0, 1.0,
    -1.0, -1.0, 1.0,    0.0, 0.0,
    -1.0, -1.0, -1.0,   1.0, 0.0,
    -1.0, -1.0, -1.0,   1.0, 0.0,
    -1.0, 1.0, -1.0,    1.0, 1.0,
    -1.0, 1.0, 1.0,     0.0, 1.0,
    // right
    1.0, 1.0, 1.0,      1.0, 1.0,
    1.0, -1.0, -1.0,    0.0, 0.0,
    1.0, -1.0, 1.0,     1.0, 0.0,
    1.0, -1.0, -1.0,    0.0, 0.0,
    1.0, 1.0, 1.0,      1.0, 1.0,
    1.0, 1.0, -1.0,     0.0, 1.0,
};

TRSkyBox::TRSkyBox(std::string cubeTextureNames[6])
{
    for (size_t i = 0; i < 6; i++)
        mCubeTexture[i] = nullptr;

    for (size_t i = 0; i < 6; i++)
    {
        truLoadVec3(cube, i * 6, 6, 0, 5, mCubeData[i].vertices);
        truLoadVec2(cube, i * 6, 6, 3, 5, mCubeData[i].texcoords);
        mCubeTexture[i] = new TRTexture(cubeTextureNames[i].c_str());
        if (!mCubeTexture[i]->OK())
            return;
    }
    mOK = true;
}

TRSkyBox::~TRSkyBox()
{
    for (size_t i = 0; i < 6; i++)
        if (mCubeTexture[i])
            delete mCubeTexture[i];
}

void TRSkyBox::draw()
{
    if (!mOK)
        return;

    TRCullFaceMode oldCullFaceMode = trGetCullFaceMode();
    trCullFaceMode(TR_CCW);
    for (size_t i = 0; i < 6; i++)
    {
        trBindTexture(mCubeTexture[i], TEXTURE_DIFFUSE);
        trDrawArrays(TR_TRIANGLES, mCubeData[i], &mShader);
    }
    trCullFaceMode(oldCullFaceMode);
}

bool TRSkyBox::OK()
{
    return mOK;
}
