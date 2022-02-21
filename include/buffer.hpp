#ifndef __TOPGUN_BUFFER__
#define __TOPGUN_BUFFER__

#include <glm/glm.hpp>
#include <mutex>

#ifndef __NEED_BUFFER_LOCK__
#define __NEED_BUFFER_LOCK__ (1)
#endif

namespace TGRenderer
{
    constexpr int BUFFER_CHANNEL = 4;
    class TRBuffer
    {
        public:
            void * getRawData();
            uint32_t getW() const;
            uint32_t getH() const;
            void setViewport(int x, int y, uint32_t w, uint32_t h);
            glm::vec2 viewportTransform(glm::vec4 &ndc_v) const;
            glm::uvec4 getDrawArea();
            void setBgColor(float r, float g, float b);
            virtual void clearColor();
            void clearDepth();
            void clearStencil();

            // offset for depth buffer and stencil, pre-calculate for performance.
            size_t getOffset(int x, int y) const;
            virtual size_t getStride() const;
            virtual void drawPixel(int x, int y, float color[]);
            float getDepth(size_t offset) const;
            void updateDepth(size_t offset, float depth);
            uint8_t getStencil(size_t offset) const;
            void updateStencil(size_t offset, uint8_t stencil);
#if __NEED_BUFFER_LOCK__
            std::mutex & getMutex(size_t offset);
#endif
            void setExtBuffer(void *addr);

            TRBuffer(int w, int h, bool alloc = true);
            TRBuffer(const TRBuffer &&) = delete;

            virtual ~TRBuffer();

            bool OK() const;

        protected:
            uint8_t *mData = nullptr;
            bool mOK = false;
            uint32_t mW = 0;
            uint32_t mH = 0;
            uint8_t mBgColor3i[BUFFER_CHANNEL] = { 0, 0, 0 };
            float mBgColor3f[BUFFER_CHANNEL] = { 0.0f, 0.0f, 0.0f };

        private:
            float *mDepth = nullptr;
            uint8_t *mStencil = nullptr;

            int mVX = 0;
            int mVY = 0;
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
