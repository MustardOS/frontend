#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "../common/var_store.h"

static int parse_ns(const char *s) {
    if (!s) return -1;
    if (s[0] >= '0' && s[0] <= '3' && s[1] == '\0') return s[0] - '0';
    if (strcasecmp(s, "global") == 0 || strcasecmp(s, "config") == 0) return vs_ns_global;
    if (strcasecmp(s, "device") == 0) return vs_ns_device;
    if (strcasecmp(s, "kiosk") == 0) return vs_ns_kiosk;
    if (strcasecmp(s, "system") == 0) return vs_ns_system;
    return -1;
}

static const char *ns_name(const var_ns_t ns) {
    switch (ns) {
        case vs_ns_global:
            return "global";
        case vs_ns_device:
            return "device";
        case vs_ns_kiosk:
            return "kiosk";
        case vs_ns_system:
            return "system";
        default:
            return "?";
    }
}

static int cmd_build(void) {
    var_store_t vs;
    var_dirs_t dirs;
    vs_default_dirs(&dirs);

    if (vs_open(&vs, vs_cache_path(), 1, VS_DEF_CAP, 1) != vs_ok) {
        fprintf(stderr, "muvarctl: build: cannot open cache '%s'\n", vs_cache_path());
        return 1;
    }

    const int rc = vs_build(&vs, &dirs);
    vs_close(&vs);

    if (rc < 0) {
        fprintf(stderr, "muvarctl: build: %d entries skipped\n", -rc);
        return 1;
    }

    return 0;
}

static int cmd_get(const char *ns_arg, const char *key) {
    const int ns = parse_ns(ns_arg);
    if (ns < 0 || !key) return 1;

    var_store_t vs;

    if (vs_open(&vs, vs_cache_path(), 0, 0, 0) != vs_ok) return 0;

    char out[VS_VAL_MAX];
    const int rc = vs_get(&vs, (var_ns_t) ns, key, out, sizeof(out));
    vs_close(&vs);

    if (rc == vs_ok) fputs(out, stdout);

    return rc == vs_err_inval ? 1 : 0;
}

static int cmd_set(const char *ns_arg, const char *key, const char *value, const int defer) {
    const int ns = parse_ns(ns_arg);
    if (ns < 0 || !key || !value) return 1;

    var_store_t vs;
    var_dirs_t dirs;
    vs_default_dirs(&dirs);

    if (vs_open(&vs, vs_cache_path(), 1, VS_DEF_CAP, 1) != vs_ok) {
        return defer || vs_write(&dirs, (var_ns_t) ns, key, value) != vs_ok;
    }

    const int rc = defer ? vs_set(&vs, (var_ns_t) ns, key, value) : vs_store(&vs, &dirs, (var_ns_t) ns, key, value);
    vs_close(&vs);

    return rc == vs_ok ? 0 : 1;
}

static int cmd_del(const char *ns_arg, const char *key) {
    const int ns = parse_ns(ns_arg);
    if (ns < 0 || !key) return 1;

    var_store_t vs;
    if (vs_open(&vs, vs_cache_path(), 0, 0, 1) != vs_ok) return 0;

    const int rc = vs_del(&vs, (var_ns_t) ns, key);
    vs_close(&vs);

    return rc == vs_ok || rc == vs_notfound ? 0 : 1;
}

static int cmd_flush(void) {
    var_store_t vs;
    var_dirs_t dirs;
    vs_default_dirs(&dirs);

    if (vs_open(&vs, vs_cache_path(), 0, 0, 1) != vs_ok) return 0;

    const int failed = vs_flush(&vs, &dirs);
    vs_close(&vs);

    if (failed > 0) {
        fprintf(stderr, "muvarctl: flush: %d entries failed to write\n", failed);
        return 1;
    }

    return 0;
}

static int cmd_dump(const char *ns_arg) {
    const int filter_ns = ns_arg ? parse_ns(ns_arg) : -1;

    var_store_t vs;
    if (vs_open(&vs, vs_cache_path(), 0, 0, 0) != vs_ok) {
        fprintf(stderr, "muvarctl: dump: no cache present\n");
        return 1;
    }

    for (uint32_t i = 0; i < vs.hdr->capacity; i++) {
        const var_slot_t *s = &vs.slots[i];
        if (!s->occupied || s->tombstone) continue;
        if (filter_ns >= 0 && s->ns != (uint8_t) filter_ns) continue;

        char key[VS_KEY_MAX], value[VS_VAL_MAX];
        vs_copy_out(key, sizeof(key), s->key, VS_KEY_MAX);
        vs_copy_out(value, sizeof(value), s->value, VS_VAL_MAX);

        printf("%-7s %-4s %-40s %s\n", ns_name((var_ns_t) s->ns), s->dirty ? "DIRT" : "-", key, value);
    }

    vs_close(&vs);
    return 0;
}

static void usage(const char *argv0) {
    fprintf(
        stderr,
        "usage: %s build\n"
        "       %s get   <ns> <key>\n"
        "       %s set   [--defer] <ns> <key> <value>\n"
        "       %s del   <ns> <key>\n"
        "       %s flush\n"
        "       %s dump  [ns]\n"
        "  ns := global|device|kiosk|system  (or 0-3)\n",
        argv0, argv0, argv0, argv0, argv0, argv0
    );
}

int main(const int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "build") == 0) return cmd_build();
    if (strcmp(argv[1], "flush") == 0) return cmd_flush();
    if (strcmp(argv[1], "dump") == 0) return cmd_dump(argc > 2 ? argv[2] : NULL);

    if (strcmp(argv[1], "get") == 0) {
        if (argc != 4) {
            usage(argv[0]);
            return 2;
        }
        return cmd_get(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "set") == 0) {
        const int defer = argc > 2 && strcmp(argv[2], "--defer") == 0;
        const int base = defer ? 3 : 2;
        if (argc != base + 3) {
            usage(argv[0]);
            return 2;
        }
        return cmd_set(argv[base], argv[base + 1], argv[base + 2], defer);
    }

    if (strcmp(argv[1], "del") == 0) {
        if (argc != 4) {
            usage(argv[0]);
            return 2;
        }
        return cmd_del(argv[2], argv[3]);
    }

    usage(argv[0]);
    return 2;
}
