#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "trapi.hpp"
#include "program.hpp"
#include "window.hpp"
#include "utils.hpp"

constexpr int WIDTH = 1280;
constexpr int HEIGHT = 800;

using namespace TGRenderer;

class TextureMapExShader : public TextureMapShader
{
    private:
        bool fragment(FSInData *fsdata, float color[])
        {
            int frame = *reinterpret_cast<int *>(trGetUniformData());
            glm::vec2 texCoord = fsdata->getVec2(SH_TEXCOORD);
            texCoord.x -= frame * 0.001f;
            textureCoordWrap(texCoord);
            float *c = texture2D(TEXTURE_DIFFUSE, texCoord.x, texCoord.y);
            color[0] = c[0];
            color[1] = c[1];
            color[2] = c[2];
            return true;
        }
};

int main()
{
    TRWindow w(WIDTH, HEIGHT, "Sphere demo");
    if (!w.OK())
        return 1;

    TRMeshData sphere;
    truCreateSphere(sphere, 30, 30);

    glm::mat4 viewMat = glm::lookAt(
                glm::vec3(2,1,2),
                glm::vec3(0,0,0),
                glm::vec3(0,1,0));

    glm::mat4 projMat = glm::perspective(glm::radians(75.0f), (float)WIDTH / (float)HEIGHT, 0.1f, 100.0f);

    TRTexture earthTex("examples/res/earth.tga");
    if (!earthTex.OK())
    {
        std::cout << "Load earthTexture failed." << std::endl;
        abort();
    }

    int frame = 0;
    TextureMapPhongShader shader;

    PhongUniformData unidata;
    unidata.mLightPosition = glm::vec3(100.0f, 0.0f, 0.0f);
    unidata.mViewLightPosition = trGetMat4(MAT4_VIEW) * glm::vec4(unidata.mLightPosition, 1.0f);

    TRMeshData BGPlane;
    truCreateQuadPlane(BGPlane);
    TextureMapExShader texShader;
    TRTexture bgTex("examples/res/stars.tga");
    if (!bgTex.OK())
    {
        std::cout << "Load background texture failed." << std::endl;
        abort();
    }

    glm::mat4 self = glm::rotate(glm::mat4(1.0f), glm::radians(-23.5f), glm::vec3(0.0f, 0.0f, 1.0f));
    while (!w.shouldStop()) {
        frame++;
        trSetMat4(glm::rotate(self, glm::radians(1.0f * frame), glm::vec3(0.0f, 1.0f, 0.0f)), MAT4_MODEL);
        trSetMat4(viewMat, MAT4_VIEW);
        trSetMat4(projMat, MAT4_PROJ);

        trClear(TR_CLEAR_DEPTH_BIT | TR_CLEAR_COLOR_BIT | TR_CLEAR_STENCIL_BIT);
        trEnableStencilWrite(true);
        trBindTexture(&earthTex, TEXTURE_DIFFUSE);
        trSetUniformData(&unidata);
        trDrawArrays(TR_TRIANGLES, sphere, &shader);

        trEnableDepthTest(false);
        trEnableStencilTest(true);
        trEnableStencilWrite(false);
        trResetMat4(MAT4_MODEL);
        trResetMat4(MAT4_VIEW);
        trResetMat4(MAT4_PROJ);
        trBindTexture(&bgTex, TEXTURE_DIFFUSE);
        trSetUniformData(&frame);
        trDrawArrays(TR_TRIANGLES, BGPlane, &texShader);
        trEnableDepthTest(true);
        trEnableStencilTest(false);

        w.swapBuffer();
        w.pollEvent();
    }
    std::cout << "frames = " << frame << std::endl;
    return 0;
}
