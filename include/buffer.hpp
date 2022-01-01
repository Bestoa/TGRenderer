#ifndef __TOPGUN_BUFFER__
#define __TOPGUN_BUFFER__

#include <glm/glm.hpp>

#define BUFFER_CHANNEL (3)
class TRBuffer
{
    public:
        uint8_t *mData = nullptr;
        uint32_t mW = 0;
        uint32_t mH = 0;

        std::mutex mDepthMutex;

        void setViewport(int x, int y, int w, int h);
        void viewport(glm::vec2 &screen_v, glm::vec4 &ndc_v);
        void setBgColor(float r, float g, float b);
        virtual void clearColor();
        void clearDepth();

        size_t getOffset(int x, int y);
        virtual void setColor(size_t offset, float c[BUFFER_CHANNEL]);
        bool depthTest(size_t offset, float depth);

        void setExtBuffer(void *addr);

        TRBuffer() = delete;
        TRBuffer(int w, int h, bool alloc = true);
        TRBuffer(const TRBuffer &) = delete;
        TRBuffer& operator=(const TRBuffer &) = delete;

        virtual ~TRBuffer();

        bool isValid();

    protected:
        bool mValid = false;
        uint8_t mBgColor3i[BUFFER_CHANNEL] = { 0, 0, 0 };
        float mBgColor3f[BUFFER_CHANNEL] = { 0.0f, 0.0f, 0.0f };

    private:
        float *mDepth = nullptr;

        uint32_t mVX = 0;
        uint32_t mVY = 0;
        uint32_t mVW = 0;
        uint32_t mVH = 0;

        // data was allocated by us.
        bool mAlloc = true;
        unsigned int mId = 0;
};
#endif
