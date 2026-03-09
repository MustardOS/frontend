#include <unistd.h>
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include "../../common/log.h"
#include "../../common/inotify.h"
#include "../common/common.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/colour.h"
#include "../common/filter.h"
#include "../common/rotate.h"
#include "../common/scale.h"
#include "../common/stretch.h"
#include "../overlay/base.h"
#include "../overlay/battery.h"
#include "../overlay/bright.h"
#include "../overlay/volume.h"
#include "../hook.h"

#define RENDER_PATHS 8

static int fb_cached_w = 0;
static int fb_cached_h = 0;

static GLuint gles_prog = 0;

static GLint gles_a_pos = -1;
static GLint gles_a_uv = -1;
static GLint gles_u_alpha = -1;

static GLint gles_u_tex_overlay = -1;
static GLint gles_u_tex_content = -1;

static GLuint gles_prog_content = 0;
static GLint gles_u_brightness = -1;
static GLint gles_u_contrast = -1;
static GLint gles_u_saturation = -1;
static GLint gles_u_hueshift = -1;
static GLint gles_u_gamma = -1;

static GLint gles_u_filter = -1;
static GLint gles_u_filter_enabled = -1;

static GLint gles_content_a_pos = -1;
static GLint gles_content_a_uv = -1;

static int prog_attempted = 0;
static int prog_ready = 0;

static SDL_GLContext last_ctx = NULL;

static GLuint content_tex = 0;
static int content_tex_w = 0;
static int content_tex_h = 0;

static GLint content_tex_internal = GL_RGBA;
static GLenum content_tex_format = GL_RGBA;
static int content_copy_supported = 1;

static GLuint output_tex = 0;
static GLuint output_fbo = 0;
static int output_tex_w = 0;
static int output_tex_h = 0;

static GLuint content_fbo = 0;
static GLuint current_program = 0;

static GLuint overlay_tex = 0;
static GLuint overlay_fbo = 0;

static int overlay_tex_w = 0;
static int overlay_tex_h = 0;
static int overlay_valid = 0;

typedef struct {
    int fb_w;
    int fb_h;

    int base_disabled;
    GLuint base_tex;
    float base_alpha;

    int battery_step;
    GLuint battery_tex;
    float battery_alpha;

    int bright_visible;
    int bright_step;
    GLuint bright_tex;
    float bright_alpha;

    int volume_visible;
    int volume_step;
    GLuint volume_tex;
    float volume_alpha;
} overlay_state_t;

static overlay_state_t overlay_cache;

typedef struct {
    GLint program;
    GLint active_tex;
    GLint tex_binding;

    GLint framebuffer;
    GLint viewport[4];

    GLboolean blend;
    GLint blend_src_rgb;
    GLint blend_dst_rgb;
    GLint blend_eq_rgb;

    GLboolean scissor_test;
    GLint scissor_box[4];

    GLboolean depth_test;
    GLint array_buffer;

    int max_attribs;
    struct {
        GLint enabled;
        GLint size;
        GLint type;
        GLint normal;
        GLint stride;
        GLvoid *pointer;
        GLint buffer;
    } attrib[16];
} gles_state_t;

typedef enum {
    RENDER_PATH_UNKNOWN = 0,
    RENDER_PATH_DEFAULT_FBO,
    RENDER_PATH_OFFSCREEN_FBO,
} render_path_t;

static render_path_t render_path = RENDER_PATH_UNKNOWN;
static GLint render_nonzero_fbo = 0;
static int render_window_calls = 0;
static int render_nonzero_count = 0;
static int render_zero_count = 0;

static GLint render_fbo_ring[RENDER_PATHS];
static int render_fbo_ring_head = 0;

static void reset_render_path(void) {
    render_path = RENDER_PATH_UNKNOWN;

    render_nonzero_fbo = 0;
    render_window_calls = 0;
    render_nonzero_count = 0;
    render_zero_count = 0;
    render_fbo_ring_head = 0;

    memset(render_fbo_ring, 0, sizeof(render_fbo_ring));
}

static void detect_render_path(GLint app_fbo) {
    if (app_fbo != 0) render_nonzero_fbo = app_fbo;

    if (render_window_calls >= RENDER_PATHS) {
        const GLint evicted = render_fbo_ring[render_fbo_ring_head];
        if (evicted != 0) {
            render_nonzero_count--;
        } else {
            render_zero_count--;
        }
    }

    render_fbo_ring[render_fbo_ring_head] = app_fbo;
    render_fbo_ring_head = (render_fbo_ring_head + 1) % RENDER_PATHS;

    if (app_fbo != 0) {
        render_nonzero_count++;
    } else {
        render_zero_count++;
    }

    if (render_window_calls < RENDER_PATHS) render_window_calls++;

    if (render_window_calls < RENDER_PATHS) {
        render_path = RENDER_PATH_UNKNOWN;
        return;
    }

    if (render_nonzero_count > 0) {
        render_path = RENDER_PATH_OFFSCREEN_FBO;
    } else {
        render_path = RENDER_PATH_DEFAULT_FBO;
    }
}

static inline int content_pass_needed(int rot) {
    const struct colour_state *a = colour_adjust_get();
    const colour_filter_matrix_t *f = colour_filter_get();

    if (rot == ROTATE_0 &&
        a->brightness == 0.0f &&
        a->contrast == 1.0f &&
        a->saturation == 1.0f &&
        a->hueshift == 0.0f &&
        a->gamma == 1.0f &&
        !f->enabled) {
        return 0;
    }

    return 1;
}

static struct {
    float brightness, contrast, saturation, hueshift, gamma;
    int filter_enabled;
    float filter[9];
    int valid;
} content_uniform_cache;

static const char *vs_src =
        "attribute vec2 a_pos;"
        "attribute vec2 a_uv;"
        "varying vec2 v_uv;"

        "void main(){"
        "    gl_Position = vec4(a_pos, 0.0, 1.0);"
        "    v_uv = a_uv;"
        "}";

static const char *fs_overlay_src =
        "precision mediump float;"
        "uniform sampler2D u_tex;"
        "uniform float u_alpha;"
        "varying vec2 v_uv;"

        "void main(){"
        "    gl_FragColor = texture2D(u_tex, v_uv) * vec4(1.0, 1.0, 1.0, u_alpha);"
        "}";

static const char *fs_content_src =
        "precision mediump float;"
        "uniform sampler2D u_tex;"
        "uniform float u_brightness;"
        "uniform float u_contrast;"
        "uniform float u_saturation;"
        "uniform float u_hueshift;"
        "uniform float u_gamma;"
        "uniform mat3 u_filter;"
        "uniform int u_filter_enabled;"
        "varying vec2 v_uv;"

        "vec3 apply_colour(vec3 c) {"
        "    c += u_brightness;"
        "    c = (c - 0.5) * u_contrast + 0.5;"
        "    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));"
        "    c = mix(vec3(l), c, u_saturation);"
        "    float cosH = cos(u_hueshift);"
        "    float sinH = sin(u_hueshift);"
        "    mat3 hueMat = mat3("
        "        0.299 + 0.701*cosH + 0.168*sinH, 0.587 - 0.587*cosH + 0.330*sinH, 0.114 - 0.114*cosH - 0.497*sinH,"
        "        0.299 - 0.299*cosH - 0.328*sinH, 0.587 + 0.413*cosH + 0.035*sinH, 0.114 - 0.114*cosH + 0.292*sinH,"
        "        0.299 - 0.300*cosH + 1.250*sinH, 0.587 - 0.588*cosH - 1.050*sinH, 0.114 + 0.886*cosH - 0.203*sinH"
        "    );"
        "    c = clamp(hueMat * c, 0.0, 1.0);"
        "    if (u_filter_enabled != 0) { c = clamp(u_filter * c, 0.0, 1.0); }"
        "    c = pow(c, vec3(1.0 / u_gamma));"
        "    return c;"
        "}"

        "void main(){"
        "    vec4 t = texture2D(u_tex, v_uv);"
        "    vec3 rgb = apply_colour(t.rgb);"
        "    gl_FragColor = vec4(rgb, t.a);"
        "}";

static void destroy_content(void) {
    if (content_tex) {
        glDeleteTextures(1, &content_tex);
        content_tex = 0;
    }

    if (content_fbo) {
        glDeleteFramebuffers(1, &content_fbo);
        content_fbo = 0;
    }

    content_tex_w = 0;
    content_tex_h = 0;
}

static void destroy_overlay(void) {
    if (gles_prog) {
        glDeleteProgram(gles_prog);
        gles_prog = 0;
    }

    gles_a_pos = -1;
    gles_a_uv = -1;
    gles_u_alpha = -1;

    gles_u_tex_overlay = -1;
    gles_u_tex_content = -1;

    current_program = 0;

    prog_ready = 0;
    prog_attempted = 0;
}

static void destroy_overlay_all(void) {
    if (overlay_tex) {
        glDeleteTextures(1, &overlay_tex);
        overlay_tex = 0;
    }

    if (overlay_fbo) {
        glDeleteFramebuffers(1, &overlay_fbo);
        overlay_fbo = 0;
    }

    overlay_tex_w = 0;
    overlay_tex_h = 0;

    overlay_valid = 0;
    memset(&overlay_cache, 0, sizeof(overlay_cache));
}

static inline void use_program(GLuint program) {
    if (current_program != program) {
        glUseProgram(program);
        current_program = program;
    }
}

static void ensure_content_tex(int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (content_tex && w == content_tex_w && h == content_tex_h) return;

    destroy_content();

    glGenTextures(1, &content_tex);
    glBindTexture(GL_TEXTURE_2D, content_tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, content_tex_internal, w, h, 0, content_tex_format, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &content_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, content_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, content_tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        destroy_content();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    content_tex_w = w;
    content_tex_h = h;
}

static void rotate_uv(gl_vtx_t out[4], int rot) {
    float u0 = 0.0f, v0 = 0.0f;
    float u1 = 0.0f, v1 = 1.0f;
    float u2 = 1.0f, v2 = 0.0f;
    float u3 = 1.0f, v3 = 1.0f;

    if (rot == ROTATE_90) {
        out[0].u = u1;
        out[0].v = v1;
        out[1].u = u3;
        out[1].v = v3;
        out[2].u = u0;
        out[2].v = v0;
        out[3].u = u2;
        out[3].v = v2;
        return;
    }

    if (rot == ROTATE_180) {
        out[0].u = u3;
        out[0].v = v3;
        out[1].u = u2;
        out[1].v = v2;
        out[2].u = u1;
        out[2].v = v1;
        out[3].u = u0;
        out[3].v = v0;
        return;
    }

    if (rot == ROTATE_270) {
        out[0].u = u2;
        out[0].v = v2;
        out[1].u = u0;
        out[1].v = v0;
        out[2].u = u3;
        out[2].v = v3;
        out[3].u = u1;
        out[3].v = v1;
        return;
    }

    out[0].u = u0;
    out[0].v = v0;
    out[1].u = u1;
    out[1].v = v1;
    out[2].u = u2;
    out[2].v = v2;
    out[3].u = u3;
    out[3].v = v3;
}

static void build_fullscreen_quad(gl_vtx_t out[4], int rot) {
    out[0].x = -1.0f;
    out[0].y = 1.0f;
    out[1].x = -1.0f;
    out[1].y = -1.0f;
    out[2].x = 1.0f;
    out[2].y = 1.0f;
    out[3].x = 1.0f;
    out[3].y = -1.0f;

    switch (rot) {
        case ROTATE_90:
            out[0].u = 0.0f;
            out[0].v = 0.0f;
            out[1].u = 1.0f;
            out[1].v = 0.0f;
            out[2].u = 0.0f;
            out[2].v = 1.0f;
            out[3].u = 1.0f;
            out[3].v = 1.0f;
            break;

        case ROTATE_180:
            out[0].u = 1.0f;
            out[0].v = 0.0f;
            out[1].u = 1.0f;
            out[1].v = 1.0f;
            out[2].u = 0.0f;
            out[2].v = 0.0f;
            out[3].u = 0.0f;
            out[3].v = 1.0f;
            break;

        case ROTATE_270:
            out[0].u = 1.0f;
            out[0].v = 1.0f;
            out[1].u = 0.0f;
            out[1].v = 1.0f;
            out[2].u = 1.0f;
            out[2].v = 0.0f;
            out[3].u = 0.0f;
            out[3].v = 0.0f;
            break;

        default: // ROTATE_0
            out[0].u = 0.0f;
            out[0].v = 1.0f;
            out[1].u = 0.0f;
            out[1].v = 0.0f;
            out[2].u = 1.0f;
            out[2].v = 1.0f;
            out[3].u = 1.0f;
            out[3].v = 0.0f;
            break;
    }
}

static void build_quad_ndc(gl_vtx_t out[4], int tex_w, int tex_h, int fb_w, int fb_h, int anchor, int scale) {
    const int rot = rotate_read_cached();

    int draw_w_i = tex_w;
    int draw_h_i = tex_h;

    stretch_draw_size(tex_w, tex_h, fb_w, fb_h, scale, rot, &draw_w_i, &draw_h_i);

    float draw_w = (float) draw_w_i;
    float draw_h = (float) draw_h_i;

    if (draw_w < 1.0f) draw_w = 1.0f;
    if (draw_h < 1.0f) draw_h = 1.0f;

    if (fb_w < 1) fb_w = 1;
    if (fb_h < 1) fb_h = 1;

    const float w_ndc = (draw_w * (2.0f / (float) fb_w));
    const float h_ndc = (draw_h * (2.0f / (float) fb_h));

    float x0, x1, y0, y1;

    switch (get_anchor_rotate(anchor, rot)) {
        case ANCHOR_TOP_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_TOP_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_TOP_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_CENTRE_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_CENTRE_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_CENTRE_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_BOTTOM_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        case ANCHOR_BOTTOM_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        case ANCHOR_BOTTOM_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        default:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
    }

    out[0].x = x0;
    out[0].y = y0;
    out[1].x = x0;
    out[1].y = y1;
    out[2].x = x1;
    out[2].y = y0;
    out[3].x = x1;
    out[3].y = y1;

    rotate_uv(out, rot);
}

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);

    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (ok != GL_TRUE) {
        char log_buffer[512];
        GLsizei n = 0;

        glGetShaderInfoLog(sh, (GLsizei) sizeof(log_buffer), &n, log_buffer);
        LOG_ERROR("stage", "GL shader compile failed: %.*s", (int) n, log_buffer);

        glDeleteShader(sh);
        return 0;
    }

    return sh;
}

static GLuint link_program(GLuint vs, GLuint fs, int is_overlay) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);

    if (is_overlay) {
        glBindAttribLocation(p, 14, "a_pos");
        glBindAttribLocation(p, 15, "a_uv");
    } else {
        glBindAttribLocation(p, 12, "a_pos");
        glBindAttribLocation(p, 13, "a_uv");
    }

    glLinkProgram(p);

    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);

    if (ok != GL_TRUE) {
        char log_buffer[512];
        GLsizei n = 0;

        glGetProgramInfoLog(p, (GLsizei) sizeof(log_buffer), &n, log_buffer);
        LOG_ERROR("stage", "GL program link failed: %.*s", (int) n, log_buffer);

        glDeleteProgram(p);
        return 0;
    }

    return p;
}

static void ensure_content_program(void) {
    if (gles_prog_content) return;

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_content_src);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);

        return;
    }

    gles_prog_content = link_program(vs, fs, 0);

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!gles_prog_content) return;

    gles_content_a_pos = glGetAttribLocation(gles_prog_content, "a_pos");
    gles_content_a_uv = glGetAttribLocation(gles_prog_content, "a_uv");

    if (gles_content_a_pos < 0 || gles_content_a_uv < 0) {
        glDeleteProgram(gles_prog_content);
        gles_prog_content = 0;

        return;
    }

    gles_u_tex_content = glGetUniformLocation(gles_prog_content, "u_tex");
    gles_u_filter = glGetUniformLocation(gles_prog_content, "u_filter");
    gles_u_filter_enabled = glGetUniformLocation(gles_prog_content, "u_filter_enabled");
    gles_u_brightness = glGetUniformLocation(gles_prog_content, "u_brightness");
    gles_u_contrast = glGetUniformLocation(gles_prog_content, "u_contrast");
    gles_u_saturation = glGetUniformLocation(gles_prog_content, "u_saturation");
    gles_u_hueshift = glGetUniformLocation(gles_prog_content, "u_hueshift");
    gles_u_gamma = glGetUniformLocation(gles_prog_content, "u_gamma");

    content_uniform_cache.valid = 0;
}

static void save_gles_state(gles_state_t *st) {
    glGetIntegerv(GL_CURRENT_PROGRAM, &st->program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &st->active_tex);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &st->tex_binding);

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &st->framebuffer);
    glGetIntegerv(GL_VIEWPORT, st->viewport);

    st->blend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC_RGB, &st->blend_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &st->blend_dst_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &st->blend_eq_rgb);

    st->scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, st->scissor_box);

    st->depth_test = glIsEnabled(GL_DEPTH_TEST);

    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &st->array_buffer);

    GLint max = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max);
    if (max > 16) max = 16;
    st->max_attribs = max;

    for (int i = 0; i < max; i++) {
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &st->attrib[i].enabled);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &st->attrib[i].size);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &st->attrib[i].type);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &st->attrib[i].normal);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &st->attrib[i].stride);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &st->attrib[i].buffer);

        glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &st->attrib[i].pointer);
    }
}

static void restore_gles_state(const gles_state_t *st) {
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) st->framebuffer);

    glViewport(st->viewport[0], st->viewport[1], st->viewport[2], st->viewport[3]);

    if (st->depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (st->scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);

    glScissor(st->scissor_box[0], st->scissor_box[1], st->scissor_box[2], st->scissor_box[3]);

    if (st->blend) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }

    glBlendEquation(st->blend_eq_rgb);
    glBlendFunc(st->blend_src_rgb, st->blend_dst_rgb);

    glUseProgram(st->program);
    glActiveTexture(st->active_tex);
    glBindTexture(GL_TEXTURE_2D, (GLuint) st->tex_binding);

    for (int i = 0; i < st->max_attribs; i++) {
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint) st->attrib[i].buffer);

        if (st->attrib[i].size > 0) {
            glVertexAttribPointer((GLuint) i, st->attrib[i].size, st->attrib[i].type, (GLboolean) st->attrib[i].normal,
                                  st->attrib[i].stride, st->attrib[i].pointer);
        }

        if (st->attrib[i].enabled) {
            glEnableVertexAttribArray((GLuint) i);
        } else {
            glDisableVertexAttribArray((GLuint) i);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, (GLuint) st->array_buffer);
}

static void on_context_changed(void) {
    reset_render_path();

    content_tex_format = GL_RGBA;
    content_copy_supported = 1;

    colour_adjust_reset();
    colour_adjust_get();

    colour_filter_reset();
    colour_filter_get();

    content_uniform_cache.valid = 0;

    destroy_content();

    if (output_tex) {
        glDeleteTextures(1, &output_tex);
        output_tex = 0;
    }

    if (output_fbo) {
        glDeleteFramebuffers(1, &output_fbo);
        output_fbo = 0;
    }

    output_tex_w = 0;
    output_tex_h = 0;

    destroy_overlay_all();
    destroy_base_gles();
    destroy_overlay();

    base_rotate_cached = ROTATE_0;
    base_nop_last = -1;
    vtx_base_valid = 0;

    battery_rotate_cached = ROTATE_0;
    battery_preload_gles_done = 0;
    battery_disabled_gles = 0;
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (battery_gles_tex[i]) {
            glDeleteTextures(1, &battery_gles_tex[i]);
            battery_gles_tex[i] = 0;
        }
    }
    vtx_battery_valid = 0;

    bright_rotate_cached = ROTATE_0;
    bright_preload_gles_done = 0;
    bright_disabled_gles = 0;
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (bright_gles_tex[i]) {
            glDeleteTextures(1, &bright_gles_tex[i]);
            bright_gles_tex[i] = 0;
        }
    }
    vtx_bright_valid = 0;

    volume_rotate_cached = ROTATE_0;
    volume_preload_gles_done = 0;
    volume_disabled_gles = 0;
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (volume_gles_tex[i]) {
            glDeleteTextures(1, &volume_gles_tex[i]);
            volume_gles_tex[i] = 0;
        }
    }
    vtx_volume_valid = 0;
}

static void ensure_context(SDL_Window *window) {
    if (!window) return;

    SDL_GLContext ctx = SDL_GL_GetCurrentContext();
    if (!ctx) return;

    if (last_ctx != ctx) {
        last_ctx = ctx;
        on_context_changed();
    }
}

static void ensure_program(void) {
    if (prog_ready || prog_attempted) return;
    prog_attempted = 1;

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_overlay_src);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }

    gles_prog = link_program(vs, fs, 1);

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!gles_prog) return;

    gles_a_pos = glGetAttribLocation(gles_prog, "a_pos");
    gles_a_uv = glGetAttribLocation(gles_prog, "a_uv");

    gles_u_tex_overlay = glGetUniformLocation(gles_prog, "u_tex");
    gles_u_alpha = glGetUniformLocation(gles_prog, "u_alpha");

    if (gles_a_pos < 0 || gles_a_uv < 0 || gles_u_tex_overlay < 0) {
        LOG_ERROR("stage", "gles locations missing (pos=%d uv=%d tex=%d)", gles_a_pos, gles_a_uv, gles_u_tex_overlay);
        destroy_overlay();
        return;
    }

    if (gles_u_alpha < 0) LOG_WARN("stage", "u_alpha uniform not found... alpha disabled!");

    prog_ready = 1;
}

static void draw_quad_content(GLuint tex, const gl_vtx_t vtx[4]) {
    if (!gles_prog_content || !tex) return;

    use_program(gles_prog_content);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    if (gles_u_tex_content >= 0) glUniform1i(gles_u_tex_content, 0);

    const void *base = (const void *) vtx;

    glEnableVertexAttribArray((GLuint) gles_content_a_pos);
    glEnableVertexAttribArray((GLuint) gles_content_a_uv);

    glVertexAttribPointer((GLuint) gles_content_a_pos, 2, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(gl_vtx_t), base);
    glVertexAttribPointer((GLuint) gles_content_a_uv, 2, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(gl_vtx_t), (const char *) base + offsetof(gl_vtx_t, u));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray((GLuint) gles_content_a_pos);
    glDisableVertexAttribArray((GLuint) gles_content_a_uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void draw_quad_overlay(GLuint tex, const gl_vtx_t vtx[4], float alpha) {
    if (!prog_ready || !tex) return;

    use_program(gles_prog);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    if (gles_u_tex_overlay >= 0) glUniform1i(gles_u_tex_overlay, 0);
    if (gles_u_alpha >= 0) glUniform1f(gles_u_alpha, alpha);

    const void *base = (const void *) vtx;

    glEnableVertexAttribArray((GLuint) gles_a_pos);
    glEnableVertexAttribArray((GLuint) gles_a_uv);

    glVertexAttribPointer((GLuint) gles_a_pos, 2, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(gl_vtx_t), base);
    glVertexAttribPointer((GLuint) gles_a_uv, 2, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(gl_vtx_t), (const char *) base + offsetof(gl_vtx_t, u));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray((GLuint) gles_a_pos);
    glDisableVertexAttribArray((GLuint) gles_a_uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void ensure_output_tex(int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (output_tex && w == output_tex_w && h == output_tex_h) return;

    if (output_tex) {
        glDeleteTextures(1, &output_tex);
        output_tex = 0;
    }
    if (output_fbo) {
        glDeleteFramebuffers(1, &output_fbo);
        output_fbo = 0;
    }

    glGenTextures(1, &output_tex);
    glBindTexture(GL_TEXTURE_2D, output_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &output_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteTextures(1, &output_tex);
        output_tex = 0;
        glDeleteFramebuffers(1, &output_fbo);
        output_fbo = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    output_tex_w = w;
    output_tex_h = h;
}

static void ensure_overlay_tex(int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (overlay_tex && w == overlay_tex_w && h == overlay_tex_h) return;

    destroy_overlay_all();

    glGenTextures(1, &overlay_tex);
    glBindTexture(GL_TEXTURE_2D, overlay_tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, &overlay_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, overlay_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, overlay_tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        destroy_overlay_all();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    overlay_tex_w = w;
    overlay_tex_h = h;
}

// Compare two overlay states and return any tracked fields if they differ.
static inline int overlay_change(const overlay_state_t *a, const overlay_state_t *b) {
    if (a->fb_w != b->fb_w) return 1;
    if (a->fb_h != b->fb_h) return 1;

    if (a->base_disabled != b->base_disabled) return 1;
    if (a->base_tex != b->base_tex) return 1;
    if (a->base_alpha != b->base_alpha) return 1;

    if (a->battery_step != b->battery_step) return 1;
    if (a->battery_tex != b->battery_tex) return 1;
    if (a->battery_alpha != b->battery_alpha) return 1;

    if (a->bright_visible != b->bright_visible) return 1;
    if (a->bright_step != b->bright_step) return 1;
    if (a->bright_tex != b->bright_tex) return 1;
    if (a->bright_alpha != b->bright_alpha) return 1;

    if (a->volume_visible != b->volume_visible) return 1;
    if (a->volume_step != b->volume_step) return 1;
    if (a->volume_tex != b->volume_tex) return 1;
    if (a->volume_alpha != b->volume_alpha) return 1;

    return 0;
}

static overlay_state_t build_overlay_state(int fb_w, int fb_h, int base_disabled, int battery_step) {
    overlay_state_t st;

    memset(&st, 0, sizeof(st));

    st.fb_w = fb_w;
    st.fb_h = fb_h;

    st.base_disabled = base_disabled;
    st.base_tex = (!base_disabled && base_gles_ready) ? base_gles_tex : 0;
    st.base_alpha = (!base_disabled && base_gles_ready) ? get_alpha_cached(&overlay_alpha_cache) : 0.0f;

    st.battery_step = battery_step;
    st.battery_tex = (battery_step >= 0 && battery_step < INDICATOR_STEPS) ? battery_gles_tex[battery_step] : 0;
    st.battery_alpha = st.battery_tex ? get_alpha_cached(&battery_alpha_cache) : 0.0f;

    st.bright_visible = bright_is_visible() ? 1 : 0;
    st.bright_step = bright_last_step;
    st.bright_tex = (st.bright_visible && st.bright_step >= 0 && st.bright_step < INDICATOR_STEPS) ? bright_gles_tex[st.bright_step] : 0;
    st.bright_alpha = st.bright_tex ? get_alpha_cached(&bright_alpha_cache) : 0.0f;

    st.volume_visible = volume_is_visible() ? 1 : 0;
    st.volume_step = volume_last_step;
    st.volume_tex = (st.volume_visible && st.volume_step >= 0 && st.volume_step < INDICATOR_STEPS) ? volume_gles_tex[st.volume_step] : 0;
    st.volume_alpha = st.volume_tex ? get_alpha_cached(&volume_alpha_cache) : 0.0f;

    return st;
}

static void rebuild_overlay(const overlay_state_t *st) {
    ensure_overlay_tex(st->fb_w, st->fb_h);
    if (!overlay_tex || !overlay_fbo) return;

    glBindFramebuffer(GL_FRAMEBUFFER, overlay_fbo);
    glViewport(0, 0, st->fb_w, st->fb_h);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (st->base_tex && vtx_base_valid) draw_quad_overlay(st->base_tex, vtx_base, st->base_alpha);
    if (st->battery_tex && vtx_battery_valid) draw_quad_overlay(st->battery_tex, vtx_battery, st->battery_alpha);
    if (st->bright_tex && vtx_bright_valid) draw_quad_overlay(st->bright_tex, vtx_bright, st->bright_alpha);
    if (st->volume_tex && vtx_volume_valid) draw_quad_overlay(st->volume_tex, vtx_volume, st->volume_alpha);

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) 0);

    overlay_cache = *st;
    overlay_valid = 1;
}

// Compare two colour filter matrix values and return if any value differs.
static inline int matrix_change(const float *a, const float *b) {
    for (int i = 0; i < 9; i++) {
        if (a[i] != b[i]) return 1;
    }

    return 0;
}

static int draw_rotated_content(int fb_w, int fb_h, int rot, GLint dst_fbo) {
    if (!content_copy_supported) return 0;
    if (!content_pass_needed(rot)) return 0;

    ensure_content_program();
    if (!gles_prog_content) return 0;

    ensure_content_tex(fb_w, fb_h);
    if (!content_tex) return 0;

    ensure_output_tex(fb_w, fb_h);
    if (!output_tex || !output_fbo) return 0;

    // This may looking fucking stupid but it does correct some colour filter
    // support for some content like: Chromium BSU - Honestly I don't get it.
    while (glGetError() != GL_NO_ERROR) {}

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) dst_fbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, content_tex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, fb_w, fb_h);

    if (glGetError() != GL_NO_ERROR) {
        reset_render_path();
        return 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
    glViewport(0, 0, fb_w, fb_h);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    use_program(gles_prog_content);

    const struct colour_state *adjust = colour_adjust_get();
    const colour_filter_matrix_t *filter = colour_filter_get();

    if (!content_uniform_cache.valid || content_uniform_cache.brightness != adjust->brightness) {
        if (gles_u_brightness >= 0) glUniform1f(gles_u_brightness, adjust->brightness);
        content_uniform_cache.brightness = adjust->brightness;
    }

    if (!content_uniform_cache.valid || content_uniform_cache.contrast != adjust->contrast) {
        if (gles_u_contrast >= 0) glUniform1f(gles_u_contrast, adjust->contrast);
        content_uniform_cache.contrast = adjust->contrast;
    }

    if (!content_uniform_cache.valid || content_uniform_cache.saturation != adjust->saturation) {
        if (gles_u_saturation >= 0) glUniform1f(gles_u_saturation, adjust->saturation);
        content_uniform_cache.saturation = adjust->saturation;
    }

    if (!content_uniform_cache.valid || content_uniform_cache.hueshift != adjust->hueshift) {
        if (gles_u_hueshift >= 0) glUniform1f(gles_u_hueshift, adjust->hueshift);
        content_uniform_cache.hueshift = adjust->hueshift;
    }

    if (!content_uniform_cache.valid || content_uniform_cache.gamma != adjust->gamma) {
        if (gles_u_gamma >= 0) glUniform1f(gles_u_gamma, adjust->gamma);
        content_uniform_cache.gamma = adjust->gamma;
    }

    int enabled = filter->enabled ? 1 : 0;

    if (!content_uniform_cache.valid || content_uniform_cache.filter_enabled != enabled) {
        if (gles_u_filter_enabled >= 0) glUniform1i(gles_u_filter_enabled, enabled);
        content_uniform_cache.filter_enabled = enabled;
    }

    if (enabled && gles_u_filter >= 0) {
        if (!content_uniform_cache.valid || matrix_change(content_uniform_cache.filter, filter->matrix)) {
            glUniformMatrix3fv(gles_u_filter, 1, GL_FALSE, filter->matrix);
            memcpy(content_uniform_cache.filter, filter->matrix, sizeof(float) * 9);
        }
    }

    content_uniform_cache.valid = 1;

    gl_vtx_t vtx[4];
    build_fullscreen_quad(vtx, rot);

    draw_quad_content(content_tex, vtx);

    return 1;
}

#define UPDATE_GEOM_CACHE(LAYER, TYPE)                           \
    do {                                                         \
        int gc = get_##TYPE##_cached(&(LAYER##_##TYPE##_cache)); \
        if (gc != LAYER##_##TYPE##_cached) {                     \
            LAYER##_##TYPE##_cached = gc;                        \
            vtx_##LAYER##_valid = 0;                             \
        }                                                        \
    } while (0)

#define UPDATE_ROT_CACHE(LAYER)           \
    do {                                  \
        int r = rotate_read_cached();     \
        if (r != LAYER##_rotate_cached) { \
            LAYER##_rotate_cached = r;    \
            vtx_##LAYER##_valid = 0;      \
        }                                 \
    } while (0)

static void update_geometry_caches(void) {
    UPDATE_GEOM_CACHE(base, anchor);
    UPDATE_GEOM_CACHE(base, scale);
    UPDATE_ROT_CACHE(base);

    UPDATE_GEOM_CACHE(battery, anchor);
    UPDATE_GEOM_CACHE(battery, scale);
    UPDATE_ROT_CACHE(battery);

    UPDATE_GEOM_CACHE(bright, anchor);
    UPDATE_GEOM_CACHE(bright, scale);
    UPDATE_ROT_CACHE(bright);

    UPDATE_GEOM_CACHE(volume, anchor);
    UPDATE_GEOM_CACHE(volume, scale);
    UPDATE_ROT_CACHE(volume);
}

static void stage_draw(int fb_w, int fb_h) {
    ensure_program();

    // TODO: For future reference add all independent overlay layers here
    gl_battery_overlay_init();

    const int base_disabled = ino_proc ? base_overlay_disabled_cached : (access(BASE_OVERLAY_NOP, F_OK) == 0);
    if (base_disabled != base_nop_last) {
        if (base_disabled) destroy_base_gles();

        vtx_base_valid = 0;
        overlay_valid = 0;
        base_nop_last = base_disabled;
    }

    // Only init base when truly enabled...
    if (!base_disabled) gl_base_overlay_init(render_window);

    update_geometry_caches();

    if (!base_disabled && base_gles_ready && !vtx_base_valid) {
        build_quad_ndc(vtx_base, base_gles_w, base_gles_h,
                       fb_w, fb_h, base_anchor_cached, base_scale_cached);
        vtx_base_valid = 1;
        overlay_valid = 0;
    }

    int battery_step = battery_last_step;
    if (battery_step >= 0 && battery_step < INDICATOR_STEPS && battery_gles_tex[battery_step] && !vtx_battery_valid) {
        build_quad_ndc(vtx_battery, battery_gles_w[battery_step], battery_gles_h[battery_step],
                       fb_w, fb_h, battery_anchor_cached, battery_scale_cached);
        vtx_battery_valid = 1;
        overlay_valid = 0;
    }

    if (bright_is_visible()) {
        gl_bright_overlay_init();
        int step = bright_last_step;
        if (step >= 0 && step < INDICATOR_STEPS && bright_gles_tex[step] && !vtx_bright_valid) {
            build_quad_ndc(vtx_bright, bright_gles_w[step], bright_gles_h[step],
                           fb_w, fb_h, bright_anchor_cached, bright_scale_cached);
            vtx_bright_valid = 1;
            overlay_valid = 0;
        }
    }

    if (volume_is_visible()) {
        gl_volume_overlay_init();
        int step = volume_last_step;
        if (step >= 0 && step < INDICATOR_STEPS && volume_gles_tex[step] && !vtx_volume_valid) {
            build_quad_ndc(vtx_volume, volume_gles_w[step], volume_gles_h[step],
                           fb_w, fb_h, volume_anchor_cached, volume_scale_cached);
            vtx_volume_valid = 1;
            overlay_valid = 0;
        }
    }

    gles_state_t st;
    save_gles_state(&st);

    glDisable(GL_DEPTH_TEST);

    // Ensure we preserve the aspect ration and blending methods
    const int rot = rotate_read_cached();

    const int use_offscreen = (render_path == RENDER_PATH_OFFSCREEN_FBO) && (render_nonzero_fbo != 0);
    const GLint dst_fbo = use_offscreen ? render_nonzero_fbo : 0;

    ensure_content_program();

    int content_ran = 0;
    if (content_copy_supported && content_pass_needed(rot)) {
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        content_ran = draw_rotated_content(fb_w, fb_h, rot, dst_fbo);
    }

    overlay_state_t batch_state = build_overlay_state(fb_w, fb_h, base_disabled, battery_step);
    if (!overlay_valid || overlay_change(&overlay_cache, &batch_state)) rebuild_overlay(&batch_state);

    if (content_ran && output_tex) {
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) dst_fbo);
        glViewport(0, 0, fb_w, fb_h);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);

        gl_vtx_t vtx_out[4];
        build_fullscreen_quad(vtx_out, ROTATE_0);
        draw_quad_overlay(output_tex, vtx_out, 1.0f);
    }

    if (overlay_valid && overlay_tex) {
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) dst_fbo);
        glViewport(0, 0, fb_w, fb_h);
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        gl_vtx_t vtx[4];
        build_fullscreen_quad(vtx, ROTATE_0);
        draw_quad_overlay(overlay_tex, vtx, 1.0f);
    }

    restore_gles_state(&st);
    current_program = (GLuint) st.program;
}

void SDL_GL_SwapWindow(SDL_Window *window) {
    if (is_overlay_disabled()) {
        if (real_SDL_GL_SwapWindow) real_SDL_GL_SwapWindow(window);
        return;
    }

    if (!real_SDL_GL_SwapWindow) return;

    base_inotify_check();
    if (ino_proc) inotify_check(ino_proc);

    render_window = window;
    ensure_context(window);

    battery_overlay_update();
    bright_overlay_update();
    volume_overlay_update();

    int nw = 0;
    int nh = 0;
    SDL_GL_GetDrawableSize(window, &nw, &nh);

    // Sample the framebuffer binding and detect the render path
    // then reset if required on context change!
    GLint app_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &app_fbo);
    detect_render_path(app_fbo);

    static int last_rot = ROTATE_0;
    const int r = rotate_read_cached();

    int invalidate = 0;

    if (r != last_rot) {
        last_rot = r;
        invalidate = 1;
    }

    if (nw != fb_cached_w || nh != fb_cached_h) {
        fb_cached_w = nw;
        fb_cached_h = nh;
        invalidate = 1;
    }

    if (invalidate) {
        vtx_base_valid = 0;
        vtx_battery_valid = 0;
        vtx_bright_valid = 0;
        vtx_volume_valid = 0;
        overlay_valid = 0;
    }

    if (fb_cached_w > 0 && fb_cached_h > 0) stage_draw(fb_cached_w, fb_cached_h);
    real_SDL_GL_SwapWindow(window);
}
