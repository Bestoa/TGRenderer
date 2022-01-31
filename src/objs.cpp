#include <iostream>
#include <fstream>
#include <string>

#include "trapi.hpp"
#include "utils.hpp"
#include "objs.hpp"

using namespace std;
using namespace TGRenderer;

TRObj::~TRObj()
{
    cout << "Destory TRObj" << endl;
    if (mTextureDiffuse)
        delete mTextureDiffuse;
    if (mTextureSpecular)
        delete mTextureSpecular;
    if (mTextureGlow)
        delete mTextureGlow;
    if (mTextureNormal)
        delete mTextureNormal;
}

bool TRObj::OK() const
{
    return mOK;
}

float TRObj::getFloorYAxis() const
{
    float floorY = 100.f;
    for (auto &v3 : mMeshData.vertices)
        if (v3.y < floorY)
            floorY = v3.y;
    return floorY - 0.1;
}


TRObj::TRObj(const char *config)
{
    cout << "Create TRObj..." << endl;

    ifstream in(config);
    string line;

    if (in.bad())
    {
        cout << "Open config file faile!" << endl;
        return;
    }

    cout << "Loading OBJ..." << endl;
    getline(in, line);
    if (!line.length() || !truLoadObj(line.c_str(), mMeshData.vertices, mMeshData.texcoords, mMeshData.normals))
    {
        cout << "Load OBJ file error!" << endl;
        return;
    }

    if (mMeshData.vertices.size() != mMeshData.texcoords.size()
            || mMeshData.vertices.size() != mMeshData.normals.size()
            || mMeshData.vertices.size() % 3 != 0)
    {
        cout << "Mesh data is invalid." << endl;
        return;
    }
    mMeshData.computeTangent();
    mMeshData.fillSpriteColor();

    cout << "Loading diffuse texture..." << endl;
    getline(in, line);
    if (line.length())
        mTextureDiffuse = new TRTexture(line.c_str());

    if (!mTextureDiffuse || !mTextureDiffuse->OK())
    {
        cout << "Load diffuse texture error!" << endl;
        return;
    }

    getline(in, line);
    if (line.length() && line != "null")
    {
        cout << "Load specular texture..." << endl;
        mTextureSpecular = new TRTexture(line.c_str());
        if (!mTextureSpecular || !mTextureDiffuse->OK())
            mTextureSpecular = nullptr;
    }

    getline(in, line);
    if (line.length() && line != "null")
    {
        cout << "Load glow texture..." << endl;
        mTextureGlow = new TRTexture(line.c_str());
        if (!mTextureGlow || !mTextureGlow->OK())
            mTextureGlow = nullptr;
    }

    getline(in, line);
    if (line.length() && line != "null")
    {
        cout << "Load normal texutre..." << endl;
        mTextureNormal = new TRTexture(line.c_str());
        if (!mTextureNormal || !mTextureNormal->OK())
            mTextureNormal = nullptr;
    }

    in.close();

    cout << "Create TRObj done.\n" << endl;

    mOK = true;
}

bool TRObj::draw(int id)
{
    if (OK() == false)
        return false;

    trBindTexture(mTextureDiffuse, TEXTURE_DIFFUSE);
    trBindTexture(nullptr, TEXTURE_SPECULAR);
    trBindTexture(nullptr, TEXTURE_GLOW);
    trBindTexture(nullptr, TEXTURE_NORMAL);

    if (mTextureSpecular && mTextureSpecular->OK())
        trBindTexture(mTextureSpecular, TEXTURE_SPECULAR);

    if (mTextureGlow && mTextureGlow->OK())
        trBindTexture(mTextureGlow, TEXTURE_GLOW);

    if (mTextureNormal && mTextureNormal->OK())
        trBindTexture(mTextureNormal, TEXTURE_NORMAL);

    trDrawArrays(TR_TRIANGLES, mMeshData, mShaders[id]);

    return true;
}

bool TRObj::drawShadowMap()
{
    if (OK() == false)
        return false;

    trDrawArrays(TR_TRIANGLES, mMeshData, &mShadowShader);
    return true;
}
