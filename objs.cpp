#include <iostream>
#include <fstream>
#include <string>

#include "tr.h"
#include "utils.h"
#include "objs.h"

using namespace std;

TRObj::TRObj()
{
    cout << "Create TRObj" << endl;
    ZERO(mTextureDiffuse);
    ZERO(mTextureSpecular);
    ZERO(mTextureGlow);
    ZERO(mTextureNormal);
}

TRObj::~TRObj()
{
    cout << "Destory TRObj" << endl;
    destory_texture(mTextureDiffuse);
    destory_texture(mTextureSpecular);
    destory_texture(mTextureGlow);
    destory_texture(mTextureNormal);
}

bool TRObj::is_valid()
{
    if (mMeshData.vertices.size() != mMeshData.uvs.size()
            || mMeshData.vertices.size() != mMeshData.normals.size()
            || mMeshData.vertices.size() % 3 != 0
            || is_valid_texture(mTextureDiffuse) == false)
        return false;
    else
        return true;
}


bool TRObj::load_from_config_file(const char *config_file_name)
{
    ifstream in(config_file_name);
    string line;

    if (in.bad())
    {
        cout << "Open config file faile!" << endl;
        return false;
    }

    cout << "Loading obj..." << endl;
    getline(in, line);
    if (!line.length() || !load_obj(line.c_str(), mMeshData.vertices, mMeshData.uvs, mMeshData.normals))
    {
        cout << "Load obj file error!" << endl;
        return false;
    }

    cout << "Loading diffuse texture..." << endl;
    getline(in, line);
    if (!line.length() || !load_texture(line.c_str(), mTextureDiffuse))
    {
        cout << "Load diffuse texture error!" << endl;
        return false;
    }

    getline(in, line);
    if (line.length() && line != "null")
    {
        cout << "Load specular texture..." << endl;
        load_texture(line.c_str(), mTextureSpecular);
    }

    getline(in, line);
    if (line.length() && line != "null")
    {
        cout << "Load glow texture..." << endl;
        load_texture(line.c_str(), mTextureGlow);
    }

    getline(in, line);
    if (line.length() && line != "null")
    {
        cout << "Load normal texutre..." << endl;
        load_texture(line.c_str(), mTextureNormal);
    }

    in.close();
    return true;
}

bool TRObj::draw()
{
    if (is_valid() == false)
        return false;

    trBindTexture(&mTextureDiffuse, TEXTURE_DIFFUSE);
    trBindTexture(NULL, TEXTURE_SPECULAR);
    trBindTexture(NULL, TEXTURE_GLOW);
    trBindTexture(NULL, TEXTURE_NORMAL);

    if (is_valid_texture(mTextureSpecular))
        trBindTexture(&mTextureSpecular, TEXTURE_SPECULAR);

    if (is_valid_texture(mTextureGlow))
        trBindTexture(&mTextureGlow, TEXTURE_GLOW);

    if (is_valid_texture(mTextureNormal))
        trBindTexture(&mTextureNormal, TEXTURE_NORMAL);

    trTriangles(mMeshData, DRAW_WITH_TEXTURE);

    return true;
}
