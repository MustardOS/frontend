#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include "client.h"

typedef struct {
    int window;
    EGLint width;
    EGLint height;
} surface_state;

typedef struct {
    EGLint version;
} context_state;

#define DISPLAY_HANDLE ((EGLDisplay) (uintptr_t) 0x6D75474CU)
#define CONFIG_HANDLE  ((EGLConfig) (uintptr_t) 1U)

static __thread EGLint egl_error = EGL_SUCCESS;
static int initialised;
static EGLenum bound_api = EGL_OPENGL_ES_API;
static EGLContext current_context = EGL_NO_CONTEXT;
static EGLSurface current_draw = EGL_NO_SURFACE;
static EGLSurface current_read = EGL_NO_SURFACE;
static EGLint swap_interval = 1;

static EGLBoolean fail(EGLint error) {
    egl_error = error;
    return EGL_FALSE;
}

static int valid_display(EGLDisplay dpy) {
    if (dpy != DISPLAY_HANDLE) {
        egl_error = EGL_BAD_DISPLAY;
        return 0;
    }
    if (!initialised) {
        egl_error = EGL_NOT_INITIALIZED;
        return 0;
    }
    return 1;
}

static EGLint config_value(EGLint attribute, int *known) {
    *known = 1;
    switch (attribute) {
        case EGL_BUFFER_SIZE:
            return 32;
        case EGL_RED_SIZE:
        case EGL_GREEN_SIZE:
        case EGL_BLUE_SIZE:
        case EGL_ALPHA_SIZE:
            return 8;
        case EGL_DEPTH_SIZE:
            return 24;
        case EGL_STENCIL_SIZE:
            return 8;
        case EGL_CONFIG_ID:
            return 1;
        case EGL_RENDERABLE_TYPE:
        case EGL_CONFORMANT:
            return EGL_OPENGL_ES2_BIT;
        case EGL_SURFACE_TYPE:
            return EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
        case EGL_COLOR_BUFFER_TYPE:
            return EGL_RGB_BUFFER;
        case EGL_CONFIG_CAVEAT:
        case EGL_TRANSPARENT_TYPE:
        case EGL_NATIVE_VISUAL_TYPE:
            return EGL_NONE;
        case EGL_MAX_PBUFFER_WIDTH:
        case EGL_MAX_PBUFFER_HEIGHT:
            return 4096;
        case EGL_MAX_PBUFFER_PIXELS:
            return 4096 * 4096;
        case EGL_MAX_SWAP_INTERVAL:
            return 1;
        case EGL_NATIVE_RENDERABLE:
        case EGL_BIND_TO_TEXTURE_RGB:
        case EGL_BIND_TO_TEXTURE_RGBA:
            return EGL_FALSE;
        case EGL_SAMPLES:
        case EGL_SAMPLE_BUFFERS:
        case EGL_LEVEL:
        case EGL_MIN_SWAP_INTERVAL:
        case EGL_NATIVE_VISUAL_ID:
        case EGL_LUMINANCE_SIZE:
        case EGL_ALPHA_MASK_SIZE:
        case EGL_TRANSPARENT_RED_VALUE:
        case EGL_TRANSPARENT_GREEN_VALUE:
        case EGL_TRANSPARENT_BLUE_VALUE:
            return 0;
        default:
            *known = 0;
            return 0;
    }
}

static int config_matches(const EGLint *attrib_list) {
    if (!attrib_list) return 1;
    for (const EGLint *a = attrib_list; a[0] != EGL_NONE; a += 2) {
        const EGLint want = a[1];
        if (want == EGL_DONT_CARE) continue;
        int known = 0;
        const EGLint have = config_value(a[0], &known);
        switch (a[0]) {
            case EGL_RED_SIZE:
            case EGL_GREEN_SIZE:
            case EGL_BLUE_SIZE:
            case EGL_ALPHA_SIZE:
            case EGL_DEPTH_SIZE:
            case EGL_STENCIL_SIZE:
            case EGL_BUFFER_SIZE:
            case EGL_SAMPLES:
            case EGL_SAMPLE_BUFFERS:
            case EGL_LUMINANCE_SIZE:
            case EGL_ALPHA_MASK_SIZE:
                if (want > have) return 0;
                break;
            case EGL_RENDERABLE_TYPE:
            case EGL_SURFACE_TYPE:
            case EGL_CONFORMANT:
                if ((want & have) != want) return 0;
                break;
            case EGL_CONFIG_ID:
                if (want != have) return 0;
                break;
            case EGL_COLOR_BUFFER_TYPE:
                if (want != EGL_RGB_BUFFER) return 0;
                break;
            default:
                break;
        }
    }
    return 1;
}

EGLAPI EGLint EGLAPIENTRY eglGetError(void) {
    const EGLint error = egl_error;
    egl_error = EGL_SUCCESS;
    return error;
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(EGLNativeDisplayType display_id) {
    (void) display_id;
    return DISPLAY_HANDLE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor) {
    if (dpy != DISPLAY_HANDLE) return fail(EGL_BAD_DISPLAY);
    if (!mugl_connect()) return fail(EGL_NOT_INITIALIZED);
    initialised = 1;
    if (major) *major = 1;
    if (minor) *minor = 4;
    egl_error = EGL_SUCCESS;
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay dpy) {
    if (dpy != DISPLAY_HANDLE) return fail(EGL_BAD_DISPLAY);
    egl_error = EGL_SUCCESS;
    return EGL_TRUE;
}

EGLAPI const char *EGLAPIENTRY eglQueryString(EGLDisplay dpy, EGLint name) {
    if (dpy == EGL_NO_DISPLAY && name == EGL_EXTENSIONS) return "";
    if (!valid_display(dpy)) return NULL;
    switch (name) {
        case EGL_VENDOR:
            return "MustardOS";
        case EGL_VERSION:
            return "1.4 mugl";
        case EGL_EXTENSIONS:
            return "EGL_KHR_create_context EGL_KHR_surfaceless_context";
        case EGL_CLIENT_APIS:
            return "OpenGL_ES";
        default:
            egl_error = EGL_BAD_PARAMETER;
            return NULL;
    }
}

EGLAPI EGLBoolean EGLAPIENTRY
eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config) {
    if (!valid_display(dpy)) return EGL_FALSE;
    if (!num_config) return fail(EGL_BAD_PARAMETER);
    if (configs && config_size > 0) {
        configs[0] = CONFIG_HANDLE;
        *num_config = 1;
    } else {
        *num_config = configs ? 0 : 1;
    }
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY
eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config) {
    if (!valid_display(dpy)) return EGL_FALSE;
    if (!num_config) return fail(EGL_BAD_PARAMETER);
    const int match = config_matches(attrib_list);
    if (configs && config_size > 0 && match) {
        configs[0] = CONFIG_HANDLE;
        *num_config = 1;
    } else {
        *num_config = (!configs && match) ? 1 : 0;
    }
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value) {
    if (!valid_display(dpy)) return EGL_FALSE;
    if (config != CONFIG_HANDLE) return fail(EGL_BAD_CONFIG);
    int known = 0;
    const EGLint v = config_value(attribute, &known);
    if (!known) return fail(EGL_BAD_ATTRIBUTE);
    if (value) *value = v;
    return EGL_TRUE;
}

static EGLSurface make_surface(int window, EGLint width, EGLint height) {
    surface_state *s = calloc(1, sizeof(surface_state));
    if (!s) {
        egl_error = EGL_BAD_ALLOC;
        return EGL_NO_SURFACE;
    }
    s->window = window;
    s->width = width;
    s->height = height;
    return (EGLSurface) s;
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list) {
    (void) win;
    (void) attrib_list;
    if (!valid_display(dpy)) return EGL_NO_SURFACE;
    if (config != CONFIG_HANDLE) {
        egl_error = EGL_BAD_CONFIG;
        return EGL_NO_SURFACE;
    }
    return make_surface(1, (EGLint) mugl_width(), (EGLint) mugl_height());
}

EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list) {
    if (!valid_display(dpy)) return EGL_NO_SURFACE;
    if (config != CONFIG_HANDLE) {
        egl_error = EGL_BAD_CONFIG;
        return EGL_NO_SURFACE;
    }
    EGLint width = 0;
    EGLint height = 0;
    for (const EGLint *a = attrib_list; a && a[0] != EGL_NONE; a += 2) {
        if (a[0] == EGL_WIDTH)
            width = a[1];
        else if (a[0] == EGL_HEIGHT)
            height = a[1];
    }
    return make_surface(0, width, height);
}

EGLAPI EGLSurface EGLAPIENTRY
eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list) {
    (void) dpy;
    (void) config;
    (void) pixmap;
    (void) attrib_list;
    egl_error = EGL_BAD_MATCH;
    return EGL_NO_SURFACE;
}

EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferFromClientBuffer(
    EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list
) {
    (void) dpy;
    (void) buftype;
    (void) buffer;
    (void) config;
    (void) attrib_list;
    egl_error = EGL_BAD_PARAMETER;
    return EGL_NO_SURFACE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
    if (!valid_display(dpy)) return EGL_FALSE;
    if (surface == EGL_NO_SURFACE) return fail(EGL_BAD_SURFACE);
    if (surface == current_draw) current_draw = EGL_NO_SURFACE;
    if (surface == current_read) current_read = EGL_NO_SURFACE;
    free(surface);
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value) {
    if (!valid_display(dpy)) return EGL_FALSE;
    const surface_state *s = surface;
    if (!s) return fail(EGL_BAD_SURFACE);
    if (!value) return fail(EGL_BAD_PARAMETER);
    switch (attribute) {
        case EGL_WIDTH:
            *value = s->window ? (EGLint) mugl_width() : s->width;
            return EGL_TRUE;
        case EGL_HEIGHT:
            *value = s->window ? (EGLint) mugl_height() : s->height;
            return EGL_TRUE;
        case EGL_CONFIG_ID:
            *value = 1;
            return EGL_TRUE;
        case EGL_RENDER_BUFFER:
            *value = s->window ? EGL_BACK_BUFFER : EGL_SINGLE_BUFFER;
            return EGL_TRUE;
        case EGL_SWAP_BEHAVIOR:
            *value = EGL_BUFFER_DESTROYED;
            return EGL_TRUE;
        case EGL_HORIZONTAL_RESOLUTION:
        case EGL_VERTICAL_RESOLUTION:
        case EGL_PIXEL_ASPECT_RATIO:
            *value = EGL_UNKNOWN;
            return EGL_TRUE;
        case EGL_LARGEST_PBUFFER:
        case EGL_MIPMAP_LEVEL:
        case EGL_MIPMAP_TEXTURE:
        case EGL_MULTISAMPLE_RESOLVE:
            *value = 0;
            return EGL_TRUE;
        case EGL_TEXTURE_FORMAT:
        case EGL_TEXTURE_TARGET:
            *value = EGL_NO_TEXTURE;
            return EGL_TRUE;
        default:
            return fail(EGL_BAD_ATTRIBUTE);
    }
}

EGLAPI EGLBoolean EGLAPIENTRY eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value) {
    (void) surface;
    (void) attribute;
    (void) value;
    return valid_display(dpy) ? EGL_TRUE : EGL_FALSE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) {
    (void) dpy;
    (void) surface;
    (void) buffer;
    return fail(EGL_BAD_MATCH);
}

EGLAPI EGLBoolean EGLAPIENTRY eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) {
    (void) dpy;
    (void) surface;
    (void) buffer;
    return fail(EGL_BAD_MATCH);
}

EGLAPI EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api) {
    if (api != EGL_OPENGL_ES_API) return fail(EGL_BAD_PARAMETER);
    bound_api = api;
    return EGL_TRUE;
}

EGLAPI EGLenum EGLAPIENTRY eglQueryAPI(void) {
    return bound_api;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitClient(void) {
    if (mugl_connected()) glFinish();
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitGL(void) {
    return eglWaitClient();
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitNative(EGLint engine) {
    (void) engine;
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglReleaseThread(void) {
    egl_error = EGL_SUCCESS;
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay dpy, EGLint interval) {
    if (!valid_display(dpy)) return EGL_FALSE;
    swap_interval = interval < 0 ? 0 : (interval > 1 ? 1 : interval);
    const uint32_t a[1] = {(uint32_t) swap_interval};
    mugl_send(MUGL_OP_SWAP_INTERVAL, a, 1);
    return EGL_TRUE;
}

EGLAPI EGLContext EGLAPIENTRY
eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list) {
    (void) share_context;
    if (!valid_display(dpy)) return EGL_NO_CONTEXT;
    if (config != CONFIG_HANDLE && config != NULL) {
        egl_error = EGL_BAD_CONFIG;
        return EGL_NO_CONTEXT;
    }

    EGLint version = 1;
    EGLint minor = 0;
    for (const EGLint *a = attrib_list; a && a[0] != EGL_NONE; a += 2) {
        if (a[0] == EGL_CONTEXT_CLIENT_VERSION)
            version = a[1];
        else if (a[0] == EGL_CONTEXT_MINOR_VERSION_KHR)
            minor = a[1];
    }
    if (getenv("MUGL_DEBUG")) {
        fprintf(stderr, "mugl: eglCreateContext");
        for (const EGLint *a = attrib_list; a && a[0] != EGL_NONE; a += 2)
            fprintf(stderr, " 0x%x=%d", a[0], a[1]);
        fprintf(stderr, "\n");
    }
    if (version != 2 || minor != 0) {
        egl_error = EGL_BAD_MATCH;
        return EGL_NO_CONTEXT;
    }

    context_state *c = calloc(1, sizeof(context_state));
    if (!c) {
        egl_error = EGL_BAD_ALLOC;
        return EGL_NO_CONTEXT;
    }
    c->version = version;
    return (EGLContext) c;
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
    if (!valid_display(dpy)) return EGL_FALSE;
    if (ctx == EGL_NO_CONTEXT) return fail(EGL_BAD_CONTEXT);
    if (ctx == current_context) current_context = EGL_NO_CONTEXT;
    free(ctx);
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    if (!valid_display(dpy)) return EGL_FALSE;
    current_context = ctx;
    current_draw = draw;
    current_read = read;
    return EGL_TRUE;
}

EGLAPI EGLContext EGLAPIENTRY eglGetCurrentContext(void) {
    return current_context;
}

EGLAPI EGLSurface EGLAPIENTRY eglGetCurrentSurface(EGLint readdraw) {
    return readdraw == EGL_READ ? current_read : current_draw;
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetCurrentDisplay(void) {
    return current_context != EGL_NO_CONTEXT ? DISPLAY_HANDLE : EGL_NO_DISPLAY;
}

EGLAPI EGLBoolean EGLAPIENTRY eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value) {
    if (!valid_display(dpy)) return EGL_FALSE;
    const context_state *c = ctx;
    if (!c) return fail(EGL_BAD_CONTEXT);
    if (!value) return fail(EGL_BAD_PARAMETER);
    switch (attribute) {
        case EGL_CONFIG_ID:
            *value = 1;
            return EGL_TRUE;
        case EGL_CONTEXT_CLIENT_TYPE:
            *value = EGL_OPENGL_ES_API;
            return EGL_TRUE;
        case EGL_CONTEXT_CLIENT_VERSION:
            *value = c->version;
            return EGL_TRUE;
        case EGL_RENDER_BUFFER:
            *value = EGL_BACK_BUFFER;
            return EGL_TRUE;
        default:
            return fail(EGL_BAD_ATTRIBUTE);
    }
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    if (!valid_display(dpy)) return EGL_FALSE;
    const surface_state *s = surface;
    if (!s) return fail(EGL_BAD_SURFACE);
    if (s->window) mugl_swap();
    return EGL_TRUE;
}

EGLAPI EGLBoolean EGLAPIENTRY eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target) {
    (void) dpy;
    (void) surface;
    (void) target;
    return fail(EGL_BAD_NATIVE_PIXMAP);
}

static const mugl_proc egl_procs[] = {
    {"eglGetError", (void (*)(void)) eglGetError},
    {"eglGetDisplay", (void (*)(void)) eglGetDisplay},
    {"eglInitialize", (void (*)(void)) eglInitialize},
    {"eglTerminate", (void (*)(void)) eglTerminate},
    {"eglQueryString", (void (*)(void)) eglQueryString},
    {"eglGetConfigs", (void (*)(void)) eglGetConfigs},
    {"eglChooseConfig", (void (*)(void)) eglChooseConfig},
    {"eglGetConfigAttrib", (void (*)(void)) eglGetConfigAttrib},
    {"eglCreateWindowSurface", (void (*)(void)) eglCreateWindowSurface},
    {"eglCreatePbufferSurface", (void (*)(void)) eglCreatePbufferSurface},
    {"eglCreatePixmapSurface", (void (*)(void)) eglCreatePixmapSurface},
    {"eglDestroySurface", (void (*)(void)) eglDestroySurface},
    {"eglQuerySurface", (void (*)(void)) eglQuerySurface},
    {"eglBindAPI", (void (*)(void)) eglBindAPI},
    {"eglQueryAPI", (void (*)(void)) eglQueryAPI},
    {"eglWaitClient", (void (*)(void)) eglWaitClient},
    {"eglReleaseThread", (void (*)(void)) eglReleaseThread},
    {"eglCreatePbufferFromClientBuffer", (void (*)(void)) eglCreatePbufferFromClientBuffer},
    {"eglSurfaceAttrib", (void (*)(void)) eglSurfaceAttrib},
    {"eglBindTexImage", (void (*)(void)) eglBindTexImage},
    {"eglReleaseTexImage", (void (*)(void)) eglReleaseTexImage},
    {"eglSwapInterval", (void (*)(void)) eglSwapInterval},
    {"eglCreateContext", (void (*)(void)) eglCreateContext},
    {"eglDestroyContext", (void (*)(void)) eglDestroyContext},
    {"eglMakeCurrent", (void (*)(void)) eglMakeCurrent},
    {"eglGetCurrentContext", (void (*)(void)) eglGetCurrentContext},
    {"eglGetCurrentSurface", (void (*)(void)) eglGetCurrentSurface},
    {"eglGetCurrentDisplay", (void (*)(void)) eglGetCurrentDisplay},
    {"eglQueryContext", (void (*)(void)) eglQueryContext},
    {"eglWaitGL", (void (*)(void)) eglWaitGL},
    {"eglWaitNative", (void (*)(void)) eglWaitNative},
    {"eglSwapBuffers", (void (*)(void)) eglSwapBuffers},
    {"eglCopyBuffers", (void (*)(void)) eglCopyBuffers},
};

EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress(const char *procname) {
    if (!procname) return NULL;
    for (size_t i = 0; i < sizeof(egl_procs) / sizeof(egl_procs[0]); i++) {
        if (strcmp(egl_procs[i].name, procname) == 0)
            return (__eglMustCastToProperFunctionPointerType) egl_procs[i].func;
    }
    return (__eglMustCastToProperFunctionPointerType) mugl_gl_lookup(procname);
}
