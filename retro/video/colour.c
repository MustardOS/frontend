#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "../../common/config.h"
#include "../../common/fileio.h"
#include "../../common/ini.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "colour.h"
#include "gl_dispatch.h"
#include "hw_render.h"
#include "../core/muxretro.h"
#include "../core/paths.h"
#include "../settings/settings.h"

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
                            "uniform int u_colour_enabled;"
                            "uniform int u_gamma_enabled;"
                            "uniform float u_gamma_inv;"
                            "uniform mat3 u_filter;"
                            "uniform int u_filter_enabled;"
                            "uniform int u_vig_shape;"
                            "uniform vec2 u_vig_centre;"
                            "uniform vec2 u_vig_scale;"
                            "uniform vec2 u_vig_ramp;"
                            "uniform float u_vig_tone;"
                            "uniform float u_vig_amount;"
                            "\n#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
                            "varying highp vec2 v_uv;\n"
                            "#else\n"
                            "varying mediump vec2 v_uv;\n"
                            "#endif\n"

                            "vec3 apply_colour(vec3 c) {"
                            "    if (u_colour_enabled == 0) {"
                            "        if (u_filter_enabled != 0) { c = clamp(u_filter * c, 0.0, 1.0); }"
                            "        if (u_gamma_enabled != 0) { c = pow(c, vec3(u_gamma_inv)); }"
                            "        return c;"
                            "    }"
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
                            "    if (u_gamma_enabled != 0) { c = pow(c, vec3(u_gamma_inv)); }"
                            "    return c;"
                            "}"

                            "vec3 apply_vignette(vec3 c) {"
                            "    if (u_vig_shape == 0) { return c; }"
                            "    vec2 p = (v_uv - u_vig_centre) * u_vig_scale;"
                            "    float d;"
                            "    if (u_vig_shape == 2) {"
                            "        vec2 q = min(abs(p), 1.5);"
                            "        q = q * q;"
                            "        q = q * q;"
                            "        q = q * q;"
                            "        d = sqrt(sqrt(sqrt(sqrt(dot(q, q)))));"
                            "    }"
                            "    else if (u_vig_shape == 3) {"
                            "        vec2 q = min(abs(p), 1.5);"
                            "        q = q * q;"
                            "        d = sqrt(sqrt(dot(q, q)));"
                            "    }"
                            "    else if (u_vig_shape == 4) {"
                            "        float seg = 1.2566371;"
                            "        float a = mod(atan(p.y, p.x) + 6.2831853, seg) - seg * 0.5;"
                            "        d = length(p) / (1.0 - 0.7 * (abs(a) / (seg * 0.5)));"
                            "    }"
                            "    else if (u_vig_shape == 5) {"
                            "        float q = p.y * 1.5 - 0.5;"
                            "        d = max(abs(p.x) * 1.2 - q * 0.5, q);"
                            "    }"
                            "    else { d = length(p); }"
                            "    float t = clamp(d * u_vig_ramp.x + u_vig_ramp.y, 0.0, 1.0);"
                            "    t = t * t * (3.0 - 2.0 * t);"
                            "    return mix(c, vec3(u_vig_tone), t * u_vig_amount);"
                            "}"

                            "void main(){"
                            "    vec4 t = texture2D(u_tex, v_uv);"
                            "    gl_FragColor = vec4(apply_vignette(apply_colour(t.bgr)), 1.0);"
                            "}";

static const char *shader_vs_src = "attribute vec2 a_pos;\n"
                                   "attribute vec2 a_uv;\n"
                                   "varying highp vec2 v_uv;\n"
                                   "void main() {\n"
                                   "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
                                   "    v_uv = a_uv;\n"
                                   "}\n";

static const char *shader_fs_preamble = "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
                                        "precision highp float;\n"
                                        "#else\n"
                                        "precision mediump float;\n"
                                        "#endif\n"
                                        "uniform sampler2D u_tex;\n"
                                        "uniform vec2 u_resolution;\n"
                                        "uniform vec2 u_native_resolution;\n"
                                        "uniform float u_time;\n"
                                        "uniform int u_frame;\n"
                                        "varying vec2 v_uv;\n";

static GLuint prog = 0;
static GLint a_pos = -1, a_uv = -1;
static GLint u_tex = -1, u_brightness = -1, u_contrast = -1, u_saturation = -1;
static GLint u_cos_h = -1, u_sin_h = -1, u_filter = -1, u_filter_enabled = -1;
static GLint u_colour_enabled = -1, u_gamma_enabled = -1, u_gamma_inv = -1;
static GLint u_vig_shape = -1, u_vig_centre = -1, u_vig_scale = -1;
static GLint u_vig_ramp = -1, u_vig_tone = -1, u_vig_amount = -1;
static int prog_attempted = 0;
static int prog_ready = 0;

static GLuint shader_prog = 0;
static GLint sh_a_pos = -1, sh_a_uv = -1;
static GLint sh_u_tex = -1, sh_u_resolution = -1, sh_u_native_resolution = -1, sh_u_time = -1, sh_u_frame = -1;
static int shader_loaded_index = -1;
static int shader_frame_count = 0;

typedef struct {
    char name[32];
    char label[48];
    float value;
    float def;
    float min;
    float max;
    float step;
    GLint loc;
} shader_param_t;

static shader_param_t shader_params[COLOUR_SHADER_PARAM_MAX];
static int shader_params_count = 0;
static int shader_params_dirty = 0;

static SDL_Texture *adjusted_tex = NULL;
static int adjusted_w = 0;
static int adjusted_h = 0;

static SDL_Texture *work_tex = NULL;
static int work_w = 0;
static int work_h = 0;

static SDL_Texture *output_tex = NULL;
static int output_w = 0;
static int output_h = 0;

static const gl_dispatch_t *gl;

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

const char *colour_filter_preset_label(const int index) {
    if (index < 0 || index >= filter_count) return filter_labels[0];
    return filter_labels[index];
}

int colour_shader_count(void) {
    return shader_count > 0 ? shader_count : 1;
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

static int pass_suppressed;

static int preset_effects_enabled(void) {
    return !hw_render_bridge_active() || config.settings.advanced.hw_render_effects;
}

void colour_set_suppressed(const int suppressed) {
    pass_suppressed = suppressed;
}

int colour_pass_needed(void) {
    if (pass_suppressed) return 0;
    if (!preset_effects_enabled()) return 0;
    if (session_settings.colour_brightness != 0) return 1;
    if (session_settings.colour_contrast != 100) return 1;
    if (session_settings.colour_saturation != 100) return 1;
    if (session_settings.colour_hueshift != 0) return 1;
    if (session_settings.colour_gamma != 100) return 1;
    if (session_settings.colour_shader != 0) return 1;
    if (session_settings_vignette_active()) return 1;
    return current_filter()->enabled;
}

static GLuint compile_shader(const GLenum type, const char *src) {
    const GLuint shader = gl->CreateShader(type);
    gl->ShaderSource(shader, 1, &src, NULL);
    gl->CompileShader(shader);

    GLint ok = 0;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        gl->GetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOG_ERROR(mux_module, "Colour: shader compile failed: %s", log);
        gl->DeleteShader(shader);
        return 0;
    }

    return shader;
}

static void ensure_program(void) {
    if (prog_attempted) return;
    prog_attempted = 1;

    gl = gl_dispatch_acquire("Colour", gl_dispatch_colour);
    if (!gl) return;

    const GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    const GLuint fs = vs ? compile_shader(GL_FRAGMENT_SHADER, fs_src) : 0;

    if (!vs || !fs) {
        if (vs) gl->DeleteShader(vs);
        if (fs) gl->DeleteShader(fs);
        return;
    }

    prog = gl->CreateProgram();
    gl->AttachShader(prog, vs);
    gl->AttachShader(prog, fs);

    gl->BindAttribLocation(prog, 3, "a_pos");
    gl->BindAttribLocation(prog, 4, "a_uv");

    gl->LinkProgram(prog);

    gl->DeleteShader(vs);
    gl->DeleteShader(fs);

    GLint ok = 0;
    gl->GetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        gl->GetProgramInfoLog(prog, sizeof(log), NULL, log);
        LOG_ERROR(mux_module, "Colour: program link failed: %s", log);
        gl->DeleteProgram(prog);
        prog = 0;
        return;
    }

    a_pos = gl->GetAttribLocation(prog, "a_pos");
    a_uv = gl->GetAttribLocation(prog, "a_uv");
    u_tex = gl->GetUniformLocation(prog, "u_tex");
    u_brightness = gl->GetUniformLocation(prog, "u_brightness");
    u_contrast = gl->GetUniformLocation(prog, "u_contrast");
    u_saturation = gl->GetUniformLocation(prog, "u_saturation");
    u_cos_h = gl->GetUniformLocation(prog, "u_cosH");
    u_sin_h = gl->GetUniformLocation(prog, "u_sinH");
    u_colour_enabled = gl->GetUniformLocation(prog, "u_colour_enabled");
    u_gamma_enabled = gl->GetUniformLocation(prog, "u_gamma_enabled");
    u_gamma_inv = gl->GetUniformLocation(prog, "u_gamma_inv");
    u_filter = gl->GetUniformLocation(prog, "u_filter");
    u_filter_enabled = gl->GetUniformLocation(prog, "u_filter_enabled");
    u_vig_shape = gl->GetUniformLocation(prog, "u_vig_shape");
    u_vig_centre = gl->GetUniformLocation(prog, "u_vig_centre");
    u_vig_scale = gl->GetUniformLocation(prog, "u_vig_scale");
    u_vig_ramp = gl->GetUniformLocation(prog, "u_vig_ramp");
    u_vig_tone = gl->GetUniformLocation(prog, "u_vig_tone");
    u_vig_amount = gl->GetUniformLocation(prog, "u_vig_amount");

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

// We'll pinch this methodology from RetroArch since it somewhat makes sense!
static int parse_shader_params(const char *src) {
    int count = 0;
    const char *p = src;

    while (count < COLOUR_SHADER_PARAM_MAX && (p = strstr(p, "#pragma parameter")) != NULL) {
        p += strlen("#pragma parameter");

        shader_param_t sp = {0};
        sp.loc = -1;

        while (*p == ' ' || *p == '\t')
            p++;

        size_t n = 0;
        while (*p && !isspace((unsigned char) *p) && n < sizeof(sp.name) - 1)
            sp.name[n++] = *p++;
        sp.name[n] = '\0';

        while (*p == ' ' || *p == '\t')
            p++;

        if (*p == '"') {
            p++;
            size_t l = 0;
            while (*p && *p != '"' && l < sizeof(sp.label) - 1)
                sp.label[l++] = *p++;
            sp.label[l] = '\0';
            if (*p == '"') p++;
        }

        char *end;
        sp.def = strtof(p, &end);
        if (end == p) continue;
        p = end;
        sp.min = strtof(p, &end);
        if (end == p) continue;
        p = end;
        sp.max = strtof(p, &end);
        if (end == p) continue;
        p = end;
        sp.step = strtof(p, &end);
        if (end != p) p = end;

        if (!sp.name[0] || sp.max <= sp.min) continue;
        if (!sp.label[0]) snprintf(sp.label, sizeof(sp.label), "%s", sp.name);
        if (sp.step <= 0.0f) sp.step = (sp.max - sp.min) / 20.0f;
        if (sp.def < sp.min) sp.def = sp.min;
        if (sp.def > sp.max) sp.def = sp.max;

        sp.value = sp.def;
        shader_params[count++] = sp;
    }

    return count;
}

static void blank_shader_params(char *src) {
    char *p = src;

    while ((p = strstr(p, "#pragma parameter")) != NULL) {
        while (*p && *p != '\n')
            *p++ = ' ';
    }
}

static void shader_params_ini_path(char *out, const size_t len, const char *stem) {
    snprintf(out, len, "%s/%s.ini", RETRO_SHP_PATH, stem);
}

static void shader_params_load(const char *stem) {
    char path[PATH_MAX];
    shader_params_ini_path(path, sizeof(path), stem);

    mini_t *ini = mini_try_load(path);
    if (!ini) return;

    for (int i = 0; i < shader_params_count; i++) {
        shader_param_t *sp = &shader_params[i];
        float v = get_ini_float(ini, "parameters", sp->name, sp->def);
        if (v < sp->min) v = sp->min;
        if (v > sp->max) v = sp->max;
        sp->value = v;
    }

    mini_free(ini);
}

void colour_shader_params_save(void) {
    if (!shader_params_dirty || shader_params_count <= 0) return;
    if (shader_loaded_index <= 0 || shader_loaded_index >= shader_count) return;

    char path[PATH_MAX];
    shader_params_ini_path(path, sizeof(path), shader_names[shader_loaded_index]);
    create_directories(path, 1);

    mini_t *ini = mini_try_load(path);
    if (!ini) ini = mini_create(path);
    if (!ini) return;

    for (int i = 0; i < shader_params_count; i++) {
        char value[32];
        snprintf(value, sizeof(value), "%g", (double) shader_params[i].value);
        mini_set_string(ini, "parameters", shader_params[i].name, value);
    }

    const int saved = mini_save(ini, 0) == MINI_OK;
    mini_free(ini);
    if (saved)
        shader_params_dirty = 0;
    else
        LOG_ERROR(mux_module, "Could not safely write shader parameters: %s", path);
}

int colour_shader_param_count(void) {
    return shader_params_count;
}

const char *colour_shader_param_label(const int index) {
    if (index < 0 || index >= shader_params_count) return "";
    return shader_params[index].label;
}

void colour_shader_param_value_text(const int index, char *buf, const size_t len) {
    if (index < 0 || index >= shader_params_count) {
        if (len) buf[0] = '\0';
        return;
    }

    const shader_param_t *sp = &shader_params[index];

    const char *fmt = sp->step >= 1.0f     ? "%.0f"
                      : sp->step >= 0.1f   ? "%.1f"
                      : sp->step >= 0.01f  ? "%.2f"
                      : sp->step >= 0.001f ? "%.3f"
                                           : "%.4f";
    snprintf(buf, len, fmt, (double) sp->value);
}

void colour_shader_param_cycle(const int index, const int direction) {
    if (index < 0 || index >= shader_params_count || direction == 0) return;

    shader_param_t *sp = &shader_params[index];

    float v = sp->value + sp->step * (float) direction;
    if (v < sp->min) v = sp->min;
    if (v > sp->max) v = sp->max;

    const float snapped = sp->min + roundf((v - sp->min) / sp->step) * sp->step;
    if (snapped >= sp->min && snapped <= sp->max) v = snapped;

    if (v == sp->value) return;

    sp->value = v;
    shader_params_dirty = 1;
}

void colour_shader_params_reset(void) {
    for (int i = 0; i < shader_params_count; i++)
        shader_params[i].value = shader_params[i].def;

    shader_params_dirty = 1;
}

static void ensure_shader_program(void) {
    const int index = preset_effects_enabled() ? session_settings.colour_shader : 0;
    if (index == shader_loaded_index) return;
    shader_loaded_index = index;

    if (shader_prog) {
        gl->DeleteProgram(shader_prog);
        shader_prog = 0;
    }
    sh_a_pos = sh_a_uv = -1;
    sh_u_tex = sh_u_resolution = sh_u_native_resolution = sh_u_time = sh_u_frame = -1;
    shader_params_count = 0;
    shader_params_dirty = 0;

    if (index <= 0 || index >= shader_count) return;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s%s.frag", SHADER_DIR, shader_names[index]);

    char *body = read_shader_file(path);
    if (!body) return;

    char *strip = body;
    while (*strip == ' ' || *strip == '\t' || *strip == '\n' || *strip == '\r')
        strip++;
    if (strncmp(strip, "#version", 8) == 0) {
        char *eol = strchr(strip, '\n');
        LOG_WARN(mux_module, "Colour: ignoring #version in %s", path);
        strip = eol ? eol + 1 : strip + strlen(strip);
    }

    shader_params_count = parse_shader_params(strip);
    blank_shader_params(strip);

    const size_t total = strlen(shader_fs_preamble) + strlen(strip) + 1;
    char *full_src = malloc(total);
    if (!full_src) {
        free(body);
        return;
    }
    snprintf(full_src, total, "%s%s", shader_fs_preamble, strip);
    free(body);

    const GLuint vs = compile_shader(GL_VERTEX_SHADER, shader_vs_src);
    const GLuint fs = vs ? compile_shader(GL_FRAGMENT_SHADER, full_src) : 0;
    free(full_src);

    if (!vs || !fs) {
        if (vs) gl->DeleteShader(vs);
        if (fs) gl->DeleteShader(fs);
        return;
    }

    shader_prog = gl->CreateProgram();
    gl->AttachShader(shader_prog, vs);
    gl->AttachShader(shader_prog, fs);

    gl->BindAttribLocation(shader_prog, 3, "a_pos");
    gl->BindAttribLocation(shader_prog, 4, "a_uv");

    gl->LinkProgram(shader_prog);

    gl->DeleteShader(vs);
    gl->DeleteShader(fs);

    GLint ok = 0;
    gl->GetProgramiv(shader_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        gl->GetProgramInfoLog(shader_prog, sizeof(log), NULL, log);
        LOG_ERROR(mux_module, "Colour: user shader link failed: %s", log);
        gl->DeleteProgram(shader_prog);
        shader_prog = 0;
        return;
    }

    sh_a_pos = gl->GetAttribLocation(shader_prog, "a_pos");
    sh_a_uv = gl->GetAttribLocation(shader_prog, "a_uv");
    sh_u_tex = gl->GetUniformLocation(shader_prog, "u_tex");
    sh_u_resolution = gl->GetUniformLocation(shader_prog, "u_resolution");
    sh_u_native_resolution = gl->GetUniformLocation(shader_prog, "u_native_resolution");
    sh_u_time = gl->GetUniformLocation(shader_prog, "u_time");
    sh_u_frame = gl->GetUniformLocation(shader_prog, "u_frame");

    for (int i = 0; i < shader_params_count; i++)
        shader_params[i].loc = gl->GetUniformLocation(shader_prog, shader_params[i].name);

    shader_params_load(shader_names[index]);

    LOG_INFO(
        mux_module, "Colour: user shader ready: %s (%d parameter%s)", shader_names[index], shader_params_count,
        shader_params_count == 1 ? "" : "s"
    );
}

static int ensure_target(
    SDL_Renderer *renderer, SDL_Texture **tex, int *tw, int *th, const int w, const int h, const Uint32 format
) {
    if (w <= 0 || h <= 0) return 0;
    if (*tex && w == *tw && h == *th) return 1;

    if (*tex) {
        SDL_DestroyTexture(*tex);
        *tex = NULL;
    }

    *tex = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_TARGET, w, h);
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

static void set_vignette_uniforms(const int content_w, const int content_h) {
    const int active = content_w > 0 && content_h > 0 && preset_effects_enabled() && session_settings_vignette_active();

    if (u_vig_shape >= 0) gl->Uniform1i(u_vig_shape, active ? session_settings.vignette_shape : 0);
    if (!active) return;

    // Frame stretches the shape to the picture, Aspect keeps it true by measuring off the shorter side
    const int aspect = session_settings.vignette_scaling == vignette_scale_aspect;
    const float shorter = (float) (content_w < content_h ? content_w : content_h);
    const float scale_w = aspect ? shorter / (float) content_w : 1.0f;
    const float scale_h = aspect ? shorter / (float) content_h : 1.0f;

    const float half_w = (float) session_settings.vignette_width * scale_w / 200.0f;
    const float half_h = (float) session_settings.vignette_height * scale_h / 200.0f;
    const float offset_x = (float) session_settings.vignette_offset_x / 100.0f;
    const float offset_y = (float) session_settings.vignette_offset_y / 100.0f;

    const float inner = 1.0f - (float) session_settings.vignette_softness / 100.0f;
    const float range = 1.0f - inner > 0.001f ? 1.0f - inner : 0.001f;
    const float amount = (float) session_settings.vignette_strength / 100.0f;
    const float tone = session_settings.vignette_colour == vignette_colour_white ? 1.0f : 0.0f;

    if (u_vig_centre >= 0) gl->Uniform2f(u_vig_centre, 0.5f + offset_x, 0.5f + offset_y);
    if (u_vig_scale >= 0) gl->Uniform2f(u_vig_scale, 1.0f / half_w, 1.0f / half_h);
    if (u_vig_ramp >= 0) gl->Uniform2f(u_vig_ramp, 1.0f / range, -inner / range);
    if (u_vig_tone >= 0) gl->Uniform1f(u_vig_tone, tone);
    if (u_vig_amount >= 0) gl->Uniform1f(u_vig_amount, amount);
}

static void set_colour_uniforms(const int content_w, const int content_h) {
    const float brightness = (float) session_settings.colour_brightness / 100.0f;
    const float contrast = (float) session_settings.colour_contrast / 100.0f;
    const float saturation = (float) session_settings.colour_saturation / 100.0f;
    const float hue_rad = (float) session_settings.colour_hueshift * (float) M_PI / 180.0f;
    const float gamma = (float) session_settings.colour_gamma / 100.0f;

    const int colour_enabled = session_settings.colour_brightness != 0 || session_settings.colour_contrast != 100
                               || session_settings.colour_saturation != 100 || session_settings.colour_hueshift != 0;
    static const colour_filter_matrix_t disabled_filter = {0};
    const colour_filter_matrix_t *filter = preset_effects_enabled() ? current_filter() : &disabled_filter;

    if (u_tex >= 0) gl->Uniform1i(u_tex, 0);
    if (u_brightness >= 0) gl->Uniform1f(u_brightness, brightness);
    if (u_contrast >= 0) gl->Uniform1f(u_contrast, contrast);
    if (u_saturation >= 0) gl->Uniform1f(u_saturation, saturation);
    if (u_cos_h >= 0) gl->Uniform1f(u_cos_h, cosf(hue_rad));
    if (u_sin_h >= 0) gl->Uniform1f(u_sin_h, sinf(hue_rad));
    if (u_colour_enabled >= 0) gl->Uniform1i(u_colour_enabled, colour_enabled);
    if (u_gamma_enabled >= 0) gl->Uniform1i(u_gamma_enabled, session_settings.colour_gamma != 100);
    if (u_gamma_inv >= 0) gl->Uniform1f(u_gamma_inv, 1.0f / gamma);
    if (u_filter_enabled >= 0) gl->Uniform1i(u_filter_enabled, filter->enabled);
    if (u_filter >= 0) gl->UniformMatrix3fv(u_filter, 1, GL_FALSE, filter->matrix);

    set_vignette_uniforms(content_w, content_h);
}

static void set_shader_uniforms(const int res_w, const int res_h) {
    int native_w = 0, native_h = 0;
    video_bridge_get_frame_size(&native_w, &native_h);

    if (sh_u_tex >= 0) gl->Uniform1i(sh_u_tex, 0);
    if (sh_u_resolution >= 0) gl->Uniform2f(sh_u_resolution, (float) res_w, (float) res_h);
    if (sh_u_native_resolution >= 0) gl->Uniform2f(sh_u_native_resolution, (float) native_w, (float) native_h);
    if (sh_u_time >= 0) gl->Uniform1f(sh_u_time, (float) shader_frame_count);
    if (sh_u_frame >= 0) gl->Uniform1i(sh_u_frame, shader_frame_count);

    for (int i = 0; i < shader_params_count; i++)
        if (shader_params[i].loc >= 0) gl->Uniform1f(shader_params[i].loc, shader_params[i].value);
}

static int draw_gl_pass(
    SDL_Texture *src, const int user_prog, const float l, const float r, const float t, const float b, const int vp_w,
    const int vp_h, const int res_w, const int res_h
) {
    float texw = 1.0f, texh = 1.0f;
    if (SDL_GL_BindTexture(src, &texw, &texh) != 0) return 0;
    gl->ActiveTexture(GL_TEXTURE0);

    const GLfloat v_at_top = 0.0f;
    const GLfloat v_at_bottom = texh;

    const GLfloat verts[] = {
        l, t, 0.0f, v_at_top,    // top left
        r, t, texw, v_at_top,    // top right
        l, b, 0.0f, v_at_bottom, // bottom left
        r, b, texw, v_at_bottom, // bottom right
    };

    gl->Viewport(0, 0, vp_w, vp_h);

    const GLint pass_a_pos = user_prog ? sh_a_pos : a_pos;
    const GLint pass_a_uv = user_prog ? sh_a_uv : a_uv;

    gl->UseProgram(user_prog ? shader_prog : prog);

    if (user_prog) {
        set_shader_uniforms(res_w, res_h);
    } else {
        set_colour_uniforms(res_w, res_h);
    }

    gl->BindBuffer(GL_ARRAY_BUFFER, 0);

    if (pass_a_pos >= 0) {
        gl->VertexAttribPointer(pass_a_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), verts);
        gl->EnableVertexAttribArray(pass_a_pos);
    }
    if (pass_a_uv >= 0) {
        gl->VertexAttribPointer(pass_a_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), verts + 2);
        gl->EnableVertexAttribArray(pass_a_uv);
    }

    gl->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (pass_a_pos >= 0) gl->DisableVertexAttribArray(pass_a_pos);
    if (pass_a_uv >= 0) gl->DisableVertexAttribArray(pass_a_uv);

    SDL_GL_UnbindTexture(src);
    return 1;
}

void colour_render_pass(SDL_Renderer *renderer, SDL_Texture *tex, const SDL_Rect *src_rect, const SDL_Rect *dest_rect) {
    if (!colour_pass_needed()) {
        SDL_RenderCopy(renderer, tex, src_rect, dest_rect);
        return;
    }

    ensure_program();

    if (!prog_ready) {
        SDL_RenderCopy(renderer, tex, src_rect, dest_rect);
        return;
    }

    SDL_RenderFlush(renderer);

    ensure_shader_program();
    const int use_shader =
        shader_prog != 0
        && ensure_target(renderer, &work_tex, &work_w, &work_h, dest_rect->w, dest_rect->h, SDL_PIXELFORMAT_ABGR8888);

    SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
    SDL_Texture *gl_src = tex;

    if (mux_retro_get_pixel_format() != RETRO_PIXEL_FORMAT_XRGB8888 || src_rect) {
        if (!ensure_target(
                renderer, &adjusted_tex, &adjusted_w, &adjusted_h, dest_rect->w, dest_rect->h, SDL_PIXELFORMAT_ARGB8888
            )
            || SDL_SetRenderTarget(renderer, adjusted_tex) != 0) {
            SDL_RenderCopy(renderer, tex, src_rect, dest_rect);
            return;
        }
        SDL_RenderCopy(renderer, tex, src_rect, NULL);
        SDL_SetRenderTarget(renderer, prev_target);
        gl_src = adjusted_tex;
    }

    if (!ensure_target(
            renderer, &output_tex, &output_w, &output_h, dest_rect->w, dest_rect->h, SDL_PIXELFORMAT_ABGR8888
        )) {
        SDL_RenderCopy(renderer, tex, src_rect, dest_rect);
        return;
    }

    GLint prev_program = 0;
    gl->GetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

    GLint prev_viewport[4] = {0};
    gl->GetIntegerv(GL_VIEWPORT, prev_viewport);

    const GLboolean prev_blend_enabled = gl->IsEnabled(GL_BLEND);
    const GLboolean prev_scissor_enabled = gl->IsEnabled(GL_SCISSOR_TEST);

    gl->Disable(GL_BLEND);
    gl->Disable(GL_SCISSOR_TEST);

    int drew = 0;

    if (use_shader) {
        if (SDL_SetRenderTarget(renderer, work_tex) == 0) {
            const int colour_ok = draw_gl_pass(
                gl_src, 0, -1.0f, 1.0f, -1.0f, 1.0f, dest_rect->w, dest_rect->h, dest_rect->w, dest_rect->h
            );

            if (colour_ok && SDL_SetRenderTarget(renderer, output_tex) == 0) {
                shader_frame_count++;
                drew = draw_gl_pass(
                    work_tex, 1, -1.0f, 1.0f, -1.0f, 1.0f, dest_rect->w, dest_rect->h, dest_rect->w, dest_rect->h
                );
            }
        }
    }

    if (!drew && SDL_SetRenderTarget(renderer, output_tex) == 0)
        drew =
            draw_gl_pass(gl_src, 0, -1.0f, 1.0f, -1.0f, 1.0f, dest_rect->w, dest_rect->h, dest_rect->w, dest_rect->h);

    gl->UseProgram((GLuint) prev_program);
    gl->Viewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    if (prev_blend_enabled) gl->Enable(GL_BLEND);
    if (prev_scissor_enabled) gl->Enable(GL_SCISSOR_TEST);

    SDL_SetRenderTarget(renderer, prev_target);

    if (drew)
        SDL_RenderCopy(renderer, output_tex, NULL, dest_rect);
    else
        SDL_RenderCopy(renderer, tex, src_rect, dest_rect);
}
