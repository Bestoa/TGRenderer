#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "trapi.hpp"
#include "utils.hpp"
#include "objs.hpp"

using namespace std;
using namespace TGRenderer;

TRObj::~TRObj()
{
    cout << "Destory TRObj" << endl;
    if (mAttribute.map_Kd)
        delete mAttribute.map_Kd;
    if (mAttribute.map_Ks)
        delete mAttribute.map_Ks;
    if (mAttribute.map_Ke)
        delete mAttribute.map_Ke;
    if (mAttribute.map_Kn)
        delete mAttribute.map_Kn;
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
    if (!in.good())
    {
        cout << "Open config file failed!" << endl;
        return;
    }

    glm::vec3 Kd(0.0f);
    bool hasKd = false;

    while (true)
    {
        string line;
        stringstream ss;
        getline(in, line);
        if (!line.length() && in.eof())
            break;
        size_t firstSpace = line.find(" ");
        if (firstSpace == string::npos)
            continue;
        string type = line.substr(0, firstSpace);
        ss.str(line.substr(firstSpace + 1));
        if (type == "obj")
        {
            cout << "Loading OBJ..." << endl;
            if (!truLoadObj(ss.str().c_str(), mMeshData.vertices, mMeshData.texcoords, mMeshData.normals))
            {
                cout << "Load OBJ file error!" << endl;
                goto close_file;
            }

            if (mMeshData.vertices.size() != mMeshData.texcoords.size()
                    || mMeshData.vertices.size() != mMeshData.normals.size()
                    || mMeshData.vertices.size() % 3 != 0)
            {
                cout << "Mesh data is invalid." << endl;
                goto close_file;
            }
            mMeshData.computeTangent();
        }
        else if (type == "map_Kd")
            mAttribute.map_Kd = new TRTexture(ss.str().c_str());
        else if (type == "map_Ks")
            mAttribute.map_Ks = new TRTexture(ss.str().c_str());
        else if (type == "map_Ke")
            mAttribute.map_Ke = new TRTexture(ss.str().c_str());
        else if (type == "map_Kn")
            mAttribute.map_Kn = new TRTexture(ss.str().c_str());
        else if (type == "Kd")
        {
            float r, g, b;
            ss >> r >> g >> b;
            if (!ss.fail())
            {
                Kd = glm::vec3(r, g, b);
                hasKd = true;
            }
        }
        else if (type == "Ns")
            ss >> mAttribute.Ns;
        else if (type == "sharpness")
            ss >> mAttribute.sharpness;
    }

    if (hasKd)
        mMeshData.fillPureColor(Kd);
    else
        mMeshData.fillSpriteColor();

    cout << "Create TRObj done.\n" << endl;
    mOK = true;
close_file:
    in.close();
}

bool TRObj::draw(int id)
{
    if (OK() == false)
        return false;

    trBindTexture(nullptr, TEXTURE_DIFFUSE);
    trBindTexture(nullptr, TEXTURE_SPECULAR);
    trBindTexture(nullptr, TEXTURE_GLOW);
    trBindTexture(nullptr, TEXTURE_NORMAL);

    if (mAttribute.map_Kd && mAttribute.map_Kd->OK())
        trBindTexture(mAttribute.map_Kd, TEXTURE_DIFFUSE);

    if (mAttribute.map_Ks && mAttribute.map_Ks->OK())
        trBindTexture(mAttribute.map_Ks, TEXTURE_SPECULAR);

    if (mAttribute.map_Ke && mAttribute.map_Ke->OK())
        trBindTexture(mAttribute.map_Ke, TEXTURE_GLOW);

    if (mAttribute.map_Kn && mAttribute.map_Kn->OK())
        trBindTexture(mAttribute.map_Kn, TEXTURE_NORMAL);

    if (mAttribute.map_Kd == nullptr && (id == 1 || id == 3))
        id--;

    PhongUniformData *data = reinterpret_cast<PhongUniformData *>(trGetUniformData());
    /* Save the original value */
    PhongUniformData sdata = *data;
    data->mShininess = int(mAttribute.Ns);
    data->mSpecularStrength = mAttribute.sharpness / 1000.f;

    trDrawArrays(TR_TRIANGLES, mMeshData, mShaders[id]);

    *data = sdata;
    return true;
}

bool TRObj::drawShadowMap()
{
    if (OK() == false)
        return false;
    TRCullFaceMode oldCullFaceMode = trGetCullFaceMode();
    trCullFaceMode(TR_NONE);
    trDrawArrays(TR_TRIANGLES, mMeshData, &mShadowShader);
    trCullFaceMode(oldCullFaceMode);
    return true;
}
