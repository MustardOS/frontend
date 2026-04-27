#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <stddef.h>
#include <pthread.h>
#include <dlfcn.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
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
#include "../common/shader.h"
#include "../overlay/base.h"
#include "../overlay/battery.h"
#include "../overlay/bright.h"
#include "../overlay/volume.h"
#include "../overlay/notif.h"

#define RENDER_PATHS 8

#define K_INJECT_FAIL_MAX  3
#define K_UNKNOWN_SWAP_MAX 8

#define DRM_FORMAT_ARGB8888       0x34325241
#define DRM_FORMAT_ABGR8888       0x34324241
#define DRM_MODE_OBJECT_PLANE     0xeeeeeeee
#define DRM_MODE_ATOMIC_TEST_ONLY 0x00000100

#define DRM_CLIENT_CAP_UNIVERSAL_PLANES 2
#define DRM_CLIENT_CAP_ATOMIC           3
#define DRM_PLANE_TYPE_OVERLAY          0

#ifndef EGL_LINUX_DMA_BUF_EXT
#define EGL_LINUX_DMA_BUF_EXT          0x3270
#define EGL_LINUX_DRM_FOURCC_EXT       0x3271
#define EGL_DMA_BUF_PLANE0_FD_EXT      0x3272
#define EGL_DMA_BUF_PLANE0_OFFSET_EXT  0x3273
#define EGL_DMA_BUF_PLANE0_PITCH_EXT   0x3274
#endif

typedef struct {
    int count_fbs;
    uint32_t *fbs;

    int count_crtcs;
    uint32_t *crtcs;

    int count_connectors;
    uint32_t *connectors;

    int count_encoders;
    uint32_t *encoders;

    uint32_t min_width, max_width;
    uint32_t min_height, max_height;
} drm_res_t;

typedef struct {
    uint32_t crtc_id;
    uint32_t buffer_id;
    uint32_t x, y;
    uint32_t width, height;
    int mode_valid;
} drm_crtc_t;

typedef struct {
    uint32_t count_planes;
    uint32_t *planes;
} drm_pres_t;

typedef struct {
    uint32_t count_formats;
    uint32_t *formats;
    uint32_t plane_id;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t crtc_x, crtc_y;
    uint32_t x, y;
    uint32_t possible_crtcs;
    uint32_t gamma_size;
} drm_plane_t;

typedef struct {
    uint32_t count_props;
    uint32_t *props;
    uint64_t *prop_values;
} drm_objprops_t;

typedef struct {
    uint32_t prop_id;
    uint32_t flags;
    char name[32];
} drm_prop_t;

typedef struct gbm_device gbm_device_t;
typedef struct gbm_bo gbm_bo_t;
typedef struct drm_atomic_req_opaque *drm_atomic_req_t;

typedef struct {
    GLfloat x, y, u, v;
} k_vtx_t;

typedef enum {
    PATH_UNKNOWN = 0,
    PATH_FBO0,
    PATH_PLANE,
    PATH_DISABLED,
} k_path_t;

typedef enum {
    RENDER_PATH_UNKNOWN = 0,
    RENDER_PATH_DEFAULT_FBO,
    RENDER_PATH_OFFSCREEN_FBO,
} render_path_t;

static EGLBoolean (*real_eglSwapBuffers)(EGLDisplay, EGLSurface) = NULL;

static int (*real_drmModeAtomicCommit)(int, void *, uint32_t, void *) = NULL;

static int (*real_drmModePageFlip)(int, uint32_t, uint32_t, uint32_t, void *) = NULL;

static pthread_once_t hook_once = PTHREAD_ONCE_INIT;

static void resolve_hooks_once(void) {
    real_eglSwapBuffers = dlsym(RTLD_NEXT, "eglSwapBuffers");
    real_drmModeAtomicCommit = dlsym(RTLD_NEXT, "drmModeAtomicCommit");
    real_drmModePageFlip = dlsym(RTLD_NEXT, "drmModePageFlip");

    if (!real_eglSwapBuffers) LOG_ERROR("stage", "Failed to hook eglSwapBuffers");
    if (!real_drmModeAtomicCommit) LOG_WARN("stage", "drmModeAtomicCommit not found");
    if (!real_drmModePageFlip) LOG_WARN("stage", "drmModePageFlip not found");
}

static void resolve_hooks(void) {
    pthread_once(&hook_once, resolve_hooks_once);
}

static struct {
    int ready;
    void *h_drm;
    void *h_gbm;

    void *(*GetResources)(int);

    void (*FreeResources)(void *);

    void *(*GetCrtc)(int, uint32_t);

    void (*FreeCrtc)(void *);

    void *(*GetPlaneResources)(int);

    void (*FreePlaneResources)(void *);

    void *(*GetPlane)(int, uint32_t);

    void (*FreePlane)(void *);

    void *(*ObjectGetProperties)(int, uint32_t, uint32_t);

    void (*FreeObjectProperties)(void *);

    void *(*GetProperty)(int, uint32_t);

    void (*FreeProperty)(void *);

    int (*AddFB2)(int, uint32_t, uint32_t, uint32_t, const uint32_t *, const uint32_t *, const uint32_t *, uint32_t *, uint32_t);

    int (*RmFB)(int, uint32_t);

    int (*AtomicAddProperty)(void *, uint32_t, uint32_t, uint64_t);

    int (*SetClientCap)(int, uint64_t, uint64_t);

    gbm_device_t *(*gbm_create_device)(int);

    void (*gbm_device_destroy)(gbm_device_t *);

    gbm_bo_t *(*gbm_bo_create)(gbm_device_t *, uint32_t, uint32_t, uint32_t, uint32_t);

    void (*gbm_bo_destroy)(gbm_bo_t *);

    uint64_t (*gbm_bo_get_handle)(gbm_bo_t *);

    uint32_t (*gbm_bo_get_stride)(gbm_bo_t *);

    int (*gbm_bo_get_fd)(gbm_bo_t *);

    uint32_t (*gbm_bo_get_format)(gbm_bo_t *);
} dl;

static pthread_once_t dl_once = PTHREAD_ONCE_INIT;
static int dl_init_result = 0;

#define LOAD(handle, sym, name) do {               \
    *(void **) &dl.sym = dlsym(dl.handle, name);   \
    if (!dl.sym) {                                 \
        LOG_INFO("stage", "%s missing symbol: %s", \
                 #handle, name);                   \
        return 0;                                  \
    }                                              \
} while (0)

static int dl_load_all(void) {
    dl.h_drm = dlopen("libdrm.so.2", RTLD_LAZY | RTLD_LOCAL);
    if (!dl.h_drm) {
        LOG_INFO("stage", "libdrm.so.2 not present - plane path disabled");
        return 0;
    }

    dl.h_gbm = dlopen("libgbm.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!dl.h_gbm) {
        LOG_INFO("stage", "libgbm.so.1 not present - plane path disabled");
        dlclose(dl.h_drm);
        dl.h_drm = NULL;
        return 0;
    }

    LOAD(h_drm, GetResources, "drmModeGetResources");
    LOAD(h_drm, FreeResources, "drmModeFreeResources");
    LOAD(h_drm, GetCrtc, "drmModeGetCrtc");
    LOAD(h_drm, FreeCrtc, "drmModeFreeCrtc");
    LOAD(h_drm, GetPlaneResources, "drmModeGetPlaneResources");
    LOAD(h_drm, FreePlaneResources, "drmModeFreePlaneResources");
    LOAD(h_drm, GetPlane, "drmModeGetPlane");
    LOAD(h_drm, FreePlane, "drmModeFreePlane");
    LOAD(h_drm, ObjectGetProperties, "drmModeObjectGetProperties");
    LOAD(h_drm, FreeObjectProperties, "drmModeFreeObjectProperties");
    LOAD(h_drm, GetProperty, "drmModeGetProperty");
    LOAD(h_drm, FreeProperty, "drmModeFreeProperty");
    LOAD(h_drm, AddFB2, "drmModeAddFB2");
    LOAD(h_drm, RmFB, "drmModeRmFB");
    LOAD(h_drm, AtomicAddProperty, "drmModeAtomicAddProperty");
    LOAD(h_drm, SetClientCap, "drmSetClientCap");

    LOAD(h_gbm, gbm_create_device, "gbm_create_device");
    LOAD(h_gbm, gbm_device_destroy, "gbm_device_destroy");
    LOAD(h_gbm, gbm_bo_create, "gbm_bo_create");
    LOAD(h_gbm, gbm_bo_destroy, "gbm_bo_destroy");
    LOAD(h_gbm, gbm_bo_get_handle, "gbm_bo_get_handle");
    LOAD(h_gbm, gbm_bo_get_stride, "gbm_bo_get_stride");
    LOAD(h_gbm, gbm_bo_get_fd, "gbm_bo_get_fd");
    LOAD(h_gbm, gbm_bo_get_format, "gbm_bo_get_format");

    return 1;
}

static void dl_init_once(void) {
    dl_init_result = dl_load_all();
    dl.ready = dl_init_result;

    if (!dl_init_result) {
        if (dl.h_drm) {
            dlclose(dl.h_drm);
            dl.h_drm = NULL;
        }
        if (dl.h_gbm) {
            dlclose(dl.h_gbm);
            dl.h_gbm = NULL;
        }
        return;
    }

    LOG_INFO("stage", "KMS dynamic loader: libdrm + libgbm resolved");
}

static int dl_init(void) {
    pthread_once(&dl_once, dl_init_once);
    return dl_init_result;
}

static const char *k_vs_src =
        "attribute vec2 a_pos;"
        "attribute vec2 a_uv;"
        "varying vec2 v_uv;"
        "void main(){"
        "    gl_Position = vec4(a_pos, 0.0, 1.0);"
        "    v_uv = a_uv;"
        "}";

static const char *k_fs_overlay_src =
        "precision mediump float;"
        "uniform sampler2D u_tex;"
        "uniform float u_alpha;"
        "varying vec2 v_uv;"
        "void main(){"
        "    gl_FragColor = texture2D(u_tex, v_uv) * vec4(1.0, 1.0, 1.0, u_alpha);"
        "}";

static const char *k_fs_content_src =
        "precision mediump float;"
        "uniform sampler2D u_tex;"
        "uniform float u_brightness;"
        "uniform float u_contrast;"
        "uniform float u_saturation;"
        "uniform float u_cosH;"
        "uniform float u_sinH;"
        "uniform float u_gamma;"
        "uniform mat3 u_filter;"
        "uniform int u_filter_enabled;"
        "varying vec2 v_uv;"
        "vec3 apply_colour(vec3 c) {"
        "    c += u_brightness;"
        "    c = (c - 0.5) * u_contrast + 0.5;"
        "    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));"
        "    c = mix(vec3(l), c, u_saturation);"
        "    mat3 hueMat = mat3("
        "        0.299 + 0.701*u_cosH + 0.168*u_sinH, 0.587 - 0.587*u_cosH + 0.330*u_sinH, 0.114 - 0.114*u_cosH - 0.497*u_sinH,"
        "        0.299 - 0.299*u_cosH - 0.328*u_sinH, 0.587 + 0.413*u_cosH + 0.035*u_sinH, 0.114 - 0.114*u_cosH + 0.292*u_sinH,"
        "        0.299 - 0.300*u_cosH + 1.250*u_sinH, 0.587 - 0.588*u_cosH - 1.050*u_sinH, 0.114 + 0.886*u_cosH - 0.203*u_sinH"
        "    );"
        "    c = hueMat * c;"
        "    c = pow(max(c, vec3(0.0)), vec3(1.0 / u_gamma));"
        "    if (u_filter_enabled == 1) c = u_filter * c;"
        "    return c;"
        "}"
        "void main(){"
        "    vec4 t = texture2D(u_tex, v_uv);"
        "    vec3 rgb = apply_colour(t.rgb);"
        "    gl_FragColor = vec4(rgb, t.a);"
        "}";

static GLuint k_prog_overlay = 0;
static GLint k_overlay_a_pos = -1;
static GLint k_overlay_a_uv = -1;
static GLint k_overlay_u_tex = -1;
static GLint k_overlay_u_alpha = -1;

static GLuint k_prog_content = 0;
static GLint k_content_a_pos = -1;
static GLint k_content_a_uv = -1;
static GLint k_content_u_tex = -1;
static GLint k_content_u_brightness = -1;
static GLint k_content_u_contrast = -1;
static GLint k_content_u_saturation = -1;
static GLint k_content_u_cosH = -1;
static GLint k_content_u_sinH = -1;
static GLint k_content_u_gamma = -1;
static GLint k_content_u_filter = -1;
static GLint k_content_u_filter_enabled = -1;

static int k_prog_overlay_attempted = 0;
static int k_prog_overlay_ready = 0;
static int k_prog_content_attempted = 0;
static int k_prog_content_ready = 0;

static GLuint k_current_program = 0;
static int k_max_attribs = 0;

static const k_vtx_t k_fullscreen_vtx[4] = {
        {-1.0f, 1.0f,  0.0f, 1.0f},
        {-1.0f, -1.0f, 0.0f, 0.0f},
        {1.0f,  1.0f,  1.0f, 1.0f},
        {1.0f,  -1.0f, 1.0f, 0.0f},
};

typedef struct {
    GLint program, active_tex, tex_binding;
    GLint framebuffer, viewport[4];
    GLboolean blend;
    GLint blend_src, blend_dst, blend_eq;
    GLboolean scissor_test;
    GLint scissor_box[4];
    GLboolean depth_test;
    GLint array_buffer;
    int max_attribs;
    struct {
        GLint enabled, size, type, normal, stride, buffer;
        GLvoid *pointer;
    } attrib[16];
} k_gl_state_t;

static GLuint k_compile_shader(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);

    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (ok != GL_TRUE) {
        char log_buffer[512];
        GLsizei n = 0;

        glGetShaderInfoLog(sh, (GLsizei) sizeof(log_buffer), &n, log_buffer);
        LOG_ERROR("stage", "[kms] shader compile failed: %.*s", (int) n, log_buffer);

        glDeleteShader(sh);
        return 0;
    }

    return sh;
}

static GLuint k_link_program(GLuint vs, GLuint fs, int is_overlay) {
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
        LOG_ERROR("stage", "[kms] program link failed: %.*s", (int) n, log_buffer);

        glDeleteProgram(p);
        return 0;
    }

    return p;
}

static void k_use_program(GLuint p) {
    if (k_current_program != p) {
        glUseProgram(p);
        k_current_program = p;
    }
}

static void k_ensure_overlay_program(void) {
    if (k_prog_overlay_ready || k_prog_overlay_attempted) return;
    k_prog_overlay_attempted = 1;

    GLuint vs = k_compile_shader(GL_VERTEX_SHADER, k_vs_src);
    GLuint fs = k_compile_shader(GL_FRAGMENT_SHADER, k_fs_overlay_src);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }

    k_prog_overlay = k_link_program(vs, fs, 1);

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!k_prog_overlay) return;

    k_overlay_a_pos = glGetAttribLocation(k_prog_overlay, "a_pos");
    k_overlay_a_uv = glGetAttribLocation(k_prog_overlay, "a_uv");
    k_overlay_u_tex = glGetUniformLocation(k_prog_overlay, "u_tex");
    k_overlay_u_alpha = glGetUniformLocation(k_prog_overlay, "u_alpha");

    if (k_overlay_a_pos < 0 || k_overlay_a_uv < 0 || k_overlay_u_tex < 0) {
        glDeleteProgram(k_prog_overlay);
        k_prog_overlay = 0;
        return;
    }

    k_prog_overlay_ready = 1;
}

static void k_ensure_content_program(void) {
    if (k_prog_content_ready || k_prog_content_attempted) return;
    k_prog_content_attempted = 1;

    GLuint vs = k_compile_shader(GL_VERTEX_SHADER, k_vs_src);
    GLuint fs = k_compile_shader(GL_FRAGMENT_SHADER, k_fs_content_src);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }

    k_prog_content = k_link_program(vs, fs, 0);

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!k_prog_content) return;

    k_content_a_pos = glGetAttribLocation(k_prog_content, "a_pos");
    k_content_a_uv = glGetAttribLocation(k_prog_content, "a_uv");

    if (k_content_a_pos < 0 || k_content_a_uv < 0) {
        glDeleteProgram(k_prog_content);
        k_prog_content = 0;
        return;
    }

    k_content_u_tex = glGetUniformLocation(k_prog_content, "u_tex");
    k_content_u_brightness = glGetUniformLocation(k_prog_content, "u_brightness");
    k_content_u_contrast = glGetUniformLocation(k_prog_content, "u_contrast");
    k_content_u_saturation = glGetUniformLocation(k_prog_content, "u_saturation");
    k_content_u_cosH = glGetUniformLocation(k_prog_content, "u_cosH");
    k_content_u_sinH = glGetUniformLocation(k_prog_content, "u_sinH");
    k_content_u_gamma = glGetUniformLocation(k_prog_content, "u_gamma");
    k_content_u_filter = glGetUniformLocation(k_prog_content, "u_filter");
    k_content_u_filter_enabled = glGetUniformLocation(k_prog_content, "u_filter_enabled");

    k_prog_content_ready = 1;
}

static void k_save_state(k_gl_state_t *st) {
    glGetIntegerv(GL_CURRENT_PROGRAM, &st->program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &st->active_tex);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &st->tex_binding);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &st->framebuffer);
    glGetIntegerv(GL_VIEWPORT, st->viewport);

    st->blend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC_RGB, &st->blend_src);
    glGetIntegerv(GL_BLEND_DST_RGB, &st->blend_dst);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &st->blend_eq);

    st->scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, st->scissor_box);

    st->depth_test = glIsEnabled(GL_DEPTH_TEST);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &st->array_buffer);

    if (!k_max_attribs) {
        GLint m = 0;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &m);
        if (m > 16) m = 16;
        k_max_attribs = (int) m;
    }

    st->max_attribs = k_max_attribs;

    for (int i = 0; i < st->max_attribs; i++) {
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &st->attrib[i].enabled);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &st->attrib[i].size);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, &st->attrib[i].type);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &st->attrib[i].normal);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &st->attrib[i].stride);
        glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &st->attrib[i].buffer);
        glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &st->attrib[i].pointer);
    }
}

static void k_restore_state(const k_gl_state_t *st) {
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) st->framebuffer);
    glViewport(st->viewport[0], st->viewport[1], st->viewport[2], st->viewport[3]);

    if (st->depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (st->scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);

    glScissor(st->scissor_box[0], st->scissor_box[1], st->scissor_box[2], st->scissor_box[3]);

    if (st->blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glBlendEquation(st->blend_eq);
    glBlendFunc(st->blend_src, st->blend_dst);

    glUseProgram(st->program);
    glActiveTexture(st->active_tex);
    glBindTexture(GL_TEXTURE_2D, (GLuint) st->tex_binding);

    for (int i = 0; i < st->max_attribs; i++) {
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint) st->attrib[i].buffer);

        if (st->attrib[i].size > 0) {
            glVertexAttribPointer((GLuint) i, st->attrib[i].size, st->attrib[i].type,
                                  (GLboolean) st->attrib[i].normal, st->attrib[i].stride,
                                  st->attrib[i].pointer);
        }

        if (st->attrib[i].enabled) {
            glEnableVertexAttribArray((GLuint) i);
        } else {
            glDisableVertexAttribArray((GLuint) i);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, (GLuint) st->array_buffer);
}

static void k_rotate_uv(k_vtx_t out[4], int rot) {
    const float u0 = 0.0f, v0 = 0.0f;
    const float u1 = 0.0f, v1 = 1.0f;
    const float u2 = 1.0f, v2 = 0.0f;
    const float u3 = 1.0f, v3 = 1.0f;

    switch (rot) {
        case ROTATE_90:
            out[0].u = u1;
            out[0].v = v1;
            out[1].u = u3;
            out[1].v = v3;
            out[2].u = u0;
            out[2].v = v0;
            out[3].u = u2;
            out[3].v = v2;
            break;

        case ROTATE_180:
            out[0].u = u3;
            out[0].v = v3;
            out[1].u = u2;
            out[1].v = v2;
            out[2].u = u1;
            out[2].v = v1;
            out[3].u = u0;
            out[3].v = v0;
            break;

        case ROTATE_270:
            out[0].u = u2;
            out[0].v = v2;
            out[1].u = u0;
            out[1].v = v0;
            out[2].u = u3;
            out[2].v = v3;
            out[3].u = u1;
            out[3].v = v1;
            break;

        default:
            out[0].u = u0;
            out[0].v = v0;
            out[1].u = u1;
            out[1].v = v1;
            out[2].u = u2;
            out[2].v = v2;
            out[3].u = u3;
            out[3].v = v3;
            break;
    }
}

static void k_build_fullscreen_quad(k_vtx_t out[4], int rot) {
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
            out[0].u = 0;
            out[0].v = 0;
            out[1].u = 1;
            out[1].v = 0;
            out[2].u = 0;
            out[2].v = 1;
            out[3].u = 1;
            out[3].v = 1;
            break;

        case ROTATE_180:
            out[0].u = 1;
            out[0].v = 0;
            out[1].u = 1;
            out[1].v = 1;
            out[2].u = 0;
            out[2].v = 0;
            out[3].u = 0;
            out[3].v = 1;
            break;

        case ROTATE_270:
            out[0].u = 1;
            out[0].v = 1;
            out[1].u = 0;
            out[1].v = 1;
            out[2].u = 1;
            out[2].v = 0;
            out[3].u = 0;
            out[3].v = 0;
            break;

        default:
            out[0].u = 0;
            out[0].v = 1;
            out[1].u = 0;
            out[1].v = 0;
            out[2].u = 1;
            out[2].v = 1;
            out[3].u = 1;
            out[3].v = 0;
            break;
    }
}

static void k_build_quad_ndc(k_vtx_t out[4], int tex_w, int tex_h, int fb_w, int fb_h, int anchor, int scale) {
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

    const float w_ndc = draw_w * (2.0f / (float) fb_w);
    const float h_ndc = draw_h * (2.0f / (float) fb_h);

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

    k_rotate_uv(out, rot);
}

static void k_destroy_target(GLuint *tex, GLuint *fbo, int *w, int *h) {
    if (*tex) {
        glDeleteTextures(1, tex);
        *tex = 0;
    }

    if (*fbo) {
        glDeleteFramebuffers(1, fbo);
        *fbo = 0;
    }

    *w = 0;
    *h = 0;
}

static int k_ensure_target(GLuint *tex, GLuint *fbo, int *cw, int *ch, int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (*tex && w == *cw && h == *ch) return 1;

    k_destroy_target(tex, fbo, cw, ch);

    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        k_destroy_target(tex, fbo, cw, ch);
        return 0;
    }

    *cw = w;
    *ch = h;

    return 1;
}

static void k_draw_quad_overlay(GLuint tex, const k_vtx_t vtx[4], float alpha) {
    if (!k_prog_overlay_ready || !tex) return;

    k_use_program(k_prog_overlay);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    if (k_overlay_u_tex >= 0) glUniform1i(k_overlay_u_tex, 0);
    if (k_overlay_u_alpha >= 0) glUniform1f(k_overlay_u_alpha, alpha);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray((GLuint) k_overlay_a_pos);
    glEnableVertexAttribArray((GLuint) k_overlay_a_uv);

    glVertexAttribPointer((GLuint) k_overlay_a_pos, 2, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(k_vtx_t), vtx);
    glVertexAttribPointer((GLuint) k_overlay_a_uv, 2, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(k_vtx_t), (const char *) vtx + offsetof(k_vtx_t, u));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray((GLuint) k_overlay_a_pos);
    glDisableVertexAttribArray((GLuint) k_overlay_a_uv);
}

static void k_draw_quad_content(GLuint tex, const k_vtx_t vtx[4]) {
    if (!k_prog_content_ready || !tex) return;

    k_use_program(k_prog_content);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    if (k_content_u_tex >= 0) glUniform1i(k_content_u_tex, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray((GLuint) k_content_a_pos);
    glEnableVertexAttribArray((GLuint) k_content_a_uv);

    glVertexAttribPointer((GLuint) k_content_a_pos, 2, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(k_vtx_t), vtx);
    glVertexAttribPointer((GLuint) k_content_a_uv, 2, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(k_vtx_t), (const char *) vtx + offsetof(k_vtx_t, u));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray((GLuint) k_content_a_pos);
    glDisableVertexAttribArray((GLuint) k_content_a_uv);
}

static const float k_fullscreen_quad[4][4] = {
        {-1.0f, 1.0f,  0.0f, 1.0f},
        {-1.0f, -1.0f, 0.0f, 0.0f},
        {1.0f,  1.0f,  1.0f, 1.0f},
        {1.0f,  -1.0f, 1.0f, 0.0f},
};

static void k_draw_quad_shader(const shader_prog_t *shader, GLuint tex, int fb_w, int fb_h, int frame, GLint dst_fbo) {
    if (!shader || !shader->program || !tex) return;

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) dst_fbo);
    glViewport(0, 0, fb_w, fb_h);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);

    k_use_program(shader->program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    if (shader->u_tex >= 0) glUniform1i(shader->u_tex, 0);
    if (shader->u_resolution >= 0) glUniform2f(shader->u_resolution, (float) fb_w, (float) fb_h);
    if (shader->u_time >= 0) glUniform1f(shader->u_time, (float) frame);
    if (shader->u_frame >= 0) glUniform1i(shader->u_frame, frame);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, &k_fullscreen_quad[0][0]);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, &k_fullscreen_quad[0][2]);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

typedef struct {
    GLuint tex;
    int w, h;
    char path[1024];
} k_layer_t;

static k_layer_t k_layer_base = {0};
static k_layer_t k_layer_battery[INDICATOR_STEPS];
static k_layer_t k_layer_bright[INDICATOR_STEPS];
static k_layer_t k_layer_volume[INDICATOR_STEPS];

static k_vtx_t k_vtx_base[4];
static k_vtx_t k_vtx_battery[4];
static k_vtx_t k_vtx_bright[4];
static k_vtx_t k_vtx_volume[4];

static int k_vtx_base_valid = 0;
static int k_vtx_battery_valid = 0;
static int k_vtx_bright_valid = 0;
static int k_vtx_volume_valid = 0;

static int k_base_anchor_cached = -1;
static int k_base_scale_cached = -1;
static int k_battery_anchor_cached = -1;
static int k_battery_scale_cached = -1;
static int k_bright_anchor_cached = -1;
static int k_bright_scale_cached = -1;
static int k_volume_anchor_cached = -1;
static int k_volume_scale_cached = -1;

static int k_base_nop_last = -1;

static char k_dim[32];
static int k_dim_w = 0;
static int k_dim_h = 0;

static int k_loader_valid = 0;
static time_t k_loader_mtime = 0;
static char k_base_resolved_path[PATH_MAX];
static int k_base_resolved = 0;

static int k_load_png_to_layer(const char *path, k_layer_t *layer) {
    if (!path || !path[0]) return 0;
    if (layer->tex && strcmp(layer->path, path) == 0) return 1;

    SDL_Surface *raw = IMG_Load(path);
    if (!raw) {
        LOG_INFO("stage", "[kms] failed to load overlay PNG: %s", path);
        return 0;
    }

    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);
    if (!rgba) return 0;

    if (layer->tex) glDeleteTextures(1, &layer->tex);

    glGenTextures(1, &layer->tex);
    glBindTexture(GL_TEXTURE_2D, layer->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    SDL_LockSurface(rgba);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
    SDL_UnlockSurface(rgba);

    layer->w = rgba->w;
    layer->h = rgba->h;
    snprintf(layer->path, sizeof(layer->path), "%s", path);

    SDL_FreeSurface(rgba);
    return 1;
}

static void k_destroy_layer(k_layer_t *l) {
    if (l->tex) glDeleteTextures(1, &l->tex);
    memset(l, 0, sizeof(*l));
}

static void k_update_dimension(int fb_w, int fb_h) {
    if (fb_w < 1 || fb_h < 1) {
        k_dim[0] = '\0';
        return;
    }

    if (fb_w == k_dim_w && fb_h == k_dim_h && k_dim[0]) return;

    k_dim_w = fb_w;
    k_dim_h = fb_h;
    snprintf(k_dim, sizeof(k_dim), "%dx%d/", fb_w, fb_h);

    k_base_resolved = 0;
    k_base_resolved_path[0] = '\0';

    k_vtx_base_valid = 0;
    k_vtx_battery_valid = 0;
    k_vtx_bright_valid = 0;
    k_vtx_volume_valid = 0;
}

static int k_read_overlay_loader(void) {
    struct stat st;

    if (stat(OVERLAY_LOADER, &st) != 0) {
        k_loader_valid = 0;
        k_loader_mtime = 0;
        return 0;
    }

    if (k_loader_valid && st.st_mtime == k_loader_mtime) return 1;

    k_loader_valid = 0;
    k_loader_mtime = st.st_mtime;

    if (!read_line_from_file(OVERLAY_LOADER, 1, ovl_go_cache.content, sizeof(ovl_go_cache.content))) return 0;
    if (!read_line_from_file(OVERLAY_LOADER, 2, ovl_go_cache.system, sizeof(ovl_go_cache.system))) return 0;
    if (!read_line_from_file(OVERLAY_LOADER, 3, ovl_go_cache.core, sizeof(ovl_go_cache.core))) return 0;

    ovl_go_cache.valid = 1;
    ovl_go_cache.mtime = st.st_mtime;
    k_loader_valid = 1;

    return 1;
}

static const char *k_resolve_base_path(int fb_w, int fb_h) {
    k_update_dimension(fb_w, fb_h);
    if (k_base_resolved) return k_base_resolved_path;
    if (!k_read_overlay_loader()) return NULL;

    k_base_resolved_path[0] = '\0';

    if (!load_stage_image("base", ovl_go_cache.core, ovl_go_cache.system, ovl_go_cache.content, k_dim, k_base_resolved_path)) {
        base_overlay_disabled_cached = 1;
        k_base_resolved = 1;

        return NULL;
    }

    LOG_SUCCESS("stage", "[kms] Overlay loaded: %s", k_base_resolved_path);
    k_base_resolved = 1;

    return k_base_resolved_path;
}

static int k_resolve_indicator_path(const char *type, int step, char *out, size_t out_sz) {
    char name[64];

    if (!out || out_sz == 0) return 0;
    out[0] = '\0';

    if (step < 0) step = 0;
    if (step >= INDICATOR_STEPS) step = INDICATOR_STEPS - 1;

    if (!k_read_overlay_loader()) return 0;

    snprintf(name, sizeof(name), "%s_%d", type, step);
    return load_stage_image(type, ovl_go_cache.core, ovl_go_cache.system, name, k_dim, out);
}

static render_path_t k_render_path = RENDER_PATH_UNKNOWN;
static GLint k_render_nonzero_fbo = 0;
static int k_render_swap_calls = 0;
static int k_render_nonzero_count = 0;
static int k_render_zero_count = 0;
static GLint k_render_fbo_ring[RENDER_PATHS];
static int k_render_fbo_ring_head = 0;

static void k_reset_render_path(void) {
    k_render_path = RENDER_PATH_UNKNOWN;
    k_render_nonzero_fbo = 0;
    k_render_swap_calls = 0;
    k_render_nonzero_count = 0;
    k_render_zero_count = 0;
    k_render_fbo_ring_head = 0;

    memset(k_render_fbo_ring, 0, sizeof(k_render_fbo_ring));
}

static void k_detect_render_path(GLint app_fbo) {
    if (app_fbo != 0) k_render_nonzero_fbo = app_fbo;

    if (k_render_swap_calls >= RENDER_PATHS) {
        const GLint evicted = k_render_fbo_ring[k_render_fbo_ring_head];
        if (evicted != 0) {
            k_render_nonzero_count--;
        } else {
            k_render_zero_count--;
        }
    }

    k_render_fbo_ring[k_render_fbo_ring_head] = app_fbo;
    k_render_fbo_ring_head = (k_render_fbo_ring_head + 1) % RENDER_PATHS;

    if (app_fbo != 0) {
        k_render_nonzero_count++;
    } else {
        k_render_zero_count++;
    }

    if (k_render_swap_calls < RENDER_PATHS) k_render_swap_calls++;

    if (k_render_swap_calls < RENDER_PATHS) {
        k_render_path = RENDER_PATH_UNKNOWN;
        return;
    }

    if (k_render_nonzero_count > 0) {
        k_render_path = RENDER_PATH_OFFSCREEN_FBO;
    } else {
        k_render_path = RENDER_PATH_DEFAULT_FBO;
    }
}

static GLuint k_content_tex = 0;
static GLuint k_content_fbo = 0;
static int k_content_w = 0;
static int k_content_h = 0;

static GLuint k_output_tex = 0;
static GLuint k_output_fbo = 0;
static int k_output_w = 0;
static int k_output_h = 0;

static int k_content_pass_needed(int rot) {
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

// We read the framebuffer into a texture so the colour pass and
// shader pass can sample from it.  Three tiers of detection:
//
//   FAST   glCopyTexSubImage2D into a texture whose internal format matches
//          the EGL surface (Uses GL_RGB if surface has no alpha channel).
//
//   SLOW   glReadPixels(GL_RGBA) into a CPU staging buffer, then
//          glTexSubImage2D back to the GPU.  Used when the fast path is
//          rejected by the driver - 2x bus traffic versus FAST plus a
//          full GPU drain, 5-10ms delay, not great but still usable!
//
//   OFF    Both paths failed.  Colour pass and shader pass are skipped...

typedef enum {
    CAPTURE_UNTRIED = 0,
    CAPTURE_FAST,
    CAPTURE_SLOW,
    CAPTURE_OFF,
} k_capture_mode_t;

static k_capture_mode_t k_capture_mode = CAPTURE_UNTRIED;
static GLenum k_content_internal = GL_RGBA;
static GLenum k_content_format = GL_RGBA;

static unsigned char *k_read_buf = NULL;
static size_t k_read_buf_size = 0;

static void k_probe_surface_format(void) {
    EGLDisplay dpy = eglGetCurrentDisplay();
    EGLSurface surf = eglGetCurrentSurface(EGL_DRAW);

    if (dpy == EGL_NO_DISPLAY || surf == EGL_NO_SURFACE) {
        k_content_internal = GL_RGBA;
        k_content_format = GL_RGBA;

        return;
    }

    EGLint config_id = 0;
    if (!eglQuerySurface(dpy, surf, EGL_CONFIG_ID, &config_id)) {
        k_content_internal = GL_RGBA;
        k_content_format = GL_RGBA;

        return;
    }

    EGLint nconf = 0;
    EGLConfig cfg = NULL;
    const EGLint attribs[] = {EGL_CONFIG_ID, config_id, EGL_NONE};
    if (!eglChooseConfig(dpy, attribs, &cfg, 1, &nconf) || nconf < 1) {
        k_content_internal = GL_RGBA;
        k_content_format = GL_RGBA;

        return;
    }

    EGLint r = 0, g = 0, b = 0, a = 0;
    eglGetConfigAttrib(dpy, cfg, EGL_RED_SIZE, &r);
    eglGetConfigAttrib(dpy, cfg, EGL_GREEN_SIZE, &g);
    eglGetConfigAttrib(dpy, cfg, EGL_BLUE_SIZE, &b);
    eglGetConfigAttrib(dpy, cfg, EGL_ALPHA_SIZE, &a);

    if (a > 0) {
        k_content_internal = GL_RGBA;
        k_content_format = GL_RGBA;
    } else {
        k_content_internal = GL_RGB;
        k_content_format = GL_RGB;
    }

    LOG_INFO("stage", "[kms] EGL surface format: R%d G%d B%d A%d - capture texture = %s", r, g, b, a, a > 0 ? "GL_RGBA" : "GL_RGB");
}

static int k_ensure_content_target(int w, int h) {
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (k_content_tex && w == k_content_w && h == k_content_h) return 1;

    if (k_content_tex) {
        glDeleteTextures(1, &k_content_tex);
        k_content_tex = 0;
    }
    if (k_content_fbo) {
        glDeleteFramebuffers(1, &k_content_fbo);
        k_content_fbo = 0;
    }

    glGenTextures(1, &k_content_tex);
    glBindTexture(GL_TEXTURE_2D, k_content_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint) k_content_internal, w, h, 0, k_content_format, GL_UNSIGNED_BYTE, NULL);

    k_content_w = w;
    k_content_h = h;

    return k_content_tex != 0;
}

static int k_capture_via_copy(int fb_w, int fb_h) {
    while (glGetError() != GL_NO_ERROR) {}

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, k_content_tex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, fb_w, fb_h);

    return glGetError() == GL_NO_ERROR;
}

static int k_capture_via_readpixels(int fb_w, int fb_h) {
    const size_t bytes_per_pixel = (k_content_format == GL_RGB) ? 3 : 4;
    const size_t need = (size_t) fb_w * (size_t) fb_h * bytes_per_pixel;

    if (need > k_read_buf_size) {
        unsigned char *nb = realloc(k_read_buf, need);
        if (!nb) {
            LOG_WARN("stage", "[kms] capture buffer alloc failed (%zu bytes)", need);
            return 0;
        }
        k_read_buf = nb;
        k_read_buf_size = need;
    }

    while (glGetError() != GL_NO_ERROR) {}

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, fb_w, fb_h, k_content_format, GL_UNSIGNED_BYTE, k_read_buf);
    if (glGetError() != GL_NO_ERROR) return 0;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, k_content_tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fb_w, fb_h, k_content_format, GL_UNSIGNED_BYTE, k_read_buf);

    return glGetError() == GL_NO_ERROR;
}

static int k_capture_content(int fb_w, int fb_h, GLint src_fbo) {
    if (k_capture_mode == CAPTURE_OFF) return 0;
    if (k_capture_mode == CAPTURE_UNTRIED) k_probe_surface_format();

    if (!k_ensure_content_target(fb_w, fb_h)) return 0;

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) src_fbo);
    if (k_capture_mode == CAPTURE_SLOW) return k_capture_via_readpixels(fb_w, fb_h);

    if (k_capture_via_copy(fb_w, fb_h)) {
        if (k_capture_mode == CAPTURE_UNTRIED) {
            k_capture_mode = CAPTURE_FAST;
            LOG_INFO("stage", "[kms] capture: GPU fast path (glCopyTexSubImage2D)");
        }

        return 1;
    }

    if (k_capture_via_readpixels(fb_w, fb_h)) {
        k_capture_mode = CAPTURE_SLOW;
        LOG_INFO("stage", "[kms] capture: CPU slow path (glReadPixels) - format mismatch");

        return 1;
    }

    k_capture_mode = CAPTURE_OFF;
    LOG_WARN("stage", "[kms] capture: both paths failed, colour/shader passes disabled");
    k_reset_render_path();

    return 0;
}

static int k_run_content_pass(int fb_w, int fb_h, int rot, GLint src_fbo) {
    k_ensure_content_program();
    if (!k_prog_content_ready) return 0;

    if (!k_capture_content(fb_w, fb_h, src_fbo)) return 0;
    if (!k_ensure_target(&k_output_tex, &k_output_fbo, &k_output_w, &k_output_h, fb_w, fb_h)) return 0;

    glBindFramebuffer(GL_FRAMEBUFFER, k_output_fbo);
    glViewport(0, 0, fb_w, fb_h);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    k_use_program(k_prog_content);

    const struct colour_state *a = colour_adjust_get();
    const colour_filter_matrix_t *f = colour_filter_get();

    if (k_content_u_brightness >= 0) glUniform1f(k_content_u_brightness, a->brightness);
    if (k_content_u_contrast >= 0) glUniform1f(k_content_u_contrast, a->contrast);
    if (k_content_u_saturation >= 0) glUniform1f(k_content_u_saturation, a->saturation);
    if (k_content_u_cosH >= 0) glUniform1f(k_content_u_cosH, cosf(a->hueshift));
    if (k_content_u_sinH >= 0) glUniform1f(k_content_u_sinH, sinf(a->hueshift));
    if (k_content_u_gamma >= 0) glUniform1f(k_content_u_gamma, a->gamma);
    if (k_content_u_filter_enabled >= 0) glUniform1i(k_content_u_filter_enabled, f->enabled ? 1 : 0);
    if (f->enabled && k_content_u_filter >= 0) glUniformMatrix3fv(k_content_u_filter, 1, GL_FALSE, f->matrix);

    k_vtx_t v[4];
    k_build_fullscreen_quad(v, rot);
    k_draw_quad_content(k_content_tex, v);

    return 1;
}

typedef struct {
    int fb_w, fb_h;

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
} k_overlay_state_t;

static GLuint k_overlay_tex = 0;
static GLuint k_overlay_fbo = 0;
static int k_overlay_w = 0;
static int k_overlay_h = 0;
static int k_overlay_valid = 0;
static k_overlay_state_t k_overlay_cache;

static int k_overlay_state_changed(const k_overlay_state_t *a, const k_overlay_state_t *b) {
    if (a->fb_w != b->fb_w || a->fb_h != b->fb_h) return 1;
    if (a->base_disabled != b->base_disabled) return 1;
    if (a->base_tex != b->base_tex || a->base_alpha != b->base_alpha) return 1;
    if (a->battery_step != b->battery_step) return 1;
    if (a->battery_tex != b->battery_tex || a->battery_alpha != b->battery_alpha) return 1;
    if (a->bright_visible != b->bright_visible || a->bright_step != b->bright_step) return 1;
    if (a->bright_tex != b->bright_tex || a->bright_alpha != b->bright_alpha) return 1;
    if (a->volume_visible != b->volume_visible || a->volume_step != b->volume_step) return 1;
    if (a->volume_tex != b->volume_tex || a->volume_alpha != b->volume_alpha) return 1;

    return 0;
}

static k_overlay_state_t k_build_overlay_state(int fb_w, int fb_h, int base_disabled, int battery_step) {
    k_overlay_state_t st;
    memset(&st, 0, sizeof(st));

    st.fb_w = fb_w;
    st.fb_h = fb_h;

    st.base_disabled = base_disabled;
    st.base_tex = (!base_disabled && k_layer_base.tex) ? k_layer_base.tex : 0;
    st.base_alpha = st.base_tex ? get_alpha_cached(&overlay_alpha_cache) : 0.0f;

    st.battery_step = battery_step;
    st.battery_tex = (battery_step >= 0 && battery_step < INDICATOR_STEPS) ? k_layer_battery[battery_step].tex : 0;
    st.battery_alpha = st.battery_tex ? get_alpha_cached(&battery_alpha_cache) : 0.0f;

    st.bright_visible = bright_is_visible() ? 1 : 0;
    st.bright_step = bright_last_step;
    st.bright_tex = (st.bright_visible && st.bright_step >= 0 && st.bright_step < INDICATOR_STEPS) ? k_layer_bright[st.bright_step].tex : 0;
    st.bright_alpha = st.bright_tex ? get_alpha_cached(&bright_alpha_cache) : 0.0f;

    st.volume_visible = volume_is_visible() ? 1 : 0;
    st.volume_step = volume_last_step;
    st.volume_tex = (st.volume_visible && st.volume_step >= 0 && st.volume_step < INDICATOR_STEPS) ? k_layer_volume[st.volume_step].tex : 0;
    st.volume_alpha = st.volume_tex ? get_alpha_cached(&volume_alpha_cache) : 0.0f;

    return st;
}

static void k_rebuild_overlay_batch(const k_overlay_state_t *st) {
    if (!st) return;
    if (!k_ensure_target(&k_overlay_tex, &k_overlay_fbo, &k_overlay_w, &k_overlay_h, st->fb_w, st->fb_h)) return;

    glBindFramebuffer(GL_FRAMEBUFFER, k_overlay_fbo);
    glViewport(0, 0, st->fb_w, st->fb_h);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (st->base_tex && k_vtx_base_valid) k_draw_quad_overlay(st->base_tex, k_vtx_base, st->base_alpha);
    if (st->battery_tex && k_vtx_battery_valid) k_draw_quad_overlay(st->battery_tex, k_vtx_battery, st->battery_alpha);
    if (st->bright_tex && k_vtx_bright_valid) k_draw_quad_overlay(st->bright_tex, k_vtx_bright, st->bright_alpha);
    if (st->volume_tex && k_vtx_volume_valid) k_draw_quad_overlay(st->volume_tex, k_vtx_volume, st->volume_alpha);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    k_overlay_cache = *st;
    k_overlay_valid = 1;
}

static GLuint k_dim_tex = 0;

static void k_ensure_dim_tex(void) {
    static SDL_Color last_colour = {0, 0, 0, 0};

    const SDL_Color *c = &notif_cfg.dim_colour;
    const Uint8 pixel[4] = {c->r, c->g, c->b, 255};

    if (!k_dim_tex) {
        glGenTextures(1, &k_dim_tex);
        glBindTexture(GL_TEXTURE_2D, k_dim_tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        last_colour = *c;

        return;
    }

    if (c->r == last_colour.r && c->g == last_colour.g && c->b == last_colour.b) return;

    glBindTexture(GL_TEXTURE_2D, k_dim_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    last_colour = *c;
}

typedef EGLImageKHR (*PFN_eglCreateImageKHR_)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint *);

typedef EGLBoolean (*PFN_eglDestroyImageKHR_)(EGLDisplay, EGLImageKHR);

typedef void (*PFN_glEGLImageTargetTexture2DOES_)(GLenum, GLeglImageOES);

static struct {
    int ready;
    int drm_fd;

    uint32_t crtc_id;
    int crtc_w, crtc_h;
    uint32_t plane_id;
    uint32_t fmt;

    struct {
        uint32_t fb_id, crtc_id;
        uint32_t src_x, src_y, src_w, src_h;
        uint32_t crtc_x, crtc_y, crtc_w, crtc_h;
    } prop;

    gbm_device_t *gbm_dev;
    gbm_bo_t *gbm_bo;
    int dma_fd;

    uint32_t fb_id;
    EGLImageKHR image;
    GLuint gl_tex;
    GLuint gl_fbo;
    int width, height;

    PFN_eglCreateImageKHR_ eglCreateImageKHR_p;
    PFN_eglDestroyImageKHR_ eglDestroyImageKHR_p;
    PFN_glEGLImageTargetTexture2DOES_ glEGLImageTargetTexture2DOES_p;
} pl;

static int pl_find_prop(int fd, uint32_t obj, uint32_t type, const char *name, uint32_t *out) {
    drm_objprops_t *props = dl.ObjectGetProperties(fd, obj, type);
    if (!props) return 0;

    int found = 0;
    for (uint32_t i = 0; i < props->count_props && !found; i++) {
        drm_prop_t *p = dl.GetProperty(fd, props->props[i]);
        if (!p) continue;

        if (strcmp(p->name, name) == 0) {
            *out = props->props[i];
            found = 1;
        }

        dl.FreeProperty(p);
    }

    dl.FreeObjectProperties(props);
    return found;
}

static uint64_t pl_read_prop(int fd, uint32_t obj, uint32_t type, const char *name) {
    drm_objprops_t *props = dl.ObjectGetProperties(fd, obj, type);
    if (!props) return UINT64_MAX;

    uint64_t v = UINT64_MAX;
    for (uint32_t i = 0; i < props->count_props && v == UINT64_MAX; i++) {
        drm_prop_t *p = dl.GetProperty(fd, props->props[i]);
        if (!p) continue;

        if (strcmp(p->name, name) == 0) v = props->prop_values[i];

        dl.FreeProperty(p);
    }

    dl.FreeObjectProperties(props);
    return v;
}

static int pl_find_active_crtc(int fd, uint32_t *crtc_id, int *crtc_idx, int *cw, int *ch) {
    drm_res_t *r = dl.GetResources(fd);
    if (!r) return 0;

    int found = 0;
    for (int i = 0; i < r->count_crtcs && !found; i++) {
        drm_crtc_t *c = dl.GetCrtc(fd, r->crtcs[i]);
        if (!c) continue;

        if (c->buffer_id && c->width > 0 && c->height > 0) {
            *crtc_id = c->crtc_id;
            *crtc_idx = i;
            *cw = (int) c->width;
            *ch = (int) c->height;
            found = 1;
        }

        dl.FreeCrtc(c);
    }

    dl.FreeResources(r);
    return found;
}

static int pl_supports_format(drm_plane_t *p, uint32_t fmt) {
    for (uint32_t i = 0; i < p->count_formats; i++) {
        if (p->formats[i] == fmt) return 1;
    }

    return 0;
}

static int pl_resolve_plane_props(int fd, uint32_t pid) {
    return pl_find_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "FB_ID", &pl.prop.fb_id) &&
           pl_find_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "CRTC_ID", &pl.prop.crtc_id) &&
           pl_find_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "SRC_X", &pl.prop.src_x) &&
           pl_find_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "SRC_Y", &pl.prop.src_y) &&
           pl_find_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "SRC_W", &pl.prop.src_w) &&
           pl_find_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "SRC_H", &pl.prop.src_h) &&
           pl_find_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "CRTC_X", &pl.prop.crtc_x) &&
           pl_find_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "CRTC_Y", &pl.prop.crtc_y) &&
           pl_find_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "CRTC_W", &pl.prop.crtc_w) &&
           pl_find_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "CRTC_H", &pl.prop.crtc_h);
}

static int pl_plane_is_candidate(int fd, drm_plane_t *p, uint32_t pid, int crtc_idx, uint32_t fmt) {
    if (!(p->possible_crtcs & (1u << crtc_idx))) return 0;
    if (!pl_supports_format(p, fmt)) return 0;
    if (pl_read_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "type") != DRM_PLANE_TYPE_OVERLAY) return 0;

    const uint64_t in_use_crtc = pl_read_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "CRTC_ID");
    const uint64_t in_use_fb = pl_read_prop(fd, pid, DRM_MODE_OBJECT_PLANE, "FB_ID");

    if (in_use_crtc != 0 && in_use_fb != 0) return 0;

    return 1;
}

static int pl_find_overlay_plane(int fd, uint32_t crtc_id, int crtc_idx) {
    drm_pres_t *pres = dl.GetPlaneResources(fd);
    if (!pres) return 0;

    const uint32_t fmts[] = {DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888};
    int found = 0;

    for (size_t fi = 0; fi < sizeof(fmts) / sizeof(fmts[0]) && !found; fi++) {
        const uint32_t fmt = fmts[fi];

        for (uint32_t i = 0; i < pres->count_planes && !found; i++) {
            const uint32_t pid = pres->planes[i];
            drm_plane_t *p = dl.GetPlane(fd, pid);
            if (!p) continue;

            if (pl_plane_is_candidate(fd, p, pid, crtc_idx, fmt) &&
                pl_resolve_plane_props(fd, pid)) {
                pl.plane_id = pid;
                pl.crtc_id = crtc_id;
                pl.fmt = fmt;
                found = 1;
            }

            dl.FreePlane(p);
        }
    }

    dl.FreePlaneResources(pres);
    return found;
}

static int pl_resolve_egl_ext(void) {
    pl.eglCreateImageKHR_p = (PFN_eglCreateImageKHR_) eglGetProcAddress("eglCreateImageKHR");
    pl.eglDestroyImageKHR_p = (PFN_eglDestroyImageKHR_) eglGetProcAddress("eglDestroyImageKHR");
    pl.glEGLImageTargetTexture2DOES_p = (PFN_glEGLImageTargetTexture2DOES_) eglGetProcAddress("glEGLImageTargetTexture2DOES");

    return pl.eglCreateImageKHR_p && pl.eglDestroyImageKHR_p && pl.glEGLImageTargetTexture2DOES_p;
}

static void pl_shutdown(void);

static int pl_build_render_target(int w, int h) {
    const uint32_t use = 0x4 | 0x1 | 0x10;

    pl.gbm_bo = dl.gbm_bo_create(pl.gbm_dev, w, h, pl.fmt, use);
    if (!pl.gbm_bo) {
        LOG_INFO("stage", "[kms] gbm_bo_create %dx%d fmt=0x%08x failed", w, h, pl.fmt);
        return 0;
    }

    const uint32_t handle = (uint32_t) (dl.gbm_bo_get_handle(pl.gbm_bo) & 0xFFFFFFFFu);
    const uint32_t stride = dl.gbm_bo_get_stride(pl.gbm_bo);
    pl.dma_fd = dl.gbm_bo_get_fd(pl.gbm_bo);

    if (!handle || !stride || pl.dma_fd < 0) {
        LOG_INFO("stage", "[kms] GBM bo missing handle/stride/fd");
        return 0;
    }

    const uint32_t handles[4] = {handle, 0, 0, 0};
    const uint32_t pitches[4] = {stride, 0, 0, 0};
    const uint32_t offsets[4] = {0, 0, 0, 0};

    if (dl.AddFB2(pl.drm_fd, w, h, pl.fmt, handles, pitches, offsets, &pl.fb_id, 0) != 0) {
        LOG_INFO("stage", "[kms] drmModeAddFB2 failed");
        return 0;
    }

    EGLDisplay dpy = eglGetCurrentDisplay();
    if (dpy == EGL_NO_DISPLAY) return 0;

    const EGLint attribs[] = {
            EGL_WIDTH, w,
            EGL_HEIGHT, h,
            EGL_LINUX_DRM_FOURCC_EXT, (EGLint) pl.fmt,
            EGL_DMA_BUF_PLANE0_FD_EXT, pl.dma_fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint) stride,
            EGL_NONE
    };

    pl.image = pl.eglCreateImageKHR_p(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);
    if (pl.image == EGL_NO_IMAGE_KHR) {
        LOG_INFO("stage", "[kms] eglCreateImageKHR failed");
        return 0;
    }

    glGenTextures(1, &pl.gl_tex);
    glBindTexture(GL_TEXTURE_2D, pl.gl_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    pl.glEGLImageTargetTexture2DOES_p(GL_TEXTURE_2D, (GLeglImageOES) pl.image);

    if (glGetError() != GL_NO_ERROR) {
        LOG_INFO("stage", "[kms] glEGLImageTargetTexture2DOES failed");
        return 0;
    }

    glGenFramebuffers(1, &pl.gl_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, pl.gl_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pl.gl_tex, 0);

    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (st != GL_FRAMEBUFFER_COMPLETE) {
        LOG_INFO("stage", "[kms] plane FBO incomplete (0x%04x)", st);
        return 0;
    }

    pl.width = w;
    pl.height = h;
    return 1;
}

static int pl_init(int drm_fd, int w, int h) {
    if (pl.ready) return 1;
    if (!dl_init()) return 0;

    if (dl.SetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1) != 0) return 0;
    if (dl.SetClientCap(drm_fd, DRM_CLIENT_CAP_ATOMIC, 1) != 0) return 0;

    int crtc_idx = -1;
    if (!pl_find_active_crtc(drm_fd, &pl.crtc_id, &crtc_idx, &pl.crtc_w, &pl.crtc_h)) return 0;
    if (!pl_find_overlay_plane(drm_fd, pl.crtc_id, crtc_idx)) return 0;
    if (!pl_resolve_egl_ext()) return 0;

    pl.gbm_dev = dl.gbm_create_device(drm_fd);
    if (!pl.gbm_dev) return 0;

    pl.drm_fd = drm_fd;
    pl.dma_fd = -1;

    if (!pl_build_render_target(w, h)) {
        pl_shutdown();
        return 0;
    }

    pl.ready = 1;
    LOG_INFO("stage", "[kms] plane path ready: plane=%u crtc=%u fb=%u %dx%d", pl.plane_id, pl.crtc_id, pl.fb_id, w, h);

    return 1;
}

static int pl_inject(drm_atomic_req_t req) {
    if (!pl.ready || !req) return 0;

    const uint64_t src_w = (uint64_t) pl.width << 16;
    const uint64_t src_h = (uint64_t) pl.height << 16;

    int rc = 0;
    rc |= dl.AtomicAddProperty(req, pl.plane_id, pl.prop.fb_id, pl.fb_id);
    rc |= dl.AtomicAddProperty(req, pl.plane_id, pl.prop.crtc_id, pl.crtc_id);
    rc |= dl.AtomicAddProperty(req, pl.plane_id, pl.prop.src_x, 0);
    rc |= dl.AtomicAddProperty(req, pl.plane_id, pl.prop.src_y, 0);
    rc |= dl.AtomicAddProperty(req, pl.plane_id, pl.prop.src_w, src_w);
    rc |= dl.AtomicAddProperty(req, pl.plane_id, pl.prop.src_h, src_h);
    rc |= dl.AtomicAddProperty(req, pl.plane_id, pl.prop.crtc_x, 0);
    rc |= dl.AtomicAddProperty(req, pl.plane_id, pl.prop.crtc_y, 0);
    rc |= dl.AtomicAddProperty(req, pl.plane_id, pl.prop.crtc_w, (uint64_t) pl.crtc_w);
    rc |= dl.AtomicAddProperty(req, pl.plane_id, pl.prop.crtc_h, (uint64_t) pl.crtc_h);

    return rc >= 0;
}

static void pl_shutdown(void) {
    if (pl.gl_fbo) {
        glDeleteFramebuffers(1, &pl.gl_fbo);
        pl.gl_fbo = 0;
    }

    if (pl.gl_tex) {
        glDeleteTextures(1, &pl.gl_tex);
        pl.gl_tex = 0;
    }

    if (pl.image && pl.eglDestroyImageKHR_p) {
        EGLDisplay d = eglGetCurrentDisplay();
        if (d != EGL_NO_DISPLAY) pl.eglDestroyImageKHR_p(d, pl.image);
        pl.image = NULL;
    }

    if (pl.fb_id) {
        dl.RmFB(pl.drm_fd, pl.fb_id);
        pl.fb_id = 0;
    }

    if (pl.dma_fd >= 0) {
        close(pl.dma_fd);
        pl.dma_fd = -1;
    }

    if (pl.gbm_bo) {
        dl.gbm_bo_destroy(pl.gbm_bo);
        pl.gbm_bo = NULL;
    }

    if (pl.gbm_dev) {
        dl.gbm_device_destroy(pl.gbm_dev);
        pl.gbm_dev = NULL;
    }

    pl.ready = 0;
}

static void k_load_layer_textures(int fb_w, int fb_h, int base_disabled) {
    k_update_dimension(fb_w, fb_h);

    if (!base_disabled) {
        const char *p = k_resolve_base_path(fb_w, fb_h);
        if (p && p[0]) k_load_png_to_layer(p, &k_layer_base);
    }

    static int last_battery_step = -1;
    static int last_bright_step = -1;
    static int last_volume_step = -1;
    static int last_bright_visible = -1;
    static int last_volume_visible = -1;

    int s = battery_last_step;
    if (s >= 0 && s < INDICATOR_STEPS && (s != last_battery_step || !k_layer_battery[s].tex)) {
        char path[PATH_MAX];
        if (k_resolve_indicator_path("battery", s, path, sizeof(path))) k_load_png_to_layer(path, &k_layer_battery[s]);

        last_battery_step = s;
    }

    s = bright_last_step;
    const int bv = bright_is_visible();
    if (bv && s >= 0 && s < INDICATOR_STEPS && (s != last_bright_step || bv != last_bright_visible || !k_layer_bright[s].tex)) {
        char path[PATH_MAX];
        if (k_resolve_indicator_path("bright", s, path, sizeof(path))) k_load_png_to_layer(path, &k_layer_bright[s]);

        last_bright_step = s;
        last_bright_visible = bv;
    }

    s = volume_last_step;
    const int vv = volume_is_visible();
    if (vv && s >= 0 && s < INDICATOR_STEPS && (s != last_volume_step || vv != last_volume_visible || !k_layer_volume[s].tex)) {
        char path[PATH_MAX];
        if (k_resolve_indicator_path("volume", s, path, sizeof(path))) k_load_png_to_layer(path, &k_layer_volume[s]);

        last_volume_step = s;
        last_volume_visible = vv;
    }
}


#define K_UPDATE_GEOM(LAYER, FIELD) do {                     \
    int gc = get_##FIELD##_cached(&LAYER##_##FIELD##_cache); \
    if (gc != k_##LAYER##_##FIELD##_cached) {                \
        k_##LAYER##_##FIELD##_cached = gc;                   \
        k_vtx_##LAYER##_valid = 0;                           \
    }                                                        \
} while (0)

static void k_update_geometry_caches(void) {
    K_UPDATE_GEOM(base, anchor);
    K_UPDATE_GEOM(base, scale);

    K_UPDATE_GEOM(battery, anchor);
    K_UPDATE_GEOM(battery, scale);

    K_UPDATE_GEOM(bright, anchor);
    K_UPDATE_GEOM(bright, scale);

    K_UPDATE_GEOM(volume, anchor);
    K_UPDATE_GEOM(volume, scale);
}

static void k_rebuild_vtx_if_needed(k_vtx_t *vtx, int *valid, const k_layer_t *layer, int fb_w, int fb_h, int anchor, int scale) {
    if (*valid || !layer || !layer->tex) return;

    k_build_quad_ndc(vtx, layer->w, layer->h, fb_w, fb_h, anchor, scale);
    *valid = 1;
    k_overlay_valid = 0;
}

static int k_stage_has_work(int base_disabled, int rot) {
    if (k_render_path == RENDER_PATH_UNKNOWN) return 1;
    if (!k_overlay_valid) return 1;
    if (!base_disabled && k_layer_base.tex) return 1;
    if (battery_last_step >= 0 && battery_last_step < INDICATOR_STEPS && k_layer_battery[battery_last_step].tex) return 1;
    if (bright_is_visible()) return 1;
    if (volume_is_visible()) return 1;
    if (notif_is_visible()) return 1;
    if (k_content_pass_needed(rot)) return 1;
    if (shader_get()) return 1;

    return 0;
}

static int k_shader_frame = 0;

static void k_stage_draw(int fb_w, int fb_h, GLint dst_fbo) {
    if (fb_w <= 0 || fb_h <= 0) return;

    k_ensure_overlay_program();
    if (!k_prog_overlay_ready) return;

    const int base_disabled = ino_proc ? base_overlay_disabled_cached : (access(BASE_OVERLAY_NOP, F_OK) == 0);

    if (base_disabled != k_base_nop_last) {
        if (base_disabled) k_destroy_layer(&k_layer_base);

        k_vtx_base_valid = 0;
        k_overlay_valid = 0;
        k_base_nop_last = base_disabled;
    }

    k_load_layer_textures(fb_w, fb_h, base_disabled);
    k_update_geometry_caches();

    k_rebuild_vtx_if_needed(k_vtx_base, &k_vtx_base_valid, base_disabled ? NULL : &k_layer_base,
                            fb_w, fb_h, k_base_anchor_cached, k_base_scale_cached);

    const int battery_step = battery_last_step;
    if (battery_step >= 0 && battery_step < INDICATOR_STEPS) {
        k_rebuild_vtx_if_needed(k_vtx_battery, &k_vtx_battery_valid, &k_layer_battery[battery_step],
                                fb_w, fb_h, k_battery_anchor_cached, k_battery_scale_cached);
    }

    if (bright_is_visible()) {
        const int s = bright_last_step;
        if (s >= 0 && s < INDICATOR_STEPS) {
            k_rebuild_vtx_if_needed(k_vtx_bright, &k_vtx_bright_valid, &k_layer_bright[s],
                                    fb_w, fb_h, k_bright_anchor_cached, k_bright_scale_cached);
        }
    }

    if (volume_is_visible()) {
        const int s = volume_last_step;
        if (s >= 0 && s < INDICATOR_STEPS) {
            k_rebuild_vtx_if_needed(k_vtx_volume, &k_vtx_volume_valid, &k_layer_volume[s],
                                    fb_w, fb_h, k_volume_anchor_cached, k_volume_scale_cached);
        }
    }

    const int rot = rotate_read_cached();
    if (!k_stage_has_work(base_disabled, rot)) return;

    k_gl_state_t st;
    k_save_state(&st);

    glDisable(GL_DEPTH_TEST);

    const int use_offscreen = (k_render_path == RENDER_PATH_OFFSCREEN_FBO) && (k_render_nonzero_fbo != 0);
    const GLint src_fbo = use_offscreen ? k_render_nonzero_fbo : 0;

    k_ensure_content_program();

    int content_ran = 0;
    if (k_capture_mode != CAPTURE_OFF && k_content_pass_needed(rot)) {
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        content_ran = k_run_content_pass(fb_w, fb_h, rot, src_fbo);
    }

    k_overlay_state_t batch_state = k_build_overlay_state(fb_w, fb_h, base_disabled, battery_step);
    if (!k_overlay_valid || k_overlay_state_changed(&k_overlay_cache, &batch_state)) {
        k_rebuild_overlay_batch(&batch_state);
    }

    const shader_prog_t *shader = shader_get();

    if (shader) {
        int content_copy = content_ran;

        if (!content_copy && k_capture_mode != CAPTURE_OFF) {
            content_copy = k_capture_content(fb_w, fb_h, src_fbo);
        }

        if (content_copy) {
            const GLuint src_tex = (content_ran && k_output_tex) ? k_output_tex : k_content_tex;
            k_draw_quad_shader(shader, src_tex, fb_w, fb_h, k_shader_frame, dst_fbo);
        }
    } else if (content_ran && k_output_tex) {
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) dst_fbo);
        glViewport(0, 0, fb_w, fb_h);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);

        k_draw_quad_overlay(k_output_tex, k_fullscreen_vtx, 1.0f);
    }

    const int draw_overlay = k_overlay_valid && k_overlay_tex;
    const int draw_notif = notif_is_visible() && gl_notif_prepare(fb_w, fb_h);

    if (draw_overlay || draw_notif) {
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) dst_fbo);
        glViewport(0, 0, fb_w, fb_h);
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (draw_overlay) k_draw_quad_overlay(k_overlay_tex, k_fullscreen_vtx, 1.0f);

        if (draw_notif) {
            if (gl_notif_needs_dim()) {
                k_ensure_dim_tex();
                if (k_dim_tex) k_draw_quad_overlay(k_dim_tex, k_fullscreen_vtx, (float) notif_cfg.dim_alpha / 255.0f);
            }

            k_draw_quad_overlay(gl_notif_get_tex(), (const k_vtx_t *) gl_notif_get_vtx(), 1.0f);
        }
    }

    k_restore_state(&st);
    k_current_program = (GLuint) st.program;
}

static k_path_t k_path = PATH_UNKNOWN;
static int k_legacy_seen = 0;
static int k_plane_tried = 0;
static int k_inject_fail = 0;
static int k_unknown_swaps = 0;
static int k_drm_fd = -1;

static EGLContext k_last_ctx = EGL_NO_CONTEXT;
static EGLSurface k_last_surf = EGL_NO_SURFACE;
static int k_fb_w_cached = 0;
static int k_fb_h_cached = 0;

static void k_on_context_changed(void) {
    k_destroy_target(&k_content_tex, &k_content_fbo, &k_content_w, &k_content_h);
    k_destroy_target(&k_output_tex, &k_output_fbo, &k_output_w, &k_output_h);
    k_destroy_target(&k_overlay_tex, &k_overlay_fbo, &k_overlay_w, &k_overlay_h);

    if (k_dim_tex) {
        glDeleteTextures(1, &k_dim_tex);
        k_dim_tex = 0;
    }

    k_destroy_layer(&k_layer_base);
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        k_destroy_layer(&k_layer_battery[i]);
        k_destroy_layer(&k_layer_bright[i]);
        k_destroy_layer(&k_layer_volume[i]);
    }

    if (k_prog_overlay) {
        glDeleteProgram(k_prog_overlay);
        k_prog_overlay = 0;
    }
    if (k_prog_content) {
        glDeleteProgram(k_prog_content);
        k_prog_content = 0;
    }

    k_prog_overlay_attempted = 0;
    k_prog_overlay_ready = 0;
    k_prog_content_attempted = 0;
    k_prog_content_ready = 0;
    k_current_program = 0;
    k_max_attribs = 0;

    k_overlay_valid = 0;
    k_vtx_base_valid = 0;
    k_vtx_battery_valid = 0;
    k_vtx_bright_valid = 0;
    k_vtx_volume_valid = 0;

    k_base_anchor_cached = -1;
    k_base_scale_cached = -1;
    k_battery_anchor_cached = -1;
    k_battery_scale_cached = -1;
    k_bright_anchor_cached = -1;
    k_bright_scale_cached = -1;
    k_volume_anchor_cached = -1;
    k_volume_scale_cached = -1;

    k_base_nop_last = -1;
    k_base_resolved = 0;
    k_base_resolved_path[0] = '\0';
    k_loader_valid = 0;

    k_capture_mode = CAPTURE_UNTRIED;
    k_content_internal = GL_RGBA;
    k_content_format = GL_RGBA;

    if (k_read_buf) {
        free(k_read_buf);
        k_read_buf = NULL;
        k_read_buf_size = 0;
    }

    memset(&k_overlay_cache, 0, sizeof(k_overlay_cache));

    k_reset_render_path();
    if (pl.ready) pl_shutdown();

    colour_adjust_reset();
    colour_filter_reset();
}

static int k_query_surface(EGLDisplay d, EGLSurface s, int *w, int *h) {
    EGLint sw = 0, sh = 0;

    if (!eglQuerySurface(d, s, EGL_WIDTH, &sw)) return 0;
    if (!eglQuerySurface(d, s, EGL_HEIGHT, &sh)) return 0;
    if (sw <= 0 || sh <= 0) return 0;

    *w = sw;
    *h = sh;
    return 1;
}

static void k_check_context(EGLDisplay d, EGLSurface s) {
    EGLContext ctx = eglGetCurrentContext();
    if (ctx == EGL_NO_CONTEXT) return;
    if (ctx == k_last_ctx && s == k_last_surf) return;

    k_last_ctx = ctx;
    k_last_surf = s;

    LOG_INFO("stage", "[kms] context/surface changed - resetting (dpy=%p ctx=%p surf=%p)", (void *) d, (void *) ctx, (void *) s);

    k_on_context_changed();
    k_plane_tried = 0;
}

static k_path_t k_try_promote_plane(int drm_fd, int w, int h) {
    if (k_plane_tried) return k_path;
    k_plane_tried = 1;

    if (k_legacy_seen) return PATH_FBO0;
    if (w <= 0 || h <= 0) return PATH_FBO0;
    if (pl_init(drm_fd, w, h)) return PATH_PLANE;

    return PATH_FBO0;
}

static void k_draw_into_fbo0(int w, int h) {
    GLint prev = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev);

    k_stage_draw(w, h, 0);

    if (prev != 0) glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) prev);
}

static void k_draw_into_plane(int w, int h) {
    GLuint fbo = pl.gl_fbo;
    if (!fbo) {
        k_draw_into_fbo0(w, h);
        return;
    }

    GLint prev_fbo = 0;
    GLint prev_vp[4] = {0};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
    glGetIntegerv(GL_VIEWPORT, prev_vp);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    k_stage_draw(w, h, (GLint) fbo);

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) prev_fbo);
    glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    resolve_hooks();
    if (!real_eglSwapBuffers) return EGL_FALSE;

    if (is_overlay_disabled() || k_path == PATH_DISABLED) return real_eglSwapBuffers(dpy, surface);
    if (eglGetCurrentSurface(EGL_DRAW) != surface) return real_eglSwapBuffers(dpy, surface);

    base_inotify_check();
    if (ino_proc) inotify_check(ino_proc);

    k_check_context(dpy, surface);

    int w = 0;
    int h = 0;

    if (!k_query_surface(dpy, surface, &w, &h)) return real_eglSwapBuffers(dpy, surface);

    battery_overlay_update();
    bright_overlay_update();
    volume_overlay_update();
    notif_update();
    shader_reload();

    if (k_render_path == RENDER_PATH_UNKNOWN) {
        GLint app_fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &app_fbo);
        k_detect_render_path(app_fbo);
    }

    if (w != k_fb_w_cached || h != k_fb_h_cached) {
        k_fb_w_cached = w;
        k_fb_h_cached = h;

        k_vtx_base_valid = 0;
        k_vtx_battery_valid = 0;
        k_vtx_bright_valid = 0;
        k_vtx_volume_valid = 0;
        k_overlay_valid = 0;

        if (pl.ready && (pl.width != w || pl.height != h)) {
            LOG_INFO("stage", "[kms] surface resized from %dx%d to %dx%d - resetting plane", pl.width, pl.height, w, h);
            pl_shutdown();
            k_path = PATH_UNKNOWN;
            k_plane_tried = 0;
            k_inject_fail = 0;
        }
    }

    if (k_path == PATH_UNKNOWN) {
        if (++k_unknown_swaps >= K_UNKNOWN_SWAP_MAX) {
            LOG_INFO("stage", "[kms] no DRM hook fired after %d swaps - settling on FBO 0", k_unknown_swaps);
            k_path = PATH_FBO0;
            k_plane_tried = 1;
        }
        k_draw_into_fbo0(w, h);
    } else if (k_path == PATH_PLANE) {
        k_draw_into_plane(w, h);
    } else {
        k_draw_into_fbo0(w, h);
    }

    EGLBoolean ret = real_eglSwapBuffers(dpy, surface);
    k_shader_frame = (k_shader_frame + 1) & 0xFFFF;

    return ret;
}

int drmModeAtomicCommit(int fd, void *req, uint32_t flags, void *user_data) {
    resolve_hooks();
    if (!real_drmModeAtomicCommit) return -1;

    if (is_overlay_disabled() || k_path == PATH_DISABLED || !req) return real_drmModeAtomicCommit(fd, req, flags, user_data);
    if (flags & DRM_MODE_ATOMIC_TEST_ONLY) return real_drmModeAtomicCommit(fd, req, flags, user_data);

    if (k_drm_fd < 0) k_drm_fd = fd;

    if (k_path == PATH_UNKNOWN || (k_path == PATH_FBO0 && !k_plane_tried)) {
        if (eglGetCurrentContext() != EGL_NO_CONTEXT) k_path = k_try_promote_plane(fd, k_fb_w_cached, k_fb_h_cached);
    }

    if (k_path == PATH_PLANE) {
        if (pl_inject((drm_atomic_req_t) req)) {
            k_inject_fail = 0;
        } else {
            k_inject_fail++;
            if (k_inject_fail >= K_INJECT_FAIL_MAX) {
                LOG_WARN("stage", "[kms] plane inject failed %d times - demoting", k_inject_fail);
                pl_shutdown();
                k_path = PATH_FBO0;
                k_plane_tried = 1;
            }
        }
    }

    int ret = real_drmModeAtomicCommit(fd, req, flags, user_data);

    if (ret != 0 && k_path == PATH_PLANE) {
        k_inject_fail++;
        if (k_inject_fail >= K_INJECT_FAIL_MAX) {
            LOG_WARN("stage", "[kms] atomic commit failed %d times after plane injection - demoting", k_inject_fail);
            pl_shutdown();
            k_path = PATH_FBO0;
            k_plane_tried = 1;
        }
    }

    return ret;
}

int drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id, uint32_t flags, void *user_data) {
    resolve_hooks();
    if (!real_drmModePageFlip) return -1;

    (void) fd;

    if (!k_legacy_seen) {
        k_legacy_seen = 1;
        LOG_INFO("stage", "[kms] legacy page flip seen - using FBO 0 path");
    }

    if (k_path == PATH_PLANE) {
        LOG_WARN("stage", "[kms] legacy flip after plane promotion - demoting");
        pl_shutdown();
    }

    if (k_path == PATH_UNKNOWN || k_path == PATH_PLANE) {
        k_path = PATH_FBO0;
        k_plane_tried = 1;
    }

    return real_drmModePageFlip(fd, crtc_id, fb_id, flags, user_data);
}
