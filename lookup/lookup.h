#pragma once

#include <stddef.h>

typedef struct {
    const char *name;
    const char *value;
} LookupName;

#define DECLARE_LOOKUP_BLOCK(X)                                                \
    extern const LookupName lookup_##X##_table[];                              \
    extern const size_t lookup_##X##_count;                                    \
    const char *lookup_##X(const char *name);                                  \
    const char *r_lookup_##X(const char *value);                               \
    void lookup_##X##_multi(const char *term, void (*emit)(const char *name,   \
                            const char *value, void *udata), void *udata);     \
    void r_lookup_##X##_multi(const char *term, void (*emit)(const char *name, \
                              const char *value, void *udata), void *udata);

DECLARE_LOOKUP_BLOCK(0)

DECLARE_LOOKUP_BLOCK(1)

DECLARE_LOOKUP_BLOCK(2)

DECLARE_LOOKUP_BLOCK(3)

DECLARE_LOOKUP_BLOCK(4)

DECLARE_LOOKUP_BLOCK(5)

DECLARE_LOOKUP_BLOCK(6)

DECLARE_LOOKUP_BLOCK(7)

DECLARE_LOOKUP_BLOCK(8)

DECLARE_LOOKUP_BLOCK(9)

DECLARE_LOOKUP_BLOCK(a)

DECLARE_LOOKUP_BLOCK(b)

DECLARE_LOOKUP_BLOCK(c)

DECLARE_LOOKUP_BLOCK(d)

DECLARE_LOOKUP_BLOCK(e)

DECLARE_LOOKUP_BLOCK(f)

DECLARE_LOOKUP_BLOCK(g)

DECLARE_LOOKUP_BLOCK(h)

DECLARE_LOOKUP_BLOCK(i)

DECLARE_LOOKUP_BLOCK(j)

DECLARE_LOOKUP_BLOCK(k)

DECLARE_LOOKUP_BLOCK(l)

DECLARE_LOOKUP_BLOCK(m)

DECLARE_LOOKUP_BLOCK(n)

DECLARE_LOOKUP_BLOCK(o)

DECLARE_LOOKUP_BLOCK(p)

DECLARE_LOOKUP_BLOCK(q)

DECLARE_LOOKUP_BLOCK(r)

DECLARE_LOOKUP_BLOCK(s)

DECLARE_LOOKUP_BLOCK(t)

DECLARE_LOOKUP_BLOCK(u)

DECLARE_LOOKUP_BLOCK(v)

DECLARE_LOOKUP_BLOCK(w)

DECLARE_LOOKUP_BLOCK(x)

DECLARE_LOOKUP_BLOCK(y)

DECLARE_LOOKUP_BLOCK(z)

const char *lookup(const char *name);

const char *r_lookup(const char *value);
