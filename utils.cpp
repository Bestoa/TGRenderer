#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "tr.h"

void save_ppm(const char *name, uint8_t *buffer, int width, int height)
{
    FILE *fp = fopen(name, "wb");
    if (!fp)
        abort();
    fprintf(fp, "P6\n%d %d\n255\n", width, height);
    fwrite(buffer, 1, width * height * 3, fp);
    fclose(fp);
}
#define __LINE_LEN__ (128)
bool load_ppm_texture(
        const char * path,
        TRTexture &tex
        )
{
    FILE * file = fopen(path, "rb");
    char line[__LINE_LEN__];
    uint8_t *data_line;
    if (file == NULL)
    {
        printf("Impossible to open the texture file ! Are you in the right path ?\n");
        return false;
    }
    // read header
    if (!fgets(line, __LINE_LEN__, file) || strncmp("P6", line, 2))
        goto read_error;

    // read w h
    if (!fgets(line, __LINE_LEN__, file))
        goto read_error;

    // skip comment
    if (line[0] == '#' && !fgets(line, __LINE_LEN__, file))
        goto read_error;
    sscanf(line, "%d%d\n", &tex.w, &tex.h);
    printf("Texutre: %dx%d\n", tex.w, tex.h);
    tex.stride = tex.w * 3;

    if (!fgets(line, __LINE_LEN__, file) || strncmp("255", line, 3))
        goto read_error;

    tex.data = new float[tex.stride * tex.h];
    if (!tex.data)
        goto read_error;

    data_line = new uint8_t[tex.stride];
    if (!data_line)
        goto free_texture;
    for (int i = 0; i < tex.h; i++)
    {
        if (fread(data_line, 1, tex.stride, file) != size_t(tex.stride))
            goto free_texture;
        for (int j = 0; j < tex.stride; j++)
            tex.data[tex.stride * i + j] = (float)data_line[j] / 255.0f;
    }

    fclose(file);

    return true;
free_texture:
    delete tex.data;
read_error:
    fclose(file);
    printf("Read ppm file failed\n");
    return false;
}
#undef __LINE_LEN__

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
