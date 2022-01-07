#ifndef __TOPGUN_BUFFER__
#define __TOPGUN_BUFFER__

#include <glm/glm.hpp>
#include <mutex>

#define __NEED_BUFFER_LOCK__ (1)

namespace TGRenderer
{
    constexpr int BUFFER_CHANNEL = 3;
    class TRBuffer
    {
        public:
            uint8_t *mData = nullptr;
            uint32_t mW = 0;
            uint32_t mH = 0;

            void setViewport(int x, int y, int w, int h);
            void viewport(glm::vec2 &screen_v, glm::vec4 &ndc_v);
            void setBgColor(float r, float g, float b);
            virtual void clearColor();
            void clearDepth();
            void clearStencil();

            size_t getOffset(int x, int y);
            virtual void setColor(size_t offset, float c[BUFFER_CHANNEL]);
            bool depthTest(size_t offset, float depth);
            void stencilFunc(size_t offset);
            bool stencilTest(size_t offset);
#if __NEED_BUFFER_LOCK__
            std::mutex & getMutex(size_t offset);
#endif
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
            uint8_t *mStencil = nullptr;

            uint32_t mVX = 0;
            uint32_t mVY = 0;
            uint32_t mVW = 0;
            uint32_t mVH = 0;

            // data was allocated by us.
            bool mAlloc = true;
            unsigned int mId = 0;
#if __NEED_BUFFER_LOCK__
            // 1024 pixels share one mutex
            constexpr static int MUTEX_PIXEL_SHIT = 10;
            std::mutex *mMutex = nullptr;
#endif
    };
}
#endif
