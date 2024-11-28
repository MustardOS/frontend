#include "lookup.h"

const char *lookup(const char *name) {
    if (!name || !name[0]) return NULL;

    switch (name[0]) {
        case '0':
            return lookup_0(name);
        case '1':
            return lookup_1(name);
        case '2':
            return lookup_2(name);
        case '3':
            return lookup_3(name);
        case '4':
            return lookup_4(name);
        case '5':
            return lookup_5(name);
        case '6':
            return lookup_6(name);
        case '7':
            return lookup_7(name);
        case '8':
            return lookup_8(name);
        case '9':
            return lookup_9(name);
        case 'a':
            return lookup_a(name);
        case 'b':
            return lookup_b(name);
        case 'c':
            return lookup_c(name);
        case 'd':
            return lookup_d(name);
        case 'e':
            return lookup_e(name);
        case 'f':
            return lookup_f(name);
        case 'g':
            return lookup_g(name);
        case 'h':
            return lookup_h(name);
        case 'i':
            return lookup_i(name);
        case 'j':
            return lookup_j(name);
        case 'k':
            return lookup_k(name);
        case 'l':
            return lookup_l(name);
        case 'm':
            return lookup_m(name);
        case 'n':
            return lookup_n(name);
        case 'o':
            return lookup_o(name);
        case 'p':
            return lookup_p(name);
        case 'q':
            return lookup_q(name);
        case 'r':
            return lookup_r(name);
        case 's':
            return lookup_s(name);
        case 't':
            return lookup_t(name);
        case 'u':
            return lookup_u(name);
        case 'v':
            return lookup_v(name);
        case 'w':
            return lookup_w(name);
        case 'x':
            return lookup_x(name);
        case 'y':
            return lookup_y(name);
        case 'z':
            return lookup_z(name);
        default:
            return NULL;
    }
}

const char *r_lookup(const char *value) {
    if (!value) return NULL;

    const char *result;

    result = r_lookup_0(value);
    if (result) return result;

    result = r_lookup_1(value);
    if (result) return result;

    result = r_lookup_2(value);
    if (result) return result;

    result = r_lookup_3(value);
    if (result) return result;

    result = r_lookup_4(value);
    if (result) return result;

    result = r_lookup_5(value);
    if (result) return result;

    result = r_lookup_6(value);
    if (result) return result;

    result = r_lookup_7(value);
    if (result) return result;

    result = r_lookup_8(value);
    if (result) return result;

    result = r_lookup_9(value);
    if (result) return result;

    result = r_lookup_a(value);
    if (result) return result;

    result = r_lookup_b(value);
    if (result) return result;

    result = r_lookup_c(value);
    if (result) return result;

    result = r_lookup_d(value);
    if (result) return result;

    result = r_lookup_e(value);
    if (result) return result;

    result = r_lookup_f(value);
    if (result) return result;

    result = r_lookup_g(value);
    if (result) return result;

    result = r_lookup_h(value);
    if (result) return result;

    result = r_lookup_i(value);
    if (result) return result;

    result = r_lookup_j(value);
    if (result) return result;

    result = r_lookup_k(value);
    if (result) return result;

    result = r_lookup_l(value);
    if (result) return result;

    result = r_lookup_m(value);
    if (result) return result;

    result = r_lookup_n(value);
    if (result) return result;

    result = r_lookup_o(value);
    if (result) return result;

    result = r_lookup_p(value);
    if (result) return result;

    result = r_lookup_q(value);
    if (result) return result;

    result = r_lookup_r(value);
    if (result) return result;

    result = r_lookup_s(value);
    if (result) return result;

    result = r_lookup_t(value);
    if (result) return result;

    result = r_lookup_u(value);
    if (result) return result;

    result = r_lookup_v(value);
    if (result) return result;

    result = r_lookup_w(value);
    if (result) return result;

    result = r_lookup_x(value);
    if (result) return result;

    result = r_lookup_y(value);
    if (result) return result;

    result = r_lookup_z(value);
    if (result) return result;

    return NULL; // you get nothing, you lose, good day sir
}
