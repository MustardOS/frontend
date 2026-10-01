#include "effects.h"

#include <GLES2/gl2.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#include <module/muxshare.h>
#include <common/base/function_pointer.h>
#include "../settings/assets.h"
#include "../core/paths.h"

#define SHADER_FILE_MAX (128 * 1024)
#define SHADER_PARAM_MAX 16

typedef struct {
    char name[32];
    char label[64];
    float def;
    float min;
    float max;
    float step;
    float value;
    GLint location;
} shader_parameter;

static SDL_Texture *input_texture;
static SDL_Texture *output_texture;
static SDL_Texture *colour_texture;
static int input_width;
static int input_height;
static int output_width;
static int output_height;
static int colour_width;
static int colour_height;
static GLuint program;
static GLuint colour_program;
static GLint attribute_position = -1;
static GLint attribute_uv = -1;
static GLint uniform_texture = -1;
static GLint uniform_resolution = -1;
static GLint uniform_native_resolution = -1;
static GLint uniform_source_resolution = -1;
static GLint uniform_texture_resolution = -1;
static GLint uniform_source_extent = -1;
static GLint uniform_time = -1;
static GLint uniform_frame = -1;
static GLint colour_attribute_position = -1;
static GLint colour_attribute_uv = -1;
static GLint colour_uniform_texture = -1;
static GLint colour_uniform_matrix[9];
static GLint colour_uniform_brightness = -1;
static GLint colour_uniform_contrast = -1;
static GLint colour_uniform_saturation = -1;
static GLint colour_uniform_hue = -1;
static GLint colour_uniform_gamma = -1;
static shader_parameter parameters[SHADER_PARAM_MAX];
static int parameter_count;
static int shader_frame;
static int revision = 1;
static int loaded_revision;
static int colour_loaded_revision;
static int colour_filter_active;
static float colour_matrix[9];
static char loaded_key[MAX_BUFFER_SIZE];

typedef struct {
    GLuint(GL_APIENTRY *CreateShader)(GLenum);
    void(GL_APIENTRY *ShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
    void(GL_APIENTRY *CompileShader)(GLuint);
    void(GL_APIENTRY *GetShaderiv)(GLuint, GLenum, GLint *);
    void(GL_APIENTRY *GetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    void(GL_APIENTRY *DeleteShader)(GLuint);
    GLuint(GL_APIENTRY *CreateProgram)(void);
    void(GL_APIENTRY *AttachShader)(GLuint, GLuint);
    void(GL_APIENTRY *BindAttribLocation)(GLuint, GLuint, const GLchar *);
    void(GL_APIENTRY *LinkProgram)(GLuint);
    void(GL_APIENTRY *DeleteProgram)(GLuint);
    void(GL_APIENTRY *GetProgramiv)(GLuint, GLenum, GLint *);
    void(GL_APIENTRY *GetProgramInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
    GLint(GL_APIENTRY *GetAttribLocation)(GLuint, const GLchar *);
    GLint(GL_APIENTRY *GetUniformLocation)(GLuint, const GLchar *);
    void(GL_APIENTRY *ActiveTexture)(GLenum);
    void(GL_APIENTRY *Viewport)(GLint, GLint, GLsizei, GLsizei);
    void(GL_APIENTRY *UseProgram)(GLuint);
    void(GL_APIENTRY *Uniform1i)(GLint, GLint);
    void(GL_APIENTRY *Uniform1f)(GLint, GLfloat);
    void(GL_APIENTRY *Uniform2f)(GLint, GLfloat, GLfloat);
    void(GL_APIENTRY *BindBuffer)(GLenum, GLuint);
    void(GL_APIENTRY *VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
    void(GL_APIENTRY *EnableVertexAttribArray)(GLuint);
    void(GL_APIENTRY *DisableVertexAttribArray)(GLuint);
    void(GL_APIENTRY *DrawArrays)(GLenum, GLint, GLsizei);
    void(GL_APIENTRY *GetIntegerv)(GLenum, GLint *);
    GLboolean(GL_APIENTRY *IsEnabled)(GLenum);
    void(GL_APIENTRY *Enable)(GLenum);
    void(GL_APIENTRY *Disable)(GLenum);
} wasabi_gl_api;

static wasabi_gl_api gl_api;
static SDL_GLContext gl_context;

static int load_gl(void) {
    const SDL_GLContext current = SDL_GL_GetCurrentContext();
    if (!current) return 0;
    if (current == gl_context && gl_api.CreateShader) return 1;
    memset(&gl_api, 0, sizeof(gl_api));
    gl_context = current;
#define LOAD_GL(name)                                                                                                  \
    do {                                                                                                               \
        MUOS_FUNCTION_ASSIGN(gl_api.name, SDL_GL_GetProcAddress("gl" #name));                                         \
        if (!gl_api.name) return 0;                                                                                    \
    } while (0)
    LOAD_GL(CreateShader);
    LOAD_GL(ShaderSource);
    LOAD_GL(CompileShader);
    LOAD_GL(GetShaderiv);
    LOAD_GL(GetShaderInfoLog);
    LOAD_GL(DeleteShader);
    LOAD_GL(CreateProgram);
    LOAD_GL(AttachShader);
    LOAD_GL(BindAttribLocation);
    LOAD_GL(LinkProgram);
    LOAD_GL(DeleteProgram);
    LOAD_GL(GetProgramiv);
    LOAD_GL(GetProgramInfoLog);
    LOAD_GL(GetAttribLocation);
    LOAD_GL(GetUniformLocation);
    LOAD_GL(ActiveTexture);
    LOAD_GL(Viewport);
    LOAD_GL(UseProgram);
    LOAD_GL(Uniform1i);
    LOAD_GL(Uniform1f);
    LOAD_GL(Uniform2f);
    LOAD_GL(BindBuffer);
    LOAD_GL(VertexAttribPointer);
    LOAD_GL(EnableVertexAttribArray);
    LOAD_GL(DisableVertexAttribArray);
    LOAD_GL(DrawArrays);
    LOAD_GL(GetIntegerv);
    LOAD_GL(IsEnabled);
    LOAD_GL(Enable);
    LOAD_GL(Disable);
#undef LOAD_GL
    return 1;
}

#define glCreateShader gl_api.CreateShader
#define glShaderSource gl_api.ShaderSource
#define glCompileShader gl_api.CompileShader
#define glGetShaderiv gl_api.GetShaderiv
#define glGetShaderInfoLog gl_api.GetShaderInfoLog
#define glDeleteShader gl_api.DeleteShader
#define glCreateProgram gl_api.CreateProgram
#define glAttachShader gl_api.AttachShader
#define glBindAttribLocation gl_api.BindAttribLocation
#define glLinkProgram gl_api.LinkProgram
#define glDeleteProgram gl_api.DeleteProgram
#define glGetProgramiv gl_api.GetProgramiv
#define glGetProgramInfoLog gl_api.GetProgramInfoLog
#define glGetAttribLocation gl_api.GetAttribLocation
#define glGetUniformLocation gl_api.GetUniformLocation
#define glActiveTexture gl_api.ActiveTexture
#define glViewport gl_api.Viewport
#define glUseProgram gl_api.UseProgram
#define glUniform1i gl_api.Uniform1i
#define glUniform1f gl_api.Uniform1f
#define glUniform2f gl_api.Uniform2f
#define glBindBuffer gl_api.BindBuffer
#define glVertexAttribPointer gl_api.VertexAttribPointer
#define glEnableVertexAttribArray gl_api.EnableVertexAttribArray
#define glDisableVertexAttribArray gl_api.DisableVertexAttribArray
#define glDrawArrays gl_api.DrawArrays
#define glGetIntegerv gl_api.GetIntegerv
#define glIsEnabled gl_api.IsEnabled
#define glEnable gl_api.Enable
#define glDisable gl_api.Disable

static const char vertex_source[] =
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying highp vec2 v_uv;\n"
    "void main(){ gl_Position=vec4(a_pos,0.0,1.0); v_uv=a_uv; }\n";

static const char fragment_preamble[] =
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec2 u_resolution;\n"
    "uniform vec2 u_native_resolution;\n"
    "uniform vec2 u_source_resolution;\n"
    "uniform vec2 u_texture_resolution;\n"
    "uniform vec2 u_source_uv_extent;\n"
    "uniform float u_time;\n"
    "uniform int u_frame;\n"
    "varying vec2 v_uv;\n";

static const char colour_fragment[] =
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "uniform sampler2D u_tex;\n"
    "uniform float u_m0; uniform float u_m1; uniform float u_m2;\n"
    "uniform float u_m3; uniform float u_m4; uniform float u_m5;\n"
    "uniform float u_m6; uniform float u_m7; uniform float u_m8;\n"
    "uniform float u_brightness; uniform float u_contrast; uniform float u_saturation;\n"
    "uniform float u_hue; uniform float u_gamma;\n"
    "varying vec2 v_uv;\n"
    "void main(){\n"
    " vec4 sample=texture2D(u_tex,v_uv); vec3 c=sample.rgb;\n"
    " c+=u_brightness; c=(c-0.5)*u_contrast+0.5;\n"
    " float l=dot(c,vec3(0.2126,0.7152,0.0722)); c=mix(vec3(l),c,u_saturation);\n"
    " float cs=cos(u_hue); float sn=sin(u_hue);\n"
    " mat3 hueMat=mat3(\n"
    "  0.299+0.701*cs+0.168*sn,0.587-0.587*cs+0.330*sn,0.114-0.114*cs-0.497*sn,\n"
    "  0.299-0.299*cs-0.328*sn,0.587+0.413*cs+0.035*sn,0.114-0.114*cs+0.292*sn,\n"
    "  0.299-0.300*cs+1.250*sn,0.587-0.588*cs-1.050*sn,0.114+0.886*cs-0.203*sn);\n"
    " c=clamp(hueMat*c,0.0,1.0);\n"
    " c=vec3(dot(c,vec3(u_m0,u_m3,u_m6)),dot(c,vec3(u_m1,u_m4,u_m7)),dot(c,vec3(u_m2,u_m5,u_m8)));\n"
    " c=pow(clamp(c,0.0,1.0),vec3(u_gamma));\n"
    " gl_FragColor=vec4(clamp(c,0.0,1.0),sample.a);\n"
    "}\n";

static void destroy_program(void) {
    if (program && gl_api.DeleteProgram && SDL_GL_GetCurrentContext()) glDeleteProgram(program);
    program = 0;
    attribute_position = attribute_uv = -1;
    uniform_texture = uniform_resolution = uniform_native_resolution = -1;
    uniform_source_resolution = uniform_texture_resolution = uniform_source_extent = -1;
    uniform_time = uniform_frame = -1;
    parameter_count = 0;
}

static int read_colour_matrix(float matrix[9]) {
    for (int index = 0; index < 9; index++) matrix[index] = index % 4 == 0 ? 1.0f : 0.0f;
    wasabi_assets_refresh(wasabi_asset_filter);
    const int selected = wasabi_asset_selected(wasabi_asset_filter);
    const char *path = wasabi_asset_path(wasabi_asset_filter, selected);
    if (selected <= 0 || !path || !path[0]) return 0;
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    char line[128];
    float parsed[9] = {0};
    int in_matrix = 0;
    int row = 0;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || line[0] == '#') continue;
        if (line[0] == '[') {
            in_matrix = strcmp(line, "[matrix]") == 0;
            continue;
        }
        if (!in_matrix || row >= 3) continue;
        char *cursor = line;
        for (int column = 0; column < 3; column++) {
            char *end = NULL;
            const float value = strtof(cursor, &end);
            if (end == cursor || !isfinite(value)) {
                fclose(file);
                return 0;
            }
            parsed[row * 3 + column] = value;
            cursor = end;
        }
        row++;
    }
    fclose(file);
    if (row != 3) return 0;
    memcpy(matrix, parsed, sizeof(parsed));
    return 1;
}

static GLuint compile_shader(const GLenum type, const char *source) {
    const GLuint shader = glCreateShader(type);
    if (!shader) return 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint okay = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &okay);
    if (!okay) {
        char message[512];
        glGetShaderInfoLog(shader, sizeof(message), NULL, message);
        LOG_ERROR(mux_module, "Wasabi shader compilation failed: %s", message);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static int load_colour_program(void) {
    if (colour_program) return 1;
    if (!load_gl()) return 0;
    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment = vertex ? compile_shader(GL_FRAGMENT_SHADER, colour_fragment) : 0;
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }
    colour_program = glCreateProgram();
    glAttachShader(colour_program, vertex);
    glAttachShader(colour_program, fragment);
    glBindAttribLocation(colour_program, 3, "a_pos");
    glBindAttribLocation(colour_program, 4, "a_uv");
    glLinkProgram(colour_program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint okay = 0;
    glGetProgramiv(colour_program, GL_LINK_STATUS, &okay);
    if (!okay) {
        glDeleteProgram(colour_program);
        colour_program = 0;
        return 0;
    }
    colour_attribute_position = glGetAttribLocation(colour_program, "a_pos");
    colour_attribute_uv = glGetAttribLocation(colour_program, "a_uv");
    colour_uniform_texture = glGetUniformLocation(colour_program, "u_tex");
    const char *matrix_names[] = {"u_m0", "u_m1", "u_m2", "u_m3", "u_m4", "u_m5", "u_m6", "u_m7", "u_m8"};
    for (int index = 0; index < 9; index++)
        colour_uniform_matrix[index] = glGetUniformLocation(colour_program, matrix_names[index]);
    colour_uniform_brightness = glGetUniformLocation(colour_program, "u_brightness");
    colour_uniform_contrast = glGetUniformLocation(colour_program, "u_contrast");
    colour_uniform_saturation = glGetUniformLocation(colour_program, "u_saturation");
    colour_uniform_hue = glGetUniformLocation(colour_program, "u_hue");
    colour_uniform_gamma = glGetUniformLocation(colour_program, "u_gamma");
    return 1;
}

static char *read_shader(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    const long length = ftell(file);
    if (length <= 0 || length > SHADER_FILE_MAX || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    char *source = malloc((size_t) length + 1);
    if (!source) {
        fclose(file);
        return NULL;
    }
    const size_t read = fread(source, 1, (size_t) length, file);
    fclose(file);
    source[read] = '\0';
    return source;
}

static void parse_parameters(char *source) {
    parameter_count = 0;
    char *cursor = source;
    while (parameter_count < SHADER_PARAM_MAX && (cursor = strstr(cursor, "#pragma parameter"))) {
        char *line_end = strchr(cursor, '\n');
        char *read = cursor + strlen("#pragma parameter");
        while (*read == ' ' || *read == '\t') read++;
        shader_parameter parameter = {.location = -1};
        size_t length = 0;
        while (*read && !isspace((unsigned char) *read) && length + 1 < sizeof(parameter.name))
            parameter.name[length++] = *read++;
        parameter.name[length] = '\0';
        while (*read == ' ' || *read == '\t') read++;
        if (*read == '"') {
            read++;
            size_t label_length = 0;
            while (*read && *read != '"' && label_length + 1 < sizeof(parameter.label))
                parameter.label[label_length++] = *read++;
            parameter.label[label_length] = '\0';
            while (*read && *read != '"') read++;
            if (*read == '"') read++;
        }
        char *end = NULL;
        parameter.def = strtof(read, &end);
        if (end != read) read = end;
        parameter.min = strtof(read, &end);
        if (end != read) read = end;
        parameter.max = strtof(read, &end);
        if (end != read) read = end;
        parameter.step = strtof(read, &end);
        if (parameter.name[0] && parameter.max > parameter.min) {
            if (!parameter.label[0]) snprintf(parameter.label, sizeof(parameter.label), "%s", parameter.name);
            if (parameter.step <= 0.0f) parameter.step = (parameter.max - parameter.min) / 20.0f;
            if (parameter.def < parameter.min) parameter.def = parameter.min;
            if (parameter.def > parameter.max) parameter.def = parameter.max;
            parameter.value = parameter.def;
            parameters[parameter_count++] = parameter;
        }
        char *blank_end = line_end ? line_end : cursor + strlen(cursor);
        while (cursor < blank_end) *cursor++ = ' ';
        if (!line_end) break;
        cursor = line_end + 1;
    }
}

static void parameter_path(char *path, const size_t size) {
    snprintf(path, size, WASABI_SHARE_PATH "shaderopt/%s.ini", config.video.shader);
}

static void load_parameter_values(void) {
    char path[PATH_MAX];
    parameter_path(path, sizeof(path));
    FILE *file = fopen(path, "r");
    if (!file) return;
    char line[128];
    while (fgets(line, sizeof(line), file)) {
        char *separator = strchr(line, '=');
        if (!separator) continue;
        *separator++ = '\0';
        char *end = NULL;
        const float value = strtof(separator, &end);
        if (end == separator) continue;
        for (int index = 0; index < parameter_count; index++) {
            if (strcmp(parameters[index].name, line) != 0) continue;
            parameters[index].value = fminf(parameters[index].max, fmaxf(parameters[index].min, value));
            break;
        }
    }
    fclose(file);
}

static void save_parameter_values(void) {
    char path[PATH_MAX];
    parameter_path(path, sizeof(path));
    create_directories(path, 1);
    char output[2048];
    size_t used = 0;
    for (int index = 0; index < parameter_count && used < sizeof(output); index++)
        used += (size_t) snprintf(output + used, sizeof(output) - used, "%s=%g\n", parameters[index].name,
                                  (double) parameters[index].value);
    write_text_to_file_atomic(path, CHAR, output);
}

static void blank_filter_pragmas(char *source) {
    char *line = source;
    while (*line) {
        char *end = strchr(line, '\n');
        char *cursor = line;
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (strncasecmp(cursor, "#pragma filter", 14) == 0
            || strncasecmp(cursor, "#pragma direct-source", 21) == 0)
            while (*cursor && *cursor != '\n') *cursor++ = ' ';
        if (!end) break;
        line = end + 1;
    }
}

static int load_program(void) {
    if (loaded_revision == revision && strcmp(loaded_key, config.video.shader) == 0) return program != 0;
    loaded_revision = revision;
    snprintf(loaded_key, sizeof(loaded_key), "%s", config.video.shader);
    destroy_program();
    if (!config.video.shader[0] || strcasecmp(config.video.shader, "none") == 0) return 0;
    if (!load_gl()) return 0;

    wasabi_assets_refresh(wasabi_asset_shader);
    const int selected = wasabi_asset_selected(wasabi_asset_shader);
    const char *path = wasabi_asset_path(wasabi_asset_shader, selected);
    if (selected <= 0 || !path || !path[0]) return 0;
    char *body = read_shader(path);
    if (!body) return 0;

    char *start = body;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
    if (strncmp(start, "#version", 8) == 0) {
        char *end = strchr(start, '\n');
        if (end)
            memset(start, ' ', (size_t) (end - start));
        else
            *start = '\0';
    }
    parse_parameters(body);
    load_parameter_values();
    blank_filter_pragmas(body);

    const size_t source_length = strlen(fragment_preamble) + strlen(body) + 1;
    char *fragment_source = malloc(source_length);
    if (!fragment_source) {
        free(body);
        return 0;
    }
    snprintf(fragment_source, source_length, "%s%s", fragment_preamble, body);
    free(body);

    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    const GLuint fragment = vertex ? compile_shader(GL_FRAGMENT_SHADER, fragment_source) : 0;
    free(fragment_source);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }
    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 3, "a_pos");
    glBindAttribLocation(program, 4, "a_uv");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint okay = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &okay);
    if (!okay) {
        char message[512];
        glGetProgramInfoLog(program, sizeof(message), NULL, message);
        LOG_ERROR(mux_module, "Wasabi shader linking failed: %s", message);
        destroy_program();
        return 0;
    }

    attribute_position = glGetAttribLocation(program, "a_pos");
    attribute_uv = glGetAttribLocation(program, "a_uv");
    uniform_texture = glGetUniformLocation(program, "u_tex");
    uniform_resolution = glGetUniformLocation(program, "u_resolution");
    uniform_native_resolution = glGetUniformLocation(program, "u_native_resolution");
    uniform_source_resolution = glGetUniformLocation(program, "u_source_resolution");
    uniform_texture_resolution = glGetUniformLocation(program, "u_texture_resolution");
    uniform_source_extent = glGetUniformLocation(program, "u_source_uv_extent");
    uniform_time = glGetUniformLocation(program, "u_time");
    uniform_frame = glGetUniformLocation(program, "u_frame");
    for (int index = 0; index < parameter_count; index++)
        parameters[index].location = glGetUniformLocation(program, parameters[index].name);
    shader_frame = 0;
    return 1;
}

static int ensure_texture(SDL_Renderer *renderer, SDL_Texture **texture, int *current_width, int *current_height,
                          const int width, const int height) {
    if (*texture && *current_width == width && *current_height == height) return 1;
    if (*texture) SDL_DestroyTexture(*texture);
    *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, width, height);
    if (!*texture) {
        *current_width = *current_height = 0;
        return 0;
    }
    *current_width = width;
    *current_height = height;
    return 1;
}

static int draw_shader(
    SDL_Texture *source, const int source_width, const int source_height, const int width, const int height
) {
    float texture_width = 1.0f;
    float texture_height = 1.0f;
    glActiveTexture(GL_TEXTURE0);
    if (SDL_GL_BindTexture(source, &texture_width, &texture_height) != 0) return 0;
    const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, texture_width, 0.0f,
        -1.0f, 1.0f, 0.0f, texture_height, 1.0f, 1.0f, texture_width, texture_height,
    };
    glViewport(0, 0, width, height);
    glUseProgram(program);
    if (uniform_texture >= 0) glUniform1i(uniform_texture, 0);
    if (uniform_resolution >= 0) glUniform2f(uniform_resolution, (float) width, (float) height);
    if (uniform_native_resolution >= 0)
        glUniform2f(uniform_native_resolution, (float) source_width, (float) source_height);
    if (uniform_source_resolution >= 0)
        glUniform2f(uniform_source_resolution, (float) source_width, (float) source_height);
    if (uniform_texture_resolution >= 0)
        glUniform2f(
            uniform_texture_resolution, (float) source_width / texture_width,
            (float) source_height / texture_height
        );
    if (uniform_source_extent >= 0) glUniform2f(uniform_source_extent, texture_width, texture_height);
    if (uniform_time >= 0) glUniform1f(uniform_time, (float) shader_frame);
    if (uniform_frame >= 0) glUniform1i(uniform_frame, shader_frame);
    for (int index = 0; index < parameter_count; index++)
        if (parameters[index].location >= 0) glUniform1f(parameters[index].location, parameters[index].value);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (attribute_position >= 0) {
        glVertexAttribPointer(attribute_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);
        glEnableVertexAttribArray((GLuint) attribute_position);
    }
    if (attribute_uv >= 0) {
        glVertexAttribPointer(attribute_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices + 2);
        glEnableVertexAttribArray((GLuint) attribute_uv);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (attribute_position >= 0) glDisableVertexAttribArray((GLuint) attribute_position);
    if (attribute_uv >= 0) glDisableVertexAttribArray((GLuint) attribute_uv);
    SDL_GL_UnbindTexture(source);
    shader_frame++;
    return 1;
}

static int colour_effects_active(float matrix[9]) {
    if (colour_loaded_revision != revision) {
        colour_filter_active = read_colour_matrix(colour_matrix);
        colour_loaded_revision = revision;
    }
    memcpy(matrix, colour_matrix, sizeof(colour_matrix));
    return colour_filter_active || config.video.brightness != 0 || config.video.contrast != 100
           || config.video.saturation != 100 || config.video.hue_shift != 0 || config.video.gamma != 100;
}

static int draw_colour(SDL_Texture *source, const int width, const int height, const float matrix[9]) {
    float texture_width = 1.0f;
    float texture_height = 1.0f;
    glActiveTexture(GL_TEXTURE0);
    if (SDL_GL_BindTexture(source, &texture_width, &texture_height) != 0) return 0;
    const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, texture_width, 0.0f,
        -1.0f, 1.0f, 0.0f, texture_height, 1.0f, 1.0f, texture_width, texture_height,
    };
    glViewport(0, 0, width, height);
    glUseProgram(colour_program);
    if (colour_uniform_texture >= 0) glUniform1i(colour_uniform_texture, 0);
    for (int index = 0; index < 9; index++)
        if (colour_uniform_matrix[index] >= 0) glUniform1f(colour_uniform_matrix[index], matrix[index]);
    if (colour_uniform_brightness >= 0)
        glUniform1f(colour_uniform_brightness, (float) config.video.brightness / 100.0f);
    if (colour_uniform_contrast >= 0)
        glUniform1f(colour_uniform_contrast, (float) config.video.contrast / 100.0f);
    if (colour_uniform_saturation >= 0)
        glUniform1f(colour_uniform_saturation, (float) config.video.saturation / 100.0f);
    if (colour_uniform_hue >= 0)
        glUniform1f(colour_uniform_hue, (float) config.video.hue_shift * 0.01745329251994329577f);
    if (colour_uniform_gamma >= 0)
        glUniform1f(colour_uniform_gamma, 100.0f / (float) config.video.gamma);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (colour_attribute_position >= 0) {
        glVertexAttribPointer(colour_attribute_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);
        glEnableVertexAttribArray((GLuint) colour_attribute_position);
    }
    if (colour_attribute_uv >= 0) {
        glVertexAttribPointer(colour_attribute_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices + 2);
        glEnableVertexAttribArray((GLuint) colour_attribute_uv);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    if (colour_attribute_position >= 0) glDisableVertexAttribArray((GLuint) colour_attribute_position);
    if (colour_attribute_uv >= 0) glDisableVertexAttribArray((GLuint) colour_attribute_uv);
    SDL_GL_UnbindTexture(source);
    return 1;
}

int video_effects_render(
    SDL_Renderer *renderer, SDL_Texture *source, const SDL_Rect *source_rect, const SDL_Rect *destination,
    const double rotation, const SDL_RendererFlip flip
) {
    if (!renderer || !source || !source_rect || !destination || destination->w <= 0 || destination->h <= 0)
        return 0;
    float matrix[9];
    const int colour = colour_effects_active(matrix);
    const int shader = load_program();
    if (!shader && !colour) return 0;
    if (colour && !load_colour_program()) return 0;
    if (!ensure_texture(renderer, &input_texture, &input_width, &input_height, source_rect->w, source_rect->h)
        || !ensure_texture(renderer, &output_texture, &output_width, &output_height, destination->w, destination->h))
        return 0;

    SDL_Texture *previous_target = SDL_GetRenderTarget(renderer);
    if (SDL_SetRenderTarget(renderer, input_texture) != 0) return 0;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, source, source_rect, NULL);
    GLint previous_program = 0;
    GLint previous_viewport[4] = {0};
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
    glGetIntegerv(GL_VIEWPORT, previous_viewport);
    const GLboolean blend = glIsEnabled(GL_BLEND);
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    SDL_Texture *result = output_texture;
    int drawn = 0;
    if (shader) {
        if (SDL_SetRenderTarget(renderer, output_texture) != 0) goto restore;
        SDL_RenderFlush(renderer);
        drawn = draw_shader(input_texture, input_width, input_height, destination->w, destination->h);
        if (!drawn) goto restore;
        if (colour) {
            if (!ensure_texture(
                    renderer, &colour_texture, &colour_width, &colour_height, destination->w, destination->h
                ))
                goto restore;
            if (SDL_SetRenderTarget(renderer, colour_texture) != 0) goto restore;
            SDL_RenderFlush(renderer);
            drawn = draw_colour(output_texture, destination->w, destination->h, matrix);
            result = colour_texture;
        }
    } else {
        if (SDL_SetRenderTarget(renderer, output_texture) != 0) goto restore;
        SDL_RenderFlush(renderer);
        drawn = draw_colour(input_texture, destination->w, destination->h, matrix);
    }
restore:
    glUseProgram((GLuint) previous_program);
    glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
    if (blend) glEnable(GL_BLEND);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    SDL_SetRenderTarget(renderer, previous_target);
    if (!drawn) return 0;
    SDL_RenderCopyEx(renderer, result, NULL, destination, rotation, NULL, flip);
    return 1;
}

void video_effects_changed(void) { revision++; }

int video_effects_parameter_count(void) {
    return load_program() ? parameter_count : 0;
}

const char *video_effects_parameter_label(const int index) {
    return index >= 0 && index < parameter_count ? parameters[index].label : "";
}

void video_effects_parameter_value(const int index, char *value, const size_t size) {
    if (!value || !size) return;
    if (index < 0 || index >= parameter_count) {
        value[0] = '\0';
        return;
    }
    snprintf(value, size, "%g", (double) parameters[index].value);
}

int video_effects_parameter_cycle(const int index, const int direction) {
    if (index < 0 || index >= parameter_count) return 0;
    shader_parameter *parameter = &parameters[index];
    parameter->value += (direction < 0 ? -1.0f : 1.0f) * parameter->step;
    if (parameter->value < parameter->min) parameter->value = parameter->min;
    if (parameter->value > parameter->max) parameter->value = parameter->max;
    save_parameter_values();
    return 1;
}

void video_effects_parameters_reset(void) {
    for (int index = 0; index < parameter_count; index++) parameters[index].value = parameters[index].def;
    save_parameter_values();
}

void video_effects_close(void) {
    destroy_program();
    if (colour_program && gl_api.DeleteProgram && SDL_GL_GetCurrentContext()) glDeleteProgram(colour_program);
    colour_program = 0;
    if (input_texture) SDL_DestroyTexture(input_texture);
    if (output_texture) SDL_DestroyTexture(output_texture);
    if (colour_texture) SDL_DestroyTexture(colour_texture);
    input_texture = output_texture = colour_texture = NULL;
    input_width = input_height = output_width = output_height = colour_width = colour_height = 0;
    loaded_key[0] = '\0';
    loaded_revision = 0;
    colour_loaded_revision = 0;
}
