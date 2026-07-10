#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "../common/init.h"
#include "../common/log.h"
#include "colour.h"
#include "muxretro.h"
#include "settings.h"

#define FILTER_DIR "/opt/muos/share/filter/"
#define SHADER_DIR "/opt/muos/share/shader/"

#define SHADER_MAX_FILE_BYTES (64 * 1024)

typedef struct {
    int enabled;
    float matrix[9];
} colour_filter_matrix_t;

static char filter_names[COLOUR_FILTER_MAX][64];
static char filter_labels[COLOUR_FILTER_MAX][64];
static int filter_count = 0;

static char shader_names[COLOUR_SHADER_MAX][64];
static char shader_labels[COLOUR_SHADER_MAX][64];
static int shader_count = 0;

static int filter_loaded_index = -1;
static colour_filter_matrix_t filter_current;

static const char *vs_src = "attribute vec2 a_pos;"
                            "attribute vec2 a_uv;"
                            "varying highp vec2 v_uv;"

                            "void main(){"
                            "    gl_Position = vec4(a_pos, 0.0, 1.0);"
                            "    v_uv = a_uv;"
                            "}";

static const char *fs_src = "precision mediump float;"
                            "uniform sampler2D u_tex;"
                            "uniform float u_brightness;"
                            "uniform float u_contrast;"
                            "uniform float u_saturation;"
                            "uniform float u_cosH;"
                            "uniform float u_sinH;"
                            "uniform float u_gamma;"
                            "uniform mat3 u_filter;"
                            "uniform int u_filter_enabled;"
                            "varying highp vec2 v_uv;"

                            "vec3 apply_colour(vec3 c) {"
                            "    c += u_brightness;"
                            "    c = (c - 0.5) * u_contrast + 0.5;"
                            "    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));"
                            "    c = mix(vec3(l), c, u_saturation);"
                            "    mat3 hueMat = mat3("
                            "        0.299 + 0.701*u_cosH + 0.168*u_sinH, 0.587 - 0.587*u_cosH + 0.330*u_sinH, "
                            "0.114 - 0.114*u_cosH - 0.497*u_sinH,"
                            "        0.299 - 0.299*u_cosH - 0.328*u_sinH, 0.587 + 0.413*u_cosH + 0.035*u_sinH, "
                            "0.114 - 0.114*u_cosH + 0.292*u_sinH,"
                            "        0.299 - 0.300*u_cosH + 1.250*u_sinH, 0.587 - 0.588*u_cosH - 1.050*u_sinH, "
                            "0.114 + 0.886*u_cosH - 0.203*u_sinH"
                            "    );"
                            "    c = clamp(hueMat * c, 0.0, 1.0);"
                            "    if (u_filter_enabled != 0) { c = clamp(u_filter * c, 0.0, 1.0); }"
                            "    c = pow(c, vec3(1.0 / u_gamma));"
                            "    return c;"
                            "}"

                            "void main(){"
                            "    vec4 t = texture2D(u_tex, v_uv);"
                            "    vec3 corrected = vec3(t.b, t.g, t.r);"
                            "    gl_FragColor = vec4(apply_colour(corrected), 1.0);"
                            "}";

static const char *shader_vs_src = "attribute vec2 a_pos;\n"
                                   "attribute vec2 a_uv;\n"
                                   "varying vec2 v_uv;\n"
                                   "void main() {\n"
                                   "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
                                   "    v_uv = a_uv;\n"
                                   "}\n";

static const char *shader_fs_preamble = "precision mediump float;\n"
                                        "uniform sampler2D u_tex;\n"
                                        "uniform vec2 u_resolution;\n"
                                        "uniform vec2 u_native_resolution;\n"
                                        "uniform float u_time;\n"
                                        "uniform int u_frame;\n"
                                        "varying vec2 v_uv;\n";

static GLuint prog = 0;
static GLint a_pos = -1, a_uv = -1;
static GLint u_tex = -1, u_brightness = -1, u_contrast = -1, u_saturation = -1;
static GLint u_cos_h = -1, u_sin_h = -1, u_gamma = -1, u_filter = -1, u_filter_enabled = -1;
static int prog_attempted = 0;
static int prog_ready = 0;

static GLuint shader_prog = 0;
static GLint sh_a_pos = -1, sh_a_uv = -1;
static GLint sh_u_tex = -1, sh_u_resolution = -1, sh_u_native_resolution = -1, sh_u_time = -1, sh_u_frame = -1;
static int shader_loaded_index = -1;
static int shader_frame_count = 0;

static SDL_Texture *adjusted_tex = NULL;
static int adjusted_w = 0;
static int adjusted_h = 0;

static SDL_Texture *work_tex = NULL;
static int work_w = 0;
static int work_h = 0;

static GLuint(GL_APIENTRY *p_glCreateShader)(GLenum type) = NULL;
static void(GL_APIENTRY *p_glShaderSource)(
    GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length
) = NULL;
static void(GL_APIENTRY *p_glCompileShader)(GLuint shader) = NULL;
static void(GL_APIENTRY *p_glGetShaderiv)(GLuint shader, GLenum pname, GLint *params) = NULL;
static void(GL_APIENTRY *p_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) = NULL;
static void(GL_APIENTRY *p_glDeleteShader)(GLuint shader) = NULL;
static GLuint(GL_APIENTRY *p_glCreateProgram)(void) = NULL;
static void(GL_APIENTRY *p_glAttachShader)(GLuint program, GLuint shader) = NULL;
static void(GL_APIENTRY *p_glLinkProgram)(GLuint program) = NULL;
static void(GL_APIENTRY *p_glDeleteProgram)(GLuint program) = NULL;
static void(GL_APIENTRY *p_glGetProgramiv)(GLuint program, GLenum pname, GLint *params) = NULL;
static void(GL_APIENTRY *p_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) =
    NULL;
static GLint(GL_APIENTRY *p_glGetAttribLocation)(GLuint program, const GLchar *name) = NULL;
static GLint(GL_APIENTRY *p_glGetUniformLocation)(GLuint program, const GLchar *name) = NULL;
static void(GL_APIENTRY *p_glUseProgram)(GLuint program) = NULL;
static void(GL_APIENTRY *p_glVertexAttribPointer)(
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer
) = NULL;
static void(GL_APIENTRY *p_glEnableVertexAttribArray)(GLuint index) = NULL;
static void(GL_APIENTRY *p_glDisableVertexAttribArray)(GLuint index) = NULL;
static void(GL_APIENTRY *p_glUniform1i)(GLint location, GLint v0) = NULL;
static void(GL_APIENTRY *p_glUniform1f)(GLint location, GLfloat v0) = NULL;
static void(GL_APIENTRY *p_glUniform2f)(GLint location, GLfloat v0, GLfloat v1) = NULL;
static void(GL_APIENTRY *p_glUniformMatrix3fv)(
    GLint location, GLsizei count, GLboolean transpose, const GLfloat *value
) = NULL;
static void(GL_APIENTRY *p_glDrawArrays)(GLenum mode, GLint first, GLsizei count) = NULL;
static void(GL_APIENTRY *p_glBindBuffer)(GLenum target, GLuint buffer) = NULL;
static void(GL_APIENTRY *p_glActiveTexture)(GLenum texture) = NULL;
static void(GL_APIENTRY *p_glBindAttribLocation)(GLuint program, GLuint index, const GLchar *name) = NULL;
static void(GL_APIENTRY *p_glGetIntegerv)(GLenum pname, GLint *params) = NULL;
static void(GL_APIENTRY *p_glEnable)(GLenum cap) = NULL;
static void(GL_APIENTRY *p_glDisable)(GLenum cap) = NULL;
static GLboolean(GL_APIENTRY *p_glIsEnabled)(GLenum cap) = NULL;
static void(GL_APIENTRY *p_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height) = NULL;

static int gl_funcs_ready = 0;

static int load_gl_functions(void) {
    if (gl_funcs_ready) return 1;

#define LOAD_GL(name)                                                                                                  \
    do {                                                                                                               \
        p_##name = (void *) SDL_GL_GetProcAddress(#name);                                                              \
        if (!p_##name) {                                                                                               \
            LOG_ERROR(mux_module, "Colour: failed to resolve GL function %s", #name);                                  \
            return 0;                                                                                                  \
        }                                                                                                              \
    } while (0)

    LOAD_GL(glCreateShader);
    LOAD_GL(glShaderSource);
    LOAD_GL(glCompileShader);
    LOAD_GL(glGetShaderiv);
    LOAD_GL(glGetShaderInfoLog);
    LOAD_GL(glDeleteShader);
    LOAD_GL(glCreateProgram);
    LOAD_GL(glAttachShader);
    LOAD_GL(glLinkProgram);
    LOAD_GL(glDeleteProgram);
    LOAD_GL(glGetProgramiv);
    LOAD_GL(glGetProgramInfoLog);
    LOAD_GL(glGetAttribLocation);
    LOAD_GL(glGetUniformLocation);
    LOAD_GL(glUseProgram);
    LOAD_GL(glVertexAttribPointer);
    LOAD_GL(glEnableVertexAttribArray);
    LOAD_GL(glDisableVertexAttribArray);
    LOAD_GL(glUniform1i);
    LOAD_GL(glUniform1f);
    LOAD_GL(glUniform2f);
    LOAD_GL(glUniformMatrix3fv);
    LOAD_GL(glDrawArrays);
    LOAD_GL(glBindBuffer);
    LOAD_GL(glActiveTexture);
    LOAD_GL(glBindAttribLocation);
    LOAD_GL(glGetIntegerv);
    LOAD_GL(glEnable);
    LOAD_GL(glDisable);
    LOAD_GL(glIsEnabled);
    LOAD_GL(glViewport);

#undef LOAD_GL

    gl_funcs_ready = 1;
    return 1;
}

static int name_cmp(const void *a, const void *b) {
    return strcmp(a, b);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t')
        s++;

    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    *end = '\0';

    return s;
}

static void load_filter_label(const char *stem, char *out, const size_t out_len) {
    snprintf(out, out_len, "%s", stem);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s%s.ini", FILTER_DIR, stem);

    FILE *f = fopen(path, "r");
    if (!f) return;

    int in_profile = 0;
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == 0 || line[0] == '#') continue;

        if (line[0] == '[') {
            in_profile = strcmp(line, "[profile]") == 0;
            continue;
        }
        if (!in_profile) continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';

        const char *key = trim(line);
        const char *value = trim(eq + 1);

        if (strcasecmp(key, "name") == 0 && *value) {
            snprintf(out, out_len, "%s", value);
            break;
        }
    }

    fclose(f);
}

static void load_shader_label(const char *stem, char *out, const size_t out_len) {
    snprintf(out, out_len, "%s", stem);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s%s.frag", SHADER_DIR, stem);

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[256];

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;

        char *s = trim(line);
        if (!*s) continue;
        if (strncmp(s, "//", 2) != 0) break;

        s = trim(s + 2);

        char *colon = strchr(s, ':');
        if (!colon) continue;
        *colon = '\0';

        const char *key = trim(s);
        const char *value = trim(colon + 1);

        if (strcasecmp(key, "name") == 0 && *value) {
            snprintf(out, out_len, "%s", value);
            break;
        }
    }

    fclose(f);
}

static int scan_presets(const char *dir_path, const char *ext, char names[][64], const int max_count) {
    int count = 0;
    snprintf(names[count++], 64, "none");

    DIR *dir = opendir(dir_path);
    if (!dir) return count;

    char scanned[COLOUR_SHADER_MAX][64];
    int scanned_count = 0;

    const size_t ext_len = strlen(ext);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && scanned_count < max_count) {
        const size_t name_len = strlen(entry->d_name);
        if (name_len <= ext_len || strcmp(entry->d_name + name_len - ext_len, ext) != 0) continue;

        const size_t stem_len = name_len - ext_len;
        if (stem_len == 0 || stem_len >= 64) continue;

        if (stem_len == 4 && strncmp(entry->d_name, "none", 4) == 0) continue;

        snprintf(scanned[scanned_count], 64, "%.*s", (int) stem_len, entry->d_name);
        scanned_count++;
    }
    closedir(dir);

    qsort(scanned, (size_t) scanned_count, sizeof(scanned[0]), name_cmp);

    for (int i = 0; i < scanned_count && count < max_count; i++)
        snprintf(names[count++], 64, "%s", scanned[i]);

    return count;
}

void colour_init(void) {
    filter_count = scan_presets(FILTER_DIR, ".ini", filter_names, COLOUR_FILTER_MAX);
    for (int i = 0; i < filter_count; i++)
        load_filter_label(filter_names[i], filter_labels[i], sizeof(filter_labels[0]));

    shader_count = scan_presets(SHADER_DIR, ".frag", shader_names, COLOUR_SHADER_MAX);
    for (int i = 0; i < shader_count; i++)
        load_shader_label(shader_names[i], shader_labels[i], sizeof(shader_labels[0]));

    LOG_INFO(mux_module, "Colour: found %d filter preset(s), %d shader(s)", filter_count, shader_count);
}

int colour_filter_preset_count(void) {
    return filter_count > 0 ? filter_count : 1;
}

const char *colour_filter_preset_name(const int index) {
    if (index < 0 || index >= filter_count) return filter_names[0];
    return filter_names[index];
}

const char *colour_filter_preset_label(const int index) {
    if (index < 0 || index >= filter_count) return filter_labels[0];
    return filter_labels[index];
}

int colour_shader_count(void) {
    return shader_count > 0 ? shader_count : 1;
}

const char *colour_shader_name(const int index) {
    if (index < 0 || index >= shader_count) return shader_names[0];
    return shader_names[index];
}

const char *colour_shader_label(const int index) {
    if (index < 0 || index >= shader_count) return shader_labels[0];
    return shader_labels[index];
}

static int parse_float3(const char *line, float *a, float *b, float *c) {
    char *end;
    *a = strtof(line, &end);
    if (end == line) return 0;

    const char *p = end;
    *b = strtof(p, &end);
    if (end == p) return 0;

    p = end;
    *c = strtof(p, &end);
    if (end == p) return 0;

    return 1;
}

static int load_filter_file(const char *name, colour_filter_matrix_t *out) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s%s.ini", FILTER_DIR, name);

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    memset(out, 0, sizeof(*out));
    out->enabled = 1;

    int section_matrix = 0;
    int row = 0;
    int seen_matrix = 0;
    char line[128];

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == 0 || line[0] == '#') continue;

        if (line[0] == '[') {
            section_matrix = strcmp(line, "[matrix]") == 0;
            if (section_matrix) seen_matrix = 1;
            continue;
        }

        if (section_matrix && row < 3) {
            float a, b, c;
            if (!parse_float3(line, &a, &b, &c)) {
                fclose(f);
                return 0;
            }

            out->matrix[row * 3 + 0] = a;
            out->matrix[row * 3 + 1] = b;
            out->matrix[row * 3 + 2] = c;
            row++;
        }
    }

    fclose(f);
    return seen_matrix && row == 3;
}

static const colour_filter_matrix_t *current_filter(void) {
    const int index = session_settings.colour_filter;

    if (index == filter_loaded_index) return &filter_current;

    if (index <= 0 || index >= filter_count || strcmp(filter_names[index], "none") == 0) {
        memset(&filter_current, 0, sizeof(filter_current));
        filter_current.enabled = 0;
    } else if (!load_filter_file(filter_names[index], &filter_current)) {
        memset(&filter_current, 0, sizeof(filter_current));
        filter_current.enabled = 0;
    }

    filter_loaded_index = index;
    return &filter_current;
}

void colour_refresh(void) {
    filter_loaded_index = -1;
}

int colour_pass_needed(void) {
    if (session_settings.colour_brightness != 0) return 1;
    if (session_settings.colour_contrast != 100) return 1;
    if (session_settings.colour_saturation != 100) return 1;
    if (session_settings.colour_hueshift != 0) return 1;
    if (session_settings.colour_gamma != 100) return 1;
    if (session_settings.colour_shader != 0) return 1;
    return current_filter()->enabled;
}

static GLuint compile_shader(const GLenum type, const char *src) {
    const GLuint shader = p_glCreateShader(type);
    p_glShaderSource(shader, 1, &src, NULL);
    p_glCompileShader(shader);

    GLint ok = 0;
    p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        p_glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOG_ERROR(mux_module, "Colour: shader compile failed: %s", log);
        p_glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static void ensure_program(void) {
    if (prog_attempted) return;
    prog_attempted = 1;

    if (!load_gl_functions()) return;

    const GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    const GLuint fs = vs ? compile_shader(GL_FRAGMENT_SHADER, fs_src) : 0;

    if (!vs || !fs) {
        if (vs) p_glDeleteShader(vs);
        if (fs) p_glDeleteShader(fs);
        return;
    }

    prog = p_glCreateProgram();
    p_glAttachShader(prog, vs);
    p_glAttachShader(prog, fs);

    p_glBindAttribLocation(prog, 3, "a_pos");
    p_glBindAttribLocation(prog, 4, "a_uv");

    p_glLinkProgram(prog);

    p_glDeleteShader(vs);
    p_glDeleteShader(fs);

    GLint ok = 0;
    p_glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        p_glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        LOG_ERROR(mux_module, "Colour: program link failed: %s", log);
        p_glDeleteProgram(prog);
        prog = 0;
        return;
    }

    a_pos = p_glGetAttribLocation(prog, "a_pos");
    a_uv = p_glGetAttribLocation(prog, "a_uv");
    u_tex = p_glGetUniformLocation(prog, "u_tex");
    u_brightness = p_glGetUniformLocation(prog, "u_brightness");
    u_contrast = p_glGetUniformLocation(prog, "u_contrast");
    u_saturation = p_glGetUniformLocation(prog, "u_saturation");
    u_cos_h = p_glGetUniformLocation(prog, "u_cosH");
    u_sin_h = p_glGetUniformLocation(prog, "u_sinH");
    u_gamma = p_glGetUniformLocation(prog, "u_gamma");
    u_filter = p_glGetUniformLocation(prog, "u_filter");
    u_filter_enabled = p_glGetUniformLocation(prog, "u_filter_enabled");

    prog_ready = 1;
    LOG_INFO(mux_module, "Colour: shader program ready");
}

static char *read_shader_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        LOG_ERROR(mux_module, "Colour: cannot open shader: %s", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    rewind(f);

    if (sz <= 0 || sz > SHADER_MAX_FILE_BYTES) {
        LOG_ERROR(mux_module, "Colour: bad shader size (%ld bytes): %s", sz, path);
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t) sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    const size_t n = fread(buf, 1, (size_t) sz, f);
    buf[n] = '\0';
    fclose(f);

    return buf;
}

static void ensure_shader_program(void) {
    const int index = session_settings.colour_shader;
    if (index == shader_loaded_index) return;
    shader_loaded_index = index;

    if (shader_prog) {
        p_glDeleteProgram(shader_prog);
        shader_prog = 0;
    }
    sh_a_pos = sh_a_uv = -1;
    sh_u_tex = sh_u_resolution = sh_u_native_resolution = sh_u_time = sh_u_frame = -1;

    if (index <= 0 || index >= shader_count) return;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s%s.frag", SHADER_DIR, shader_names[index]);

    char *body = read_shader_file(path);
    if (!body) return;

    const size_t total = strlen(shader_fs_preamble) + strlen(body) + 1;
    char *full_src = malloc(total);
    if (!full_src) {
        free(body);
        return;
    }
    snprintf(full_src, total, "%s%s", shader_fs_preamble, body);
    free(body);

    const GLuint vs = compile_shader(GL_VERTEX_SHADER, shader_vs_src);
    const GLuint fs = vs ? compile_shader(GL_FRAGMENT_SHADER, full_src) : 0;
    free(full_src);

    if (!vs || !fs) {
        if (vs) p_glDeleteShader(vs);
        if (fs) p_glDeleteShader(fs);
        return;
    }

    shader_prog = p_glCreateProgram();
    p_glAttachShader(shader_prog, vs);
    p_glAttachShader(shader_prog, fs);

    p_glBindAttribLocation(shader_prog, 3, "a_pos");
    p_glBindAttribLocation(shader_prog, 4, "a_uv");

    p_glLinkProgram(shader_prog);

    p_glDeleteShader(vs);
    p_glDeleteShader(fs);

    GLint ok = 0;
    p_glGetProgramiv(shader_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        p_glGetProgramInfoLog(shader_prog, sizeof(log), NULL, log);
        LOG_ERROR(mux_module, "Colour: user shader link failed: %s", log);
        p_glDeleteProgram(shader_prog);
        shader_prog = 0;
        return;
    }

    sh_a_pos = p_glGetAttribLocation(shader_prog, "a_pos");
    sh_a_uv = p_glGetAttribLocation(shader_prog, "a_uv");
    sh_u_tex = p_glGetUniformLocation(shader_prog, "u_tex");
    sh_u_resolution = p_glGetUniformLocation(shader_prog, "u_resolution");
    sh_u_native_resolution = p_glGetUniformLocation(shader_prog, "u_native_resolution");
    sh_u_time = p_glGetUniformLocation(shader_prog, "u_time");
    sh_u_frame = p_glGetUniformLocation(shader_prog, "u_frame");

    LOG_INFO(mux_module, "Colour: user shader ready: %s", shader_names[index]);
}

static int ensure_target(SDL_Renderer *renderer, SDL_Texture **tex, int *tw, int *th, const int w, const int h) {
    if (w <= 0 || h <= 0) return 0;
    if (*tex && w == *tw && h == *th) return 1;

    if (*tex) {
        SDL_DestroyTexture(*tex);
        *tex = NULL;
    }

    *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if (!*tex) {
        LOG_ERROR(mux_module, "Colour: failed to create render target: %s", SDL_GetError());
        *tw = 0;
        *th = 0;
        return 0;
    }

    *tw = w;
    *th = h;
    return 1;
}

static void set_colour_uniforms(void) {
    const float brightness = (float) session_settings.colour_brightness / 100.0f;
    const float contrast = (float) session_settings.colour_contrast / 100.0f;
    const float saturation = (float) session_settings.colour_saturation / 100.0f;
    const float hue_rad = (float) session_settings.colour_hueshift * (float) M_PI / 180.0f;
    const float gamma = (float) session_settings.colour_gamma / 100.0f;
    const colour_filter_matrix_t *filter = current_filter();

    if (u_tex >= 0) p_glUniform1i(u_tex, 0);
    if (u_brightness >= 0) p_glUniform1f(u_brightness, brightness);
    if (u_contrast >= 0) p_glUniform1f(u_contrast, contrast);
    if (u_saturation >= 0) p_glUniform1f(u_saturation, saturation);
    if (u_cos_h >= 0) p_glUniform1f(u_cos_h, cosf(hue_rad));
    if (u_sin_h >= 0) p_glUniform1f(u_sin_h, sinf(hue_rad));
    if (u_gamma >= 0) p_glUniform1f(u_gamma, gamma);
    if (u_filter_enabled >= 0) p_glUniform1i(u_filter_enabled, filter->enabled);
    if (u_filter >= 0) p_glUniformMatrix3fv(u_filter, 1, GL_FALSE, filter->matrix);
}

static void set_shader_uniforms(const int res_w, const int res_h) {
    int native_w = 0, native_h = 0;
    video_bridge_get_frame_size(&native_w, &native_h);

    if (sh_u_tex >= 0) p_glUniform1i(sh_u_tex, 0);
    if (sh_u_resolution >= 0) p_glUniform2f(sh_u_resolution, (float) res_w, (float) res_h);
    if (sh_u_native_resolution >= 0) p_glUniform2f(sh_u_native_resolution, (float) native_w, (float) native_h);
    if (sh_u_time >= 0) p_glUniform1f(sh_u_time, (float) shader_frame_count);
    if (sh_u_frame >= 0) p_glUniform1i(sh_u_frame, shader_frame_count);
}

static int draw_gl_pass(
    SDL_Texture *src, const int user_prog, const float l, const float r, const float t, const float b, const int flip_v,
    const int vp_w, const int vp_h, const int res_w, const int res_h
) {
    float texw = 1.0f, texh = 1.0f;
    if (SDL_GL_BindTexture(src, &texw, &texh) != 0) return 0;
    p_glActiveTexture(GL_TEXTURE0);

    const GLfloat v_at_top = flip_v ? texh : 0.0f;
    const GLfloat v_at_bottom = flip_v ? 0.0f : texh;

    const GLfloat verts[] = {
        l, t, 0.0f, v_at_top,    // top left
        r, t, texw, v_at_top,    // top right
        l, b, 0.0f, v_at_bottom, // bottom left
        r, b, texw, v_at_bottom, // bottom right
    };

    p_glViewport(0, 0, vp_w, vp_h);

    const GLint pass_a_pos = user_prog ? sh_a_pos : a_pos;
    const GLint pass_a_uv = user_prog ? sh_a_uv : a_uv;

    p_glUseProgram(user_prog ? shader_prog : prog);

    if (user_prog) {
        set_shader_uniforms(res_w, res_h);
    } else {
        set_colour_uniforms();
    }

    p_glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (pass_a_pos >= 0) {
        p_glVertexAttribPointer(pass_a_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), verts);
        p_glEnableVertexAttribArray(pass_a_pos);
    }
    if (pass_a_uv >= 0) {
        p_glVertexAttribPointer(pass_a_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), verts + 2);
        p_glEnableVertexAttribArray(pass_a_uv);
    }

    p_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (pass_a_pos >= 0) p_glDisableVertexAttribArray(pass_a_pos);
    if (pass_a_uv >= 0) p_glDisableVertexAttribArray(pass_a_uv);

    SDL_GL_UnbindTexture(src);
    return 1;
}

void colour_render_pass(SDL_Renderer *renderer, SDL_Texture *tex, const SDL_Rect *dest_rect) {
    if (!colour_pass_needed()) {
        SDL_RenderCopy(renderer, tex, NULL, dest_rect);
        return;
    }

    ensure_program();

    if (!prog_ready) {
        SDL_RenderCopy(renderer, tex, NULL, dest_rect);
        return;
    }

    ensure_shader_program();
    const int use_shader =
        shader_prog != 0 && ensure_target(renderer, &work_tex, &work_w, &work_h, dest_rect->w, dest_rect->h);

    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
    SDL_Texture *gl_src = tex;

    if (mux_retro_get_pixel_format() != RETRO_PIXEL_FORMAT_XRGB8888) {
        if (!ensure_target(renderer, &adjusted_tex, &adjusted_w, &adjusted_h, dest_rect->w, dest_rect->h)
            || SDL_SetRenderTarget(renderer, adjusted_tex) != 0) {
            SDL_RenderCopy(renderer, tex, NULL, dest_rect);
            return;
        }
        SDL_RenderCopy(renderer, tex, NULL, NULL);
        SDL_SetRenderTarget(renderer, prev_target);
        gl_src = adjusted_tex;
    }

    int out_w = 0, out_h = 0;
    if (prev_target) {
        SDL_QueryTexture(prev_target, NULL, NULL, &out_w, &out_h);
    } else {
        SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    }
    if (out_w <= 0 || out_h <= 0) {
        SDL_RenderCopy(renderer, tex, NULL, dest_rect);
        return;
    }

    const float ndc_left = ((float) dest_rect->x / (float) out_w) * 2.0f - 1.0f;
    const float ndc_right = ((float) (dest_rect->x + dest_rect->w) / (float) out_w) * 2.0f - 1.0f;
    const float ndc_top = 1.0f - ((float) dest_rect->y / (float) out_h) * 2.0f;
    const float ndc_bottom = 1.0f - ((float) (dest_rect->y + dest_rect->h) / (float) out_h) * 2.0f;

    GLint prev_program = 0;
    p_glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

    GLint prev_viewport[4] = {0};
    p_glGetIntegerv(GL_VIEWPORT, prev_viewport);

    const GLboolean prev_blend_enabled = p_glIsEnabled(GL_BLEND);
    const GLboolean prev_scissor_enabled = p_glIsEnabled(GL_SCISSOR_TEST);

    p_glDisable(GL_BLEND);
    p_glDisable(GL_SCISSOR_TEST);

    int drew = 0;

    if (use_shader) {
        if (SDL_SetRenderTarget(renderer, work_tex) == 0) {
            const int colour_ok =
                draw_gl_pass(gl_src, 0, -1.0f, 1.0f, 1.0f, -1.0f, 1, dest_rect->w, dest_rect->h, 0, 0);
            SDL_SetRenderTarget(renderer, prev_target);

            if (colour_ok) {
                shader_frame_count++;
                drew = draw_gl_pass(
                    work_tex, 1, ndc_left, ndc_right, ndc_top, ndc_bottom, 0, out_w, out_h, dest_rect->w, dest_rect->h
                );
            }
        }
    }

    if (!drew) drew = draw_gl_pass(gl_src, 0, ndc_left, ndc_right, ndc_top, ndc_bottom, 0, out_w, out_h, 0, 0);

    p_glUseProgram((GLuint) prev_program);
    p_glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    if (prev_blend_enabled) p_glEnable(GL_BLEND);
    if (prev_scissor_enabled) p_glEnable(GL_SCISSOR_TEST);

    if (!drew) SDL_RenderCopy(renderer, tex, NULL, dest_rect);
}
