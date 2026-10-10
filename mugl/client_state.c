#include <stdlib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "client.h"
#include "gen_ops.h"

#define MAX_UNITS 32

typedef struct {
    int known;
    GLint min_filter;
    GLint mag_filter;
    GLint wrap_s;
    GLint wrap_t;
} texture_params;

static const GLenum tracked_caps[] = {
    GL_BLEND,        GL_CULL_FACE,           GL_DEPTH_TEST,
    GL_DITHER,       GL_POLYGON_OFFSET_FILL, GL_SCISSOR_TEST,
    GL_STENCIL_TEST, GL_SAMPLE_COVERAGE,     GL_SAMPLE_ALPHA_TO_COVERAGE,
};

#define CAP_COUNT (sizeof(tracked_caps) / sizeof(tracked_caps[0]))

static int cache_state = -1;
static int caps_ready;
static GLboolean caps[CAP_COUNT];
static GLenum blend[4] = {GL_ONE, GL_ZERO, GL_ONE, GL_ZERO};
static GLenum blend_equation[2] = {GL_FUNC_ADD, GL_FUNC_ADD};
static GLfloat blend_colour[4];
static GLint program = 0;
static int program_valid = 1;
static GLuint active_unit;
static GLuint bound[MAX_UNITS][2];
static texture_params *params;
static GLuint params_capacity;
static GLint viewport[4];
static int viewport_known;

int mugl_cache_on(void) {
    if (cache_state == -1) cache_state = getenv("MUGL_NOCACHE") == NULL;
    return cache_state;
}

static int cap_index(GLenum cap) {
    if (!caps_ready) {
        for (size_t i = 0; i < CAP_COUNT; i++)
            caps[i] = tracked_caps[i] == GL_DITHER;
        caps_ready = 1;
    }
    for (size_t i = 0; i < CAP_COUNT; i++) {
        if (tracked_caps[i] == cap) return (int) i;
    }
    return -1;
}

static void send1(uint32_t op, uint32_t a) {
    mugl_send(op, &a, 1);
}

static void send_cap(uint32_t op, GLenum cap, GLboolean value) {
    const int i = cap_index(cap);
    if (mugl_cache_on() && i >= 0 && caps[i] == value) return;
    if (i >= 0) caps[i] = value;
    send1(op, cap);
}

GL_APICALL void GL_APIENTRY glEnable(GLenum cap) {
    send_cap(MUGL_OP_glEnable, cap, GL_TRUE);
}

GL_APICALL void GL_APIENTRY glDisable(GLenum cap) {
    send_cap(MUGL_OP_glDisable, cap, GL_FALSE);
}

GL_APICALL GLboolean GL_APIENTRY glIsEnabled(GLenum cap) {
    const int i = cap_index(cap);
    if (mugl_cache_on() && i >= 0) return caps[i];
    const uint32_t a = cap;
    return (GLboolean) mugl_call_u32(MUGL_OP_glIsEnabled, &a, 1);
}

GL_APICALL void GL_APIENTRY glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) {
    if (mugl_cache_on() && blend[0] == srcRGB && blend[1] == dstRGB && blend[2] == srcAlpha && blend[3] == dstAlpha)
        return;
    blend[0] = srcRGB;
    blend[1] = dstRGB;
    blend[2] = srcAlpha;
    blend[3] = dstAlpha;
    const uint32_t a[4] = {srcRGB, dstRGB, srcAlpha, dstAlpha};
    mugl_send(MUGL_OP_glBlendFuncSeparate, a, 4);
}

GL_APICALL void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor) {
    glBlendFuncSeparate(sfactor, dfactor, sfactor, dfactor);
}

GL_APICALL void GL_APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    if (mugl_cache_on() && blend_equation[0] == modeRGB && blend_equation[1] == modeAlpha) return;
    blend_equation[0] = modeRGB;
    blend_equation[1] = modeAlpha;
    const uint32_t a[2] = {modeRGB, modeAlpha};
    mugl_send(MUGL_OP_glBlendEquationSeparate, a, 2);
}

GL_APICALL void GL_APIENTRY glBlendEquation(GLenum mode) {
    glBlendEquationSeparate(mode, mode);
}

GL_APICALL void GL_APIENTRY glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    const GLfloat colour[4] = {red, green, blue, alpha};
    if (mugl_cache_on() && memcmp(colour, blend_colour, sizeof(colour)) == 0) return;
    memcpy(blend_colour, colour, sizeof(colour));
    uint32_t a[4];
    memcpy(a, colour, sizeof(a));
    mugl_send(MUGL_OP_glBlendColor, a, 4);
}

GL_APICALL void GL_APIENTRY glUseProgram(GLuint id) {
    if (mugl_cache_on() && program_valid && program == (GLint) id) return;
    program = (GLint) id;
    program_valid = 1;
    send1(MUGL_OP_glUseProgram, id);
}

GL_APICALL void GL_APIENTRY glDeleteProgram(GLuint id) {
    if (id && (GLint) id == program) program_valid = 0;
    mugl_program_forget(id);
    send1(MUGL_OP_glDeleteProgram, id);
}

GL_APICALL void GL_APIENTRY glActiveTexture(GLenum texture) {
    const GLuint unit = texture - GL_TEXTURE0;
    if (mugl_cache_on() && unit < MAX_UNITS && unit == active_unit) return;
    if (unit < MAX_UNITS) active_unit = unit;
    send1(MUGL_OP_glActiveTexture, texture);
}

static int target_slot(GLenum target) {
    if (target == GL_TEXTURE_2D) return 0;
    if (target == GL_TEXTURE_CUBE_MAP) return 1;
    return -1;
}

GL_APICALL void GL_APIENTRY glBindTexture(GLenum target, GLuint texture) {
    const int slot = target_slot(target);
    if (mugl_cache_on() && slot >= 0 && bound[active_unit][slot] == texture) return;
    if (slot >= 0) bound[active_unit][slot] = texture;
    const uint32_t a[2] = {target, texture};
    mugl_send(MUGL_OP_glBindTexture, a, 2);
}

static texture_params *params_for(GLuint id) {
    if (id >= params_capacity) {
        GLuint capacity = params_capacity ? params_capacity : 256;
        while (capacity <= id)
            capacity *= 2;
        texture_params *grown = realloc(params, capacity * sizeof(texture_params));
        if (!grown) return NULL;
        memset(grown + params_capacity, 0, (capacity - params_capacity) * sizeof(texture_params));
        params = grown;
        params_capacity = capacity;
    }
    texture_params *p = &params[id];
    if (!p->known) {
        p->known = 1;
        p->min_filter = GL_NEAREST_MIPMAP_LINEAR;
        p->mag_filter = GL_LINEAR;
        p->wrap_s = GL_REPEAT;
        p->wrap_t = GL_REPEAT;
    }
    return p;
}

static GLint *param_slot(GLenum target, GLenum pname) {
    const int slot = target_slot(target);
    if (slot < 0) return NULL;
    texture_params *p = params_for(bound[active_unit][slot]);
    if (!p) return NULL;
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            return &p->min_filter;
        case GL_TEXTURE_MAG_FILTER:
            return &p->mag_filter;
        case GL_TEXTURE_WRAP_S:
            return &p->wrap_s;
        case GL_TEXTURE_WRAP_T:
            return &p->wrap_t;
        default:
            return NULL;
    }
}

GL_APICALL void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param) {
    GLint *slot = param_slot(target, pname);
    if (mugl_cache_on() && slot && *slot == param) return;
    if (slot) *slot = param;
    const uint32_t a[3] = {target, pname, (uint32_t) param};
    mugl_send(MUGL_OP_glTexParameteri, a, 3);
}

GL_APICALL void GL_APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    GLint *slot = param_slot(target, pname);
    if (slot && param == (GLfloat) (GLint) param) {
        if (mugl_cache_on() && *slot == (GLint) param) return;
        *slot = (GLint) param;
    }
    uint32_t a[3] = {target, pname, 0};
    memcpy(&a[2], &param, 4);
    mugl_send(MUGL_OP_glTexParameterf, a, 3);
}

GL_APICALL void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    const GLint next[4] = {x, y, width, height};
    if (mugl_cache_on() && viewport_known && memcmp(next, viewport, sizeof(next)) == 0) return;
    memcpy(viewport, next, sizeof(next));
    viewport_known = 1;
    const uint32_t a[4] = {(uint32_t) x, (uint32_t) y, (uint32_t) width, (uint32_t) height};
    mugl_send(MUGL_OP_glViewport, a, 4);
}

void mugl_state_textures_deleted(GLsizei n, const GLuint *ids) {
    for (GLsizei i = 0; ids && i < n; i++) {
        if (!ids[i]) continue;
        if (ids[i] < params_capacity) params[ids[i]].known = 0;
        for (GLuint unit = 0; unit < MAX_UNITS; unit++) {
            for (int slot = 0; slot < 2; slot++) {
                if (bound[unit][slot] == ids[i]) bound[unit][slot] = 0;
            }
        }
    }
}

int mugl_state_query(GLenum pname, GLint *out) {
    if (!mugl_cache_on() || !out) return 0;
    switch (pname) {
        case GL_VIEWPORT:
            if (!viewport_known) return 0;
            memcpy(out, viewport, sizeof(viewport));
            return 1;
        case GL_CURRENT_PROGRAM:
            if (!program_valid) return 0;
            out[0] = program;
            return 1;
        case GL_ACTIVE_TEXTURE:
            out[0] = (GLint) (GL_TEXTURE0 + active_unit);
            return 1;
        case GL_TEXTURE_BINDING_2D:
            out[0] = (GLint) bound[active_unit][0];
            return 1;
        case GL_TEXTURE_BINDING_CUBE_MAP:
            out[0] = (GLint) bound[active_unit][1];
            return 1;
        case GL_ARRAY_BUFFER_BINDING:
            out[0] = (GLint) mugl_bound_array_buffer();
            return 1;
        case GL_ELEMENT_ARRAY_BUFFER_BINDING:
            out[0] = (GLint) mugl_bound_element_buffer();
            return 1;
        default:
            return 0;
    }
}
