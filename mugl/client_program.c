#include <stdlib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include "client.h"
#include "gen_ops.h"

typedef struct {
    char *name;
    size_t base_len;
    GLint size;
    GLenum type;
    uint32_t loc_count;
    GLint *locs;
} program_var;

typedef struct {
    int loaded;
    GLint linked;
    uint32_t uniform_count;
    uint32_t attrib_count;
    GLint uniform_max;
    GLint attrib_max;
    program_var *uniforms;
    program_var *attribs;
} program_info;

static program_info *programs;
static GLuint program_capacity;

static void free_vars(program_var *vars, uint32_t count) {
    for (uint32_t i = 0; vars && i < count; i++) {
        free(vars[i].name);
        free(vars[i].locs);
    }
    free(vars);
}

void mugl_program_forget(GLuint id) {
    if (id >= program_capacity) return;
    program_info *p = &programs[id];
    free_vars(p->uniforms, p->uniform_count);
    free_vars(p->attribs, p->attrib_count);
    memset(p, 0, sizeof(*p));
}

static program_info *program_slot(GLuint id) {
    if (!id) return NULL;
    if (id >= program_capacity) {
        GLuint capacity = program_capacity ? program_capacity : 64;
        while (capacity <= id)
            capacity *= 2;
        program_info *grown = realloc(programs, capacity * sizeof(program_info));
        if (!grown) return NULL;
        memset(grown + program_capacity, 0, (capacity - program_capacity) * sizeof(program_info));
        programs = grown;
        program_capacity = capacity;
    }
    return &programs[id];
}

typedef struct {
    const uint8_t *data;
    uint32_t len;
    uint32_t pos;
    int bad;
} reader;

static uint32_t read_word(reader *r) {
    uint32_t v = 0;
    if (r->pos + 4U > r->len) {
        r->bad = 1;
        return 0;
    }
    memcpy(&v, r->data + r->pos, 4);
    r->pos += 4U;
    return v;
}

static char *read_name(reader *r) {
    const uint32_t len = read_word(r);
    if (r->bad || len > r->len - r->pos) {
        r->bad = 1;
        return NULL;
    }
    char *name = malloc(len + 1U);
    if (!name) {
        r->bad = 1;
        return NULL;
    }
    memcpy(name, r->data + r->pos, len);
    name[len] = '\0';
    r->pos += (len + 3U) & ~3U;
    return name;
}

static size_t base_length(const char *name) {
    const size_t len = strlen(name);
    if (len > 3 && strcmp(name + len - 3, "[0]") == 0) return len - 3;
    return len;
}

static int parse_vars(reader *r, program_var *vars, uint32_t count, int uniform) {
    for (uint32_t i = 0; i < count; i++) {
        program_var *v = &vars[i];
        v->size = (GLint) read_word(r);
        v->type = read_word(r);
        v->loc_count = read_word(r);
        v->name = read_name(r);
        if (r->bad || !v->name || v->loc_count > 4096U) return 0;
        v->base_len = uniform ? base_length(v->name) : strlen(v->name);

        v->locs = calloc(v->loc_count ? v->loc_count : 1U, sizeof(GLint));
        if (!v->locs) return 0;
        for (uint32_t l = 0; l < v->loc_count; l++)
            v->locs[l] = (GLint) read_word(r);
        if (r->bad) return 0;
    }
    return 1;
}

static program_info *program_load(GLuint id) {
    if (!mugl_cache_on()) return NULL;
    program_info *p = program_slot(id);
    if (!p) return NULL;
    if (p->loaded) return p->linked ? p : NULL;

    const uint32_t w[1] = {id};
    mugl_trace(MUGL_OP_PROGRAM_INFO, w, 1, " ...");
    mugl_msg_begin(MUGL_OP_PROGRAM_INFO, 4);
    mugl_msg_put(w, 4);
    uint32_t len = 0;
    const uint8_t *data = mugl_msg_wait(&len);

    reader r = {data, len, 0, 0};
    p->loaded = 1;
    p->linked = (GLint) read_word(&r);
    const uint32_t uniforms = read_word(&r);
    const uint32_t attribs = read_word(&r);
    p->uniform_max = (GLint) read_word(&r);
    p->attrib_max = (GLint) read_word(&r);

    int ok = !r.bad && p->linked && uniforms <= 4096U && attribs <= 256U;
    if (ok) {
        p->uniforms = calloc(uniforms ? uniforms : 1U, sizeof(program_var));
        p->attribs = calloc(attribs ? attribs : 1U, sizeof(program_var));
        p->uniform_count = uniforms;
        p->attrib_count = attribs;
        ok = p->uniforms && p->attribs && parse_vars(&r, p->uniforms, uniforms, 1)
             && parse_vars(&r, p->attribs, attribs, 0);
    }
    mugl_unlock();

    if (!ok) {
        mugl_program_forget(id);
        p->loaded = 1;
        return NULL;
    }
    return p;
}

GL_APICALL void GL_APIENTRY glLinkProgram(GLuint program) {
    mugl_program_forget(program);
    const uint32_t a[1] = {program};
    mugl_send(MUGL_OP_glLinkProgram, a, 1);
}

static int split_index(const char *name, size_t *base_len, uint32_t *index) {
    const size_t len = strlen(name);
    *base_len = len;
    *index = 0;
    if (len < 4 || name[len - 1] != ']') return 1;

    size_t open = len - 2;
    while (open > 0 && name[open] >= '0' && name[open] <= '9')
        open--;
    if (name[open] != '[' || open == len - 2) return 0;

    uint32_t value = 0;
    for (size_t i = open + 1; i < len - 1; i++) {
        value = value * 10U + (uint32_t) (name[i] - '0');
        if (value > 65535U) return 0;
    }
    *base_len = open;
    *index = value;
    return 1;
}

int mugl_program_location(int uniform, GLuint id, const char *name, GLint *out) {
    if (!name) return 0;
    const program_info *p = program_load(id);
    if (!p) return 0;

    *out = -1;
    if (!uniform) {
        for (uint32_t i = 0; i < p->attrib_count; i++) {
            if (strcmp(p->attribs[i].name, name) == 0) {
                *out = p->attribs[i].locs[0];
                break;
            }
        }
        return 1;
    }

    for (uint32_t i = 0; i < p->uniform_count; i++) {
        if (strcmp(p->uniforms[i].name, name) == 0) {
            *out = p->uniforms[i].locs[0];
            return 1;
        }
    }

    size_t base_len = 0;
    uint32_t index = 0;
    if (!split_index(name, &base_len, &index)) return 1;
    for (uint32_t i = 0; i < p->uniform_count; i++) {
        const program_var *v = &p->uniforms[i];
        if (v->base_len != base_len || strncmp(v->name, name, base_len) != 0) continue;
        if (index < v->loc_count) *out = v->locs[index];
        return 1;
    }
    return 1;
}

int mugl_program_active(
    int uniform, GLuint id, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name
) {
    const program_info *p = program_load(id);
    if (!p) return 0;

    const uint32_t count = uniform ? p->uniform_count : p->attrib_count;
    if (index >= count) {
        mugl_set_error(GL_INVALID_VALUE);
        return 1;
    }

    const program_var *v = uniform ? &p->uniforms[index] : &p->attribs[index];
    if (size) *size = v->size;
    if (type) *type = v->type;

    size_t n = strlen(v->name);
    if (bufSize <= 0)
        n = 0;
    else if (n > (size_t) bufSize - 1U)
        n = (size_t) bufSize - 1U;
    if (name && bufSize > 0) {
        memcpy(name, v->name, n);
        name[n] = '\0';
    }
    if (length) *length = (GLsizei) n;
    return 1;
}

int mugl_program_iv(GLuint id, GLenum pname, GLint *out) {
    if (!out) return 0;
    switch (pname) {
        case GL_LINK_STATUS:
        case GL_ACTIVE_UNIFORMS:
        case GL_ACTIVE_ATTRIBUTES:
        case GL_ACTIVE_UNIFORM_MAX_LENGTH:
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
            break;
        default:
            return 0;
    }

    const program_info *p = program_load(id);
    if (!p) return 0;

    switch (pname) {
        case GL_LINK_STATUS:
            *out = p->linked;
            break;
        case GL_ACTIVE_UNIFORMS:
            *out = (GLint) p->uniform_count;
            break;
        case GL_ACTIVE_ATTRIBUTES:
            *out = (GLint) p->attrib_count;
            break;
        case GL_ACTIVE_UNIFORM_MAX_LENGTH:
            *out = p->uniform_max;
            break;
        default:
            *out = p->attrib_max;
            break;
    }
    return 1;
}
