#include <cstdio>
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
    return stbi_write_png(name, buffer->mW, buffer->mH, TGRenderer::BUFFER_CHANNEL, buffer->mData, buffer->mW * TGRenderer::BUFFER_CHANNEL);
}

void truLoadVec4(float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec4> &out)
{
    for (size_t i = start * stride + offset, j = 0; j < len; i += stride, j++)
        out.push_back(glm::make_vec4(&data[i]));
}

void truLoadVec3(float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec3> &out)
{
    for (size_t i = start * stride + offset, j = 0; j < len; i += stride, j++)
        out.push_back(glm::make_vec3(&data[i]));
}

void truLoadVec2(float *data, size_t start, size_t len, size_t offset, size_t stride, std::vector<glm::vec2> &out)
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
    printf("Loading OBJ file %s...\n", path);

    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_texcoords;
    std::vector<glm::vec3> temp_normals;
    int faces = 0;

    FILE * file = fopen(path, "r");
    if (file == NULL)
    {
        printf("Impossible to open the obj file ! Are you in the right path ?\n");
        return false;
    }

    while(true)
    {
        char lineHeader[128];
        // read the first word of the line
        int res = fscanf(file, "%s", lineHeader);
        if (res == EOF)
            break; // EOF = End Of File. Quit the loop.

        // else : parse lineHeader

        if ( strcmp( lineHeader, "v" ) == 0 ){
            glm::vec3 vertex;
            fscanf(file, "%f %f %f\n", &vertex.x, &vertex.y, &vertex.z );
            temp_vertices.push_back(vertex);
        }else if ( strcmp( lineHeader, "vt" ) == 0 ){
            glm::vec2 uv;
            fscanf(file, "%f %f\n", &uv.x, &uv.y );
            uv.y = uv.y;
            temp_texcoords.push_back(uv);
        }else if ( strcmp( lineHeader, "vn" ) == 0 ){
            glm::vec3 normal;
            fscanf(file, "%f %f %f\n", &normal.x, &normal.y, &normal.z );
            temp_normals.push_back(normal);
        }else if ( strcmp( lineHeader, "f" ) == 0 ){
            std::string vertex1, vertex2, vertex3;
            unsigned int vertexIndex[3], uvIndex[3], normalIndex[3];
            int matches = fscanf(file, "%u/%u/%u %u/%u/%u %u/%u/%u\n", &vertexIndex[0], &uvIndex[0], &normalIndex[0], &vertexIndex[1], &uvIndex[1], &normalIndex[1], &vertexIndex[2], &uvIndex[2], &normalIndex[2] );
            if (matches != 9){
                printf("File can't be read by our simple parser :-( Try exporting with other options\n");
                fclose(file);
                return false;
            }
            vertexIndices.push_back(vertexIndex[0]);
            vertexIndices.push_back(vertexIndex[1]);
            vertexIndices.push_back(vertexIndex[2]);
            uvIndices    .push_back(uvIndex[0]);
            uvIndices    .push_back(uvIndex[1]);
            uvIndices    .push_back(uvIndex[2]);
            normalIndices.push_back(normalIndex[0]);
            normalIndices.push_back(normalIndex[1]);
            normalIndices.push_back(normalIndex[2]);
            faces++;
        }else{
            // Probably a comment, eat up the rest of the line
            char stupidBuffer[1000];
            fgets(stupidBuffer, 1000, file);
        }

    }
    printf("Faces: %d\n", faces);

    // For each vertex of each triangle
    for( unsigned int i=0; i<vertexIndices.size(); i++ ){

        // Get the indices of its attributes
        unsigned int vertexIndex = vertexIndices[i];
        unsigned int uvIndex = uvIndices[i];
        unsigned int normalIndex = normalIndices[i];

        // Get the attributes thanks to the index
        glm::vec3 vertex = temp_vertices[ vertexIndex-1 ];
        glm::vec2 uv = temp_texcoords[ uvIndex-1 ];
        glm::vec3 normal = temp_normals[ normalIndex-1 ];

        // Put the attributes in buffers
        out_vertices.push_back(vertex);
        out_texcoords     .push_back(uv);
        out_normals .push_back(normal);

    }
    fclose(file);
    return true;
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

glm::vec3 get_point(float u, float v)
{
    constexpr float PI = 3.1415926;
    float a1 = PI * v, a2 = PI * u * 2;
    float x = sin(a1) * cos(a2);
    float y = cos(a1);
    float z = sin(a1) * sin(a2);
    return glm::vec3(x, y, z);
}

void truCreateSphere(TGRenderer::TRMeshData &mesh, int uStepNum, int vStepNum)
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
    mesh.fillSpriteColor();
    mesh.computeTangent();
}

