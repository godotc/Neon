#ifndef GLINTERNAL_VERTEXBUFFER_H
#define GLINTERNAL_VERTEXBUFFER_H

#include <glad/glad.h>

namespace glinternal
{

class VertexBuffer
{
  public:
    VertexBuffer(GLenum buffertype, size_t datasize, float *data, GLuint drawType);

    GLuint Id;
};

}; // namespace glinternal

#endif