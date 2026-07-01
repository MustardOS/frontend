#include <stdint.h>
#include <string.h>
#include "util.h"
#include "lookup.h"
#include "../lookup/0.h"
#include "../lookup/1.h"
#include "../lookup/2.h"
#include "../lookup/3.h"
#include "../lookup/4.h"
#include "../lookup/5.h"
#include "../lookup/6.h"
#include "../lookup/7.h"
#include "../lookup/8.h"
#include "../lookup/9.h"
#include "../lookup/a.h"
#include "../lookup/b.h"
#include "../lookup/c.h"
#include "../lookup/d.h"
#include "../lookup/e.h"
#include "../lookup/f.h"
#include "../lookup/g.h"
#include "../lookup/h.h"
#include "../lookup/i.h"
#include "../lookup/j.h"
#include "../lookup/k.h"
#include "../lookup/l.h"
#include "../lookup/m.h"
#include "../lookup/n.h"
#include "../lookup/o.h"
#include "../lookup/p.h"
#include "../lookup/q.h"
#include "../lookup/r.h"
#include "../lookup/s.h"
#include "../lookup/t.h"
#include "../lookup/u.h"
#include "../lookup/v.h"
#include "../lookup/w.h"
#include "../lookup/x.h"
#include "../lookup/y.h"
#include "../lookup/z.h"

static void get_table(const size_t index, const lookup_internal_name **table, size_t *count) {
    switch (index) {
        case 0:
            *table = lookup_0_table;
            *count = lookup_0_count;
            return;
        case 1:
            *table = lookup_1_table;
            *count = lookup_1_count;
            return;
        case 2:
            *table = lookup_2_table;
            *count = lookup_2_count;
            return;
        case 3:
            *table = lookup_3_table;
            *count = lookup_3_count;
            return;
        case 4:
            *table = lookup_4_table;
            *count = lookup_4_count;
            return;
        case 5:
            *table = lookup_5_table;
            *count = lookup_5_count;
            return;
        case 6:
            *table = lookup_6_table;
            *count = lookup_6_count;
            return;
        case 7:
            *table = lookup_7_table;
            *count = lookup_7_count;
            return;
        case 8:
            *table = lookup_8_table;
            *count = lookup_8_count;
            return;
        case 9:
            *table = lookup_9_table;
            *count = lookup_9_count;
            return;
        case 10:
            *table = lookup_a_table;
            *count = lookup_a_count;
            return;
        case 11:
            *table = lookup_b_table;
            *count = lookup_b_count;
            return;
        case 12:
            *table = lookup_c_table;
            *count = lookup_c_count;
            return;
        case 13:
            *table = lookup_d_table;
            *count = lookup_d_count;
            return;
        case 14:
            *table = lookup_e_table;
            *count = lookup_e_count;
            return;
        case 15:
            *table = lookup_f_table;
            *count = lookup_f_count;
            return;
        case 16:
            *table = lookup_g_table;
            *count = lookup_g_count;
            return;
        case 17:
            *table = lookup_h_table;
            *count = lookup_h_count;
            return;
        case 18:
            *table = lookup_i_table;
            *count = lookup_i_count;
            return;
        case 19:
            *table = lookup_j_table;
            *count = lookup_j_count;
            return;
        case 20:
            *table = lookup_k_table;
            *count = lookup_k_count;
            return;
        case 21:
            *table = lookup_l_table;
            *count = lookup_l_count;
            return;
        case 22:
            *table = lookup_m_table;
            *count = lookup_m_count;
            return;
        case 23:
            *table = lookup_n_table;
            *count = lookup_n_count;
            return;
        case 24:
            *table = lookup_o_table;
            *count = lookup_o_count;
            return;
        case 25:
            *table = lookup_p_table;
            *count = lookup_p_count;
            return;
        case 26:
            *table = lookup_q_table;
            *count = lookup_q_count;
            return;
        case 27:
            *table = lookup_r_table;
            *count = lookup_r_count;
            return;
        case 28:
            *table = lookup_s_table;
            *count = lookup_s_count;
            return;
        case 29:
            *table = lookup_t_table;
            *count = lookup_t_count;
            return;
        case 30:
            *table = lookup_u_table;
            *count = lookup_u_count;
            return;
        case 31:
            *table = lookup_v_table;
            *count = lookup_v_count;
            return;
        case 32:
            *table = lookup_w_table;
            *count = lookup_w_count;
            return;
        case 33:
            *table = lookup_x_table;
            *count = lookup_x_count;
            return;
        case 34:
            *table = lookup_y_table;
            *count = lookup_y_count;
            return;
        case 35:
            *table = lookup_z_table;
            *count = lookup_z_count;
            return;
        default:
            *table = NULL;
            *count = 0;
    }
}

static int letter_index(const unsigned char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'z') {
        return 10 + (c - 'a');
    }
    return -1;
}

typedef struct {
    uint32_t *order;
    int built;
} lookup_sort_cache;

static lookup_sort_cache sort_cache[LOOKUP_TABLE_COUNT];

static const lookup_internal_name *sort_table;

static int cmp_order_by_name(const void *a, const void *b) {
    const uint32_t ia = *(const uint32_t *) a;
    const uint32_t ib = *(const uint32_t *) b;
    return strcmp(sort_table[ia].name, sort_table[ib].name);
}

static void ensure_sorted(const lookup_internal_name *table, const size_t count, lookup_sort_cache *cache) {
    if (cache->built) {
        return;
    }

    cache->order = mux_malloc(count * sizeof(uint32_t));
    for (uint32_t i = 0; i < count; i++) {
        cache->order[i] = i;
    }

    sort_table = table;
    qsort(cache->order, count, sizeof(uint32_t), cmp_order_by_name);

    cache->built = 1;
}

static const char *
lookup_generic(const lookup_internal_name *table, const size_t count, lookup_sort_cache *cache, const char *name) {
    if (count == 0) {
        return NULL;
    }

    ensure_sorted(table, count, cache);

    size_t lo = 0, hi = count;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        const int cmp = strcmp(table[cache->order[mid]].name, name);

        if (cmp == 0) {
            return table[cache->order[mid]].value;
        }

        if (cmp < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return NULL;
}

static const char *r_lookup_generic(const lookup_internal_name *table, const size_t count, const char *value) {
    for (size_t i = 0; i < count; i++) {
        if (strstr(table[i].value, value)) {
            return table[i].name;
        }
    }

    return NULL;
}

static void lookup_generic_multi(
    const lookup_internal_name *table, const size_t count, const char *term,
    void (*emit)(const char *name, const char *value, void *udata), void *udata
) {
    if (!term) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (strcasestr(table[i].name, term)) {
            emit(table[i].name, table[i].value, udata);
        }
    }
}

static void r_lookup_generic_multi(
    const lookup_internal_name *table, const size_t count, const char *term,
    void (*emit)(const char *name, const char *value, void *udata), void *udata
) {
    if (!term) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        if (strcasestr(table[i].value, term)) {
            emit(table[i].name, table[i].value, udata);
        }
    }
}

const char *lookup(const char *name) {
    if (!name || !name[0]) {
        return NULL;
    }

    const int idx = letter_index((unsigned char) name[0]);
    if (idx < 0) {
        return NULL;
    }

    const lookup_internal_name *table;
    size_t count;
    get_table((size_t) idx, &table, &count);

    return lookup_generic(table, count, &sort_cache[idx], name);
}

const char *r_lookup(const char *value) {
    if (!value || !value[0]) {
        return NULL;
    }

    for (size_t i = 0; i < LOOKUP_TABLE_COUNT; i++) {
        const lookup_internal_name *table;
        size_t count;
        get_table(i, &table, &count);

        const char *result = r_lookup_generic(table, count, value);
        if (result) {
            return result;
        }
    }

    return NULL; // you get nothing, you lose, good day sir
}

void lookup_multi_at(
    const size_t index, const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata
) {
    if (index >= LOOKUP_TABLE_COUNT) {
        return;
    }

    const lookup_internal_name *table;
    size_t count;
    get_table(index, &table, &count);

    lookup_generic_multi(table, count, term, emit, udata);
}

void r_lookup_multi_at(
    const size_t index, const char *term, void (*emit)(const char *name, const char *value, void *udata), void *udata
) {
    if (index >= LOOKUP_TABLE_COUNT) {
        return;
    }

    const lookup_internal_name *table;
    size_t count;
    get_table(index, &table, &count);

    r_lookup_generic_multi(table, count, term, emit, udata);
}
