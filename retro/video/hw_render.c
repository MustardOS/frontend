#include <stddef.h>
#include <GLES2/gl2.h>
#include "../../common/init.h"
#include "../../common/log.h"
#include "colour.h"
#include "hw_render.h"

#ifndef GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH24_STENCIL8_OES 0x88F0
#endif
#ifndef GL_DEPTH_STENCIL_OES
#define GL_DEPTH_STENCIL_OES 0x84F9
#endif

static const char *vs_src = "attribute vec2 a_pos;"
                            "attribute vec2 a_uv;"
                            "varying vec2 v_uv;"
                            "void main(){"
                            "    gl_Position = vec4(a_pos, 0.0, 1.0);"
                            "    v_uv = a_uv;"
                            "}";

static const char *fs_src = "precision mediump float;"
                            "uniform sampler2D u_tex;"
                            "uniform int u_swap;"
                            "varying vec2 v_uv;"
                            "void main(){"
                            "    vec4 t = texture2D(u_tex, v_uv);"
                            "    gl_FragColor = u_swap != 0 ? vec4(t.b, t.g, t.r, 1.0) : vec4(t.rgb, 1.0);"
                            "}";

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
static void(GL_APIENTRY *p_glDrawArrays)(GLenum mode, GLint first, GLsizei count) = NULL;
static void(GL_APIENTRY *p_glBindBuffer)(GLenum target, GLuint buffer) = NULL;
static void(GL_APIENTRY *p_glActiveTexture)(GLenum texture) = NULL;
static void(GL_APIENTRY *p_glEnable)(GLenum cap) = NULL;
static void(GL_APIENTRY *p_glDisable)(GLenum cap) = NULL;
static void(GL_APIENTRY *p_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height) = NULL;
static void(GL_APIENTRY *p_glClearColor)(GLfloat r, GLfloat g, GLfloat b, GLfloat a) = NULL;
static void(GL_APIENTRY *p_glClear)(GLbitfield mask) = NULL;
static void(GL_APIENTRY *p_glColorMask)(GLboolean r, GLboolean g, GLboolean b, GLboolean a) = NULL;
static void(GL_APIENTRY *p_glDepthMask)(GLboolean flag) = NULL;
static void(GL_APIENTRY *p_glGetIntegerv)(GLenum pname, GLint *params) = NULL;
static void(GL_APIENTRY *p_glGetBooleanv)(GLenum pname, GLboolean *params) = NULL;
static void(GL_APIENTRY *p_glGetFloatv)(GLenum pname, GLfloat *params) = NULL;
static GLboolean(GL_APIENTRY *p_glIsEnabled)(GLenum cap) = NULL;
static void(GL_APIENTRY *p_glGetVertexAttribiv)(GLuint index, GLenum pname, GLint *params) = NULL;
static void(GL_APIENTRY *p_glBlendFuncSeparate)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) = NULL;
static void(GL_APIENTRY *p_glBlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha) = NULL;
static void(GL_APIENTRY *p_glFrontFace)(GLenum mode) = NULL;
static void(GL_APIENTRY *p_glCullFace)(GLenum mode) = NULL;
static void(GL_APIENTRY *p_glPixelStorei)(GLenum pname, GLint param) = NULL;
static void(GL_APIENTRY *p_glScissor)(GLint x, GLint y, GLsizei width, GLsizei height) = NULL;

static void(GL_APIENTRY *p_glGenFramebuffers)(GLsizei n, GLuint *framebuffers) = NULL;
static void(GL_APIENTRY *p_glDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers) = NULL;
static void(GL_APIENTRY *p_glBindFramebuffer)(GLenum target, GLuint framebuffer) = NULL;
static void(GL_APIENTRY *p_glFramebufferTexture2D)(
    GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level
) = NULL;
static void(GL_APIENTRY *p_glFramebufferRenderbuffer)(
    GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer
) = NULL;
static GLenum(GL_APIENTRY *p_glCheckFramebufferStatus)(GLenum target) = NULL;

static void(GL_APIENTRY *p_glGenRenderbuffers)(GLsizei n, GLuint *renderbuffers) = NULL;
static void(GL_APIENTRY *p_glDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers) = NULL;
static void(GL_APIENTRY *p_glBindRenderbuffer)(GLenum target, GLuint renderbuffer) = NULL;
static void(GL_APIENTRY *p_glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) =
    NULL;

static void(GL_APIENTRY *p_glGenTextures)(GLsizei n, GLuint *textures) = NULL;
static void(GL_APIENTRY *p_glDeleteTextures)(GLsizei n, const GLuint *textures) = NULL;
static void(GL_APIENTRY *p_glBindTexture)(GLenum target, GLuint texture) = NULL;
static void(GL_APIENTRY *p_glTexImage2D)(
    GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format,
    GLenum type, const void *pixels
) = NULL;
static void(GL_APIENTRY *p_glTexParameteri)(GLenum target, GLenum pname, GLint param) = NULL;

static int gl_funcs_ready = 0;

static int load_gl_functions(void) {
    if (gl_funcs_ready) return 1;

#define LOAD_GL(name)                                                                                                  \
    do {                                                                                                               \
        p_##name = (void *) SDL_GL_GetProcAddress(#name);                                                              \
        if (!p_##name) {                                                                                               \
            LOG_ERROR(mux_module, "hw_render: failed to resolve GL function %s", #name);                               \
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
    LOAD_GL(glDrawArrays);
    LOAD_GL(glBindBuffer);
    LOAD_GL(glActiveTexture);
    LOAD_GL(glEnable);
    LOAD_GL(glDisable);
    LOAD_GL(glViewport);
    LOAD_GL(glClearColor);
    LOAD_GL(glClear);
    LOAD_GL(glColorMask);
    LOAD_GL(glDepthMask);
    LOAD_GL(glGetIntegerv);
    LOAD_GL(glGetBooleanv);
    LOAD_GL(glGetFloatv);
    LOAD_GL(glIsEnabled);
    LOAD_GL(glGetVertexAttribiv);
    LOAD_GL(glBlendFuncSeparate);
    LOAD_GL(glBlendEquationSeparate);
    LOAD_GL(glFrontFace);
    LOAD_GL(glCullFace);
    LOAD_GL(glPixelStorei);
    LOAD_GL(glScissor);
    LOAD_GL(glGenFramebuffers);
    LOAD_GL(glDeleteFramebuffers);
    LOAD_GL(glBindFramebuffer);
    LOAD_GL(glFramebufferTexture2D);
    LOAD_GL(glFramebufferRenderbuffer);
    LOAD_GL(glCheckFramebufferStatus);
    LOAD_GL(glGenRenderbuffers);
    LOAD_GL(glDeleteRenderbuffers);
    LOAD_GL(glBindRenderbuffer);
    LOAD_GL(glRenderbufferStorage);
    LOAD_GL(glGenTextures);
    LOAD_GL(glDeleteTextures);
    LOAD_GL(glBindTexture);
    LOAD_GL(glTexImage2D);
    LOAD_GL(glTexParameteri);

#undef LOAD_GL

    gl_funcs_ready = 1;
    return 1;
}

static retro_hw_context_reset_t core_context_reset = NULL;
static retro_hw_context_reset_t core_context_destroy = NULL;
static int want_depth = 0;
static int want_stencil = 0;
static int flip_needed = 1;

static int active = 0;
static int context_ready = 0;

static GLuint fbo = 0;
static GLuint colour_tex = 0;
static GLuint depth_stencil_rb = 0;
static int target_w = 0;
static int target_h = 0;

static unsigned frame_valid_w = 0;
static unsigned frame_valid_h = 0;

static GLuint prog = 0;
static GLint a_pos = -1, a_uv = -1, u_tex = -1, u_swap = -1;
static int prog_ready = 0;

static SDL_Texture *filter_src_tex = NULL;
static int filter_src_w = 0;
static int filter_src_h = 0;

int hw_render_bridge_active(void) {
    return active;
}

int hw_render_bridge_negotiate(struct retro_hw_render_callback *cb) {
    if (!cb) return 0;

    if (cb->context_type != RETRO_HW_CONTEXT_OPENGLES2) {
        LOG_WARN(
            mux_module, "hw_render: core requested unsupported context type %d (only GLES2 is available)",
            (int) cb->context_type
        );
        return 0;
    }

    if (!load_gl_functions()) return 0;

    core_context_reset = cb->context_reset;
    core_context_destroy = cb->context_destroy;
    want_depth = cb->depth;
    want_stencil = cb->stencil;

    flip_needed = cb->bottom_left_origin;

    cb->get_current_framebuffer = hw_render_bridge_get_current_framebuffer;
    cb->get_proc_address = hw_render_bridge_get_proc_address;

    active = 1;
    LOG_INFO(
        mux_module, "hw_render: accepted GLES2 hardware-render request (depth=%d stencil=%d bottom_left_origin=%d)",
        want_depth, want_stencil, cb->bottom_left_origin
    );
    return 1;
}

static int compile_shader(const GLenum type, const char *src, GLuint *out) {
    const GLuint shader = p_glCreateShader(type);
    p_glShaderSource(shader, 1, &src, NULL);
    p_glCompileShader(shader);

    GLint ok = 0;
    p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        p_glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOG_ERROR(mux_module, "hw_render: shader compile failed: %s", log);
        p_glDeleteShader(shader);
        return 0;
    }

    *out = shader;
    return 1;
}

static void ensure_program(void) {
    if (prog_ready) return;
    prog_ready = 1;

    GLuint vs = 0, fs = 0;
    if (!compile_shader(GL_VERTEX_SHADER, vs_src, &vs)) return;
    if (!compile_shader(GL_FRAGMENT_SHADER, fs_src, &fs)) {
        p_glDeleteShader(vs);
        return;
    }

    prog = p_glCreateProgram();
    p_glAttachShader(prog, vs);
    p_glAttachShader(prog, fs);
    p_glLinkProgram(prog);

    GLint linked = 0;
    p_glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    p_glDeleteShader(vs);
    p_glDeleteShader(fs);

    if (!linked) {
        char log[512];
        p_glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        LOG_ERROR(mux_module, "hw_render: program link failed: %s", log);
        p_glDeleteProgram(prog);
        prog = 0;
        return;
    }

    a_pos = p_glGetAttribLocation(prog, "a_pos");
    a_uv = p_glGetAttribLocation(prog, "a_uv");
    u_tex = p_glGetUniformLocation(prog, "u_tex");
    u_swap = p_glGetUniformLocation(prog, "u_swap");
}

static void destroy_target(void) {
    if (fbo) {
        p_glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
    if (colour_tex) {
        p_glDeleteTextures(1, &colour_tex);
        colour_tex = 0;
    }
    if (depth_stencil_rb) {
        p_glDeleteRenderbuffers(1, &depth_stencil_rb);
        depth_stencil_rb = 0;
    }
    target_w = 0;
    target_h = 0;
}

void hw_render_bridge_configure(const unsigned max_width, const unsigned max_height) {
    if (!active || max_width == 0 || max_height == 0) return;
    if ((int) max_width == target_w && (int) max_height == target_h) return;

    const int first_time = fbo == 0;
    hw_render_bridge_context_save();

    destroy_target();

    p_glGenTextures(1, &colour_tex);
    p_glBindTexture(GL_TEXTURE_2D, colour_tex);
    p_glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei) max_width, (GLsizei) max_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL
    );
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    p_glBindTexture(GL_TEXTURE_2D, 0);

    p_glGenFramebuffers(1, &fbo);
    p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour_tex, 0);

    if (want_depth) {
        p_glGenRenderbuffers(1, &depth_stencil_rb);
        p_glBindRenderbuffer(GL_RENDERBUFFER, depth_stencil_rb);

        if (want_stencil) {
            p_glRenderbufferStorage(
                GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, (GLsizei) max_width, (GLsizei) max_height
            );
            p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_stencil_rb);
            p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_stencil_rb);
        } else {
            p_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, (GLsizei) max_width, (GLsizei) max_height);
            p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_stencil_rb);
        }
    }

    const GLenum status = p_glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG_ERROR(mux_module, "hw_render: framebuffer incomplete (status 0x%x) - disabling hardware render", status);
        p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        destroy_target();
        hw_render_bridge_context_restore();
        active = 0;
        return;
    }

    p_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    p_glClear(
        GL_COLOR_BUFFER_BIT | (want_depth ? GL_DEPTH_BUFFER_BIT : 0) | (want_stencil ? GL_STENCIL_BUFFER_BIT : 0)
    );
    p_glBindFramebuffer(GL_FRAMEBUFFER, 0);

    target_w = (int) max_width;
    target_h = (int) max_height;
    frame_valid_w = max_width;
    frame_valid_h = max_height;

    ensure_program();

    if (first_time) {
        context_ready = 1;
        if (core_context_reset) core_context_reset();
    }

    hw_render_bridge_context_restore();
}

uintptr_t hw_render_bridge_get_current_framebuffer(void) {
    return (uintptr_t) fbo;
}

retro_proc_address_t hw_render_bridge_get_proc_address(const char *sym) {
    return (retro_proc_address_t) SDL_GL_GetProcAddress(sym);
}

void hw_render_bridge_notify_frame(const unsigned width, const unsigned height) {
    if (width == 0 || height == 0) return;
    frame_valid_w = width;
    frame_valid_h = height;
}

#define HW_TEX_UNITS  3
#define HW_ATTRIB_MAX 16

typedef struct {
    GLint program;
    GLint viewport[4];
    GLint scissor_box[4];
    GLboolean scissor_en;
    GLboolean blend_en;
    GLboolean depth_test_en;
    GLboolean stencil_test_en;
    GLboolean cull_face_en;
    GLboolean dither_en;
    GLint blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha;
    GLint blend_eq_rgb, blend_eq_alpha;
    GLboolean color_mask[4];
    GLboolean depth_mask;
    GLfloat clear_color[4];
    GLint front_face;
    GLint cull_face_mode;
    GLint unpack_alignment;
    GLint active_texture;
    GLint tex_binding[HW_TEX_UNITS];
    GLint array_buffer;
    GLint element_array_buffer;
    GLint framebuffer;
    GLint attrib_count;
    GLint attrib_enabled[HW_ATTRIB_MAX];
    int valid;
} gl_host_state_t;

static gl_host_state_t sdl_state;
static gl_host_state_t core_state;

static void set_cap(const GLenum cap, const GLboolean enabled) {
    if (enabled) {
        p_glEnable(cap);
    } else {
        p_glDisable(cap);
    }
}

static void gl_state_capture(gl_host_state_t *s) {
    p_glGetIntegerv(GL_CURRENT_PROGRAM, &s->program);
    p_glGetIntegerv(GL_VIEWPORT, s->viewport);
    p_glGetIntegerv(GL_SCISSOR_BOX, s->scissor_box);

    s->scissor_en = p_glIsEnabled(GL_SCISSOR_TEST);
    s->blend_en = p_glIsEnabled(GL_BLEND);
    s->depth_test_en = p_glIsEnabled(GL_DEPTH_TEST);
    s->stencil_test_en = p_glIsEnabled(GL_STENCIL_TEST);
    s->cull_face_en = p_glIsEnabled(GL_CULL_FACE);
    s->dither_en = p_glIsEnabled(GL_DITHER);

    p_glGetIntegerv(GL_BLEND_SRC_RGB, &s->blend_src_rgb);
    p_glGetIntegerv(GL_BLEND_DST_RGB, &s->blend_dst_rgb);
    p_glGetIntegerv(GL_BLEND_SRC_ALPHA, &s->blend_src_alpha);
    p_glGetIntegerv(GL_BLEND_DST_ALPHA, &s->blend_dst_alpha);
    p_glGetIntegerv(GL_BLEND_EQUATION_RGB, &s->blend_eq_rgb);
    p_glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &s->blend_eq_alpha);

    p_glGetBooleanv(GL_COLOR_WRITEMASK, s->color_mask);
    p_glGetBooleanv(GL_DEPTH_WRITEMASK, &s->depth_mask);
    p_glGetFloatv(GL_COLOR_CLEAR_VALUE, s->clear_color);

    p_glGetIntegerv(GL_FRONT_FACE, &s->front_face);
    p_glGetIntegerv(GL_CULL_FACE_MODE, &s->cull_face_mode);
    p_glGetIntegerv(GL_UNPACK_ALIGNMENT, &s->unpack_alignment);

    p_glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s->array_buffer);
    p_glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &s->element_array_buffer);
    p_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s->framebuffer);

    p_glGetIntegerv(GL_ACTIVE_TEXTURE, &s->active_texture);
    for (int i = 0; i < HW_TEX_UNITS; i++) {
        p_glActiveTexture(GL_TEXTURE0 + (GLenum) i);
        p_glGetIntegerv(GL_TEXTURE_BINDING_2D, &s->tex_binding[i]);
    }
    p_glActiveTexture((GLenum) s->active_texture);

    GLint max_attribs = 0;
    p_glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attribs);
    s->attrib_count = max_attribs < HW_ATTRIB_MAX ? max_attribs : HW_ATTRIB_MAX;
    for (GLint i = 0; i < s->attrib_count; i++)
        p_glGetVertexAttribiv((GLuint) i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &s->attrib_enabled[i]);

    s->valid = 1;
}

static void gl_state_apply(const gl_host_state_t *s) {
    if (!s->valid) return;

    p_glUseProgram((GLuint) s->program);
    p_glViewport(s->viewport[0], s->viewport[1], s->viewport[2], s->viewport[3]);
    p_glScissor(s->scissor_box[0], s->scissor_box[1], s->scissor_box[2], s->scissor_box[3]);

    set_cap(GL_SCISSOR_TEST, s->scissor_en);
    set_cap(GL_BLEND, s->blend_en);
    set_cap(GL_DEPTH_TEST, s->depth_test_en);
    set_cap(GL_STENCIL_TEST, s->stencil_test_en);
    set_cap(GL_CULL_FACE, s->cull_face_en);
    set_cap(GL_DITHER, s->dither_en);

    p_glBlendFuncSeparate(
        (GLenum) s->blend_src_rgb, (GLenum) s->blend_dst_rgb, (GLenum) s->blend_src_alpha, (GLenum) s->blend_dst_alpha
    );
    p_glBlendEquationSeparate((GLenum) s->blend_eq_rgb, (GLenum) s->blend_eq_alpha);

    p_glColorMask(s->color_mask[0], s->color_mask[1], s->color_mask[2], s->color_mask[3]);
    p_glDepthMask(s->depth_mask);
    p_glClearColor(s->clear_color[0], s->clear_color[1], s->clear_color[2], s->clear_color[3]);

    p_glFrontFace((GLenum) s->front_face);
    p_glCullFace((GLenum) s->cull_face_mode);
    p_glPixelStorei(GL_UNPACK_ALIGNMENT, s->unpack_alignment);

    p_glBindBuffer(GL_ARRAY_BUFFER, (GLuint) s->array_buffer);
    p_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint) s->element_array_buffer);
    p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) s->framebuffer);

    for (int i = 0; i < HW_TEX_UNITS; i++) {
        p_glActiveTexture(GL_TEXTURE0 + (GLenum) i);
        p_glBindTexture(GL_TEXTURE_2D, (GLuint) s->tex_binding[i]);
    }
    p_glActiveTexture((GLenum) s->active_texture);

    for (GLint i = 0; i < s->attrib_count; i++) {
        if (s->attrib_enabled[i]) {
            p_glEnableVertexAttribArray((GLuint) i);
        } else {
            p_glDisableVertexAttribArray((GLuint) i);
        }
    }
}

void hw_render_bridge_context_save(void) {
    if (!active || !gl_funcs_ready) return;
    gl_state_capture(&sdl_state);
}

void hw_render_bridge_context_restore(void) {
    if (!active || !gl_funcs_ready || !sdl_state.valid) return;

    gl_state_capture(&core_state);
    gl_state_apply(&sdl_state);
}

void hw_render_bridge_enter_core_call(void) {
    if (!active || !gl_funcs_ready) return;
    gl_state_capture(&sdl_state);
    gl_state_apply(&core_state);
}

void hw_render_bridge_exit_core_call(void) {
    if (!active || !gl_funcs_ready || !sdl_state.valid) return;
    gl_state_capture(&core_state);
    gl_state_apply(&sdl_state);
}

static void draw_hw_quad(
    const float l, const float r, const float t, const float b, const float u_max, const float v_at_top,
    const float v_at_bottom, const int vp_w, const int vp_h, const int swap_channels
) {
    const GLfloat verts[] = {
        l, t, 0.0f,  v_at_top,    // top left
        r, t, u_max, v_at_top,    // top right
        l, b, 0.0f,  v_at_bottom, // bottom left
        r, b, u_max, v_at_bottom, // bottom right
    };

    GLint prev_program = 0;
    p_glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

    GLint prev_viewport[4] = {0};
    p_glGetIntegerv(GL_VIEWPORT, prev_viewport);

    GLint prev_array_buffer = 0;
    p_glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_array_buffer);

    GLint prev_active_texture = 0;
    p_glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_texture);
    p_glActiveTexture(GL_TEXTURE0);

    GLint prev_texture0 = 0;
    p_glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture0);

    const GLboolean prev_blend_enabled = p_glIsEnabled(GL_BLEND);
    const GLboolean prev_scissor_enabled = p_glIsEnabled(GL_SCISSOR_TEST);

    GLint prev_attrib_pos_enabled = 0, prev_attrib_uv_enabled = 0;
    if (a_pos >= 0) p_glGetVertexAttribiv((GLuint) a_pos, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &prev_attrib_pos_enabled);
    if (a_uv >= 0) p_glGetVertexAttribiv((GLuint) a_uv, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &prev_attrib_uv_enabled);

    p_glDisable(GL_SCISSOR_TEST);
    p_glDisable(GL_BLEND);
    p_glViewport(0, 0, vp_w, vp_h);
    p_glUseProgram(prog);
    p_glBindTexture(GL_TEXTURE_2D, colour_tex);
    if (u_tex >= 0) p_glUniform1i(u_tex, 0);
    if (u_swap >= 0) p_glUniform1i(u_swap, swap_channels);

    p_glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (a_pos >= 0) {
        p_glVertexAttribPointer(a_pos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), verts);
        p_glEnableVertexAttribArray(a_pos);
    }
    if (a_uv >= 0) {
        p_glVertexAttribPointer(a_uv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), verts + 2);
        p_glEnableVertexAttribArray(a_uv);
    }

    p_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (a_pos >= 0 && !prev_attrib_pos_enabled) p_glDisableVertexAttribArray(a_pos);
    if (a_uv >= 0 && !prev_attrib_uv_enabled) p_glDisableVertexAttribArray(a_uv);

    p_glBindTexture(GL_TEXTURE_2D, (GLuint) prev_texture0);
    p_glActiveTexture((GLenum) prev_active_texture);
    p_glBindBuffer(GL_ARRAY_BUFFER, (GLuint) prev_array_buffer);
    p_glUseProgram((GLuint) prev_program);
    p_glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
    if (prev_blend_enabled) p_glEnable(GL_BLEND);
    if (prev_scissor_enabled) p_glEnable(GL_SCISSOR_TEST);
}

static int ensure_filter_src(SDL_Renderer *renderer) {
    const int w = (int) frame_valid_w;
    const int h = (int) frame_valid_h;
    if (w <= 0 || h <= 0) return 0;
    if (filter_src_tex && w == filter_src_w && h == filter_src_h) return 1;

    if (filter_src_tex) {
        SDL_DestroyTexture(filter_src_tex);
        filter_src_tex = NULL;
    }

    filter_src_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if (!filter_src_tex) {
        LOG_ERROR(mux_module, "hw_render: failed to create filter source texture: %s", SDL_GetError());
        filter_src_w = 0;
        filter_src_h = 0;
        return 0;
    }

    filter_src_w = w;
    filter_src_h = h;
    return 1;
}

void hw_render_bridge_draw(SDL_Renderer *renderer, const SDL_Rect *dest_rect) {
    if (!active || !context_ready || !colour_tex || target_w == 0 || target_h == 0) return;
    if (!prog) return;

    SDL_RenderFlush(renderer);

    const float u_max = target_w > 0 ? (float) frame_valid_w / (float) target_w : 1.0f;
    const float v_max = target_h > 0 ? (float) frame_valid_h / (float) target_h : 1.0f;

    if (colour_pass_needed() && ensure_filter_src(renderer)) {
        SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
        if (SDL_SetRenderTarget(renderer, filter_src_tex) == 0) {
            const float v_at_top = flip_needed ? 0.0f : v_max;
            const float v_at_bottom = flip_needed ? v_max : 0.0f;

            draw_hw_quad(-1.0f, 1.0f, 1.0f, -1.0f, u_max, v_at_top, v_at_bottom, filter_src_w, filter_src_h, 1);
            SDL_SetRenderTarget(renderer, prev_target);

            colour_render_pass(renderer, filter_src_tex, dest_rect);
            return;
        }
        SDL_SetRenderTarget(renderer, prev_target);
    }

    int out_w = 0, out_h = 0;
    SDL_Texture *cur_target = SDL_GetRenderTarget(renderer);
    if (cur_target) {
        SDL_QueryTexture(cur_target, NULL, NULL, &out_w, &out_h);
    } else {
        SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
    }
    if (out_w <= 0 || out_h <= 0) return;

    const float ndc_left = ((float) dest_rect->x / (float) out_w) * 2.0f - 1.0f;
    const float ndc_right = ((float) (dest_rect->x + dest_rect->w) / (float) out_w) * 2.0f - 1.0f;
    const float ndc_top = 1.0f - ((float) dest_rect->y / (float) out_h) * 2.0f;
    const float ndc_bottom = 1.0f - ((float) (dest_rect->y + dest_rect->h) / (float) out_h) * 2.0f;

    const float v_at_top = flip_needed ? v_max : 0.0f;
    const float v_at_bottom = flip_needed ? 0.0f : v_max;

    draw_hw_quad(ndc_left, ndc_right, ndc_top, ndc_bottom, u_max, v_at_top, v_at_bottom, out_w, out_h, 0);
}

void hw_render_bridge_shutdown(void) {
    if (!gl_funcs_ready) {
        active = 0;
        context_ready = 0;
        return;
    }

    if (active && context_ready && core_context_destroy) {
        hw_render_bridge_enter_core_call();
        core_context_destroy();
        hw_render_bridge_exit_core_call();
    }

    destroy_target();

    if (filter_src_tex) {
        SDL_DestroyTexture(filter_src_tex);
        filter_src_tex = NULL;
    }
    filter_src_w = 0;
    filter_src_h = 0;

    if (prog) {
        p_glDeleteProgram(prog);
        prog = 0;
    }
    prog_ready = 0;
    a_pos = a_uv = u_tex = u_swap = -1;

    core_context_reset = NULL;
    core_context_destroy = NULL;

    active = 0;
    context_ready = 0;

    frame_valid_w = 0;
    frame_valid_h = 0;
}
