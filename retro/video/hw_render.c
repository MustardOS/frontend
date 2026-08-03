#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "../../common/display.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "colour.h"
#include "filters/filters.h"
#include "hw_render.h"
#include "overlay_bridge.h"
#include "../core/governor_boost.h"
#include "../core/perf.h"
#include "../settings/settings.h"

#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif
#ifndef GL_READ_FRAMEBUFFER_BINDING
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#endif
#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER 0x88EB
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_UNIFORM_BUFFER
#define GL_UNIFORM_BUFFER 0x8A11
#endif
#ifndef GL_TRANSFORM_FEEDBACK
#define GL_TRANSFORM_FEEDBACK 0x8E22
#endif
#ifndef GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH24_STENCIL8_OES 0x88F0
#endif
#ifndef GL_DEPTH_STENCIL_OES
#define GL_DEPTH_STENCIL_OES 0x84F9
#endif
#ifndef GL_SAMPLE_BUFFERS
#define GL_SAMPLE_BUFFERS 0x80A8
#endif
#ifndef GL_SAMPLES
#define GL_SAMPLES 0x80A9
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
static GLboolean(GL_APIENTRY *p_glIsTexture)(GLuint texture) = NULL;
static const GLubyte *(GL_APIENTRY *p_glGetString)(GLenum name) = NULL;

static void(GL_APIENTRY *p_glFlush)(void) = NULL;

// ES3 only, so these are loaded best effort and stay NULL on an ES2 driver
static int es3_available = 0;
static void(GL_APIENTRY *p_glBindVertexArray)(GLuint array) = NULL;
static void(GL_APIENTRY *p_glBindSampler)(GLuint unit, GLuint sampler) = NULL;
static void(GL_APIENTRY *p_glBindTransformFeedback)(GLenum target, GLuint id) = NULL;

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
    LOAD_GL(glIsTexture);
    LOAD_GL(glGetString);
    LOAD_GL(glFlush);

#undef LOAD_GL

    p_glBindVertexArray = SDL_GL_GetProcAddress("glBindVertexArray");
    p_glBindSampler = SDL_GL_GetProcAddress("glBindSampler");
    p_glBindTransformFeedback = SDL_GL_GetProcAddress("glBindTransformFeedback");

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

#define HW_TARGET_COUNT_MAX              2
#define HW_DOUBLE_BUFFER_MAX_EXTRA_BYTES (16u * 1024u * 1024u)

static GLuint fbo[HW_TARGET_COUNT_MAX] = {0};
static GLuint colour_tex[HW_TARGET_COUNT_MAX] = {0};
static GLuint depth_stencil_rb = 0;
static int target_count = 1;
static int render_index = 0;
static int display_index = 0;
static int target_queried = 0;
static int handed_index = 0;
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
static SDL_Renderer *cached_output_renderer = NULL;
static int cached_output_w = 0;
static int cached_output_h = 0;

typedef struct {
    GLint program;
    GLint array_buffer;
    GLint active_texture;
    GLint texture0;
    GLboolean blend_enabled;
    GLboolean scissor_enabled;
    GLint attrib_pos_enabled;
    GLint attrib_uv_enabled;
    int valid;
} quad_restore_state_t;

static quad_restore_state_t cached_quad_restore;

static GLint target_filter_param(void) {
    return texture_filter_wants_linear_sample(session_settings.texture_filter) ? GL_LINEAR : GL_NEAREST;
}

static SDL_Window *gl_window = NULL;
static SDL_GLContext sdl_ctx = NULL;
static SDL_GLContext core_ctx = NULL;

static int owns_context(void) {
    return core_ctx != NULL;
}

static const char *profile_name(const int profile) {
    return profile == SDL_GL_CONTEXT_PROFILE_ES ? "GLES" : profile == SDL_GL_CONTEXT_PROFILE_CORE ? "GL core" : "GL";
}

#ifndef GL_CONTEXT_PROFILE_MASK
#define GL_CONTEXT_PROFILE_MASK 0x9126
#endif
#ifndef GL_CONTEXT_CORE_PROFILE_BIT
#define GL_CONTEXT_CORE_PROFILE_BIT 0x00000001
#endif

static int current_context_is(const int profile, const int major, const int minor) {
    const char *version = (const char *) p_glGetString(GL_VERSION);
    if (!version) return 0;

    const int is_es = strncmp(version, "OpenGL ES", 9) == 0;
    if (is_es != (profile == SDL_GL_CONTEXT_PROFILE_ES)) return 0;

    const char *digits = version;
    while (*digits && (*digits < '0' || *digits > '9'))
        digits++;

    int have_major = 0, have_minor = 0;
    if (sscanf(digits, "%d.%d", &have_major, &have_minor) != 2) return 0;

    if (have_major != major) {
        if (have_major < major) return 0;
    } else if (have_minor < minor) {
        return 0;
    }

    if (profile == SDL_GL_CONTEXT_PROFILE_CORE) {
        if (have_major < 3 || (have_major == 3 && have_minor < 2)) return 0;

        GLint mask = 0;
        p_glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);

        return (mask & GL_CONTEXT_CORE_PROFILE_BIT) != 0;
    }

    return 1;
}

static int create_shared_context(const int profile, const int major, const int minor) {
    gl_window = display_get_window();
    if (!gl_window) return 0;

    sdl_ctx = SDL_GL_GetCurrentContext();
    if (!sdl_ctx) return 0;

    int was_share = 0, was_profile = 0, was_major = 0, was_minor = 0;
    SDL_GL_GetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, &was_share);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &was_profile);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &was_major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &was_minor);

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);

    core_ctx = SDL_GL_CreateContext(gl_window);

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, was_share);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, was_profile);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, was_major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, was_minor);

    if (!core_ctx) {
        LOG_WARN(
            mux_module, "hw_render: no shared %s %d.%d context (%s)", profile_name(profile), major, minor,
            SDL_GetError()
        );
        SDL_GL_MakeCurrent(gl_window, sdl_ctx);
        return 0;
    }

    /* SDL_GL_CreateContext leaves the new context current - the UI needs its own back */
    SDL_GL_MakeCurrent(gl_window, sdl_ctx);
    return 1;
}

static void destroy_shared_context(void) {
    if (!core_ctx) return;

    if (gl_window && sdl_ctx) SDL_GL_MakeCurrent(gl_window, sdl_ctx);

    SDL_GL_DeleteContext(core_ctx);
    core_ctx = NULL;
}

static int force_shared_context(void) {
    const char *value = getenv("MUX_RETRO_FORCE_SHARED_GL");
    return value && *value && *value != '0';
}

static int shared_context_usable(const int profile, const int major, const int minor) {
    SDL_GL_MakeCurrent(gl_window, core_ctx);

    if (!current_context_is(profile, major, minor)) {
        const char *got = (const char *) p_glGetString(GL_VERSION);
        LOG_WARN(
            mux_module, "hw_render: asked for %s %d.%d, driver gave '%s'", profile_name(profile), major, minor,
            got ? got : "unknown"
        );
        SDL_GL_MakeCurrent(gl_window, sdl_ctx);
        return 0;
    }

    GLuint probe = 0;
    p_glGenTextures(1, &probe);
    if (probe) {
        p_glBindTexture(GL_TEXTURE_2D, probe);
        p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        p_glBindTexture(GL_TEXTURE_2D, 0);
    }

    SDL_GL_MakeCurrent(gl_window, sdl_ctx);
    const int visible = probe != 0 && p_glIsTexture(probe);

    SDL_GL_MakeCurrent(gl_window, core_ctx);
    if (probe) p_glDeleteTextures(1, &probe);
    SDL_GL_MakeCurrent(gl_window, sdl_ctx);

    if (!visible) LOG_WARN(mux_module, "hw_render: %s context is not sharing objects", profile_name(profile));

    return visible;
}

static char backend_desc[64] = "";

int hw_render_bridge_owns_context(void) {
    return active && gl_funcs_ready && owns_context();
}

int hw_render_bridge_active(void) {
    return active;
}

const char *hw_render_bridge_description(void) {
    return active && backend_desc[0] ? backend_desc : NULL;
}

static int context_requirements(
    const struct retro_hw_render_callback *cb, int *profile, int *major, int *minor, const char **label
) {
    switch (cb->context_type) {
        case RETRO_HW_CONTEXT_OPENGLES2:
            *profile = SDL_GL_CONTEXT_PROFILE_ES;
            *major = 2;
            *minor = 0;
            *label = "GLES2";
            return 1;
        case RETRO_HW_CONTEXT_OPENGLES3:
            *profile = SDL_GL_CONTEXT_PROFILE_ES;
            *major = 3;
            *minor = 0;
            *label = "GLES3";
            return 1;
        case RETRO_HW_CONTEXT_OPENGLES_VERSION:
            *profile = SDL_GL_CONTEXT_PROFILE_ES;
            *major = cb->version_major > 0 ? (int) cb->version_major : 3;
            *minor = (int) cb->version_minor;
            *label = "GLES";
            return 1;
        case RETRO_HW_CONTEXT_OPENGL:
            *profile = SDL_GL_CONTEXT_PROFILE_COMPATIBILITY;
            *major = cb->version_major > 0 ? (int) cb->version_major : 2;
            *minor = (int) cb->version_minor;
            *label = "GL";
            return 1;
        case RETRO_HW_CONTEXT_OPENGL_CORE:
            *profile = SDL_GL_CONTEXT_PROFILE_CORE;
            *major = cb->version_major > 0 ? (int) cb->version_major : 3;
            *minor = (int) cb->version_minor;
            *label = "GL core";
            return 1;
        default:
            return 0;
    }
}

int hw_render_bridge_negotiate(struct retro_hw_render_callback *cb) {
    if (!cb) return 0;

    int profile = 0, major = 0, minor = 0;
    const char *label = NULL;

    if (!context_requirements(cb, &profile, &major, &minor, &label)) {
        LOG_WARN(
            mux_module, "hw_render: core requested context type %d, which has no GL mapping", (int) cb->context_type
        );
        return 0;
    }

    if (!load_gl_functions()) return 0;

    const int ui_context_compatible = current_context_is(profile, major, minor);
    const int forced_shared = force_shared_context();
    if (forced_shared)
        LOG_WARN(mux_module, "hw_render: MUX_RETRO_FORCE_SHARED_GL is set - skipping the dedicated context");

    const int dedicated_context_ready =
        !forced_shared && create_shared_context(profile, major, minor) && shared_context_usable(profile, major, minor);

    if (dedicated_context_ready) {
        LOG_INFO(mux_module, "hw_render: using a dedicated shared-object context for %s %d.%d", label, major, minor);
    } else {
        destroy_shared_context();

        if (ui_context_compatible) {
            LOG_WARN(
                mux_module,
                "hw_render: dedicated %s %d.%d context unavailable - falling back to the UI's shared context", label,
                major, minor
            );
        } else {
            LOG_WARN(
                mux_module, "hw_render: cannot provide %s %d.%d - core will fall back to software rendering", label,
                major, minor
            );
            return 0;
        }
    }

    core_context_reset = cb->context_reset;
    core_context_destroy = cb->context_destroy;
    want_depth = cb->depth;
    want_stencil = cb->stencil;

    flip_needed = cb->bottom_left_origin;

    cb->get_current_framebuffer = hw_render_bridge_get_current_framebuffer;
    cb->get_proc_address = hw_render_bridge_get_proc_address;

    active = 1;
    snprintf(
        backend_desc, sizeof(backend_desc), "%s %d.%d (%s)", label, major, minor,
        owns_context() ? "dedicated" : "shared"
    );

    LOG_INFO(
        mux_module, "hw_render: accepted %s request as %s (depth=%d stencil=%d bottom_left_origin=%d)", label,
        backend_desc, want_depth, want_stencil, cb->bottom_left_origin
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
    for (int i = 0; i < HW_TARGET_COUNT_MAX; i++) {
        if (fbo[i]) {
            p_glDeleteFramebuffers(1, &fbo[i]);
            fbo[i] = 0;
        }
        if (colour_tex[i]) {
            p_glDeleteTextures(1, &colour_tex[i]);
            colour_tex[i] = 0;
        }
    }

    if (depth_stencil_rb) {
        p_glDeleteRenderbuffers(1, &depth_stencil_rb);
        depth_stencil_rb = 0;
    }

    render_index = 0;
    display_index = 0;
    target_queried = 0;
    handed_index = 0;
    target_count = 1;
    target_w = 0;
    target_h = 0;
}

void hw_render_bridge_configure(const unsigned max_width, const unsigned max_height) {
    if (!active || max_width == 0 || max_height == 0) return;
    if ((int) max_width == target_w && (int) max_height == target_h) return;

    const int first_time = fbo[0] == 0;
    hw_render_bridge_context_save();

    if (!first_time && context_ready && core_context_destroy) core_context_destroy();
    destroy_target();

    const size_t pixels = (size_t) max_width * (size_t) max_height;
    const size_t colour_target_bytes = pixels * 4u;
    target_count = colour_target_bytes <= HW_DOUBLE_BUFFER_MAX_EXTRA_BYTES ? 2 : 1;

    if (want_depth) {
        p_glGenRenderbuffers(1, &depth_stencil_rb);
        p_glBindRenderbuffer(GL_RENDERBUFFER, depth_stencil_rb);
        p_glRenderbufferStorage(
            GL_RENDERBUFFER, want_stencil ? GL_DEPTH24_STENCIL8_OES : GL_DEPTH_COMPONENT16, (GLsizei) max_width,
            (GLsizei) max_height
        );
    }

    GLint sample_buffers = 0;
    GLint samples = 0;
    for (int i = 0; i < target_count; i++) {
        p_glGenTextures(1, &colour_tex[i]);
        p_glBindTexture(GL_TEXTURE_2D, colour_tex[i]);
        p_glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei) max_width, (GLsizei) max_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL
        );
        const GLint filter = target_filter_param();
        p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        p_glBindTexture(GL_TEXTURE_2D, 0);

        p_glGenFramebuffers(1, &fbo[i]);
        p_glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colour_tex[i], 0);

        if (want_depth) {
            p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_stencil_rb);
            if (want_stencil)
                p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_stencil_rb);
        }

        const GLenum status = p_glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            if (i > 0) {
                LOG_WARN(
                    mux_module, "hw_render: secondary framebuffer incomplete (status 0x%x) - using a single target",
                    status
                );
                p_glDeleteFramebuffers(1, &fbo[i]);
                fbo[i] = 0;
                p_glDeleteTextures(1, &colour_tex[i]);
                colour_tex[i] = 0;
                target_count = 1;
                break;
            }

            LOG_ERROR(
                mux_module, "hw_render: framebuffer incomplete (status 0x%x) - disabling hardware render", status
            );
            p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
            destroy_target();
            hw_render_bridge_context_restore();
            active = 0;
            return;
        }

        if (i == 0) {
            p_glGetIntegerv(GL_SAMPLE_BUFFERS, &sample_buffers);
            p_glGetIntegerv(GL_SAMPLES, &samples);
        }

        p_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        p_glClear(
            GL_COLOR_BUFFER_BIT | (want_depth ? GL_DEPTH_BUFFER_BIT : 0) | (want_stencil ? GL_STENCIL_BUFFER_BIT : 0)
        );
    }

    p_glBindFramebuffer(GL_FRAMEBUFFER, 0);

    target_w = (int) max_width;
    target_h = (int) max_height;
    frame_valid_w = max_width;
    frame_valid_h = max_height;

    ensure_program();

    const size_t depth_bytes = want_depth ? pixels * (want_stencil ? 4u : 2u) : 0u;
    const size_t target_bytes = colour_target_bytes * (size_t) target_count + depth_bytes;
    LOG_INFO(
        mux_module,
        "hw_render: target %ux%u (buffers=%d depth=%d stencil=%d filter=%s sample_buffers=%d samples=%d, %.1f MiB)",
        max_width, max_height, target_count, want_depth, want_stencil,
        target_filter_param() == GL_LINEAR ? "linear" : "nearest", sample_buffers, samples,
        (double) target_bytes / (1024.0 * 1024.0)
    );

    // Tell the core to rebuild against the new framebuffer, not just the first time we make one!
    context_ready = 1;
    if (core_context_reset) {
        governor_boost_begin("GL context reset");
        const Uint64 reset_started = SDL_GetPerformanceCounter();
        core_context_reset();
        const Uint64 reset_finished = SDL_GetPerformanceCounter();
        const Uint64 frequency = SDL_GetPerformanceFrequency();
        const double reset_ms =
            frequency > 0 ? (double) (reset_finished - reset_started) * 1000.0 / (double) frequency : 0.0;
        LOG_INFO(mux_module, "hw_render: core context reset took %.2f ms", reset_ms);
        governor_boost_end();
    }

    hw_render_bridge_context_restore();
}

void hw_render_bridge_apply_filter(void) {
    if (!active || !gl_funcs_ready || !colour_tex[0]) return;

    hw_render_bridge_enter_core_call();

    GLint previous = 0;
    p_glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    const GLint filter = target_filter_param();
    for (int i = 0; i < target_count; i++) {
        if (!colour_tex[i]) continue;
        p_glBindTexture(GL_TEXTURE_2D, colour_tex[i]);
        p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    }
    p_glBindTexture(GL_TEXTURE_2D, (GLuint) previous);

    hw_render_bridge_exit_core_call();
    LOG_INFO(mux_module, "hw_render: sampling changed to %s", filter == GL_LINEAR ? "linear" : "nearest");
}

uintptr_t hw_render_bridge_get_current_framebuffer(void) {
    target_queried = 1;
    handed_index = render_index;
    return fbo[render_index];
}

retro_proc_address_t hw_render_bridge_get_proc_address(const char *sym) {
    return SDL_GL_GetProcAddress(sym);
}

void hw_render_bridge_notify_frame(const unsigned width, const unsigned height) {
    if (width == 0 || height == 0) return;

    if (target_count > 1 && !target_queried) {
        LOG_WARN(mux_module, "hw_render: core reuses a cached framebuffer; pinning to a single render target!");
        target_count = 1;
        render_index = handed_index;
    }

    display_index = render_index;
    if (target_count > 1) render_index = (render_index + 1) % target_count;

    target_queried = 0;

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
    GLboolean colour_mask[4];
    GLboolean depth_mask;
    GLfloat clear_colour[4];
    GLint front_face;
    GLint cull_face_mode;
    GLint unpack_alignment;
    GLint active_texture;
    GLint tex_binding[HW_TEX_UNITS];
    GLint array_buffer;
    GLint element_array_buffer;
    GLint framebuffer;
    GLint read_framebuffer;
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

    p_glGetBooleanv(GL_COLOR_WRITEMASK, s->colour_mask);
    p_glGetBooleanv(GL_DEPTH_WRITEMASK, &s->depth_mask);
    p_glGetFloatv(GL_COLOR_CLEAR_VALUE, s->clear_colour);

    p_glGetIntegerv(GL_FRONT_FACE, &s->front_face);
    p_glGetIntegerv(GL_CULL_FACE_MODE, &s->cull_face_mode);
    p_glGetIntegerv(GL_UNPACK_ALIGNMENT, &s->unpack_alignment);

    p_glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s->array_buffer);
    p_glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &s->element_array_buffer);

    p_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s->framebuffer);
    s->read_framebuffer = s->framebuffer;
    if (es3_available) p_glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s->read_framebuffer);

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

    p_glColorMask(s->colour_mask[0], s->colour_mask[1], s->colour_mask[2], s->colour_mask[3]);
    p_glDepthMask(s->depth_mask);
    p_glClearColor(s->clear_colour[0], s->clear_colour[1], s->clear_colour[2], s->clear_colour[3]);

    p_glFrontFace((GLenum) s->front_face);
    p_glCullFace((GLenum) s->cull_face_mode);
    p_glPixelStorei(GL_UNPACK_ALIGNMENT, s->unpack_alignment);

    p_glBindBuffer(GL_ARRAY_BUFFER, (GLuint) s->array_buffer);
    p_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint) s->element_array_buffer);

    if (es3_available && s->read_framebuffer != s->framebuffer) {
        p_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint) s->framebuffer);
        p_glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint) s->read_framebuffer);
    } else {
        p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) s->framebuffer);
    }

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

static void enter_core_gl(void) {
    const uint64_t start = perf_begin();

    if (owns_context()) {
        SDL_GL_MakeCurrent(gl_window, core_ctx);
    } else {
        gl_state_capture(&sdl_state);
        gl_state_apply(&core_state);
    }

    perf_end(perf_stage_gl_enter, start);
}

static void gl_reset_es3_state(void) {
    if (p_glBindVertexArray) p_glBindVertexArray(0);

    if (p_glBindSampler) {
        for (int i = 0; i < HW_TEX_UNITS; i++)
            p_glBindSampler((GLuint) i, 0);
    }

    if (p_glBindTransformFeedback) p_glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);

    p_glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    p_glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    p_glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

static void leave_core_gl(void) {
    const uint64_t start = perf_begin();

    if (owns_context()) {
        SDL_GL_MakeCurrent(gl_window, sdl_ctx);
    } else if (sdl_state.valid) {
        gl_state_capture(&core_state);
        gl_reset_es3_state();
        gl_state_apply(&sdl_state);
    }

    perf_end(perf_stage_gl_leave, start);
}

void hw_render_bridge_flush_core_commands(void) {
    if (!active || !gl_funcs_ready) return;
    p_glFlush();
}

void hw_render_bridge_context_save(void) {
    if (!active || !gl_funcs_ready) return;

    const uint64_t start = perf_begin();

    if (owns_context()) {
        SDL_GL_MakeCurrent(gl_window, core_ctx);
    } else {
        gl_state_capture(&sdl_state);
    }

    perf_end(perf_stage_gl_enter, start);
}

void hw_render_bridge_context_restore(void) {
    if (!active || !gl_funcs_ready) return;
    if (!owns_context() && !sdl_state.valid) return;

    leave_core_gl();
}

void hw_render_bridge_enter_core_call(void) {
    if (!active || !gl_funcs_ready) return;
    enter_core_gl();
}

void hw_render_bridge_exit_core_call(void) {
    if (!active || !gl_funcs_ready) return;
    if (!owns_context() && !sdl_state.valid) return;

    leave_core_gl();
}

static void capture_quad_restore_state(quad_restore_state_t *state) {
    p_glGetIntegerv(GL_CURRENT_PROGRAM, &state->program);
    p_glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state->array_buffer);
    p_glGetIntegerv(GL_ACTIVE_TEXTURE, &state->active_texture);

    p_glActiveTexture(GL_TEXTURE0);
    p_glGetIntegerv(GL_TEXTURE_BINDING_2D, &state->texture0);

    state->blend_enabled = p_glIsEnabled(GL_BLEND);
    state->scissor_enabled = p_glIsEnabled(GL_SCISSOR_TEST);
    state->attrib_pos_enabled = 0;
    state->attrib_uv_enabled = 0;
    if (a_pos >= 0) p_glGetVertexAttribiv((GLuint) a_pos, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &state->attrib_pos_enabled);
    if (a_uv >= 0) p_glGetVertexAttribiv((GLuint) a_uv, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &state->attrib_uv_enabled);

    state->valid = 1;
}

static void restore_quad_state(const quad_restore_state_t *state) {
    if (a_pos >= 0 && !state->attrib_pos_enabled) p_glDisableVertexAttribArray((GLuint) a_pos);
    if (a_uv >= 0 && !state->attrib_uv_enabled) p_glDisableVertexAttribArray((GLuint) a_uv);

    p_glBindTexture(GL_TEXTURE_2D, (GLuint) state->texture0);
    p_glActiveTexture((GLenum) state->active_texture);
    p_glBindBuffer(GL_ARRAY_BUFFER, (GLuint) state->array_buffer);
    p_glUseProgram((GLuint) state->program);
    if (state->blend_enabled) p_glEnable(GL_BLEND);
    if (state->scissor_enabled) p_glEnable(GL_SCISSOR_TEST);
}

static void draw_hw_quad(
    const float l, const float r, const float t, const float b, const float u_min, const float u_max,
    const float v_at_top, const float v_at_bottom, const int vp_w, const int vp_h, const int swap_channels,
    const int cache_restore_state
) {
    const GLfloat verts[] = {
        l, t, u_min, v_at_top,    // top left
        r, t, u_max, v_at_top,    // top right
        l, b, u_min, v_at_bottom, // bottom left
        r, b, u_max, v_at_bottom, // bottom right
    };

    quad_restore_state_t frame_restore;
    quad_restore_state_t *restore = &frame_restore;

    if (cache_restore_state && cached_quad_restore.valid) {
        restore = &cached_quad_restore;
        p_glActiveTexture(GL_TEXTURE0);
    } else {
        capture_quad_restore_state(&frame_restore);
        if (cache_restore_state) {
            cached_quad_restore = frame_restore;
            restore = &cached_quad_restore;
        } else {
            cached_quad_restore.valid = 0;
        }
    }

    p_glDisable(GL_SCISSOR_TEST);
    p_glDisable(GL_BLEND);
    p_glViewport(0, 0, vp_w, vp_h);
    p_glUseProgram(prog);
    p_glBindTexture(GL_TEXTURE_2D, colour_tex[display_index]);
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

    restore_quad_state(restore);
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

void hw_render_bridge_draw(SDL_Renderer *renderer, const SDL_Rect *dest_rect, const SDL_Rect *src_rect) {
    if (!active || !context_ready || !colour_tex[display_index] || target_w == 0 || target_h == 0) return;
    if (!prog) return;

    SDL_RenderFlush(renderer);

    const float u_max = target_w > 0 ? (float) frame_valid_w / (float) target_w : 1.0f;
    const float v_max = target_h > 0 ? (float) frame_valid_h / (float) target_h : 1.0f;

    const float du = target_w > 0 ? 0.5f / (float) target_w : 0.0f;
    const float dv = target_h > 0 ? 0.5f / (float) target_h : 0.0f;

    if (colour_pass_needed() && ensure_filter_src(renderer)) {
        SDL_Texture *prev_target = SDL_GetRenderTarget(renderer);
        if (SDL_SetRenderTarget(renderer, filter_src_tex) == 0) {
            const float v_at_top = flip_needed ? dv : v_max - dv;
            const float v_at_bottom = flip_needed ? v_max - dv : dv;

            draw_hw_quad(
                -1.0f, 1.0f, -1.0f, 1.0f, du, u_max - du, v_at_top, v_at_bottom, filter_src_w, filter_src_h, 1, 0
            );
            SDL_SetRenderTarget(renderer, prev_target);

            colour_render_pass(renderer, filter_src_tex, src_rect, dest_rect);
            return;
        }
        SDL_SetRenderTarget(renderer, prev_target);
    }

    int out_w = 0, out_h = 0;
    Uint32 target_format = 0;
    SDL_Texture *cur_target = SDL_GetRenderTarget(renderer);
    if (cur_target) {
        SDL_QueryTexture(cur_target, &target_format, NULL, &out_w, &out_h);
    } else {
        if (cached_output_renderer != renderer || cached_output_w <= 0 || cached_output_h <= 0) {
            cached_output_renderer = renderer;
            if (SDL_GetRendererOutputSize(renderer, &cached_output_w, &cached_output_h) != 0) {
                cached_output_w = 0;
                cached_output_h = 0;
            }
        }
        out_w = cached_output_w;
        out_h = cached_output_h;
    }
    if (out_w <= 0 || out_h <= 0) return;

    const float ndc_left = ((float) dest_rect->x / (float) out_w) * 2.0f - 1.0f;
    const float ndc_right = ((float) (dest_rect->x + dest_rect->w) / (float) out_w) * 2.0f - 1.0f;

    float ndc_top = 1.0f - ((float) dest_rect->y / (float) out_h) * 2.0f;
    float ndc_bottom = 1.0f - ((float) (dest_rect->y + dest_rect->h) / (float) out_h) * 2.0f;

    if (cur_target) {
        ndc_top = -ndc_top;
        ndc_bottom = -ndc_bottom;
    }

    float u0 = 0.0f, u1 = u_max, v0 = 0.0f, v1 = v_max;
    if (src_rect) {
        u0 = (float) src_rect->x / (float) target_w;
        u1 = (float) (src_rect->x + src_rect->w) / (float) target_w;
        v0 = (float) src_rect->y / (float) target_h;
        v1 = (float) (src_rect->y + src_rect->h) / (float) target_h;
    }

    u0 += du;
    u1 -= du;
    v0 += dv;
    v1 -= dv;

    const float v_at_top = flip_needed ? v_max - v0 : v0;
    const float v_at_bottom = flip_needed ? v_max - v1 : v1;

    const int swap_target_channels = cur_target && target_format == SDL_PIXELFORMAT_ARGB8888;
    const int cache_restore_state =
        owns_context() && !cur_target && display_video_fast_path_allowed() && !overlay_bridge_active();
    draw_hw_quad(
        ndc_left, ndc_right, ndc_top, ndc_bottom, u0, u1, v_at_top, v_at_bottom, out_w, out_h, swap_target_channels,
        cache_restore_state
    );
}

void hw_render_bridge_shutdown(void) {
    if (!gl_funcs_ready) {
        destroy_shared_context();
        active = 0;
        context_ready = 0;
        return;
    }

    if (active && context_ready && core_context_destroy) {
        hw_render_bridge_enter_core_call();
        core_context_destroy();
        hw_render_bridge_exit_core_call();
    }

    if (active) enter_core_gl();

    destroy_target();

    if (prog) {
        p_glDeleteProgram(prog);
        prog = 0;
    }
    prog_ready = 0;
    a_pos = a_uv = u_tex = u_swap = -1;

    if (active) leave_core_gl();

    if (filter_src_tex) {
        SDL_DestroyTexture(filter_src_tex);
        filter_src_tex = NULL;
    }
    filter_src_w = 0;
    filter_src_h = 0;
    cached_output_renderer = NULL;
    cached_output_w = 0;
    cached_output_h = 0;
    cached_quad_restore.valid = 0;

    core_context_reset = NULL;
    core_context_destroy = NULL;

    destroy_shared_context();

    backend_desc[0] = '\0';
    active = 0;
    context_ready = 0;

    frame_valid_w = 0;
    frame_valid_h = 0;
}
