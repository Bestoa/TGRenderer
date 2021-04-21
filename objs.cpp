#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

#include "tr.h"
#include "utils.h"
#include "objs.h"

using namespace std;
using json = nlohmann::json;

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
    if (mVs.size() != mUVs.size() || mVs.size() != mNs.size() || mVs.size() % 3 != 0 || is_valid_texture(mTextureDiffuse) == false)
        return false;
    else
        return true;
}


bool TRObj::load_from_config_file(const char *config_file_name)
{
    ifstream i(config_file_name);
    json j;
    i >> j;
    i.close();

    if (j.is_null())
    {
        cout << "Parse config file error!" << endl;
        return false;
    }

    if (j["obj"].is_null() || !j["obj"].is_string() || !load_obj(j["obj"].get<string>().c_str(), mVs, mUVs, mNs))
    {
        cout << "Load obj file error!" << endl;
        return false;
    }

    if (j["diffuse"].is_null() || !j["diffuse"].is_string() || !load_texture(j["diffuse"].get<string>().c_str(), mTextureDiffuse))
    {
        cout << "Load diffuse texture error!" << endl;
        // If load texture failed, the memory of texture is freed, but the struct is not clean.
        ZERO(mTextureDiffuse);
        return false;
    }

    if (!j["specular"].is_null() && j["specular"].is_string())
        if (!load_texture(j["specular"].get<string>().c_str(), mTextureSpecular))
            ZERO(mTextureSpecular);

    if (!j["glow"].is_null() && j["glow"].is_string())
        if (!load_texture(j["glow"].get<string>().c_str(), mTextureGlow))
            ZERO(mTextureGlow);

    if (!j["normal"].is_null() && j["normal"].is_string())
        if (!load_texture(j["normal"].get<string>().c_str(), mTextureNormal))
            ZERO(mTextureNormal);


    return true;
}

bool TRObj::draw()
{
    if (is_valid() == false)
    {
        return false;
    }

    trBindTexture(&mTextureDiffuse, TEXTURE_DIFFUSE);

    if (is_valid_texture(mTextureSpecular))
        trBindTexture(&mTextureSpecular, TEXTURE_SPECULAR);
    else
        trBindTexture(NULL, TEXTURE_SPECULAR);

    if (is_valid_texture(mTextureGlow))
        trBindTexture(&mTextureGlow, TEXTURE_GLOW);
    else
        trBindTexture(NULL, TEXTURE_GLOW);

    if (is_valid_texture(mTextureNormal))
        trBindTexture(&mTextureNormal, TEXTURE_NORMAL);
    else
        trBindTexture(NULL, TEXTURE_NORMAL);

    trTriangles(mVs, mUVs, mNs);

    return true;
}
