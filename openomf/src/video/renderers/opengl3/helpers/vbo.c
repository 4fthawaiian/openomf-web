#include "video/renderers/opengl3/helpers/vbo.h"
#include "video/renderers/opengl3/helpers/bindings.h"

#include <stdlib.h>

GLuint vbo_create(GLsizeiptr size) {
    GLuint id;
    glGenBuffers(1, &id);
    bindings_bind_vbo(id);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
    return id;
}

#ifdef __EMSCRIPTEN__
/* WebGL2 has no buffer mapping; emulate it with a CPU-side staging buffer
   that gets uploaded via glBufferSubData on unmap. */
static void *vbo_mapped_ptr = NULL;

void *vbo_map(GLuint id, GLsizei size) {
    bindings_bind_vbo(id);
    vbo_mapped_ptr = malloc(size);
    return vbo_mapped_ptr;
}

void vbo_unmap(GLuint id, GLsizei size) {
    bindings_bind_vbo(id);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, vbo_mapped_ptr);
    free(vbo_mapped_ptr);
    vbo_mapped_ptr = NULL;
}
#else
void *vbo_map(GLuint id, GLsizei size) {
    bindings_bind_vbo(id);
    return glMapBufferRange(GL_ARRAY_BUFFER, 0, size,
                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
}

void vbo_unmap(GLuint id, GLsizei size) {
    bindings_bind_vbo(id);
    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, size);
    glUnmapBuffer(GL_ARRAY_BUFFER);
}
#endif

void vbo_free(GLuint id) {
    bindings_unbind_vbo(id);
    glDeleteBuffers(1, &id);
}
