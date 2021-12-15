#ifndef __TINY_RENDER__
#define __TINY_RENDER__

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <glm/glm.hpp>

#define TEXTURE_BPP (3)
struct TRTexture
{
    uint8_t *data;
    int stride;
    int w;
    int h;
};

struct TRMaterial
{
    TRTexture *diffuse;
    TRTexture *specular;
    TRTexture *glow;
    TRTexture *normal;
};

enum TRTextureType
{
    TEXTURE_DIFFUSE,
    TEXTURE_SPECULAR,
    TEXTURE_GLOW,
    TEXTURE_NORMAL,
};

class TRMeshData
{
    public:
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec3> colors;
        std::vector<glm::vec3> tangents;
};

enum TRDrawMode
{
    DRAW_WITH_TEXTURE,
    DRAW_WITH_COLOR,
    DRAW_WITH_DEMO_COLOR,
    DRAW_WIREFRAME,
};

#define BPP (3)
class TRBuffer
{
    public:
        uint8_t *data;
        float *depth;

        uint32_t w;
        uint32_t h;

        uint32_t vx;
        uint32_t vy;
        uint32_t vw;
        uint32_t vh;

        uint32_t stride;
        uint8_t bg_color[BPP];
        // data was not allocated by us.
        bool ext_buffer;
        std::mutex mDepthMutex;

        TRBuffer():data(nullptr),depth(nullptr),w(0),h(0),vx(0),vy(0),vw(0),vh(0),stride(0),ext_buffer(false)
        {
            bg_color[0] = 0;
            bg_color[1] = 0;
            bg_color[2] = 0;
        };

        void viewport(glm::vec2 &screen_v, glm::vec4 &ndc_v)
        {
            screen_v.x = vx + (w - 1) * (ndc_v.x / 2 + 0.5);
            // inverse y here
            screen_v.y = vy + (h - 1) - (h - 1) * (ndc_v.y / 2 + 0.5);
        };
};

template <class T>
static inline float interpolation(T v[3], int i, float w0, float w1, float w2)
{
    return v[0][i] * w0 + v[1][i] * w1 + v[2][i] * w2;
}

static inline float edge(glm::vec2 &a, glm::vec2 &b, glm::vec2 &c)
{
    return (c.x - a.x)*(b.y - a.y) - (c.y - a.y)*(b.x - a.x);
}

static inline bool depth_test(TRBuffer *buffer, int depth_offset, float depth)
{
    /* projection matrix will inverse z-order. */
    if (buffer->depth[depth_offset] < depth)
        return false;
    else
        buffer->depth[depth_offset] = depth;
    return true;
}

static inline glm::vec3 texture_color(float u, float v, TRTexture *texture)
{
    int x = int(u * (texture->w - 1) + 0.5);
    // inverse y here
    int y = int(texture->h - 1 - v * (texture->h - 1) + 0.5);
    // Only support RGB texture
    uint8_t *color = texture->data + y * texture->stride + x * TEXTURE_BPP;
    return glm::vec3(color[0], color[1], color[2]);
}


class TRVertShaderDataBase
{
    public:
        virtual ~TRVertShaderDataBase() {};
};

class TRFragShaderDataBase
{
    public:
        virtual ~TRFragShaderDataBase() {};
};

template <class TRVSData, class TRFSData>
class TRProgramBase
{
    public:
        TRVSData vsdata[3];
        TRFSData fsdata;
        TRBuffer *mBuffer;

        virtual ~TRProgramBase() {};

        void drawTriangle(TRBuffer *buffer, TRMeshData &mesh, size_t i)
        {
            glm::vec4 clipV[3];

            mBuffer = buffer;

            vertexData(mesh, i);
            for (size_t i = 0; i < 3; i++)
                vertex(i, clipV[i]);

            if(geometry())
                rasterization(clipV);
        };

    private:

        virtual void vertexData(TRMeshData &, size_t) = 0;
        virtual void vertex(size_t, glm::vec4 &) = 0;
        virtual bool geometry() = 0;
        virtual bool fragment(float w0, float w1, float w2, uint8_t color[3]) = 0;

        void rasterization(glm::vec4 clip_v[3])
        {
            glm::vec4 ndc_v[3];
            glm::vec2 screen_v[3];

            for (int i = 0; i < 3; i++)
            {
                ndc_v[i] = clip_v[i] / clip_v[i].w;
                mBuffer->viewport(screen_v[i], ndc_v[i]);
            }
            float area = edge(screen_v[0], screen_v[1], screen_v[2]);
            if (area <= 0) return;
            glm::vec2 left_up, right_down;
            left_up.x = glm::max(0.0f, glm::min(glm::min(screen_v[0].x, screen_v[1].x), screen_v[2].x)) + 0.5;
            left_up.y = glm::max(0.0f, glm::min(glm::min(screen_v[0].y, screen_v[1].y), screen_v[2].y)) + 0.5;
            right_down.x = glm::min(float(mBuffer->w - 1), glm::max(glm::max(screen_v[0].x, screen_v[1].x), screen_v[2].x)) + 0.5;
            right_down.y = glm::min(float(mBuffer->h - 1), glm::max(glm::max(screen_v[0].y, screen_v[1].y), screen_v[2].y)) + 0.5;
            for (int i = left_up.y; i < right_down.y; i++) {
                int offset_base = i * mBuffer->w;
                for (int j = left_up.x; j < right_down.x; j++) {
                    glm::vec2 v(j, i);
                    float w0 = edge(screen_v[1], screen_v[2], v);
                    float w1 = edge(screen_v[2], screen_v[0], v);
                    float w2 = edge(screen_v[0], screen_v[1], v);
                    int flag = 0;
                    if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                        flag = 1;
                    if (!flag)
                        continue;
                    w0 /= (area + 1e-6);
                    w1 /= (area + 1e-6);
                    w2 /= (area + 1e-6);

                    /* use ndc z to calculate depth */
                    float depth = interpolation(ndc_v, 2, w0, w1, w2);
                    /* z in ndc of opengl should between 0.0f to 1.0f */
                    if (depth > 1.0f || depth < 0.0f)
                        continue;

                    int offset = offset_base + j;
                    /* easy-z */
                    /* Do not use mutex here to speed up */
                    if (!depth_test(mBuffer, offset, depth))
                        continue;

                    uint8_t color[3];
                    if (!fragment(w0, w1, w2, color))
                        continue;

                    uint8_t *addr = &mBuffer->data[offset * BPP];
                    /* depth test */
                    {
                        std::lock_guard<std::mutex> lck(mBuffer->mDepthMutex);
                        if (!depth_test(mBuffer, offset, depth))
                            continue;

                        addr[0] = color[0];
                        addr[1] = color[1];
                        addr[2] = color[2];
                    }
                }
            }
        };
};

class WireframeVSData : public TRVertShaderDataBase
{
    public:
        glm::vec3 mV;
};

class WireframeFSData : public TRVertShaderDataBase
{
};

class WireframeProgram : public TRProgramBase<WireframeVSData, WireframeFSData>
{
    void vertexData(TRMeshData &, size_t);
    void vertex(size_t, glm::vec4 &clipV);
    bool geometry();
    bool fragment(float w0, float w1, float w2, uint8_t color[3]);
};

class ColorVSData : public TRVertShaderDataBase
{
    public:
        glm::vec3 mV;
        glm::vec3 mColor;
};

class ColorFSData : public TRFragShaderDataBase
{
    public:
        glm::vec3 mColor[3];
};

class ColorProgram : public TRProgramBase<ColorVSData, ColorFSData>
{
    void vertexData(TRMeshData &, size_t);
    void vertex(size_t, glm::vec4 &clipV);
    bool geometry();
    bool fragment(float w0, float w1, float w2, uint8_t color[3]);
};

class TextureMapVSData : public TRVertShaderDataBase
{
    public:
        glm::vec3 mV;
        glm::vec2 mTexCoord;
};

class TextureMapFSData : public TRFragShaderDataBase
{
    public:
        glm::vec2 mTexCoord[3];
};

class TextureMapProgram : public TRProgramBase<TextureMapVSData, TextureMapFSData>
{
    void vertexData(TRMeshData &, size_t);
    void vertex(size_t, glm::vec4 &clipV);
    bool geometry();
    bool fragment(float w0, float w1, float w2, uint8_t color[3]);
};

class PhongVSData : public TRVertShaderDataBase
{
    public:
        glm::vec3 mV;
        glm::vec2 mTexCoord;
        glm::vec3 mViewFragPosition;
        glm::vec3 mNormal;
        glm::vec3 mLightPosition;
        glm::vec3 mTangent;
};

class PhongFSData : public TRFragShaderDataBase
{
    public:
        glm::vec2 mTexCoord[3];
        glm::vec3 mViewFragPosition[3];
        glm::vec3 mNormal[3];
        glm::vec3 mLightPosition;
        glm::vec3 mTangent;
};

class PhongProgram : public TRProgramBase<PhongVSData, PhongFSData>
{
    void vertexData(TRMeshData &, size_t);
    void vertex(size_t, glm::vec4 &clipV);
    bool geometry();
    bool fragment(float w0, float w1, float w2, uint8_t color[3]);
};

#ifndef __BLINN_PHONG__
#define __BLINN_PHONG__ 1
#endif

void trSetRenderThreadNum(size_t num);
void trEnableLighting(bool enable);
void trSetAmbientStrength(float v);
void trSetLightColor3f(float r, float g, float b);
void trSetLightPosition3f(float x, float y, float z);
void trViewport(int x, int y, int w, int h);
void trMakeCurrent(TRBuffer &buffer);
void trBindTexture(TRTexture *texture, TRTextureType type);
void trClear();
void trClearColor3f(float r, float g, float b);
void trSetModelMat(glm::mat4 mat);
void trSetViewMat(glm::mat4 mat);
void trSetProjMat(glm::mat4 mat);
bool trCreateRenderTarget(TRBuffer &buffer, int w, int h, bool has_ext_buffer = false);
void trSetExtBufferToRenderTarget(TRBuffer &buffer, void *addr);
void trDestoryRenderTarget(TRBuffer &buffer);
void trTriangles(TRMeshData &data, TRDrawMode mode);
#endif
