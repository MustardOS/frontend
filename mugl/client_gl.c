#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "client.h"
#include "gen_ops.h"

#define MAX_ATTRIBS 16

typedef struct {
    int enabled;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    const void *pointer;
    GLuint buffer;
} attrib_state;

typedef struct {
    attrib_state attribs[MAX_ATTRIBS];
    GLuint element_buffer;
} vao_state;

typedef struct {
    uint8_t *shadow;
    uint32_t size;
    uint8_t *mapped;
} buffer_state;

static vao_state default_vao;
static vao_state *vao = &default_vao;
static vao_state **vaos;
static uint32_t vao_capacity;
static buffer_state *buffers;
static uint32_t buffer_capacity;
static GLuint array_buffer;
static GLint unpack_alignment = 4;
static GLint pack_alignment = 4;
static char *strings[8];

static const char *const supported_extensions[] = {
    "GL_OES_vertex_array_object", "GL_EXT_discard_framebuffer", "GL_OES_mapbuffer",
    "GL_OES_texture_npot", "GL_OES_depth24", "GL_OES_depth32", "GL_OES_packed_depth_stencil",
    "GL_OES_rgb8_rgba8", "GL_OES_element_index_uint", "GL_OES_standard_derivatives",
    "GL_OES_texture_float", "GL_OES_texture_half_float", "GL_OES_texture_float_linear",
    "GL_OES_texture_half_float_linear", "GL_OES_depth_texture", "GL_OES_fragment_precision_high",
    "GL_OES_compressed_ETC1_RGB8_texture", "GL_IMG_texture_compression_pvrtc", "GL_EXT_texture_format_BGRA8888",
    "GL_EXT_blend_minmax", "GL_EXT_shader_texture_lod", "GL_EXT_texture_rg", "GL_OES_vertex_half_float",
    "GL_EXT_read_format_bgra", "GL_EXT_texture_filter_anisotropic", "GL_EXT_sRGB", "GL_IMG_read_format",
    "GL_EXT_shader_framebuffer_fetch", "GL_OES_required_internalformat", "GL_EXT_color_buffer_half_float",
    "GL_OES_texture_npot", "GL_IMG_texture_npot",
};

static uint32_t f2u(GLfloat f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static buffer_state *buffer_get(GLuint id) {
    if (!id) return NULL;
    if (id >= buffer_capacity) {
        uint32_t capacity = buffer_capacity ? buffer_capacity : 64;
        while (capacity <= id) capacity *= 2;
        buffer_state *grown = realloc(buffers, capacity * sizeof(buffer_state));
        if (!grown) return NULL;
        memset(grown + buffer_capacity, 0, (capacity - buffer_capacity) * sizeof(buffer_state));
        buffers = grown;
        buffer_capacity = capacity;
    }
    return &buffers[id];
}

static vao_state *vao_get(GLuint id, int create) {
    if (!id) return &default_vao;
    if (id >= vao_capacity) {
        if (!create) return NULL;
        uint32_t capacity = vao_capacity ? vao_capacity : 32;
        while (capacity <= id) capacity *= 2;
        vao_state **grown = realloc(vaos, capacity * sizeof(vao_state *));
        if (!grown) return NULL;
        memset(grown + vao_capacity, 0, (capacity - vao_capacity) * sizeof(vao_state *));
        vaos = grown;
        vao_capacity = capacity;
    }
    if (!vaos[id] && create) vaos[id] = calloc(1, sizeof(vao_state));
    return vaos[id];
}

static GLuint bound_buffer(GLenum target) {
    if (target == GL_ARRAY_BUFFER) return array_buffer;
    if (target == GL_ELEMENT_ARRAY_BUFFER) return vao->element_buffer;
    return 0;
}

static uint32_t type_size(GLenum type) {
    switch (type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return 1;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_HALF_FLOAT_OES:
            return 2;
        default:
            return 4;
    }
}

static uint32_t pixel_size(GLenum format, GLenum type) {
    switch (type) {
        case GL_UNSIGNED_SHORT_5_6_5:
        case GL_UNSIGNED_SHORT_4_4_4_4:
        case GL_UNSIGNED_SHORT_5_5_5_1:
            return 2;
        case GL_UNSIGNED_INT_24_8_OES:
            return 4;
        default:
            break;
    }

    uint32_t channels;
    switch (format) {
        case GL_ALPHA:
        case GL_LUMINANCE:
        case GL_DEPTH_COMPONENT:
        case GL_RED_EXT:
            channels = 1;
            break;
        case GL_LUMINANCE_ALPHA:
        case GL_RG_EXT:
            channels = 2;
            break;
        case GL_RGB:
            channels = 3;
            break;
        default:
            channels = 4;
            break;
    }
    return channels * type_size(type);
}

static uint32_t image_size(GLsizei width, GLsizei height, GLenum format, GLenum type, GLint alignment) {
    if (width <= 0 || height <= 0) return 0;
    const uint32_t row = (uint32_t) width * pixel_size(format, type);
    const uint32_t a = alignment > 0 ? (uint32_t) alignment : 4U;
    const uint32_t stride = (row + a - 1U) / a * a;
    return stride * (uint32_t) (height - 1) + row;
}

static void send_words(uint32_t op, const uint32_t *words, uint32_t count, const void *data, uint32_t len) {
    mugl_msg_begin(op, count * 4U + len);
    mugl_msg_put(words, count * 4U);
    if (len) mugl_msg_put(data, len);
    mugl_msg_end();
}

static const uint8_t *call_words(uint32_t op, const uint32_t *words, uint32_t count, const void *data, uint32_t len,
                                 uint32_t *out_len) {
    mugl_msg_begin(op, count * 4U + len);
    mugl_msg_put(words, count * 4U);
    if (len) mugl_msg_put(data, len);
    return mugl_msg_wait(out_len);
}

void mugl_gl_reset(void) {
    for (int i = 0; i < (int) (sizeof(strings) / sizeof(strings[0])); i++) {
        free(strings[i]);
        strings[i] = NULL;
    }
}

GL_APICALL void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer) {
    if (target == GL_ARRAY_BUFFER) array_buffer = buffer;
    else if (target == GL_ELEMENT_ARRAY_BUFFER) vao->element_buffer = buffer;
    const uint32_t a[2] = {target, buffer};
    mugl_send(MUGL_OP_glBindBuffer, a, 2);
}

GL_APICALL void GL_APIENTRY glPixelStorei(GLenum pname, GLint param) {
    if (pname == GL_UNPACK_ALIGNMENT) unpack_alignment = param;
    else if (pname == GL_PACK_ALIGNMENT) pack_alignment = param;
    const uint32_t a[2] = {pname, (uint32_t) param};
    mugl_send(MUGL_OP_glPixelStorei, a, 2);
}

GL_APICALL void GL_APIENTRY glEnableVertexAttribArray(GLuint index) {
    if (index < MAX_ATTRIBS) vao->attribs[index].enabled = 1;
    const uint32_t a[1] = {index};
    mugl_send(MUGL_OP_glEnableVertexAttribArray, a, 1);
}

GL_APICALL void GL_APIENTRY glDisableVertexAttribArray(GLuint index) {
    if (index < MAX_ATTRIBS) vao->attribs[index].enabled = 0;
    const uint32_t a[1] = {index};
    mugl_send(MUGL_OP_glDisableVertexAttribArray, a, 1);
}

GL_APICALL void GL_APIENTRY glFinish(void) {
    mugl_call_u32(MUGL_OP_SYNC, NULL, 0);
}

GL_APICALL GLenum GL_APIENTRY glGetError(void) {
    const uint32_t local = mugl_take_error();
    if (local) return local;
    return (GLenum) mugl_call_u32(MUGL_OP_glGetError, NULL, 0);
}

GL_APICALL void GL_APIENTRY glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                                  GLsizei stride, const void *pointer) {
    if (index >= MAX_ATTRIBS) {
        mugl_set_error(GL_INVALID_VALUE);
        return;
    }

    attrib_state *a = &vao->attribs[index];
    a->size = size;
    a->type = type;
    a->normalized = normalized;
    a->stride = stride;
    a->pointer = pointer;
    a->buffer = array_buffer;

    if (array_buffer) {
        const uint32_t w[6] = {index, (uint32_t) size, type, normalized, (uint32_t) stride, (uint32_t) (uintptr_t) pointer};
        mugl_send(MUGL_OP_VERTEX_ATTRIB_POINTER, w, 6);
    }
}

GL_APICALL void GL_APIENTRY glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer) {
    if (index < MAX_ATTRIBS && pname == GL_VERTEX_ATTRIB_ARRAY_POINTER) *pointer = (void *) vao->attribs[index].pointer;
    else mugl_set_error(GL_INVALID_ENUM);
}

static int has_client_arrays(int *has_buffer_arrays) {
    int client = 0;
    *has_buffer_arrays = 0;
    for (int i = 0; i < MAX_ATTRIBS; i++) {
        const attrib_state *a = &vao->attribs[i];
        if (!a->enabled) continue;
        if (a->buffer) *has_buffer_arrays = 1;
        else if (a->pointer) client = 1;
    }
    return client;
}

static void upload_client_arrays(uint32_t first, uint32_t last) {
    for (int i = 0; i < MAX_ATTRIBS; i++) {
        const attrib_state *a = &vao->attribs[i];
        if (!a->enabled || a->buffer || !a->pointer) continue;

        const uint32_t element = (uint32_t) a->size * type_size(a->type);
        const uint32_t stride = a->stride ? (uint32_t) a->stride : element;
        const uint32_t len = (last - first) * stride + element;
        const uint8_t *src = (const uint8_t *) a->pointer + (size_t) first * stride;

        const uint32_t w[5] = {(uint32_t) i, (uint32_t) a->size, a->type, a->normalized, stride};
        send_words(MUGL_OP_CLIENT_ARRAY, w, 5, src, len);
    }
}

GL_APICALL void GL_APIENTRY glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (count <= 0 || first < 0) return;

    int buffer_arrays = 0;
    int start = first;
    if (has_client_arrays(&buffer_arrays)) {
        const uint32_t from = buffer_arrays ? 0U : (uint32_t) first;
        upload_client_arrays(from, (uint32_t) (first + count - 1));
        start = first - (int) from;
    }

    const uint32_t w[3] = {mode, (uint32_t) start, (uint32_t) count};
    mugl_send(MUGL_OP_DRAW_ARRAYS, w, 3);
}

static void index_range(const void *indices, GLenum type, GLsizei count, uint32_t *min, uint32_t *max) {
    uint32_t lo = 0xFFFFFFFFU;
    uint32_t hi = 0;
    for (GLsizei i = 0; i < count; i++) {
        uint32_t v;
        if (type == GL_UNSIGNED_BYTE) v = ((const uint8_t *) indices)[i];
        else if (type == GL_UNSIGNED_SHORT) v = ((const uint16_t *) indices)[i];
        else v = ((const uint32_t *) indices)[i];
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    *min = count > 0 ? lo : 0;
    *max = hi;
}

static void *rebase_indices(const void *indices, GLenum type, GLsizei count, uint32_t base) {
    const uint32_t size = type_size(type);
    uint8_t *copy = malloc((size_t) count * size);
    if (!copy) return NULL;
    for (GLsizei i = 0; i < count; i++) {
        if (type == GL_UNSIGNED_BYTE) copy[i] = (uint8_t) (((const uint8_t *) indices)[i] - base);
        else if (type == GL_UNSIGNED_SHORT) ((uint16_t *) copy)[i] = (uint16_t) (((const uint16_t *) indices)[i] - base);
        else ((uint32_t *) copy)[i] = ((const uint32_t *) indices)[i] - base;
    }
    return copy;
}

GL_APICALL void GL_APIENTRY glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    if (count <= 0) return;

    const uint32_t index_bytes = (uint32_t) count * type_size(type);
    int buffer_arrays = 0;
    const int client = has_client_arrays(&buffer_arrays);

    if (vao->element_buffer) {
        if (client) {
            const buffer_state *b = buffer_get(vao->element_buffer);
            const uintptr_t offset = (uintptr_t) indices;
            if (b && b->shadow && offset + index_bytes <= b->size) {
                uint32_t lo = 0;
                uint32_t hi = 0;
                index_range(b->shadow + offset, type, count, &lo, &hi);
                upload_client_arrays(0, hi);
            }
        }
        const uint32_t w[5] = {mode, (uint32_t) count, type, 1, (uint32_t) (uintptr_t) indices};
        mugl_send(MUGL_OP_DRAW_ELEMENTS, w, 5);
        return;
    }

    if (!indices) return;

    const void *data = indices;
    void *rebased = NULL;
    if (client) {
        uint32_t lo = 0;
        uint32_t hi = 0;
        index_range(indices, type, count, &lo, &hi);
        if (!buffer_arrays && lo > 0) {
            rebased = rebase_indices(indices, type, count, lo);
            if (rebased) data = rebased;
            else lo = 0;
            upload_client_arrays(rebased ? lo : 0, hi);
        } else {
            upload_client_arrays(0, hi);
        }
    }

    const uint32_t w[5] = {mode, (uint32_t) count, type, 0, 0};
    send_words(MUGL_OP_DRAW_ELEMENTS, w, 5, data, index_bytes);
    free(rebased);
}

GL_APICALL void GL_APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    buffer_state *b = buffer_get(bound_buffer(target));
    if (b) {
        free(b->shadow);
        b->shadow = NULL;
        b->size = (uint32_t) size;
        if (target == GL_ELEMENT_ARRAY_BUFFER && size > 0) {
            b->shadow = malloc((size_t) size);
            if (b->shadow) {
                if (data) memcpy(b->shadow, data, (size_t) size);
                else memset(b->shadow, 0, (size_t) size);
            }
        }
    }

    const uint32_t w[4] = {target, (uint32_t) size, usage, data ? 1U : 0U};
    send_words(MUGL_OP_BUFFER_DATA, w, 4, data, data ? (uint32_t) size : 0U);
}

GL_APICALL void GL_APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    if (!data || size <= 0) return;
    buffer_state *b = buffer_get(bound_buffer(target));
    if (b && b->shadow && (uint32_t) offset + (uint32_t) size <= b->size) memcpy(b->shadow + offset, data, (size_t) size);

    const uint32_t w[3] = {target, (uint32_t) offset, (uint32_t) size};
    send_words(MUGL_OP_BUFFER_SUB_DATA, w, 3, data, (uint32_t) size);
}

GL_APICALL void *GL_APIENTRY glMapBufferOES(GLenum target, GLenum access) {
    (void) access;
    buffer_state *b = buffer_get(bound_buffer(target));
    if (!b || b->mapped || !b->size) {
        mugl_set_error(GL_INVALID_OPERATION);
        return NULL;
    }
    b->mapped = malloc(b->size);
    if (b->mapped && b->shadow) memcpy(b->mapped, b->shadow, b->size);
    return b->mapped;
}

GL_APICALL GLboolean GL_APIENTRY glUnmapBufferOES(GLenum target) {
    buffer_state *b = buffer_get(bound_buffer(target));
    if (!b || !b->mapped) {
        mugl_set_error(GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    uint8_t *mapped = b->mapped;
    b->mapped = NULL;
    glBufferSubData(target, 0, (GLsizeiptr) b->size, mapped);
    free(mapped);
    return GL_TRUE;
}

GL_APICALL void GL_APIENTRY glGetBufferPointervOES(GLenum target, GLenum pname, void **params) {
    const buffer_state *b = buffer_get(bound_buffer(target));
    *params = (pname == GL_BUFFER_MAP_POINTER_OES && b) ? b->mapped : NULL;
}

static void gen_objects(uint32_t kind, GLsizei n, GLuint *out) {
    if (n <= 0) return;
    const uint32_t w[2] = {kind, (uint32_t) n};
    uint32_t len = 0;
    const uint8_t *r = call_words(MUGL_OP_GEN, w, 2, NULL, 0, &len);
    const uint32_t bytes = (uint32_t) n * 4U;
    memcpy(out, r, len < bytes ? len : bytes);
    mugl_unlock();
}

static void delete_objects(uint32_t kind, GLsizei n, const GLuint *ids) {
    if (n <= 0 || !ids) return;
    const uint32_t w[2] = {kind, (uint32_t) n};
    send_words(MUGL_OP_DELETE, w, 2, ids, (uint32_t) n * 4U);
}

GL_APICALL void GL_APIENTRY glGenBuffers(GLsizei n, GLuint *buffers_out) {
    gen_objects(MUGL_GEN_BUFFERS, n, buffers_out);
}

GL_APICALL void GL_APIENTRY glGenTextures(GLsizei n, GLuint *textures) {
    gen_objects(MUGL_GEN_TEXTURES, n, textures);
}

GL_APICALL void GL_APIENTRY glGenFramebuffers(GLsizei n, GLuint *framebuffers) {
    gen_objects(MUGL_GEN_FRAMEBUFFERS, n, framebuffers);
}

GL_APICALL void GL_APIENTRY glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) {
    gen_objects(MUGL_GEN_RENDERBUFFERS, n, renderbuffers);
}

GL_APICALL void GL_APIENTRY glDeleteBuffers(GLsizei n, const GLuint *ids) {
    for (GLsizei i = 0; ids && i < n; i++) {
        buffer_state *b = buffer_get(ids[i]);
        if (b) {
            free(b->shadow);
            free(b->mapped);
            memset(b, 0, sizeof(*b));
        }
        if (ids[i] && ids[i] == array_buffer) array_buffer = 0;
        if (ids[i] && ids[i] == vao->element_buffer) vao->element_buffer = 0;
        for (int a = 0; a < MAX_ATTRIBS; a++) {
            if (ids[i] && vao->attribs[a].buffer == ids[i]) vao->attribs[a].buffer = 0;
        }
    }
    delete_objects(MUGL_GEN_BUFFERS, n, ids);
}

GL_APICALL void GL_APIENTRY glDeleteTextures(GLsizei n, const GLuint *ids) {
    delete_objects(MUGL_GEN_TEXTURES, n, ids);
}

GL_APICALL void GL_APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint *ids) {
    delete_objects(MUGL_GEN_FRAMEBUFFERS, n, ids);
}

GL_APICALL void GL_APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint *ids) {
    delete_objects(MUGL_GEN_RENDERBUFFERS, n, ids);
}

GL_APICALL void GL_APIENTRY glGenVertexArraysOES(GLsizei n, GLuint *arrays) {
    if (n <= 0) return;
    const uint32_t w[1] = {(uint32_t) n};
    uint32_t len = 0;
    const uint8_t *r = call_words(MUGL_OP_GEN_VERTEX_ARRAYS, w, 1, NULL, 0, &len);
    const uint32_t bytes = (uint32_t) n * 4U;
    memcpy(arrays, r, len < bytes ? len : bytes);
    mugl_unlock();
    for (GLsizei i = 0; i < n; i++) vao_get(arrays[i], 1);
}

GL_APICALL void GL_APIENTRY glDeleteVertexArraysOES(GLsizei n, const GLuint *arrays) {
    if (n <= 0 || !arrays) return;
    for (GLsizei i = 0; i < n; i++) {
        vao_state *v = vao_get(arrays[i], 0);
        if (!v || v == &default_vao) continue;
        if (v == vao) vao = &default_vao;
        free(v);
        vaos[arrays[i]] = NULL;
    }
    const uint32_t w[1] = {(uint32_t) n};
    send_words(MUGL_OP_DELETE_VERTEX_ARRAYS, w, 1, arrays, (uint32_t) n * 4U);
}

GL_APICALL void GL_APIENTRY glBindVertexArrayOES(GLuint array) {
    vao_state *v = vao_get(array, 1);
    vao = v ? v : &default_vao;
    const uint32_t w[1] = {array};
    mugl_send(MUGL_OP_BIND_VERTEX_ARRAY, w, 1);
}

GL_APICALL GLboolean GL_APIENTRY glIsVertexArrayOES(GLuint array) {
    const uint32_t w[1] = {array};
    return (GLboolean) mugl_call_u32(MUGL_OP_IS_VERTEX_ARRAY, w, 1);
}

GL_APICALL void GL_APIENTRY glDiscardFramebufferEXT(GLenum target, GLsizei count, const GLenum *attachments) {
    if (count <= 0 || !attachments) return;
    const uint32_t w[2] = {target, (uint32_t) count};
    send_words(MUGL_OP_DISCARD_FRAMEBUFFER, w, 2, attachments, (uint32_t) count * 4U);
}

GL_APICALL void GL_APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                         GLsizei height, GLint border, GLenum format, GLenum type,
                                         const void *pixels) {
    const uint32_t len = pixels ? image_size(width, height, format, type, unpack_alignment) : 0U;
    const uint32_t w[9] = {target, (uint32_t) level, (uint32_t) internalformat, (uint32_t) width, (uint32_t) height,
                           (uint32_t) border, format, type, pixels ? 1U : 0U};
    send_words(MUGL_OP_TEX_IMAGE_2D, w, 9, pixels, len);
}

GL_APICALL void GL_APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                            GLsizei height, GLenum format, GLenum type, const void *pixels) {
    if (!pixels) return;
    const uint32_t len = image_size(width, height, format, type, unpack_alignment);
    const uint32_t w[8] = {target, (uint32_t) level, (uint32_t) xoffset, (uint32_t) yoffset, (uint32_t) width,
                           (uint32_t) height, format, type};
    send_words(MUGL_OP_TEX_SUB_IMAGE_2D, w, 8, pixels, len);
}

GL_APICALL void GL_APIENTRY glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                                                   GLsizei height, GLint border, GLsizei imageSize,
                                                   const void *data) {
    const uint32_t len = data && imageSize > 0 ? (uint32_t) imageSize : 0U;
    const uint32_t w[8] = {target, (uint32_t) level, internalformat, (uint32_t) width, (uint32_t) height,
                           (uint32_t) border, (uint32_t) imageSize, data ? 1U : 0U};
    send_words(MUGL_OP_COMPRESSED_TEX_IMAGE_2D, w, 8, data, len);
}

GL_APICALL void GL_APIENTRY glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                      GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
                                                      const void *data) {
    if (!data || imageSize <= 0) return;
    const uint32_t w[8] = {target, (uint32_t) level, (uint32_t) xoffset, (uint32_t) yoffset, (uint32_t) width,
                           (uint32_t) height, format, (uint32_t) imageSize};
    send_words(MUGL_OP_COMPRESSED_TEX_SUB_IMAGE_2D, w, 8, data, (uint32_t) imageSize);
}

GL_APICALL void GL_APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                                         void *pixels) {
    const uint32_t size = image_size(width, height, format, type, pack_alignment);
    if (!pixels || !size) return;
    if (size > MUGL_RESP_SIZE) {
        mugl_set_error(GL_OUT_OF_MEMORY);
        return;
    }
    const uint32_t w[7] = {(uint32_t) x, (uint32_t) y, (uint32_t) width, (uint32_t) height, format, type, size};
    uint32_t len = 0;
    const uint8_t *r = call_words(MUGL_OP_READ_PIXELS, w, 7, NULL, 0, &len);
    memcpy(pixels, r, len < size ? len : size);
    mugl_unlock();
}

GL_APICALL void GL_APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) {
    if (params) glTexParameterf(target, pname, params[0]);
}

GL_APICALL void GL_APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *params) {
    if (params) glTexParameteri(target, pname, params[0]);
}

GL_APICALL void GL_APIENTRY glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string,
                                           const GLint *length) {
    if (count <= 0 || !string) return;

    uint32_t total = 0;
    for (GLsizei i = 0; i < count; i++) {
        if (!string[i]) continue;
        total += length && length[i] >= 0 ? (uint32_t) length[i] : (uint32_t) strlen(string[i]);
    }

    const uint32_t w[2] = {shader, total};
    mugl_msg_begin(MUGL_OP_SHADER_SOURCE, 8U + total);
    mugl_msg_put(w, 8);
    for (GLsizei i = 0; i < count; i++) {
        if (!string[i]) continue;
        const uint32_t len = length && length[i] >= 0 ? (uint32_t) length[i] : (uint32_t) strlen(string[i]);
        mugl_msg_put(string[i], len);
    }
    mugl_msg_end();
}

static void copy_string(const uint8_t *r, uint32_t len, GLsizei bufSize, GLsizei *length, GLchar *out) {
    uint32_t n = len;
    if (bufSize <= 0) n = 0;
    else if (n > (uint32_t) bufSize - 1U) n = (uint32_t) bufSize - 1U;
    if (out && bufSize > 0) {
        memcpy(out, r, n);
        out[n] = '\0';
    }
    if (length) *length = (GLsizei) n;
}

static void get_text(uint32_t op, uint32_t kind, GLuint object, GLsizei bufSize, GLsizei *length, GLchar *out) {
    const uint32_t w[2] = {kind, object};
    uint32_t len = 0;
    const uint8_t *r = call_words(op, w, 2, NULL, 0, &len);
    copy_string(r, len, bufSize, length, out);
    mugl_unlock();
}

GL_APICALL void GL_APIENTRY glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source) {
    get_text(MUGL_OP_GET_SHADER_SOURCE, 0, shader, bufSize, length, source);
}

GL_APICALL void GL_APIENTRY glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    get_text(MUGL_OP_GET_INFO_LOG, MUGL_LOG_SHADER, shader, bufSize, length, infoLog);
}

GL_APICALL void GL_APIENTRY glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    get_text(MUGL_OP_GET_INFO_LOG, MUGL_LOG_PROGRAM, program, bufSize, length, infoLog);
}

static void get_active(uint32_t kind, GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size,
                       GLenum *type, GLchar *name) {
    const uint32_t w[3] = {kind, program, index};
    uint32_t len = 0;
    const uint8_t *r = call_words(MUGL_OP_GET_ACTIVE, w, 3, NULL, 0, &len);
    if (len >= 8) {
        int32_t s;
        uint32_t t;
        memcpy(&s, r, 4);
        memcpy(&t, r + 4, 4);
        if (size) *size = s;
        if (type) *type = t;
        copy_string(r + 8, len - 8, bufSize, length, name);
    } else {
        copy_string(r, 0, bufSize, length, name);
    }
    mugl_unlock();
}

GL_APICALL void GL_APIENTRY glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length,
                                              GLint *size, GLenum *type, GLchar *name) {
    get_active(MUGL_ACTIVE_ATTRIB, program, index, bufSize, length, size, type, name);
}

GL_APICALL void GL_APIENTRY glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length,
                                               GLint *size, GLenum *type, GLchar *name) {
    get_active(MUGL_ACTIVE_UNIFORM, program, index, bufSize, length, size, type, name);
}

GL_APICALL void GL_APIENTRY glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders) {
    const uint32_t w[1] = {program};
    uint32_t len = 0;
    const uint8_t *r = call_words(MUGL_OP_GET_ATTACHED_SHADERS, w, 1, NULL, 0, &len);
    GLsizei n = (GLsizei) (len / 4U);
    if (n > maxCount) n = maxCount;
    if (shaders && n > 0) memcpy(shaders, r, (size_t) n * 4U);
    if (count) *count = n;
    mugl_unlock();
}

GL_APICALL void GL_APIENTRY glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) {
    if (!name) return;
    const uint32_t w[2] = {program, index};
    send_words(MUGL_OP_BIND_ATTRIB_LOCATION, w, 2, name, (uint32_t) strlen(name));
}

static GLint get_location(uint32_t kind, GLuint program, const GLchar *name) {
    if (!name) return -1;
    const uint32_t w[2] = {kind, program};
    uint32_t len = 0;
    const uint8_t *r = call_words(MUGL_OP_GET_LOCATION, w, 2, name, (uint32_t) strlen(name), &len);
    int32_t location = -1;
    if (len >= 4) memcpy(&location, r, 4);
    mugl_unlock();
    return location;
}

GL_APICALL GLint GL_APIENTRY glGetAttribLocation(GLuint program, const GLchar *name) {
    return get_location(MUGL_LOC_ATTRIB, program, name);
}

GL_APICALL GLint GL_APIENTRY glGetUniformLocation(GLuint program, const GLchar *name) {
    return get_location(MUGL_LOC_UNIFORM, program, name);
}

static void uniform_v(uint32_t shape, GLint location, GLsizei count, const void *value) {
    if (count <= 0 || !value) return;
    const uint32_t components = shape & 0xFFU;
    const uint32_t w[3] = {shape, (uint32_t) location, (uint32_t) count};
    send_words(MUGL_OP_UNIFORM_V, w, 3, value, (uint32_t) count * components * 4U);
}

#define UNIFORM_SHAPE(n, is_int) ((uint32_t) (n) | ((is_int) ? 0x100U : 0U))

GL_APICALL void GL_APIENTRY glUniform1fv(GLint location, GLsizei count, const GLfloat *value) {
    uniform_v(UNIFORM_SHAPE(1, 0), location, count, value);
}

GL_APICALL void GL_APIENTRY glUniform2fv(GLint location, GLsizei count, const GLfloat *value) {
    uniform_v(UNIFORM_SHAPE(2, 0), location, count, value);
}

GL_APICALL void GL_APIENTRY glUniform3fv(GLint location, GLsizei count, const GLfloat *value) {
    uniform_v(UNIFORM_SHAPE(3, 0), location, count, value);
}

GL_APICALL void GL_APIENTRY glUniform4fv(GLint location, GLsizei count, const GLfloat *value) {
    uniform_v(UNIFORM_SHAPE(4, 0), location, count, value);
}

GL_APICALL void GL_APIENTRY glUniform1iv(GLint location, GLsizei count, const GLint *value) {
    uniform_v(UNIFORM_SHAPE(1, 1), location, count, value);
}

GL_APICALL void GL_APIENTRY glUniform2iv(GLint location, GLsizei count, const GLint *value) {
    uniform_v(UNIFORM_SHAPE(2, 1), location, count, value);
}

GL_APICALL void GL_APIENTRY glUniform3iv(GLint location, GLsizei count, const GLint *value) {
    uniform_v(UNIFORM_SHAPE(3, 1), location, count, value);
}

GL_APICALL void GL_APIENTRY glUniform4iv(GLint location, GLsizei count, const GLint *value) {
    uniform_v(UNIFORM_SHAPE(4, 1), location, count, value);
}

static void uniform_matrix(uint32_t n, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    if (count <= 0 || !value) return;
    const uint32_t w[4] = {n, (uint32_t) location, (uint32_t) count, transpose};
    send_words(MUGL_OP_UNIFORM_MATRIX_V, w, 4, value, (uint32_t) count * n * n * 4U);
}

GL_APICALL void GL_APIENTRY glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose,
                                               const GLfloat *value) {
    uniform_matrix(2, location, count, transpose, value);
}

GL_APICALL void GL_APIENTRY glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose,
                                               const GLfloat *value) {
    uniform_matrix(3, location, count, transpose, value);
}

GL_APICALL void GL_APIENTRY glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                                               const GLfloat *value) {
    uniform_matrix(4, location, count, transpose, value);
}

static void vertex_attrib_v(uint32_t n, GLuint index, const GLfloat *v) {
    if (!v) return;
    uint32_t w[6] = {n, index, 0, 0, 0, f2u(1.0f)};
    for (uint32_t i = 0; i < n; i++) w[2 + i] = f2u(v[i]);
    mugl_send(MUGL_OP_VERTEX_ATTRIB_V, w, 6);
}

GL_APICALL void GL_APIENTRY glVertexAttrib1fv(GLuint index, const GLfloat *v) {
    vertex_attrib_v(1, index, v);
}

GL_APICALL void GL_APIENTRY glVertexAttrib2fv(GLuint index, const GLfloat *v) {
    vertex_attrib_v(2, index, v);
}

GL_APICALL void GL_APIENTRY glVertexAttrib3fv(GLuint index, const GLfloat *v) {
    vertex_attrib_v(3, index, v);
}

GL_APICALL void GL_APIENTRY glVertexAttrib4fv(GLuint index, const GLfloat *v) {
    vertex_attrib_v(4, index, v);
}

static void get_v(uint32_t kind, GLenum pname, void *out) {
    const uint32_t w[2] = {kind, pname};
    uint32_t len = 0;
    const uint8_t *r = call_words(MUGL_OP_GET_V, w, 2, NULL, 0, &len);
    if (len >= 4 && out) {
        uint32_t count;
        memcpy(&count, r, 4);
        const uint32_t item = kind == MUGL_GET_BOOLEAN ? 1U : 4U;
        if (4U + count * item <= len) memcpy(out, r + 4, count * item);
    }
    mugl_unlock();
}

GL_APICALL void GL_APIENTRY glGetBooleanv(GLenum pname, GLboolean *data) {
    get_v(MUGL_GET_BOOLEAN, pname, data);
}

GL_APICALL void GL_APIENTRY glGetIntegerv(GLenum pname, GLint *data) {
    get_v(MUGL_GET_INTEGER, pname, data);
}

GL_APICALL void GL_APIENTRY glGetFloatv(GLenum pname, GLfloat *data) {
    get_v(MUGL_GET_FLOAT, pname, data);
}

static void get_obj_v(uint32_t kind, uint32_t object, uint32_t pname, uint32_t extra, void *out) {
    const uint32_t w[4] = {kind, object, pname, extra};
    uint32_t len = 0;
    const uint8_t *r = call_words(MUGL_OP_GET_OBJ_V, w, 4, NULL, 0, &len);
    if (len >= 4 && out) {
        uint32_t count;
        memcpy(&count, r, 4);
        if (4U + count * 4U <= len) memcpy(out, r + 4, count * 4U);
    }
    mugl_unlock();
}

GL_APICALL void GL_APIENTRY glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    get_obj_v(MUGL_OBJ_BUFFER_PARAM, target, pname, 0, params);
}

GL_APICALL void GL_APIENTRY glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname,
                                                                  GLint *params) {
    get_obj_v(MUGL_OBJ_FRAMEBUFFER_ATTACHMENT, target, pname, attachment, params);
}

GL_APICALL void GL_APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
    get_obj_v(MUGL_OBJ_PROGRAM, program, pname, 0, params);
}

GL_APICALL void GL_APIENTRY glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    get_obj_v(MUGL_OBJ_RENDERBUFFER, target, pname, 0, params);
}

GL_APICALL void GL_APIENTRY glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
    get_obj_v(MUGL_OBJ_SHADER, shader, pname, 0, params);
}

GL_APICALL void GL_APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params) {
    get_obj_v(MUGL_OBJ_TEX_PARAM_F, target, pname, 0, params);
}

GL_APICALL void GL_APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {
    get_obj_v(MUGL_OBJ_TEX_PARAM_I, target, pname, 0, params);
}

GL_APICALL void GL_APIENTRY glGetUniformfv(GLuint program, GLint location, GLfloat *params) {
    get_obj_v(MUGL_OBJ_UNIFORM_F, program, (uint32_t) location, 0, params);
}

GL_APICALL void GL_APIENTRY glGetUniformiv(GLuint program, GLint location, GLint *params) {
    get_obj_v(MUGL_OBJ_UNIFORM_I, program, (uint32_t) location, 0, params);
}

GL_APICALL void GL_APIENTRY glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params) {
    get_obj_v(MUGL_OBJ_VERTEX_ATTRIB_F, index, pname, 0, params);
}

GL_APICALL void GL_APIENTRY glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params) {
    get_obj_v(MUGL_OBJ_VERTEX_ATTRIB_I, index, pname, 0, params);
}

GL_APICALL void GL_APIENTRY glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range,
                                                       GLint *precision) {
    const uint32_t w[2] = {shadertype, precisiontype};
    uint32_t len = 0;
    const uint8_t *r = call_words(MUGL_OP_SHADER_PRECISION, w, 2, NULL, 0, &len);
    if (len >= 12) {
        if (range) memcpy(range, r, 8);
        if (precision) memcpy(precision, r + 8, 4);
    }
    mugl_unlock();
}

GL_APICALL void GL_APIENTRY glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryFormat,
                                           const void *binary, GLsizei length) {
    if (count <= 0 || !shaders || !binary || length < 0) return;
    const uint32_t w[3] = {(uint32_t) count, binaryFormat, (uint32_t) length};
    mugl_msg_begin(MUGL_OP_SHADER_BINARY, 12U + (uint32_t) count * 4U + (uint32_t) length);
    mugl_msg_put(w, 12);
    mugl_msg_put(shaders, (uint32_t) count * 4U);
    mugl_msg_put(binary, (uint32_t) length);
    mugl_msg_end();
}

static int extension_supported(const char *name, size_t len) {
    for (size_t i = 0; i < sizeof(supported_extensions) / sizeof(supported_extensions[0]); i++) {
        if (strlen(supported_extensions[i]) == len && strncmp(supported_extensions[i], name, len) == 0) return 1;
    }
    return 0;
}

static char *filter_extensions(const char *all) {
    char *out = calloc(1, strlen(all) + 64);
    if (!out) return NULL;
    const char *p = all;
    while (*p) {
        while (*p == ' ') p++;
        const char *start = p;
        while (*p && *p != ' ') p++;
        const size_t len = (size_t) (p - start);
        if (len && extension_supported(start, len) && !strstr(out, start)) {
            strncat(out, start, len);
            strcat(out, " ");
        }
    }
    if (!strstr(out, "GL_OES_vertex_array_object")) strcat(out, "GL_OES_vertex_array_object ");
    if (!strstr(out, "GL_OES_mapbuffer")) strcat(out, "GL_OES_mapbuffer ");
    return out;
}

GL_APICALL const GLubyte *GL_APIENTRY glGetString(GLenum name) {
    int slot;
    switch (name) {
        case GL_VENDOR:
            slot = 0;
            break;
        case GL_RENDERER:
            slot = 1;
            break;
        case GL_VERSION:
            slot = 2;
            break;
        case GL_SHADING_LANGUAGE_VERSION:
            slot = 3;
            break;
        case GL_EXTENSIONS:
            slot = 4;
            break;
        default:
            mugl_set_error(GL_INVALID_ENUM);
            return NULL;
    }

    mugl_lock();
    if (!strings[slot]) {
        const uint32_t w[1] = {name};
        uint32_t len = 0;
        const uint8_t *r = call_words(MUGL_OP_GET_STRING, w, 1, NULL, 0, &len);
        char *server = calloc(1, len + 1);
        if (server) memcpy(server, r, len);
        mugl_unlock();

        if (server) {
            if (slot == 2) {
                strings[slot] = calloc(1, len + 64);
                if (strings[slot]) snprintf(strings[slot], len + 64, "OpenGL ES 2.0 mugl (%s)", server);
                free(server);
            } else if (slot == 3) {
                strings[slot] = strdup("OpenGL ES GLSL ES 1.00");
                free(server);
            } else if (slot == 4) {
                strings[slot] = filter_extensions(server);
                free(server);
            } else {
                strings[slot] = server;
            }
        }
    }
    mugl_unlock();
    return (const GLubyte *) strings[slot];
}

static const mugl_proc extension_procs[] = {
    {"glGenVertexArraysOES", (void (*)(void)) glGenVertexArraysOES},
    {"glDeleteVertexArraysOES", (void (*)(void)) glDeleteVertexArraysOES},
    {"glBindVertexArrayOES", (void (*)(void)) glBindVertexArrayOES},
    {"glIsVertexArrayOES", (void (*)(void)) glIsVertexArrayOES},
    {"glDiscardFramebufferEXT", (void (*)(void)) glDiscardFramebufferEXT},
    {"glMapBufferOES", (void (*)(void)) glMapBufferOES},
    {"glUnmapBufferOES", (void (*)(void)) glUnmapBufferOES},
    {"glGetBufferPointervOES", (void (*)(void)) glGetBufferPointervOES},
};

static void (*lookup_exact(const char *name, size_t len))(void) {
    for (unsigned i = 0; i < mugl_gl_proc_count; i++) {
        if (strlen(mugl_gl_procs[i].name) == len && strncmp(mugl_gl_procs[i].name, name, len) == 0) {
            return mugl_gl_procs[i].func;
        }
    }
    for (size_t i = 0; i < sizeof(extension_procs) / sizeof(extension_procs[0]); i++) {
        if (strlen(extension_procs[i].name) == len && strncmp(extension_procs[i].name, name, len) == 0) {
            return extension_procs[i].func;
        }
    }
    return NULL;
}

void (*mugl_gl_lookup(const char *name))(void) {
    const size_t len = strlen(name);
    void (*func)(void) = lookup_exact(name, len);
    if (func) return func;

    static const char *const suffixes[] = {"OES", "EXT", "ARB"};
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        if (len > 3 && strcmp(name + len - 3, suffixes[i]) == 0) {
            func = lookup_exact(name, len - 3);
            if (func) return func;
        }
    }

    if (getenv("MUGL_DEBUG")) fprintf(stderr, "mugl: no function %s\n", name);
    return NULL;
}
