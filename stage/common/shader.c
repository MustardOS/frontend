#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <GLES2/gl2.h>
#include "shader.h"
#include "common.h"
#include "../../common/log.h"

#define SHADER_DIR       INTERNAL_SHARE "/shader/"
#define SHADER_NAME_PATH OVERLAY_RUNNER "shader"

#define SHADER_MAX_FILE_BYTES (64 * 1024)

static shader_prog_t active_shader;
static time_t active_shader_mtime = 0;
static int active_shader_loaded = 0;
static char active_shader_path[PATH_MAX] = {0};

static const char vertex[] =
        "attribute vec2 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
        "    v_uv = a_uv;\n"
        "}\n";

static const char frag[] =
        "precision mediump float;\n"
        "uniform sampler2D u_tex;\n"
        "uniform vec2 u_resolution;\n"
        "uniform float u_time;\n"
        "uniform int u_frame;\n"
        "varying vec2 v_uv;\n";

static char *read_shader(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        LOG_ERROR("shader", "Cannot open: %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz <= 0 || sz > SHADER_MAX_FILE_BYTES) {
        LOG_ERROR("shader", "Bad Size (%ld bytes): %s", sz, path);
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t) sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t) sz, f);
    buf[n] = '\0';
    fclose(f);

    return buf;
}

static GLuint compile_shader(GLenum type, const char **parts, int count) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, count, parts, NULL);
    glCompileShader(sh);

    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (ok != GL_TRUE) {
        char log_buf[1024];
        GLsizei len = 0;
        glGetShaderInfoLog(sh, (GLsizei) sizeof(log_buf), &len, log_buf);

        LOG_ERROR("shader", "Compile Error: %.*s", (int) len, log_buf);
        glDeleteShader(sh);

        return 0;
    }

    return sh;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();

    glAttachShader(p, vs);
    glAttachShader(p, fs);

    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_uv");

    glLinkProgram(p);

    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);

    if (ok != GL_TRUE) {
        char log_buf[512];
        GLsizei len = 0;
        glGetProgramInfoLog(p, (GLsizei) sizeof(log_buf), &len, log_buf);

        LOG_ERROR("shader", "Link Error: %.*s", (int) len, log_buf);
        glDeleteProgram(p);

        return 0;
    }

    return p;
}

int shader_load(const char *path, shader_prog_t *out) {
    if (!path || !out) return 0;

    char *frag_src = read_shader(path);
    if (!frag_src) return 0;

    const char *vs_parts[] = {vertex};
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_parts, 1);

    const char *fs_parts[] = {frag, frag_src};
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_parts, 2);

    free(frag_src);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint prog = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!prog) return 0;

    shader_prog_t sh;
    memset(&sh, 0, sizeof(sh));

    sh.program = prog;
    sh.u_tex = glGetUniformLocation(prog, "u_tex");
    sh.u_resolution = glGetUniformLocation(prog, "u_resolution");
    sh.u_time = glGetUniformLocation(prog, "u_time");
    sh.u_frame = glGetUniformLocation(prog, "u_frame");
    sh.a_pos = glGetAttribLocation(prog, "a_pos");
    sh.a_uv = glGetAttribLocation(prog, "a_uv");

    glUseProgram(prog);
    if (sh.u_tex >= 0) glUniform1i(sh.u_tex, 0);
    glUseProgram(0);

    LOG_INFO("shader", "Loaded: %s", path);
    *out = sh;

    return 1;
}

void shader_destroy(shader_prog_t *prog) {
    if (!prog) return;

    if (prog->program) {
        glDeleteProgram(prog->program);
        prog->program = 0;
    }

    memset(prog, 0, sizeof(*prog));
}

static int resolve_shader_path(char *out) {
    char name[64] = {0};

    if (!read_line_from_file(SHADER_NAME_PATH, 1, name, sizeof(name))) return 0;
    if (!name[0] || strcmp(name, "none") == 0) return 0;

    snprintf(out, PATH_MAX, "%s%s.frag", SHADER_DIR, name);
    return access(out, F_OK) == 0;
}

void shader_reload(void) {
    static int reload_tick = 0;
    if (active_shader_loaded) {
        if (++reload_tick < 60) return;
        reload_tick = 0;
    }

    char path[PATH_MAX] = {0};

    if (!resolve_shader_path(path)) {
        if (active_shader_loaded) {
            LOG_INFO("shader", "Removed... unloading!");
            shader_destroy(&active_shader);

            active_shader_loaded = 0;
            active_shader_mtime = 0;
            active_shader_path[0] = '\0';
        }

        return;
    }

    struct stat st;
    if (stat(path, &st) != 0) return;

    const int path_changed = (strcmp(path, active_shader_path) != 0);
    if (!path_changed && st.st_mtime == active_shader_mtime) return;

    snprintf(active_shader_path, sizeof(active_shader_path), "%s", path);
    active_shader_mtime = st.st_mtime;

    if (active_shader_loaded) {
        shader_destroy(&active_shader);
        active_shader_loaded = 0;
    }

    if (shader_load(path, &active_shader)) active_shader_loaded = 1;
}

const shader_prog_t *shader_get(void) {
    if (!active_shader_loaded) return NULL;

    return &active_shader;
}
