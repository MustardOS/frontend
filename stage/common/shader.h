#pragma once

#include <GLES2/gl2.h>
#include <sys/types.h>
#include <limits.h>

typedef struct {
    GLuint program;

    GLint u_tex;
    GLint u_resolution;
    GLint u_native_resolution;
    GLint u_time;
    GLint u_frame;

    GLint a_pos;
    GLint a_uv;
} shader_prog_t;

int shader_load(const char *path, shader_prog_t *out);

void shader_destroy(shader_prog_t *prog);

void shader_reload(void);

const shader_prog_t *shader_get(void);
