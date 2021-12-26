#ifndef __TOPGUN_BUFFER__
#define __TOPGUN_BUFFER__

#include <glm/glm.hpp>

#define CHANNEL (3)
class TRBuffer
{
    public:
        uint8_t *mData = nullptr;
        uint32_t mW = 0;
        uint32_t mH = 0;
        uint32_t mStride = 0;

        std::mutex mDepthMutex;

        void setViewport(int x, int y, int w, int h);
        void viewport(glm::vec2 &screen_v, glm::vec4 &ndc_v);
        void setBGColor(float r, float g, float b);
        void clearColor();
        void clearDepth();
        bool depthTest(int x, int y, float depth);

        void setExtBuffer(void *addr);

        TRBuffer() = delete;
        TRBuffer(const TRBuffer &) = delete;
        TRBuffer& operator=(const TRBuffer &) = delete;

        ~TRBuffer();

        static TRBuffer* create(int w, int h, bool ext = false);

    private:
        float *mDepth = nullptr;

        uint32_t mVX = 0;
        uint32_t mVY = 0;
        uint32_t mVW = 0;
        uint32_t mVH = 0;

        uint8_t mBGColor[CHANNEL] = { 0, 0, 0 };
        // data was not allocated by us.
        bool mExtBuffer = false;
        bool mValid = false;

        TRBuffer(int w, int h, bool ext);
};
#endif
