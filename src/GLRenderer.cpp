//
// Created by bq on 2019-08-20.
//

#include "GLRenderer.h"
#include "MeshState.h"


GLRenderer::GLRenderer() {
}

void GLRenderer::render(CacheTexture& texture) {
    mesh.primitiveMode = GL_TRIANGLES;
    mesh.indices = {meshState.getQuadListIBO(), nullptr};
    mesh.vertices = {
            0,
            1,
            &texture.mesh()[0].x, &texture.mesh()[0].u, nullptr,
            kTextureVertexStride};
    mesh.elementCount = texture.meshElementCount();

    meshState.bindMeshBuffer(mesh.vertices.bufferObject);
    meshState.bindPositionVertexPointer(&texture.mesh()[0].x, mesh.vertices.stride);
    meshState.enableTexCoordsVertexArray();
    meshState.bindTexCoordsVertexPointer(&texture.mesh()[0].u, mesh.vertices.stride);

    // indices
    meshState.bindIndicesBuffer(mesh.indices.bufferObject);

    glDrawElements(
            mesh.primitiveMode, mesh.elementCount, GL_UNSIGNED_SHORT, nullptr);

}
