#ifndef __TOPGUN_PROGRAM__
#define __TOPGUN_PROGRAM__

#include "tr.hpp"

class PhongVSData : public VSDataBase
{
    public:
        glm::vec3 mViewFragmentPosition;
        glm::vec3 mViewLightPosition;
        glm::vec3 mTangentFragmentPosition;
        glm::vec3 mTangentLightPosition;
        glm::vec4 mLightClipV;
};

class PhongFSData : public FSDataBase
{
    public:
        glm::vec3 mViewFragmentPosition[3];
        glm::vec3 mViewLightPosition;
        glm::vec3 mTangentFragmentPosition[3];
        glm::vec3 mTangentLightPosition[3];
        glm::vec4 mLightClipV[3];

};

class ColorProgram : public TRProgramBase
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase *);
    bool fragment(FSDataBase *, float color[3]);

    TRProgramBase *clone();
};

class TextureMapProgram : public TRProgramBase
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase *);
    bool fragment(FSDataBase *, float color[3]);

    TRProgramBase *clone();
};

class ColorPhongProgram : public TRProgramBase
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase *);
    void prepareFragmentData(VSDataBase *[3], FSDataBase *);
    void interpVertex(float , VSDataBase *, VSDataBase *, VSDataBase *);
    bool fragment(FSDataBase *, float color[3]);

    VSDataBase *allocVSData();
    FSDataBase *allocFSData();
    void freeShaderData();

    TRProgramBase *clone();

    PhongVSData mVSData[MAX_VSDATA_NUM];
    PhongFSData mFSData;
    int mAllocIndex = 0;
};

class TextureMapPhongProgram : public TRProgramBase
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase *);
    void prepareFragmentData(VSDataBase *[3], FSDataBase *);
    void interpVertex(float , VSDataBase *, VSDataBase *, VSDataBase *);
    bool fragment(FSDataBase *, float color[3]);

    VSDataBase *allocVSData();
    FSDataBase *allocFSData();
    void freeShaderData();

    TRProgramBase *clone();

    PhongVSData mVSData[MAX_VSDATA_NUM];
    PhongFSData mFSData;
    int mAllocIndex = 0;
};

class ShadowMapProgram : public TRProgramBase
{
    void loadVertexData(TRMeshData &, VSDataBase *, size_t);
    void vertex(VSDataBase *);
    bool fragment(FSDataBase *, float color[3]);

    TRProgramBase *clone();

    public:
        constexpr static float BIAS = 0.001f;
        constexpr static float FACTOR = 0.2f;
};
#endif
