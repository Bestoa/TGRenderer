#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "tr.h"
#include "utils.h"

void save_ppm(const char *name, uint8_t *buffer, int width, int height)
{
    FILE *fp = fopen(name, "wb");
    if (!fp)
        abort();
    fprintf(fp, "P6\n%d %d\n255\n", width, height);
    fwrite(buffer, 1, width * height * 3, fp);
    fclose(fp);
}

bool load_texture(const char *name, TRTexture &tex)
{
    int width, height, nrChannels;
    unsigned char *data = stbi_load(name, &width, &height, &nrChannels, 0);
    if (!data)
    {
        printf("Load texture %s failed.\n", name);
        return false;
    }
    if (nrChannels != 3 && nrChannels != 4)
    {
        printf("Only support RGB/RGBA texture.\n");
        return false;
    }
    tex.w = width;
    tex.h = height;
    tex.stride = tex.w * 3;
    tex.data = new uint8_t[tex.stride * tex.h];
    if (!tex.data)
        goto free_image;
    printf("Loading texture %s, size %dx%d.\n", name, tex.w, tex.h);

    for (int i = 0; i < tex.h; i++)
    {
        for (int j = 0; j < tex.w; j++)
        {
            tex.data[i * tex.stride + j * nrChannels + 0] = *(data + i * (nrChannels * width) + j * nrChannels + 0);
            tex.data[i * tex.stride + j * nrChannels + 1] = *(data + i * (nrChannels * width) + j * nrChannels + 1);
            tex.data[i * tex.stride + j * nrChannels + 2] = *(data + i * (nrChannels * width) + j * nrChannels + 2);
        }
    }

    stbi_image_free(data);
    return true;

free_image:
    stbi_image_free(data);
    return false;
}


void destory_texture(TRTexture &tex)
{
    if (tex.data)
        delete(tex.data);
    ZERO(tex);
}

bool is_valid_texture(TRTexture &tex)
{
    if (tex.data == nullptr || tex.w == 0 || tex.h == 0)
        return false;
    else
        return true;
}

bool load_obj(
        const char * path,
        std::vector<glm::vec3> & out_vertices,
        std::vector<glm::vec2> & out_uvs,
        std::vector<glm::vec3> & out_normals
        )
{
    printf("Loading OBJ file %s...\n", path);

    std::vector<unsigned int> vertexIndices, uvIndices, normalIndices;
    std::vector<glm::vec3> temp_vertices;
    std::vector<glm::vec2> temp_uvs;
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
            temp_uvs.push_back(uv);
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
        glm::vec2 uv = temp_uvs[ uvIndex-1 ];
        glm::vec3 normal = temp_normals[ normalIndex-1 ];

        // Put the attributes in buffers
        out_vertices.push_back(vertex);
        out_uvs     .push_back(uv);
        out_normals .push_back(normal);

    }
    fclose(file);
    return true;
}
