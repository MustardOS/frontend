#define _GNU_SOURCE
#include <errno.h>
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
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
#include "server_gl.h"
#include "gen_ops.h"

#define SPIN_LIMIT       4000
#define WAIT_NS          50000000L
#define MAX_ATTRIBS      MUGL_MAX_ATTRIBS
#define MAX_UNIFORM_LOCS 256
#define GET_MAX          256
#define PAYLOAD_KEEP     (4U * 1024U * 1024U)

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
static GLuint array_buffer_bound;
static int stats_on;
static uint64_t stat_spin_ns;
static uint64_t stat_sleep_ns;
static uint64_t stat_work_ns;
static uint64_t stat_swap_ns;
static uint64_t stat_msgs;
static uint64_t stat_bytes;

static uint64_t now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t) t.tv_sec * 1000000000ULL + (uint64_t) t.tv_nsec;
}
static SDL_GLContext context;

static long futex(volatile uint32_t *addr, int op, uint32_t val, const struct timespec *ts) {
    return syscall(SYS_futex, addr, op, val, ts, NULL, 0);
}

static void finish(int code) {
    server_frame_close();
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

static int timing_on;
static uint64_t frame_work_ns;

unsigned long server_client_ticks(void) {
    char path[32];
    snprintf(path, sizeof(path), "/proc/%u/stat", hdr->client_pid);
    FILE *file = fopen(path, "r");
    if (!file) return 0;

    char line[512];
    unsigned long utime = 0;
    unsigned long stime = 0;
    if (fgets(line, sizeof(line), file)) {
        const char *rest = strrchr(line, ')');
        if (rest) sscanf(rest + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &utime, &stime);
    }
    fclose(file);
    return utime + stime;
}

static int client_alive(void) {
    char path[32];
    snprintf(path, sizeof(path), "/proc/%u/stat", hdr->client_pid);
    FILE *file = fopen(path, "r");
    if (!file) return 0;

    char line[512];
    const int ok = fgets(line, sizeof(line), file) != NULL;
    fclose(file);
    if (!ok) return 0;

    const char *state = strrchr(line, ')');
    return state && state[1] == ' ' && state[2] != 'Z' && state[2] != 'X';
}

static void abandon(void) {
    server_frame_forget();
    __atomic_store_n(&hdr->server_exited, 1, __ATOMIC_RELEASE);
    _exit(0);
}

static void check_client(void) {
    if (!client_alive()) finish(0);
}

static void *watch_client(void *unused) {
    (void) unused;
    for (;;) {
        usleep(250000);
        if (!client_alive()) {
            usleep(500000);
            abandon();
        }
    }
    return NULL;
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
    uint64_t spin_start = 0;
    for (;;) {
        const uint32_t avail = __atomic_load_n(&hdr->wpos, __ATOMIC_ACQUIRE) - hdr->rpos;
        if (avail) {
            if (spin_start) stat_spin_ns += now_ns() - spin_start;
            return avail;
        }
        if (stats_on && !spin_start) spin_start = now_ns();
        if (++spins < SPIN_LIMIT) continue;
        if (spin_start) {
            const uint64_t now = now_ns();
            stat_spin_ns += now - spin_start;
            spin_start = now;
        }

        const uint32_t seen = __atomic_load_n(&hdr->wpos, __ATOMIC_ACQUIRE);
        __atomic_store_n(&hdr->reader_sleeping, 1, __ATOMIC_SEQ_CST);
        if (__atomic_load_n(&hdr->wpos, __ATOMIC_ACQUIRE) == hdr->rpos) {
            const struct timespec ts = {0, WAIT_NS};
            if (futex(&hdr->wpos, FUTEX_WAIT, seen, &ts) != 0 && errno == ETIMEDOUT) check_client();
        }
        __atomic_store_n(&hdr->reader_sleeping, 0, __ATOMIC_RELAXED);
        if (spin_start) {
            const uint64_t now = now_ns();
            stat_sleep_ns += now - spin_start;
            spin_start = now;
        }
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
    while (capacity < len + 1U)
        capacity *= 2U;
    uint8_t *grown = realloc(payload, capacity);
    if (!grown) return 0;
    payload = grown;
    payload_capacity = capacity;
    return 1;
}

static void payload_trim(void) {
    if (payload_capacity <= PAYLOAD_KEEP) return;
    free(payload);
    payload = NULL;
    payload_capacity = 0;
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
    if (kind == MUGL_GET_BOOLEAN)
        p_glGetBooleanv(pname, (GLboolean *) &out[1]);
    else if (kind == MUGL_GET_FLOAT)
        p_glGetFloatv(pname, (GLfloat *) &out[1]);
    else
        p_glGetIntegerv(pname, (GLint *) &out[1]);
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
            for (GLsizei i = 0; i < n; i++) {
                if (ids[i] && ids[i] == array_buffer_bound) array_buffer_bound = 0;
            }
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

static int stream_reserve(uint32_t index, uint32_t bytes) {
    if (bytes <= client_array_size[index]) return 1;
    void *grown = realloc(client_arrays[index], bytes);
    if (!grown) return 0;
    client_arrays[index] = grown;
    client_array_size[index] = bytes;
    return 1;
}

static void skip_bytes(uint32_t bytes) {
    if (bytes) ring_read(NULL, bytes);
}

static void read_draw(uint32_t len) {
    uint32_t w[MUGL_DRAW_WORDS];
    uint32_t used = MUGL_DRAW_WORDS * 4U;
    if (len < used) {
        skip_bytes(len);
        return;
    }
    ring_read(w, used);

    const uint32_t stream_count = w[8];
    const uint32_t attrib_count = w[9];
    const uint32_t table = (stream_count + attrib_count * MUGL_ATTRIB_WORDS) * 4U;
    if (stream_count > MAX_ATTRIBS || attrib_count > MAX_ATTRIBS || len - used < table) {
        skip_bytes(len - used);
        return;
    }

    uint32_t streams[MAX_ATTRIBS];
    uint32_t attribs[MAX_ATTRIBS * MUGL_ATTRIB_WORDS];
    ring_read(streams, stream_count * 4U);
    ring_read(attribs, attrib_count * MUGL_ATTRIB_WORDS * 4U);
    used += table;

    int ok = 1;
    for (uint32_t s = 0; s < stream_count; s++) {
        const uint32_t padded = (streams[s] + 3U) & ~3U;
        if (len - used < padded) {
            skip_bytes(len - used);
            return;
        }
        if (ok && stream_reserve(s, streams[s])) {
            ring_read(client_arrays[s], streams[s]);
            skip_bytes(padded - streams[s]);
        } else {
            ok = 0;
            skip_bytes(padded);
        }
        used += padded;
    }

    const uint32_t index_bytes = len - used;
    if (index_bytes) {
        if (!payload_reserve(index_bytes)) {
            skip_bytes(index_bytes);
            return;
        }
        ring_read(payload, index_bytes);
    }
    if (!ok) return;

    const uint32_t mask = w[6];
    const uint32_t changed = w[7];
    for (uint32_t i = 0; i < MAX_ATTRIBS; i++) {
        if (!(changed & (1U << i))) continue;
        if (mask & (1U << i))
            p_glEnableVertexAttribArray(i);
        else
            p_glDisableVertexAttribArray(i);
    }

    if (attrib_count) {
        if (array_buffer_bound) p_glBindBuffer(GL_ARRAY_BUFFER, 0);
        for (uint32_t a = 0; a < attrib_count; a++) {
            const uint32_t *v = attribs + a * MUGL_ATTRIB_WORDS;
            if (v[0] >= MAX_ATTRIBS || v[5] >= stream_count) continue;
            const uint8_t *data = (const uint8_t *) client_arrays[v[5]] + v[6];
            p_glVertexAttribPointer(v[0], (GLint) v[1], v[2], (GLboolean) v[3], (GLsizei) v[4], data);
        }
        if (array_buffer_bound) p_glBindBuffer(GL_ARRAY_BUFFER, array_buffer_bound);
    }

    const GLenum mode = w[0];
    if (!(w[1] & MUGL_DRAW_ELEMENTS)) {
        p_glDrawArrays(mode, (GLint) w[2], (GLsizei) w[3]);
    } else if (w[1] & MUGL_DRAW_INDEX_BUFFER) {
        p_glDrawElements(mode, (GLsizei) w[3], w[4], (const void *) (uintptr_t) w[5]);
    } else if (index_bytes) {
        p_glDrawElements(mode, (GLsizei) w[3], w[4], payload);
    }
}

static void handle_uniform_v(const uint32_t *a) {
    const uint32_t shape = a[0];
    const GLint location = (GLint) a[1];
    const GLsizei count = (GLsizei) a[2];
    const void *v = a + 3;
    const int is_int = (shape & 0x100U) != 0;
    switch (shape & 0xFFU) {
        case 1:
            if (is_int)
                p_glUniform1iv(location, count, v);
            else
                p_glUniform1fv(location, count, v);
            break;
        case 2:
            if (is_int)
                p_glUniform2iv(location, count, v);
            else
                p_glUniform2fv(location, count, v);
            break;
        case 3:
            if (is_int)
                p_glUniform3iv(location, count, v);
            else
                p_glUniform3fv(location, count, v);
            break;
        case 4:
            if (is_int)
                p_glUniform4iv(location, count, v);
            else
                p_glUniform4fv(location, count, v);
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

    if (kind == MUGL_ACTIVE_ATTRIB)
        p_glGetActiveAttrib(program, index, room, &written, &size, &type, name);
    else
        p_glGetActiveUniform(program, index, room, &written, &size, &type, name);

    memcpy(resp, &size, 4);
    memcpy(resp + 4, &type, 4);
    server_reply(resp, 8U + (written > 0 ? (uint32_t) written : 0U));
}

typedef struct {
    uint8_t *out;
    uint32_t len;
    int full;
} blob;

static void blob_word(blob *b, uint32_t value) {
    if (b->full || b->len + 4U > MUGL_RESP_SIZE) {
        b->full = 1;
        return;
    }
    memcpy(b->out + b->len, &value, 4);
    b->len += 4U;
}

static void blob_name(blob *b, const char *name, uint32_t len) {
    blob_word(b, len);
    const uint32_t padded = (len + 3U) & ~3U;
    if (b->full || b->len + padded > MUGL_RESP_SIZE) {
        b->full = 1;
        return;
    }
    memset(b->out + b->len, 0, padded);
    memcpy(b->out + b->len, name, len);
    b->len += padded;
}

static void handle_program_info(GLuint program) {
    static char name[MUGL_STR_SIZE];
    blob b = {resp, 0, 0};

    GLint linked = 0;
    GLint uniforms = 0;
    GLint attribs = 0;
    GLint uniform_max = 0;
    GLint attrib_max = 0;
    p_glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked) {
        p_glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniforms);
        p_glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attribs);
        p_glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &uniform_max);
        p_glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &attrib_max);
    }
    if (uniforms < 0) uniforms = 0;
    if (attribs < 0) attribs = 0;

    blob_word(&b, (uint32_t) linked);
    blob_word(&b, (uint32_t) uniforms);
    blob_word(&b, (uint32_t) attribs);
    blob_word(&b, (uint32_t) uniform_max);
    blob_word(&b, (uint32_t) attrib_max);

    for (GLint i = 0; i < uniforms; i++) {
        GLsizei written = 0;
        GLint size = 0;
        GLenum type = 0;
        p_glGetActiveUniform(program, (GLuint) i, (GLsizei) sizeof(name) - 8, &written, &size, &type, name);
        if (written < 0) written = 0;
        name[written] = '\0';

        size_t base = (size_t) written;
        if (base > 3 && strcmp(name + base - 3, "[0]") == 0) base -= 3;
        const uint32_t locs = size > 1 ? (uint32_t) (size < MAX_UNIFORM_LOCS ? size : MAX_UNIFORM_LOCS) : 1U;

        blob_word(&b, (uint32_t) size);
        blob_word(&b, type);
        blob_word(&b, locs);
        blob_name(&b, name, (uint32_t) written);

        if (locs == 1) {
            blob_word(&b, (uint32_t) p_glGetUniformLocation(program, name));
            continue;
        }
        for (uint32_t l = 0; l < locs; l++) {
            char element[MUGL_STR_SIZE + 16];
            snprintf(element, sizeof(element), "%.*s[%u]", (int) base, name, l);
            blob_word(&b, (uint32_t) p_glGetUniformLocation(program, element));
        }
    }

    for (GLint i = 0; i < attribs; i++) {
        GLsizei written = 0;
        GLint size = 0;
        GLenum type = 0;
        p_glGetActiveAttrib(program, (GLuint) i, (GLsizei) sizeof(name) - 8, &written, &size, &type, name);
        if (written < 0) written = 0;
        name[written] = '\0';

        blob_word(&b, (uint32_t) size);
        blob_word(&b, type);
        blob_word(&b, 1);
        blob_name(&b, name, (uint32_t) written);
        blob_word(&b, (uint32_t) p_glGetAttribLocation(program, name));
    }

    if (b.full) {
        b.len = 0;
        blob_word(&b, 0);
    }
    server_reply(resp, b.len);
}

static char *payload_string(uint32_t offset, uint32_t len) {
    if (len < offset) return NULL;
    payload[len] = '\0';
    return (char *) payload + offset;
}

static void capture_frame(void) {
    static long target = -1;
    static long frame;
    static int seconds;
    static uint64_t started;
    static char path[256];
    if (target == -1) {
        const char *spec = getenv("MUGL_CAPTURE");
        const char *colon = spec ? strrchr(spec, ':') : NULL;
        target = -2;
        if (colon && (size_t) (colon - spec) < sizeof(path)) {
            memcpy(path, spec, (size_t) (colon - spec));
            path[colon - spec] = '\0';
            char *end = NULL;
            target = strtol(colon + 1, &end, 10);
            seconds = end && *end == 's';
            started = now_ns();
        }
    }
    if (target < 0) return;
    ++frame;
    if (seconds) {
        if (now_ns() - started < (uint64_t) target * 1000000000ULL) return;
        target = -2;
    } else if (frame != target) {
        return;
    }

    const int w = (int) hdr->width;
    const int h = (int) hdr->height;
    uint8_t *pixels = malloc((size_t) w * (size_t) h * 4U);
    if (!pixels) return;

    server_read_frame(pixels, w, h);

    FILE *file = fopen(path, "wb");
    if (file) {
        fprintf(file, "P6\n%d %d\n255\n", w, h);
        for (int y = h - 1; y >= 0; y--) {
            for (int x = 0; x < w; x++)
                fwrite(pixels + ((size_t) y * (size_t) w + (size_t) x) * 4U, 1, 3, file);
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
    if (enabled == -1) enabled = getenv("MUGL_DEBUG") != NULL || stats_on;
    if (!enabled) return;

    const Uint64 now = SDL_GetPerformanceCounter();
    if (!since) since = now;
    frames++;
    const double elapsed = (double) (now - since) / (double) SDL_GetPerformanceFrequency();
    if (elapsed >= 5.0) {
        if (stats_on) {
            fprintf(
                stderr,
                "mugl-server: %.1f fps | per second: work %.0f ms, swap %.0f ms, spin %.0f ms, sleep %.0f ms, %.0f "
                "msgs, "
                "%.0f KB\n",
                (double) frames / elapsed, (double) (stat_work_ns - stat_swap_ns) / elapsed / 1e6,
                (double) stat_swap_ns / elapsed / 1e6, (double) stat_spin_ns / elapsed / 1e6,
                (double) stat_sleep_ns / elapsed / 1e6, (double) stat_msgs / elapsed,
                (double) stat_bytes / elapsed / 1024.0
            );
            stat_work_ns = stat_swap_ns = stat_spin_ns = stat_sleep_ns = stat_msgs = stat_bytes = 0;
        } else {
            fprintf(stderr, "mugl-server: %.1f fps\n", (double) frames / elapsed);
        }
        since = now;
        frames = 0;
    }
}

static void handle_swap(void) {
    capture_frame();
    server_frame_publish();
    const int overlay = server_overlay_active();
    if (overlay) server_overlay_draw(hdr->width, hdr->height, frame_work_ns);
    frame_work_ns = 0;
    timing_on = overlay || stats_on;
    report_rate();
    const uint64_t swap_start = stats_on ? now_ns() : 0;
    SDL_GL_SwapWindow(window);
    if (swap_start) stat_swap_ns += now_ns() - swap_start;
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    __atomic_add_fetch(&hdr->swap_seq, 1, __ATOMIC_RELEASE);
    futex(&hdr->swap_seq, FUTEX_WAKE, 1, NULL);
}

static GLint attached_renderbuffer(GLenum target, GLenum attachment) {
    GLint type = GL_NONE;
    p_glGetFramebufferAttachmentParameteriv(target, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    if (type != GL_RENDERBUFFER) return 0;
    GLint name = 0;
    p_glGetFramebufferAttachmentParameteriv(target, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
    return name;
}

static void track_bindings(uint32_t op, const uint32_t *a, uint32_t words) {
    if (op == MUGL_OP_glBindBuffer && words >= 2 && a[0] == GL_ARRAY_BUFFER) array_buffer_bound = a[1];
}

static int handle_depth_stencil(uint32_t op, const uint32_t *a, uint32_t words) {
    if (op == MUGL_OP_glRenderbufferStorage && words >= 4) {
        GLenum format = a[1];
        if (format == GL_DEPTH_COMPONENT16 || format == GL_DEPTH_COMPONENT24_OES || format == GL_STENCIL_INDEX8)
            format = GL_DEPTH24_STENCIL8_OES;
        p_glRenderbufferStorage(a[0], format, (GLsizei) a[2], (GLsizei) a[3]);
        return 1;
    }

    if (op == MUGL_OP_glFramebufferRenderbuffer && words >= 4) {
        const GLenum target = a[0];
        const GLenum attachment = a[1];
        p_glFramebufferRenderbuffer(target, attachment, a[2], a[3]);
        if (!a[3]) return 1;

        if (attachment == GL_STENCIL_ATTACHMENT) {
            const GLint depth = attached_renderbuffer(target, GL_DEPTH_ATTACHMENT);
            if (depth && (GLuint) depth != a[3])
                p_glFramebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, a[2], (GLuint) depth);
        } else if (attachment == GL_DEPTH_ATTACHMENT) {
            const GLint stencil = attached_renderbuffer(target, GL_STENCIL_ATTACHMENT);
            if (stencil && (GLuint) stencil != a[3])
                p_glFramebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, a[2], a[3]);
        }
        return 1;
    }

    return 0;
}

static void dispatch(uint32_t op, uint32_t len) {
    const uint32_t *a = (const uint32_t *) payload;
    const uint32_t words = len / 4U;

    track_bindings(op, a, words);
    if (handle_depth_stencil(op, a, words)) return;

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
                p_glTexImage2D(
                    a[0], (GLint) a[1], (GLint) a[2], (GLsizei) a[3], (GLsizei) a[4], (GLint) a[5], a[6], a[7],
                    a[8] ? (const void *) (a + 9) : NULL
                );
            }
            break;
        case MUGL_OP_TEX_SUB_IMAGE_2D:
            if (words >= 8) {
                p_glTexSubImage2D(
                    a[0], (GLint) a[1], (GLint) a[2], (GLint) a[3], (GLsizei) a[4], (GLsizei) a[5], a[6], a[7], a + 8
                );
            }
            break;
        case MUGL_OP_COMPRESSED_TEX_IMAGE_2D:
            if (words >= 8) {
                p_glCompressedTexImage2D(
                    a[0], (GLint) a[1], a[2], (GLsizei) a[3], (GLsizei) a[4], (GLint) a[5], (GLsizei) a[6],
                    a[7] ? (const void *) (a + 8) : NULL
                );
            }
            break;
        case MUGL_OP_COMPRESSED_TEX_SUB_IMAGE_2D:
            if (words >= 8) {
                p_glCompressedTexSubImage2D(
                    a[0], (GLint) a[1], (GLint) a[2], (GLint) a[3], (GLsizei) a[4], (GLsizei) a[5], a[6],
                    (GLsizei) a[7], a + 8
                );
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
                if (name && a[0] == MUGL_LOC_ATTRIB)
                    location = p_glGetAttribLocation(a[1], name);
                else if (name)
                    location = p_glGetUniformLocation(a[1], name);
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
                p_glVertexAttribPointer(
                    a[0], (GLint) a[1], a[2], (GLboolean) a[3], (GLsizei) a[4], (const void *) (uintptr_t) a[5]
                );
            }
            break;
        case MUGL_OP_PROGRAM_INFO:
            if (words >= 1) handle_program_info(a[0]);
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

    window = SDL_CreateWindow(
        "mugl", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mode.w, mode.h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN
    );
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
    server_frame_open(hdr->width, hdr->height);
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

    stats_on = getenv("MUGL_STATS") != NULL;
    timing_on = stats_on;

    const char *cpu = getenv("MUGL_SERVER_CPU");
    if (cpu && *cpu) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(atoi(cpu), &set);
        if (sched_setaffinity(0, sizeof(set), &set) != 0)
            fprintf(stderr, "mugl-server: could not pin to CPU %s\n", cpu);
    }

    start_display();

    pthread_t watcher;
    if (pthread_create(&watcher, NULL, watch_client, NULL) == 0) pthread_detach(watcher);

    __atomic_store_n(&hdr->ready, 1, __ATOMIC_RELEASE);
    futex(&hdr->ready, FUTEX_WAKE, 1, NULL);

    for (;;) {
        mugl_msg msg;
        ring_read(&msg, sizeof(msg));
        const uint32_t pad = (4U - (msg.len & 3U)) & 3U;

        if (msg.op == MUGL_OP_DRAW) {
            const uint64_t start = timing_on ? now_ns() : 0;
            read_draw(msg.len);
            if (pad) ring_read(NULL, pad);
            if (start) frame_work_ns += now_ns() - start;
            if (stats_on) {
                stat_work_ns += now_ns() - start;
                stat_msgs++;
                stat_bytes += sizeof(msg) + msg.len;
            }
            continue;
        }

        if (!payload_reserve(msg.len)) fail_start("out of memory");
        ring_read(payload, msg.len);
        if (pad) ring_read(NULL, pad);
        if (timing_on) {
            const uint64_t start = now_ns();
            dispatch(msg.op, msg.len);
            const uint64_t spent = now_ns() - start;
            if (msg.op != MUGL_OP_SWAP) frame_work_ns += spent;
            stat_work_ns += spent;
            stat_msgs++;
            stat_bytes += sizeof(msg) + msg.len;
        } else {
            dispatch(msg.op, msg.len);
        }
        if (msg.len > PAYLOAD_KEEP) payload_trim();
    }
}
