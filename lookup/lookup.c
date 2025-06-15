#include "../common/common.h"
#include "lookup.h"

const char *lookup(const char *name) {
    if (!name || !name[0]) return NULL;

    static const char *(*lookup_table[MAX_BUFFER_SIZE])(const char *) = {
            ['0'] = lookup_0, ['1'] = lookup_1, ['2'] = lookup_2, ['3'] = lookup_3,
            ['4'] = lookup_4, ['5'] = lookup_5, ['6'] = lookup_6, ['7'] = lookup_7,
            ['8'] = lookup_8, ['9'] = lookup_9, ['a'] = lookup_a, ['b'] = lookup_b,
            ['c'] = lookup_c, ['d'] = lookup_d, ['e'] = lookup_e, ['f'] = lookup_f,
            ['g'] = lookup_g, ['h'] = lookup_h, ['i'] = lookup_i, ['j'] = lookup_j,
            ['k'] = lookup_k, ['l'] = lookup_l, ['m'] = lookup_m, ['n'] = lookup_n,
            ['o'] = lookup_o, ['p'] = lookup_p, ['q'] = lookup_q, ['r'] = lookup_r,
            ['s'] = lookup_s, ['t'] = lookup_t, ['u'] = lookup_u, ['v'] = lookup_v,
            ['w'] = lookup_w, ['x'] = lookup_x, ['y'] = lookup_y, ['z'] = lookup_z
    };

    if (lookup_table[(unsigned char) name[0]]) return lookup_table[(unsigned char) name[0]](name);

    return NULL;
}

const char *r_lookup(const char *p_value) {
    if (!p_value || !p_value[0]) return NULL;

    const char *(*lookups[])(const char *) = {
            r_lookup_0, r_lookup_1, r_lookup_2, r_lookup_3,
            r_lookup_4, r_lookup_5, r_lookup_6, r_lookup_7,
            r_lookup_8, r_lookup_9, r_lookup_a, r_lookup_b,
            r_lookup_c, r_lookup_d, r_lookup_e, r_lookup_f,
            r_lookup_g, r_lookup_h, r_lookup_i, r_lookup_j,
            r_lookup_k, r_lookup_l, r_lookup_m, r_lookup_n,
            r_lookup_o, r_lookup_p, r_lookup_q, r_lookup_r,
            r_lookup_s, r_lookup_t, r_lookup_u, r_lookup_v,
            r_lookup_w, r_lookup_x, r_lookup_y, r_lookup_z
    };

    for (size_t i = 0; i < A_SIZE(lookups); i++) {
        const char *result = lookups[i](p_value);
        if (result) return result;
    }

    return NULL; // you get nothing, you lose, good day sir
}
