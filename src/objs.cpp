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

bool TRObj::isValid()
{
    return mValid;
    if (mMeshData.vertices.size() != mMeshData.uvs.size()
            || mMeshData.vertices.size() != mMeshData.normals.size()
            || mMeshData.vertices.size() % 3 != 0
            || !mTextureDiffuse->isValid())
        return false;
    else
        return true;
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
    if (!line.length() || !truLoadObj(line.c_str(), mMeshData.vertices, mMeshData.uvs, mMeshData.normals))
    {
        cout << "Load OBJ file error!" << endl;
        return;
    }

    if (mMeshData.vertices.size() != mMeshData.uvs.size()
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

    if (!mTextureDiffuse || !mTextureDiffuse->isValid())
    {
        cout << "Load diffuse texture error!" << endl;
        return;
    }

    getline(in, line);
    if (line.length() && line != "null")
    {
        cout << "Load specular texture..." << endl;
        mTextureSpecular = new TRTexture(line.c_str());
        if (!mTextureSpecular || !mTextureDiffuse->isValid())
            mTextureSpecular = nullptr;
    }

    getline(in, line);
    if (line.length() && line != "null")
    {
        cout << "Load glow texture..." << endl;
        mTextureGlow = new TRTexture(line.c_str());
        if (!mTextureGlow || !mTextureGlow->isValid())
            mTextureGlow = nullptr;
    }

    getline(in, line);
    if (line.length() && line != "null")
    {
        cout << "Load normal texutre..." << endl;
        mTextureNormal = new TRTexture(line.c_str());
        if (!mTextureNormal || !mTextureNormal->isValid())
            mTextureNormal = nullptr;
    }

    in.close();

    cout << "Create TRObj done.\n" << endl;

    mValid = true;
}

bool TRObj::draw(int id)
{
    if (isValid() == false)
        return false;

    trBindTexture(mTextureDiffuse, TEXTURE_DIFFUSE);
    trBindTexture(NULL, TEXTURE_SPECULAR);
    trBindTexture(NULL, TEXTURE_GLOW);
    trBindTexture(NULL, TEXTURE_NORMAL);

    if (mTextureSpecular && mTextureSpecular->isValid())
        trBindTexture(mTextureSpecular, TEXTURE_SPECULAR);

    if (mTextureGlow && mTextureGlow->isValid())
        trBindTexture(mTextureGlow, TEXTURE_GLOW);

    if (mTextureNormal && mTextureNormal->isValid())
        trBindTexture(mTextureNormal, TEXTURE_NORMAL);

    trTriangles(mMeshData, mProg[id]);

    return true;
}

bool TRObj::drawShadowMap()
{
    if (isValid() == false)
        return false;

    trTriangles(mMeshData, &mShadowProg);
    return true;
}
