#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <glm/ext.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "trapi.hpp"
#include "utils.hpp"

using namespace std::chrono;

bool truSavePNG(const char *name, TGRenderer::TRBuffer *buffer)
{
    return stbi_write_png(name, buffer->getW(), buffer->getH(), TGRenderer::BUFFER_CHANNEL, buffer->getRawData(), buffer->getW() * TGRenderer::BUFFER_CHANNEL);
}

void truLoadVec4(const float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec4> &out)
{
    for (size_t i = start * stride + offset, j = 0; j < len; i += stride, j++)
        out.push_back(glm::make_vec4(&data[i]));
}

void truLoadVec3(const float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec3> &out)
{
    for (size_t i = start * stride + offset, j = 0; j < len; i += stride, j++)
        out.push_back(glm::make_vec3(&data[i]));
}

void truLoadVec2(const float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec2> &out)
{
    for (size_t i = start * stride + offset, j = 0; j < len; i += stride, j++)
        out.push_back(glm::make_vec2(&data[i]));
}

thread_local system_clock::time_point gTimerBegin;
thread_local system_clock::time_point gTimerLastClick;

void truTimerBegin()
{
    gTimerBegin = system_clock::now();
    gTimerLastClick = gTimerBegin;
}

void truTimerClick()
{
    gTimerLastClick = system_clock::now();
}

double __getSeconds__(system_clock::time_point t1, system_clock::time_point t2)
{
    auto duration = duration_cast<std::chrono::microseconds>(t2 - t1);
    return (double(duration.count()) * microseconds::period::num / microseconds::period::den);
}

double truTimerGetSecondsFromClick()
{
    return __getSeconds__(gTimerLastClick, system_clock::now());
}

double truTimerGetSecondsFromBegin()
{
    return __getSeconds__(gTimerBegin, system_clock::now());
}

bool truLoadObj(
        const char * path,
        std::vector<glm::vec3> & out_vertices,
        std::vector<glm::vec2> & out_texcoords,
        std::vector<glm::vec3> & out_normals
        )
{
    std::cout << "Loading OBJ file " << path << "..." << std::endl;
    std::ifstream in(path);
    if (!in.good())
    {
        std::cout << "Impossible to open the obj file ! Are you in the right path ?" << std::endl;
        return false;
    }

    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_texcoords;
    std::vector<glm::vec3> temp_normals;
    int faces = 0;

    while(true)
    {
        std::string line;
        std::string type;
        std::stringstream ss;
        getline(in, line);
        if (!line.length() && in.eof())
            break;
        ss.str(line);
        ss >> type;
        if (type == "v")
        {
            glm::vec3 vertex;
            ss >> vertex.x >> vertex.y >> vertex.z;
            if (ss.fail())
                goto error_return;
            temp_vertices.push_back(vertex);
        }
        else if (type == "vt")
        {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            if (ss.fail())
                goto error_return;
            temp_texcoords.push_back(uv);
        }
        else if (type == "vn")
        {
            glm::vec3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            if (ss.fail())
                goto error_return;
            temp_normals.push_back(normal);
        }
        else if (type == "f")
        {
            unsigned int vertexIndex, uvIndex, normalIndex;
            char sep1, sep2;
            for (size_t i = 0; i < 3; i++)
            {
                ss >> vertexIndex >> sep1 >> uvIndex >> sep2 >> normalIndex;
                if (ss.fail() || sep1 != '/' || sep2 != '/')
                    goto error_return;
                vertexIndices.push_back(vertexIndex);
                uvIndices    .push_back(uvIndex);
                normalIndices.push_back(normalIndex);
            }
            faces++;
        }
    }
    std::cout << "Faces: " << faces << std::endl;
    // For each vertex of each triangle
    for (size_t i = 0; i < vertexIndices.size(); i++ )
    {
        // Get the indices of its attributes
        unsigned int vertexIndex    = vertexIndices[i];
        unsigned int uvIndex        = uvIndices[i];
        unsigned int normalIndex    = normalIndices[i];

        // Get the attributes thanks to the index
        glm::vec3 vertex    = temp_vertices[vertexIndex-1];
        glm::vec2 uv        = temp_texcoords[uvIndex-1];
        glm::vec3 normal    = temp_normals[normalIndex-1];

        // Put the attributes in buffers
        out_vertices    .push_back(vertex);
        out_texcoords   .push_back(uv);
        out_normals     .push_back(normal);
    }
    in.close();
    return true;

error_return:
    std::cout << "File can't be read by our simple parser :-( Try exporting with other options" << std::endl;
    in.close();
    return false;
}

void truCreateFloorPlane(TGRenderer::TRMeshData &mesh, float height, float width, const float *color)
{
    /* Workaroud: in line mode, wrap texture coord may cause a strage bug, 2.0 will be treat as 0.0 not 1.0 */
    width = width * 0.999;
    float floorData[] =
    {
        /* x, y, z, r, g, b, n.x, n.y, n.z, uv.x, uv.y */
        -width, height, -width,     color[0], color[1], color[2],   0.0, 1.0, 0.0,        0.0, width,
        -width, height,  width,     color[0], color[1], color[2],   0.0, 1.0, 0.0,        0.0,   0.0,
             0, height,  width,     color[0], color[1], color[2],   0.0, 1.0, 0.0, width*0.5f,   0.0,

             0, height,  width,     color[0], color[1], color[2],   0.0, 1.0, 0.0, width*0.5f,   0.0,
             0, height, -width,     color[0], color[1], color[2],   0.0, 1.0, 0.0, width*0.5f, width,
        -width, height, -width,     color[0], color[1], color[2],   0.0, 1.0, 0.0,        0.0, width,

             0, height, -width,     color[0], color[1], color[2],   0.0, 1.0, 0.0, width*0.5f, width,
             0, height,  width,     color[0], color[1], color[2],   0.0, 1.0, 0.0, width*0.5f,   0.0,
         width, height,  width,     color[0], color[1], color[2],   0.0, 1.0, 0.0,      width,   0.0,

         width, height,  width,     color[0], color[1], color[2],   0.0, 1.0, 0.0,      width,   0.0,
         width, height, -width,     color[0], color[1], color[2],   0.0, 1.0, 0.0,      width, width,
             0, height, -width,     color[0], color[1], color[2],   0.0, 1.0, 0.0, width*0.5f, width,
    };

    truLoadVec3(floorData, 0, 12, 0, 11, mesh.vertices);
    truLoadVec3(floorData, 0, 12, 3, 11, mesh.colors);
    truLoadVec3(floorData, 0, 12, 6, 11, mesh.normals);
    truLoadVec2(floorData, 0, 12, 9, 11, mesh.texcoords);
    mesh.computeTangent();
}

void truCreateQuadPlane(TGRenderer::TRMeshData &mesh)
{
    float planeData[] =
    {
        /* x, y, z, uv.x, uv.y */
        -1.0f,  1.0f, 0.0f,     0.0,   1.0,
        -1.0f, -1.0f, 0.0f,     0.0,   0.0,
         0.0f, -1.0f, 0.0f,     0.5,   0.0,

         0.0f, -1.0f, 0.0f,     0.5,   0.0,
         0.0f,  1.0f, 0.0f,     0.5,   1.0,
        -1.0f,  1.0f, 0.0f,     0.0,   1.0,

         0.0f,  1.0f, 0.0f,     0.5,   1.0,
         0.0f, -1.0f, 0.0f,     0.5,   0.0,
         1.0f, -1.0f, 0.0f,     1.0,   0.0,

         1.0f, -1.0f, 0.0f,     1.0,   0.0,
         1.0f,  1.0f, 0.0f,     1.0,   1.0,
         0.0f,  1.0f, 0.0f,     0.5,   1.0,
    };

    truLoadVec3(planeData, 0, 12, 0, 5, mesh.vertices);
    truLoadVec2(planeData, 0, 12, 3, 5, mesh.texcoords);
}

static inline glm::vec3 get_point(float u, float v)
{
    constexpr float PI = 3.1415926;
    float a1 = PI * v, a2 = PI * u * 2;
    float x = sin(a1) * cos(a2);
    float y = cos(a1);
    float z = sin(a1) * sin(a2);
    return glm::vec3(x, y, z);
}

void truCreateSphere(TGRenderer::TRMeshData &mesh, int uStepNum, int vStepNum, const float *color)
{

    float ustep = 1/(float)uStepNum, vstep = 1/(float)vStepNum;
    float u = 0, v = 0;
    std::vector<glm::vec3> points;

    for (int i = 0; i < uStepNum; i++)
    {
        glm::vec3 p1 = get_point(0, 0);
        glm::vec3 p2 = get_point(u + ustep, vstep);
        glm::vec3 p3 = get_point(u, vstep);
        // counter-clockwise
        mesh.vertices.push_back(p1);
        mesh.vertices.push_back(p2);
        mesh.vertices.push_back(p3);

        mesh.texcoords.push_back(glm::vec2(1, 1));
        mesh.texcoords.push_back(glm::vec2(1 - (u + ustep), 1 - vstep));
        mesh.texcoords.push_back(glm::vec2(1 - u, 1 - vstep));

        mesh.normals.push_back(p1);
        mesh.normals.push_back(p2);
        mesh.normals.push_back(p3);
        u += ustep;
    };

    u = 0;
    v = vstep;
    for (int i = 1; i < vStepNum - 1; i++)
    {
        u = 0;
        for (int j = 0; j < uStepNum; j++)
        {
            // counter-clockwise
            /*
             *   p4---p1
             *   |   / |
             *   |  /  |
             *   | /   |
             *   p2---p3
             */
            glm::vec3 p1 = get_point(u, v);
            glm::vec3 p2 = get_point(u + ustep, v + vstep);
            glm::vec3 p3 = get_point(u, v + vstep);
            glm::vec3 p4 = get_point(u + ustep, v);

            mesh.vertices.push_back(p1);
            mesh.vertices.push_back(p4);
            mesh.vertices.push_back(p2);

            mesh.texcoords.push_back(glm::vec2(1 - u, 1 - v));
            mesh.texcoords.push_back(glm::vec2(1 - (u + ustep), 1 -v));
            mesh.texcoords.push_back(glm::vec2(1 - (u + ustep), 1 - (v + vstep)));

            mesh.normals.push_back(p1);
            mesh.normals.push_back(p4);
            mesh.normals.push_back(p2);

            mesh.vertices.push_back(p1);
            mesh.vertices.push_back(p2);
            mesh.vertices.push_back(p3);

            mesh.texcoords.push_back(glm::vec2(1 - u, 1 - v));
            mesh.texcoords.push_back(glm::vec2(1 - (u + ustep), 1 - (v + vstep)));
            mesh.texcoords.push_back(glm::vec2(1 - u, 1 - (v + vstep)));

            mesh.normals.push_back(p1);
            mesh.normals.push_back(p2);
            mesh.normals.push_back(p3);

            u += ustep;
        }
        v += vstep;
    }

    u = 0;
    for (int i = 0; i < uStepNum; i++)
    {
        glm::vec3 p1 = get_point(0, 1);
        glm::vec3 p2 = get_point(u, 1 - vstep);
        glm::vec3 p3 = get_point(u + ustep, 1 - vstep);

        // counter-clockwise
        mesh.vertices.push_back(p1);
        mesh.vertices.push_back(p2);
        mesh.vertices.push_back(p3);

        mesh.texcoords.push_back(glm::vec2(1, 0));
        mesh.texcoords.push_back(glm::vec2(1 - u, vstep));
        mesh.texcoords.push_back(glm::vec2(1 - (u + ustep), vstep));

        mesh.normals.push_back(p1);
        mesh.normals.push_back(p2);
        mesh.normals.push_back(p3);

        u += ustep;
    }
    mesh.fillPureColor(glm::make_vec3(color));
    mesh.computeTangent();
}

