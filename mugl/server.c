#define _GNU_SOURCE
#include <errno.h>
#include <linux/futex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "server.h"
#include "gen_ops.h"

#define SPIN_LIMIT  4000
#define WAIT_NS     50000000L
#define MAX_ATTRIBS 16
#define GET_MAX     256

#define GLP(name) extern __typeof__(name) *p_##name
GLP(glBindBuffer);
GLP(glBufferData);
GLP(glBufferSubData);
GLP(glCompressedTexImage2D);
GLP(glCompressedTexSubImage2D);
GLP(glDeleteBuffers);
GLP(glDeleteFramebuffers);
GLP(glDeleteRenderbuffers);
GLP(glDeleteTextures);
GLP(glDrawArrays);
GLP(glDrawElements);
GLP(glFinish);
GLP(glGenBuffers);
GLP(glGenFramebuffers);
GLP(glGenRenderbuffers);
GLP(glGenTextures);
GLP(glGetActiveAttrib);
GLP(glGetActiveUniform);
GLP(glGetAttachedShaders);
GLP(glGetAttribLocation);
GLP(glGetBooleanv);
GLP(glGetBufferParameteriv);
GLP(glGetFloatv);
GLP(glGetFramebufferAttachmentParameteriv);
GLP(glGetIntegerv);
GLP(glGetProgramInfoLog);
GLP(glGetProgramiv);
GLP(glGetRenderbufferParameteriv);
GLP(glGetShaderInfoLog);
GLP(glGetShaderPrecisionFormat);
GLP(glGetShaderSource);
GLP(glGetShaderiv);
GLP(glGetString);
GLP(glGetTexParameterfv);
GLP(glGetTexParameteriv);
GLP(glGetUniformLocation);
GLP(glGetUniformfv);
GLP(glGetUniformiv);
GLP(glGetVertexAttribfv);
GLP(glGetVertexAttribiv);
GLP(glBindAttribLocation);
GLP(glReadPixels);
GLP(glShaderBinary);
GLP(glShaderSource);
GLP(glTexImage2D);
GLP(glTexSubImage2D);
GLP(glUniform1fv);
GLP(glUniform2fv);
GLP(glUniform3fv);
GLP(glUniform4fv);
GLP(glUniform1iv);
GLP(glUniform2iv);
GLP(glUniform3iv);
GLP(glUniform4iv);
GLP(glUniformMatrix2fv);
GLP(glUniformMatrix3fv);
GLP(glUniformMatrix4fv);
GLP(glVertexAttrib4f);
GLP(glVertexAttribPointer);
GLP(glBindFramebuffer);
GLP(glPixelStorei);

static PFNGLGENVERTEXARRAYSOESPROC p_glGenVertexArraysOES;
static PFNGLDELETEVERTEXARRAYSOESPROC p_glDeleteVertexArraysOES;
static PFNGLBINDVERTEXARRAYOESPROC p_glBindVertexArrayOES;
static PFNGLISVERTEXARRAYOESPROC p_glIsVertexArrayOES;
static PFNGLDISCARDFRAMEBUFFEREXTPROC p_glDiscardFramebufferEXT;

static mugl_header *hdr;
static uint8_t *ring;
static uint8_t *resp;
static uint8_t *payload;
static uint32_t payload_capacity;
static void *client_arrays[MAX_ATTRIBS];
static uint32_t client_array_size[MAX_ATTRIBS];
static SDL_Window *window;
static SDL_GLContext context;

static long futex(volatile uint32_t *addr, int op, uint32_t val, const struct timespec *ts) {
    return syscall(SYS_futex, addr, op, val, ts, NULL, 0);
}

static void finish(int code) {
    if (hdr) {
        __atomic_store_n(&hdr->server_exited, 1, __ATOMIC_RELEASE);
        futex(&hdr->resp_seq, FUTEX_WAKE, 1, NULL);
        futex(&hdr->rpos, FUTEX_WAKE, 1, NULL);
        futex(&hdr->swap_seq, FUTEX_WAKE, 1, NULL);
    }
    if (context) SDL_GL_DeleteContext(context);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    exit(code);
}

static void fail_start(const char *message) {
    if (hdr) {
        snprintf(hdr->error, sizeof(hdr->error), "%s", message);
        __atomic_store_n(&hdr->failed, 1, __ATOMIC_RELEASE);
        futex(&hdr->ready, FUTEX_WAKE, 1, NULL);
    }
    fprintf(stderr, "mugl-server: %s\n", message);
    finish(1);
}

static void check_client(void) {
    if (kill((pid_t) hdr->client_pid, 0) != 0 && errno == ESRCH) finish(0);
    if (getppid() == 1) finish(0);
}

float server_f(uint32_t value) {
    float f;
    memcpy(&f, &value, 4);
    return f;
}

void server_reply(const void *data, uint32_t len) {
    if (len > MUGL_RESP_SIZE) len = MUGL_RESP_SIZE;
    if (data && len && data != resp) memcpy(resp, data, len);
    hdr->resp_len = len;
    __atomic_add_fetch(&hdr->resp_seq, 1, __ATOMIC_RELEASE);
    futex(&hdr->resp_seq, FUTEX_WAKE, 1, NULL);
}

void server_reply_u32(uint32_t value) {
    server_reply(&value, 4);
}

int server_missing(const char *name) {
    char message[128];
    snprintf(message, sizeof(message), "GL function %s is missing", name);
    fail_start(message);
    return 0;
}

static uint32_t wait_data(void) {
    int spins = 0;
    for (;;) {
        const uint32_t avail = __atomic_load_n(&hdr->wpos, __ATOMIC_ACQUIRE) - hdr->rpos;
        if (avail) return avail;
        if (++spins < SPIN_LIMIT) continue;

        const uint32_t seen = __atomic_load_n(&hdr->wpos, __ATOMIC_ACQUIRE);
        __atomic_store_n(&hdr->reader_sleeping, 1, __ATOMIC_SEQ_CST);
        if (__atomic_load_n(&hdr->wpos, __ATOMIC_ACQUIRE) == hdr->rpos) {
            const struct timespec ts = {0, WAIT_NS};
            if (futex(&hdr->wpos, FUTEX_WAIT, seen, &ts) != 0 && errno == ETIMEDOUT) check_client();
        }
        __atomic_store_n(&hdr->reader_sleeping, 0, __ATOMIC_RELAXED);
        spins = 0;
    }
}

static void ring_read(void *dst, uint32_t len) {
    uint8_t *out = dst;
    while (len) {
        const uint32_t avail = wait_data();
        const uint32_t r = hdr->rpos;
        const uint32_t offset = r & (MUGL_RING_SIZE - 1);
        uint32_t chunk = MUGL_RING_SIZE - offset;
        if (chunk > avail) chunk = avail;
        if (chunk > len) chunk = len;
        if (out) {
            memcpy(out, ring + offset, chunk);
            out += chunk;
        }
        __atomic_store_n(&hdr->rpos, r + chunk, __ATOMIC_RELEASE);
        len -= chunk;

        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        if (__atomic_load_n(&hdr->writer_sleeping, __ATOMIC_RELAXED)) futex(&hdr->rpos, FUTEX_WAKE, 1, NULL);
    }
}

static int payload_reserve(uint32_t len) {
    if (len + 1U <= payload_capacity) return 1;
    uint32_t capacity = payload_capacity ? payload_capacity : 65536U;
    while (capacity < len + 1U) capacity *= 2U;
    uint8_t *grown = realloc(payload, capacity);
    if (!grown) return 0;
    payload = grown;
    payload_capacity = capacity;
    return 1;
}

static uint32_t get_count(GLenum pname) {
    GLint n = 0;
    switch (pname) {
        case GL_COMPRESSED_TEXTURE_FORMATS:
            p_glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &n);
            return n > 0 && n < GET_MAX ? (uint32_t) n : 0U;
        case GL_SHADER_BINARY_FORMATS:
            p_glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &n);
            return n > 0 && n < GET_MAX ? (uint32_t) n : 0U;
        case GL_VIEWPORT:
        case GL_SCISSOR_BOX:
        case GL_COLOR_CLEAR_VALUE:
        case GL_COLOR_WRITEMASK:
        case GL_BLEND_COLOR:
            return 4;
        case GL_ALIASED_LINE_WIDTH_RANGE:
        case GL_ALIASED_POINT_SIZE_RANGE:
        case GL_DEPTH_RANGE:
        case GL_MAX_VIEWPORT_DIMS:
            return 2;
        default:
            return 1;
    }
}

static void handle_get_v(const uint32_t *a) {
    const uint32_t kind = a[0];
    const GLenum pname = a[1];
    uint32_t out[1 + GET_MAX];
    memset(out, 0, sizeof(out));
    const uint32_t count = get_count(pname);
    out[0] = count;
    if (kind == MUGL_GET_BOOLEAN) p_glGetBooleanv(pname, (GLboolean *) &out[1]);
    else if (kind == MUGL_GET_FLOAT) p_glGetFloatv(pname, (GLfloat *) &out[1]);
    else p_glGetIntegerv(pname, (GLint *) &out[1]);
    server_reply(out, 4U + count * (kind == MUGL_GET_BOOLEAN ? 1U : 4U));
}

static uint32_t uniform_count(GLuint program, GLint location, int is_int) {
    uint32_t probe_a[16];
    uint32_t probe_b[16];
    for (int i = 0; i < 16; i++) {
        probe_a[i] = 0x7FC0DEADU;
        probe_b[i] = 0x12345679U;
    }
    if (is_int) {
        p_glGetUniformiv(program, location, (GLint *) probe_a);
        p_glGetUniformiv(program, location, (GLint *) probe_b);
    } else {
        p_glGetUniformfv(program, location, (GLfloat *) probe_a);
        p_glGetUniformfv(program, location, (GLfloat *) probe_b);
    }
    uint32_t count = 1;
    for (uint32_t i = 0; i < 16; i++) {
        if (probe_a[i] != 0x7FC0DEADU || probe_b[i] != 0x12345679U) count = i + 1U;
    }
    return count;
}

static void handle_get_obj_v(const uint32_t *a) {
    const uint32_t kind = a[0];
    const uint32_t object = a[1];
    const uint32_t pname = a[2];
    const uint32_t extra = a[3];
    uint32_t out[17];
    memset(out, 0, sizeof(out));
    uint32_t count = 1;

    switch (kind) {
        case MUGL_OBJ_BUFFER_PARAM:
            p_glGetBufferParameteriv(object, pname, (GLint *) &out[1]);
            break;
        case MUGL_OBJ_FRAMEBUFFER_ATTACHMENT:
            p_glGetFramebufferAttachmentParameteriv(object, extra, pname, (GLint *) &out[1]);
            break;
        case MUGL_OBJ_PROGRAM:
            p_glGetProgramiv(object, pname, (GLint *) &out[1]);
            break;
        case MUGL_OBJ_RENDERBUFFER:
            p_glGetRenderbufferParameteriv(object, pname, (GLint *) &out[1]);
            break;
        case MUGL_OBJ_SHADER:
            p_glGetShaderiv(object, pname, (GLint *) &out[1]);
            break;
        case MUGL_OBJ_TEX_PARAM_F:
            p_glGetTexParameterfv(object, pname, (GLfloat *) &out[1]);
            break;
        case MUGL_OBJ_TEX_PARAM_I:
            p_glGetTexParameteriv(object, pname, (GLint *) &out[1]);
            break;
        case MUGL_OBJ_UNIFORM_F:
            count = uniform_count(object, (GLint) pname, 0);
            p_glGetUniformfv(object, (GLint) pname, (GLfloat *) &out[1]);
            break;
        case MUGL_OBJ_UNIFORM_I:
            count = uniform_count(object, (GLint) pname, 1);
            p_glGetUniformiv(object, (GLint) pname, (GLint *) &out[1]);
            break;
        case MUGL_OBJ_VERTEX_ATTRIB_F:
            count = pname == GL_CURRENT_VERTEX_ATTRIB ? 4U : 1U;
            p_glGetVertexAttribfv(object, pname, (GLfloat *) &out[1]);
            break;
        case MUGL_OBJ_VERTEX_ATTRIB_I:
            count = pname == GL_CURRENT_VERTEX_ATTRIB ? 4U : 1U;
            p_glGetVertexAttribiv(object, pname, (GLint *) &out[1]);
            break;
        default:
            count = 0;
            break;
    }

    out[0] = count;
    server_reply(out, 4U + count * 4U);
}

static void handle_gen(const uint32_t *a) {
    const uint32_t kind = a[0];
    GLsizei n = (GLsizei) a[1];
    if (n < 0) n = 0;
    if ((uint32_t) n > MUGL_RESP_SIZE / 4U) n = (GLsizei) (MUGL_RESP_SIZE / 4U);
    GLuint *ids = (GLuint *) resp;
    memset(ids, 0, (size_t) n * 4U);
    switch (kind) {
        case MUGL_GEN_BUFFERS:
            p_glGenBuffers(n, ids);
            break;
        case MUGL_GEN_TEXTURES:
            p_glGenTextures(n, ids);
            break;
        case MUGL_GEN_FRAMEBUFFERS:
            p_glGenFramebuffers(n, ids);
            break;
        case MUGL_GEN_RENDERBUFFERS:
            p_glGenRenderbuffers(n, ids);
            break;
        default:
            break;
    }
    server_reply(ids, (uint32_t) n * 4U);
}

static void handle_delete(const uint32_t *a, uint32_t len) {
    const uint32_t kind = a[0];
    GLsizei n = (GLsizei) a[1];
    if (8U + (uint32_t) n * 4U > len) n = (GLsizei) ((len - 8U) / 4U);
    const GLuint *ids = a + 2;
    switch (kind) {
        case MUGL_GEN_BUFFERS:
            p_glDeleteBuffers(n, ids);
            break;
        case MUGL_GEN_TEXTURES:
            p_glDeleteTextures(n, ids);
            break;
        case MUGL_GEN_FRAMEBUFFERS:
            p_glDeleteFramebuffers(n, ids);
            break;
        case MUGL_GEN_RENDERBUFFERS:
            p_glDeleteRenderbuffers(n, ids);
            break;
        default:
            break;
    }
}

static void handle_client_array(const uint32_t *a, uint32_t len) {
    const uint32_t index = a[0];
    if (index >= MAX_ATTRIBS || len < 20U) return;
    const uint32_t bytes = len - 20U;

    if (bytes > client_array_size[index]) {
        void *grown = realloc(client_arrays[index], bytes);
        if (!grown) return;
        client_arrays[index] = grown;
        client_array_size[index] = bytes;
    }
    memcpy(client_arrays[index], a + 5, bytes);

    GLint previous = 0;
    p_glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous);
    if (previous) p_glBindBuffer(GL_ARRAY_BUFFER, 0);
    p_glVertexAttribPointer(index, (GLint) a[1], a[2], (GLboolean) a[3], (GLsizei) a[4], client_arrays[index]);
    if (previous) p_glBindBuffer(GL_ARRAY_BUFFER, (GLuint) previous);
}

static void handle_draw_elements(const uint32_t *a, uint32_t len) {
    const GLenum mode = a[0];
    const GLsizei count = (GLsizei) a[1];
    const GLenum type = a[2];
    if (a[3]) {
        p_glDrawElements(mode, count, type, (const void *) (uintptr_t) a[4]);
        return;
    }
    if (len <= 20U) return;
    p_glDrawElements(mode, count, type, a + 5);
}

static void handle_uniform_v(const uint32_t *a) {
    const uint32_t shape = a[0];
    const GLint location = (GLint) a[1];
    const GLsizei count = (GLsizei) a[2];
    const void *v = a + 3;
    const int is_int = (shape & 0x100U) != 0;
    switch (shape & 0xFFU) {
        case 1:
            if (is_int) p_glUniform1iv(location, count, v);
            else p_glUniform1fv(location, count, v);
            break;
        case 2:
            if (is_int) p_glUniform2iv(location, count, v);
            else p_glUniform2fv(location, count, v);
            break;
        case 3:
            if (is_int) p_glUniform3iv(location, count, v);
            else p_glUniform3fv(location, count, v);
            break;
        case 4:
            if (is_int) p_glUniform4iv(location, count, v);
            else p_glUniform4fv(location, count, v);
            break;
        default:
            break;
    }
}

static void handle_uniform_matrix(const uint32_t *a) {
    const GLint location = (GLint) a[1];
    const GLsizei count = (GLsizei) a[2];
    const GLboolean transpose = (GLboolean) a[3];
    const GLfloat *v = (const GLfloat *) (a + 4);
    switch (a[0]) {
        case 2:
            p_glUniformMatrix2fv(location, count, transpose, v);
            break;
        case 3:
            p_glUniformMatrix3fv(location, count, transpose, v);
            break;
        case 4:
            p_glUniformMatrix4fv(location, count, transpose, v);
            break;
        default:
            break;
    }
}

static void reply_text(const char *text) {
    const uint32_t len = text ? (uint32_t) strlen(text) : 0U;
    server_reply(text, len);
}

static void handle_get_info(const uint32_t *a) {
    const uint32_t kind = a[0];
    const GLuint object = a[1];
    GLint length = 0;
    GLsizei written = 0;
    char *out = (char *) resp;

    if (kind == MUGL_LOG_PROGRAM) {
        p_glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
        if (length > 0 && (uint32_t) length < MUGL_RESP_SIZE) p_glGetProgramInfoLog(object, length, &written, out);
    } else {
        p_glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
        if (length > 0 && (uint32_t) length < MUGL_RESP_SIZE) p_glGetShaderInfoLog(object, length, &written, out);
    }
    server_reply(out, written > 0 ? (uint32_t) written : 0U);
}

static void handle_get_shader_source(const uint32_t *a) {
    const GLuint shader = a[1];
    GLint length = 0;
    GLsizei written = 0;
    p_glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &length);
    if (length > 0 && (uint32_t) length < MUGL_RESP_SIZE) p_glGetShaderSource(shader, length, &written, (char *) resp);
    server_reply(resp, written > 0 ? (uint32_t) written : 0U);
}

static void handle_get_active(const uint32_t *a) {
    const uint32_t kind = a[0];
    const GLuint program = a[1];
    const GLuint index = a[2];
    GLint size = 0;
    GLenum type = 0;
    GLsizei written = 0;
    char *name = (char *) resp + 8;
    const GLsizei room = (GLsizei) (MUGL_STR_SIZE - 9U);

    if (kind == MUGL_ACTIVE_ATTRIB) p_glGetActiveAttrib(program, index, room, &written, &size, &type, name);
    else p_glGetActiveUniform(program, index, room, &written, &size, &type, name);

    memcpy(resp, &size, 4);
    memcpy(resp + 4, &type, 4);
    server_reply(resp, 8U + (written > 0 ? (uint32_t) written : 0U));
}

static char *payload_string(uint32_t offset, uint32_t len) {
    if (len < offset) return NULL;
    payload[len] = '\0';
    return (char *) payload + offset;
}

static void capture_frame(void) {
    static long target = -1;
    static long frame;
    static char path[256];
    if (target == -1) {
        const char *spec = getenv("MUGL_CAPTURE");
        const char *colon = spec ? strrchr(spec, ':') : NULL;
        target = -2;
        if (colon && (size_t) (colon - spec) < sizeof(path)) {
            memcpy(path, spec, (size_t) (colon - spec));
            path[colon - spec] = '\0';
            target = atol(colon + 1);
        }
    }
    if (++frame != target) return;

    const int w = (int) hdr->width;
    const int h = (int) hdr->height;
    uint8_t *pixels = malloc((size_t) w * (size_t) h * 4U);
    if (!pixels) return;

    GLint previous = 0;
    p_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    if (previous) p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLint alignment = 4;
    p_glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    p_glPixelStorei(GL_PACK_ALIGNMENT, 1);
    p_glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    p_glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    if (previous) p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) previous);

    FILE *file = fopen(path, "wb");
    if (file) {
        fprintf(file, "P6\n%d %d\n255\n", w, h);
        for (int y = h - 1; y >= 0; y--) {
            for (int x = 0; x < w; x++) fwrite(pixels + ((size_t) y * (size_t) w + (size_t) x) * 4U, 1, 3, file);
        }
        fclose(file);
        fprintf(stderr, "mugl-server: captured frame %ld to %s\n", frame, path);
    }
    free(pixels);
}

static void report_rate(void) {
    static int enabled = -1;
    static Uint64 since;
    static unsigned frames;
    if (enabled == -1) enabled = getenv("MUGL_DEBUG") != NULL;
    if (!enabled) return;

    const Uint64 now = SDL_GetPerformanceCounter();
    if (!since) since = now;
    frames++;
    const double elapsed = (double) (now - since) / (double) SDL_GetPerformanceFrequency();
    if (elapsed >= 5.0) {
        fprintf(stderr, "mugl-server: %.1f fps\n", (double) frames / elapsed);
        since = now;
        frames = 0;
    }
}

static void handle_swap(void) {
    capture_frame();
    report_rate();
    SDL_GL_SwapWindow(window);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    __atomic_add_fetch(&hdr->swap_seq, 1, __ATOMIC_RELEASE);
    futex(&hdr->swap_seq, FUTEX_WAKE, 1, NULL);
}

static void dispatch(uint32_t op, uint32_t len) {
    const uint32_t *a = (const uint32_t *) payload;
    const uint32_t words = len / 4U;

    if (op >= MUGL_OP_GENERATED_BASE) {
        if (server_dispatch_generated(op, a, words) != 1) {
            fprintf(stderr, "mugl-server: unknown or short operation %u\n", op);
        }
        return;
    }

    switch (op) {
        case MUGL_OP_SWAP:
            handle_swap();
            break;
        case MUGL_OP_SWAP_INTERVAL:
            if (words >= 1) SDL_GL_SetSwapInterval((int) a[0]);
            break;
        case MUGL_OP_BYE:
            finish(0);
            break;
        case MUGL_OP_SYNC:
            p_glFinish();
            server_reply(NULL, 0);
            break;
        case MUGL_OP_GET_STRING:
            reply_text(words >= 1 ? (const char *) p_glGetString(a[0]) : NULL);
            break;
        case MUGL_OP_GET_V:
            if (words >= 2) handle_get_v(a);
            break;
        case MUGL_OP_GET_OBJ_V:
            if (words >= 4) handle_get_obj_v(a);
            break;
        case MUGL_OP_GEN:
            if (words >= 2) handle_gen(a);
            break;
        case MUGL_OP_DELETE:
            if (words >= 2) handle_delete(a, len);
            break;
        case MUGL_OP_BUFFER_DATA:
            if (words >= 4) p_glBufferData(a[0], (GLsizeiptr) a[1], a[3] ? (const void *) (a + 4) : NULL, a[2]);
            break;
        case MUGL_OP_BUFFER_SUB_DATA:
            if (words >= 3) p_glBufferSubData(a[0], (GLintptr) a[1], (GLsizeiptr) a[2], a + 3);
            break;
        case MUGL_OP_TEX_IMAGE_2D:
            if (words >= 9) {
                p_glTexImage2D(a[0], (GLint) a[1], (GLint) a[2], (GLsizei) a[3], (GLsizei) a[4], (GLint) a[5], a[6],
                               a[7], a[8] ? (const void *) (a + 9) : NULL);
            }
            break;
        case MUGL_OP_TEX_SUB_IMAGE_2D:
            if (words >= 8) {
                p_glTexSubImage2D(a[0], (GLint) a[1], (GLint) a[2], (GLint) a[3], (GLsizei) a[4], (GLsizei) a[5], a[6],
                                  a[7], a + 8);
            }
            break;
        case MUGL_OP_COMPRESSED_TEX_IMAGE_2D:
            if (words >= 8) {
                p_glCompressedTexImage2D(a[0], (GLint) a[1], a[2], (GLsizei) a[3], (GLsizei) a[4], (GLint) a[5],
                                         (GLsizei) a[6], a[7] ? (const void *) (a + 8) : NULL);
            }
            break;
        case MUGL_OP_COMPRESSED_TEX_SUB_IMAGE_2D:
            if (words >= 8) {
                p_glCompressedTexSubImage2D(a[0], (GLint) a[1], (GLint) a[2], (GLint) a[3], (GLsizei) a[4],
                                            (GLsizei) a[5], a[6], (GLsizei) a[7], a + 8);
            }
            break;
        case MUGL_OP_READ_PIXELS:
            if (words >= 7) {
                const uint32_t size = a[6] <= MUGL_RESP_SIZE ? a[6] : 0U;
                if (size) p_glReadPixels((GLint) a[0], (GLint) a[1], (GLsizei) a[2], (GLsizei) a[3], a[4], a[5], resp);
                server_reply(resp, size);
            }
            break;
        case MUGL_OP_SHADER_SOURCE:
            if (words >= 2) {
                const GLchar *text = (const GLchar *) (a + 2);
                const GLint length = (GLint) (len - 8U < a[1] ? len - 8U : a[1]);
                p_glShaderSource(a[0], 1, &text, &length);
            }
            break;
        case MUGL_OP_GET_SHADER_SOURCE:
            if (words >= 2) handle_get_shader_source(a);
            break;
        case MUGL_OP_GET_INFO_LOG:
            if (words >= 2) handle_get_info(a);
            break;
        case MUGL_OP_GET_ACTIVE:
            if (words >= 3) handle_get_active(a);
            break;
        case MUGL_OP_GET_ATTACHED_SHADERS:
            if (words >= 1) {
                GLsizei count = 0;
                p_glGetAttachedShaders(a[0], 64, &count, (GLuint *) resp);
                server_reply(resp, count > 0 ? (uint32_t) count * 4U : 0U);
            }
            break;
        case MUGL_OP_BIND_ATTRIB_LOCATION:
            if (words >= 2) {
                const char *name = payload_string(8, len);
                if (name) p_glBindAttribLocation(a[0], a[1], name);
            }
            break;
        case MUGL_OP_GET_LOCATION:
            if (words >= 2) {
                const char *name = payload_string(8, len);
                GLint location = -1;
                if (name && a[0] == MUGL_LOC_ATTRIB) location = p_glGetAttribLocation(a[1], name);
                else if (name) location = p_glGetUniformLocation(a[1], name);
                server_reply_u32((uint32_t) location);
            }
            break;
        case MUGL_OP_UNIFORM_V:
            if (words >= 3) handle_uniform_v(a);
            break;
        case MUGL_OP_UNIFORM_MATRIX_V:
            if (words >= 4) handle_uniform_matrix(a);
            break;
        case MUGL_OP_VERTEX_ATTRIB_V:
            if (words >= 6) p_glVertexAttrib4f(a[1], server_f(a[2]), server_f(a[3]), server_f(a[4]), server_f(a[5]));
            break;
        case MUGL_OP_VERTEX_ATTRIB_POINTER:
            if (words >= 6) {
                p_glVertexAttribPointer(a[0], (GLint) a[1], a[2], (GLboolean) a[3], (GLsizei) a[4],
                                        (const void *) (uintptr_t) a[5]);
            }
            break;
        case MUGL_OP_CLIENT_ARRAY:
            if (words >= 5) handle_client_array(a, len);
            break;
        case MUGL_OP_DRAW_ARRAYS:
            if (words >= 3) p_glDrawArrays(a[0], (GLint) a[1], (GLsizei) a[2]);
            break;
        case MUGL_OP_DRAW_ELEMENTS:
            if (words >= 5) handle_draw_elements(a, len);
            break;
        case MUGL_OP_SHADER_PRECISION:
            if (words >= 2) {
                GLint out[3] = {0, 0, 0};
                p_glGetShaderPrecisionFormat(a[0], a[1], out, &out[2]);
                server_reply(out, 12);
            }
            break;
        case MUGL_OP_SHADER_BINARY:
            if (words >= 3) {
                const GLsizei count = (GLsizei) a[0];
                const GLuint *shaders = a + 3;
                const uint8_t *binary = (const uint8_t *) (a + 3 + count);
                p_glShaderBinary(count, shaders, a[1], binary, (GLsizei) a[2]);
            }
            break;
        case MUGL_OP_DISCARD_FRAMEBUFFER:
            if (words >= 2 && p_glDiscardFramebufferEXT) p_glDiscardFramebufferEXT(a[0], (GLsizei) a[1], a + 2);
            break;
        case MUGL_OP_GEN_VERTEX_ARRAYS:
            if (words >= 1) {
                GLsizei n = (GLsizei) a[0];
                if ((uint32_t) n > 4096U) n = 4096;
                memset(resp, 0, (size_t) n * 4U);
                if (p_glGenVertexArraysOES) p_glGenVertexArraysOES(n, (GLuint *) resp);
                server_reply(resp, (uint32_t) n * 4U);
            }
            break;
        case MUGL_OP_DELETE_VERTEX_ARRAYS:
            if (words >= 1 && p_glDeleteVertexArraysOES) p_glDeleteVertexArraysOES((GLsizei) a[0], a + 1);
            break;
        case MUGL_OP_BIND_VERTEX_ARRAY:
            if (words >= 1 && p_glBindVertexArrayOES) p_glBindVertexArrayOES(a[0]);
            break;
        case MUGL_OP_IS_VERTEX_ARRAY:
            server_reply_u32(words >= 1 && p_glIsVertexArrayOES ? p_glIsVertexArrayOES(a[0]) : 0U);
            break;
        default:
            fprintf(stderr, "mugl-server: unknown operation %u\n", op);
            break;
    }
}

static void start_display(void) {
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) fail_start(SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(0, &mode) != 0) fail_start(SDL_GetError());

    window = SDL_CreateWindow("mugl", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mode.w, mode.h,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (!window) fail_start(SDL_GetError());

    context = SDL_GL_CreateContext(window);
    if (!context) fail_start(SDL_GetError());
    SDL_GL_SetSwapInterval(1);

    if (!server_load_gl()) fail_start("GL functions are missing");

    p_glGenVertexArraysOES = (PFNGLGENVERTEXARRAYSOESPROC) SDL_GL_GetProcAddress("glGenVertexArraysOES");
    p_glDeleteVertexArraysOES = (PFNGLDELETEVERTEXARRAYSOESPROC) SDL_GL_GetProcAddress("glDeleteVertexArraysOES");
    p_glBindVertexArrayOES = (PFNGLBINDVERTEXARRAYOESPROC) SDL_GL_GetProcAddress("glBindVertexArrayOES");
    p_glIsVertexArrayOES = (PFNGLISVERTEXARRAYOESPROC) SDL_GL_GetProcAddress("glIsVertexArrayOES");
    p_glDiscardFramebufferEXT = (PFNGLDISCARDFRAMEBUFFEREXTPROC) SDL_GL_GetProcAddress("glDiscardFramebufferEXT");

    int width = 0;
    int height = 0;
    SDL_GL_GetDrawableSize(window, &width, &height);
    hdr->width = (uint32_t) width;
    hdr->height = (uint32_t) height;
}

int main(int argc, char **argv) {
    const int fd = argc > 1 ? atoi(argv[1]) : MUGL_SHM_FD;
    uint8_t *base = mmap(NULL, MUGL_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        fprintf(stderr, "mugl-server: could not map the shared memory\n");
        return 1;
    }
    close(fd);

    hdr = MUGL_HDR(base);
    if (hdr->magic != MUGL_MAGIC || hdr->version != MUGL_VERSION) {
        fprintf(stderr, "mugl-server: client protocol does not match\n");
        return 1;
    }
    ring = MUGL_RING(base);
    resp = MUGL_RESP(base);
    hdr->server_pid = (uint32_t) getpid();

    start_display();

    __atomic_store_n(&hdr->ready, 1, __ATOMIC_RELEASE);
    futex(&hdr->ready, FUTEX_WAKE, 1, NULL);

    for (;;) {
        mugl_msg msg;
        ring_read(&msg, sizeof(msg));
        if (!payload_reserve(msg.len)) fail_start("out of memory");
        ring_read(payload, msg.len);
        const uint32_t pad = (4U - (msg.len & 3U)) & 3U;
        if (pad) ring_read(NULL, pad);
        dispatch(msg.op, msg.len);
    }
}
