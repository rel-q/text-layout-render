//
// Created by bq on 2019-08-20.
//

#ifndef FONT_DEMO_GLRENDERER_H
#define FONT_DEMO_GLRENDERER_H

#include "glad/glad.h"
#include "Vertex.h"
#include "CacheTexture.h"
#include "MeshState.h"

class GLRenderer {
public:
    struct Mesh {
        GLuint primitiveMode; // GL_TRIANGLES and GL_TRIANGLE_STRIP supported

        // buffer object and void* are mutually exclusive.
        // Only GL_UNSIGNED_SHORT supported.
        struct Indices {
            GLuint bufferObject;
            const void* indices;
        } indices;

        // buffer object and void*s are mutually exclusive.
        // TODO: enforce mutual exclusion with restricted setters and/or unions
        struct Vertices {
            GLuint bufferObject;
            int attribFlags;
            const void* position;
            const void* texCoord;
            const void* color;
            GLsizei stride;
        } vertices;

        int elementCount;
        TextureVertex mappedVertices[4];
    } mesh;

    MeshState meshState;

    GLRenderer();

    void render(CacheTexture& texture);

};

#endif //FONT_DEMO_GLRENDERER_H
