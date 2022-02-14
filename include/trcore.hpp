#ifndef __TOPGUN_CORE__
#define __TOPGUN_CORE__

#include "trapi.hpp"

namespace TGRenderer
{
    constexpr int MAX_VSDATA_NUM = 10;

    class Program
    {
        public:
            Program() = default;
            Program(const Program &&) = delete;

            void drawPrimsInstranced(TRMeshData &mesh, size_t index, size_t num);
            void setBuffer(TRBuffer *buffer);
            void setShader(Shader *shader);

        private:
            TRBuffer *mBuffer = nullptr;
            Shader *mShader = nullptr;
            VSOutData mVSOutData[MAX_VSDATA_NUM];
            FSInData mFSInData;
            int mAllocIndex = 0;
#if __DEBUG_FINISH_CB__
            bool mDrawSth = false;
#endif

            VSOutData *allocVSOutData();
            /* Free vsdata which were allocated by us.
             * Suggest to use pre-allocated mode, just need to reset the index.
             * Otherwise you need to record all the vsdata pointer and free them. */
            void freeShaderData();
            void preDraw();
            void postDraw();
            /* Prepare fragment data for interpolation. Put { V0.AAA, V1.AAA - V0.AAA, V2.AAA - V0.AAA } into FSInData. */
            void prepareFragmentData(VSOutData *vsdata[], int num);
            /* Add a new vertex if needed when clipping on W axis. Formula: new V.AAA = in2.AAA + t * (in1.AAA -in2.AAA) */
            void getIntersectionVertex(VSOutData *in1, VSOutData *in2, VSOutData *outV);
            void clipLineOnWAxis(VSOutData *in1, VSOutData *in2, VSOutData *out[4], size_t &index);
            void clipOnWAxis(VSOutData *in[3], VSOutData *out[4], size_t &index);
            void drawPoint(TRMeshData &mesh, size_t index);
            void drawLine(TRMeshData &mesh, size_t index);
            void drawTriangle(TRMeshData &mesh, size_t index);
            void rasterizationPoint(VSOutData *vsdata);
            void rasterizationLine(VSOutData *vsdata[2]);
            void rasterizationWireframe(VSOutData *vsdata[3]);
            void rasterizationTriangle(VSOutData *vsdata[3]);
            void drawPixel(int x, int y, float depth);
    };
}
#endif
