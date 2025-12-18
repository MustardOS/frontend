#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <linux/limits.h>

#include "../common/common.h"
#include "../lookup/lookup.h"
#include "../common/json/json.h"

#define LOOKUP_DIR_PATH OPT_SHARE_PATH  "lookup/"
#define LOOKUP_INTERNAL LOOKUP_DIR_PATH "internal.txt"
#define LOOKUP_GLOBAL   LOOKUP_DIR_PATH "global.txt"

#define FOLDER_JSON INFO_NAM_PATH "/%s.json"
#define GLOBAL_JSON INFO_NAM_PATH "/global.json"

static char *json_cache_data = NULL;
static struct json json_cache_root;

static char term_lower[MAX_BUFFER_SIZE];
static char *term = NULL;
static char *folder = NULL;

static int gen_mode = 0;
static int gen_internal = 0;
static int gen_global = 0;
static int gen_folder = 0;
static int gen_all = 0;

struct result_item {
    char *name;
    char *value;
};

struct results {
    struct result_item *items;
    size_t count;
    size_t cap;
};

enum lookup_mode {
    LOOKUP_KEY,
    LOOKUP_VALUE
};

static void results_reserve(struct results *r) {
    r->items = malloc(MAX_BUFFER_SIZE * sizeof(struct result_item));
    r->cap = MAX_BUFFER_SIZE;
    r->count = 0;
}

static char *dup_fast(const char *s) {
    size_t len = strlen(s) + 1;
    char *m = malloc(len);

    if (!m) return NULL;
    memcpy(m, s, len);

    return m;
}

static void results_push(struct results *r, const char *name, const char *value) {
    if (r->count == r->cap) {
        size_t new_cap = r->cap * 2;
        struct result_item *new = realloc(r->items, new_cap * sizeof(struct result_item));

        if (!new) return;

        r->items = new;
        r->cap = new_cap;
    }

    r->items[r->count].name = dup_fast(name);
    r->items[r->count].value = dup_fast(value);

    r->count++;
}

static int load_json_cache(const char *path) {
    if (json_cache_data) {
        free(json_cache_data);
        json_cache_data = NULL;
    }

    json_cache_data = read_all_char_from(path);
    if (!json_cache_data) return 0;

    if (!json_valid(json_cache_data)) {
        free(json_cache_data);
        json_cache_data = NULL;
        return 0;
    }

    json_cache_root = json_parse(json_cache_data);
    return json_exists(json_cache_root);
}

static int casefold_match(const char *h_stack) {
    if (term_lower[0] == '\0') return 1;
    unsigned char first = term_lower[0];

    while (*h_stack) {
        if (tolower((unsigned char) *h_stack) == first) {
            const char *h = h_stack + 1;
            const char *n = term_lower + 1;

            while (*h && *n && tolower((unsigned char) *h) == (unsigned char) *n) {
                h++;
                n++;
            }

            if (*n == '\0') return 1;
        }
        h_stack++;
    }

    return 0;
}

static void lookup_common(struct results *out, enum lookup_mode mode) {
    if (!json_cache_data) return;

    struct json key = json_first(json_cache_root);
    while (json_exists(key)) {
        struct json val = json_next(key);

        char k_buf[MAX_BUFFER_SIZE];
        char v_buf[MAX_BUFFER_SIZE];

        json_string_copy(key, k_buf, sizeof(k_buf));
        json_string_copy(val, v_buf, sizeof(v_buf));

        const char *match = (mode == LOOKUP_KEY) ? k_buf : v_buf;
        if (casefold_match(match)) results_push(out, k_buf, v_buf);

        key = json_next(val);
    }
}

static inline void lookup_key(struct results *out) {
    lookup_common(out, LOOKUP_KEY);
}

static inline void lookup_value(struct results *out) {
    lookup_common(out, LOOKUP_VALUE);
}

static int cmp_items(const void *a, const void *b) {
    const struct result_item *A = a;
    const struct result_item *B = b;

    int ap = str_startswith(A->value, term);
    int bp = str_startswith(B->value, term);

    if (ap != bp) return (bp - ap);

    const char *v1 = A->value;
    const char *v2 = B->value;

    int rc = str_compare(&v1, &v2);
    if (rc != 0) return rc;

    const char *n1 = A->name;
    const char *n2 = B->name;

    return str_compare(&n1, &n2);
}

static void dedupe_results(struct results *r) {
    if (r->count < 2) return;

    size_t w = 0;
    for (size_t i = 1; i < r->count; i++) {
        int same = !strcasecmp(r->items[w].name, r->items[i].name)
                && !strcasecmp(r->items[w].value, r->items[i].value);

        if (!same) {
            r->items[++w] = r->items[i];
        } else {
            free(r->items[i].name);
            free(r->items[i].value);
        }
    }

    r->count = w + 1;
}

static struct {
    void (*forward_multi)(const char *, void (*emit)(const char *, const char *, void *), void *);
    void (*reverse_multi)(const char *, void (*emit)(const char *, const char *, void *), void *);
} multi_table[] = {
        {lookup_0_multi, r_lookup_0_multi},
        {lookup_1_multi, r_lookup_1_multi},
        {lookup_2_multi, r_lookup_2_multi},
        {lookup_3_multi, r_lookup_3_multi},
        {lookup_4_multi, r_lookup_4_multi},
        {lookup_5_multi, r_lookup_5_multi},
        {lookup_6_multi, r_lookup_6_multi},
        {lookup_7_multi, r_lookup_7_multi},
        {lookup_8_multi, r_lookup_8_multi},
        {lookup_9_multi, r_lookup_9_multi},
        {lookup_a_multi, r_lookup_a_multi},
        {lookup_b_multi, r_lookup_b_multi},
        {lookup_c_multi, r_lookup_c_multi},
        {lookup_d_multi, r_lookup_d_multi},
        {lookup_e_multi, r_lookup_e_multi},
        {lookup_f_multi, r_lookup_f_multi},
        {lookup_g_multi, r_lookup_g_multi},
        {lookup_h_multi, r_lookup_h_multi},
        {lookup_i_multi, r_lookup_i_multi},
        {lookup_j_multi, r_lookup_j_multi},
        {lookup_k_multi, r_lookup_k_multi},
        {lookup_l_multi, r_lookup_l_multi},
        {lookup_m_multi, r_lookup_m_multi},
        {lookup_n_multi, r_lookup_n_multi},
        {lookup_o_multi, r_lookup_o_multi},
        {lookup_p_multi, r_lookup_p_multi},
        {lookup_q_multi, r_lookup_q_multi},
        {lookup_r_multi, r_lookup_r_multi},
        {lookup_s_multi, r_lookup_s_multi},
        {lookup_t_multi, r_lookup_t_multi},
        {lookup_u_multi, r_lookup_u_multi},
        {lookup_v_multi, r_lookup_v_multi},
        {lookup_w_multi, r_lookup_w_multi},
        {lookup_x_multi, r_lookup_x_multi},
        {lookup_y_multi, r_lookup_y_multi},
        {lookup_z_multi, r_lookup_z_multi},
};

static void emit_pair(const char *name, const char *value, void *ud) {
    results_push((struct results *) ud, name, value);
}

static void write_results_to_file(struct results *r, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return;

    term = "";
    qsort(r->items, r->count, sizeof(struct result_item), cmp_items);
    dedupe_results(r);

    for (size_t i = 0; i < r->count; i++) {
        if (!r->items[i].name || !r->items[i].value) continue;

        char id_lc[MAX_BUFFER_SIZE];
        size_t j = 0;

        for (; r->items[i].name[j] && j < sizeof(id_lc) - 1; j++) {
            id_lc[j] = tolower((unsigned char) r->items[i].name[j]);
        }

        id_lc[j] = 0;
        fprintf(fp, "%s|%s\n", id_lc, r->items[i].value);
    }

    fclose(fp);
}

static void generate_internal(void) {
    struct results r;
    results_reserve(&r);

    for (size_t j = 0; j < A_SIZE(multi_table); j++) {
        multi_table[j].forward_multi("", emit_pair, &r);
        multi_table[j].reverse_multi("", emit_pair, &r);
    }

    write_results_to_file(&r, LOOKUP_INTERNAL);
}

static void generate_global(void) {
    struct results r;
    results_reserve(&r);

    if (load_json_cache(GLOBAL_JSON)) {
        lookup_key(&r);
        lookup_value(&r);
    }

    write_results_to_file(&r, LOOKUP_GLOBAL);
}

static void generate_folder(const char *folder_name) {
    char f_path[PATH_MAX];
    snprintf(f_path, sizeof(f_path), LOOKUP_DIR_PATH "%s.txt", folder_name);

    char j_path[PATH_MAX];
    snprintf(j_path, sizeof(j_path), FOLDER_JSON, folder_name);

    struct results r;
    results_reserve(&r);

    if (load_json_cache(j_path)) {
        lookup_key(&r);
        lookup_value(&r);
    }

    write_results_to_file(&r, f_path);
}

static void generate_all(void) {
    generate_internal();
    generate_global();

    DIR *d = opendir(INFO_NAM_PATH);
    if (!d) return;

    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strstr(e->d_name, ".json")) continue;
        if (!strcmp(e->d_name, "global.json")) continue;
        if (!strcmp(e->d_name, "folder.json")) continue;

        char folder_name[MAX_BUFFER_SIZE];
        strncpy(folder_name, e->d_name, sizeof(folder_name));
        folder_name[strlen(folder_name) - 5] = '\0';

        generate_folder(folder_name);
    }

    closedir(d);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s [--gen-all] [--gen-internal] [--gen-global]\n"
                "          [--gen-folder <folder>] [--folder <folder>] <term>\n",
                argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--gen-all")) {
            gen_all = 1;
            gen_mode = 1;
            continue;
        }
        if (!strcmp(argv[i], "--gen-internal")) {
            gen_internal = 1;
            gen_mode = 1;
            continue;
        }
        if (!strcmp(argv[i], "--gen-global")) {
            gen_global = 1;
            gen_mode = 1;
            continue;
        }
        if (!strcmp(argv[i], "--gen-folder")) {
            if (i + 1 < argc) {
                folder = argv[++i];
                gen_folder = 1;
                gen_mode = 1;
            } else {
                fprintf(stderr, "Error: missing folder name for --gen-folder\n");
                return 1;
            }
            continue;
        }
        if (!strcmp(argv[i], "--folder") || !strcmp(argv[i], "-f")) {
            if (i + 1 < argc) folder = argv[++i];
            else {
                fprintf(stderr, "Error: missing argument for --folder\n");
                return 1;
            }
            continue;
        }
        term = argv[i];
    }

    if (gen_mode) {
        if (gen_all) {
            generate_all();
            return 0;
        }
        if (gen_internal) {
            generate_internal();
            return 0;
        }
        if (gen_global) {
            generate_global();
            return 0;
        }
        if (gen_folder && folder) {
            generate_folder(folder);
            return 0;
        }
        return 0;
    }

    if (!term) {
        fprintf(stderr, "Error: missing search term\n");
        return 1;
    }

    size_t i = 0;
    for (; term[i]; i++) term_lower[i] = tolower((unsigned char) term[i]);
    term_lower[i] = 0;

    char json_path[PATH_MAX];
    if (folder) {
        snprintf(json_path, sizeof(json_path), FOLDER_JSON, folder);
        if (!file_exist(json_path)) snprintf(json_path, sizeof(json_path), "%s", GLOBAL_JSON);
    } else {
        snprintf(json_path, sizeof(json_path), "%s", GLOBAL_JSON);
    }

    load_json_cache(json_path);

    struct results out;
    results_reserve(&out);

    unsigned char f0 = tolower((unsigned char) term[0]);
    int forward_start = 0, forward_end = A_SIZE(multi_table);

    if (isalnum(f0)) {
        if (f0 >= '0' && f0 <= '9') {
            forward_start = f0 - '0';
            forward_end = forward_start + 1;
        } else if (f0 >= 'a' && f0 <= 'z') {
            forward_start = 10 + (f0 - 'a');
            forward_end = forward_start + 1;
        }
    }

    for (int j = forward_start; j < forward_end; j++) {
        multi_table[j].forward_multi(term, emit_pair, &out);
    }

    for (size_t j = 0; j < A_SIZE(multi_table); j++) {
        multi_table[j].reverse_multi(term, emit_pair, &out);
    }

    lookup_key(&out);
    lookup_value(&out);

    qsort(out.items, out.count, sizeof(struct result_item), cmp_items);
    dedupe_results(&out);

    if (out.count == 0) return 0;

    for (size_t j = 0; j < out.count; j++) {
        printf("%s|%s\n", out.items[j].name, out.items[j].value);
    }

    return 0;
}
