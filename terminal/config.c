#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <common/config/config.h>
#include <common/storage/fileio.h>
#include "config.h"

static void ensure_parent_dir(const char *filepath) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", filepath);

    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) fprintf(stderr, "[CFG] mkdir %s: %s\n", tmp, strerror(errno));
        *p = '/';
    }
}

static char *ltrim(char *s) {
    while (*s && isspace((unsigned char) *s))
        s++;
    return s;
}

static void rtrim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char) s[n - 1]))
        s[--n] = '\0';
}

static int parse_hex_colour(const char *hex, SDL_Color *out) {
    if (!hex) return 0;

    while (*hex && isspace((unsigned char) *hex))
        hex++;
    if (*hex == '#') hex++;

    while (*hex && isspace((unsigned char) *hex))
        hex++;
    if (strlen(hex) < 6) return 0;

    unsigned int r, g, b;
    if (sscanf(hex, "%2x%2x%2x", &r, &g, &b) != 3) return 0;

    out->r = (Uint8) r;
    out->g = (Uint8) g;
    out->b = (Uint8) b;
    out->a = 255;

    return 1;
}

static void parse_muxterm_conf(const char *path, MuxtermConfig *cfg) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    fprintf(stderr, "[CFG] reading muxterm config: %s\n", path);

    char line[PATH_MAX];
    while (fgets(line, sizeof(line), f)) {
        char *p = ltrim(line);
        rtrim(p);

        if (!*p || *p == '#') continue;

        char *eq = strchr(p, '=');
        if (!eq) continue;

        *eq = '\0';

        char *key = p;
        char *val = ltrim(eq + 1);

        rtrim(key);
        rtrim(val);

        if (strcmp(key, "width") == 0) {
            int v = atoi(val);
            if (v > 0) cfg->width = v;
        } else if (strcmp(key, "height") == 0) {
            int v = atoi(val);
            if (v > 0) cfg->height = v;
        } else if (strcmp(key, "term_font_size") == 0) {
            int v = atoi(val);
            if (v > 0) {
                cfg->term_font_size = v;
                cfg->term_font_size_explicit = 1;
            }
        } else if (strcmp(key, "menu_font_size") == 0) {
            int v = atoi(val);
            if (v > 0) cfg->menu_font_size = v;
        } else if (strcmp(key, "scrollback") == 0) {
            int v = atoi(val);
            if (v > 0) cfg->scrollback = v;
        } else if (strcmp(key, "scrollback_path") == 0) {
            if (*val) snprintf(cfg->scrollback_path, sizeof(cfg->scrollback_path), "%s", val);
        } else if (strcmp(key, "readonly") == 0) {
            cfg->readonly = atoi(val) != 0;
        } else if (strcmp(key, "zoom") == 0) {
            float v = (float) atof(val);
            if (v > 0.0f) cfg->zoom = v;
        } else if (strcmp(key, "rotate") == 0) {
            int v = atoi(val);
            if (v >= 0 && v <= 3) cfg->rotate = v;
        } else if (strcmp(key, "underscan") == 0) {
            cfg->underscan = atoi(val) != 0;
        } else if (strcmp(key, "term_font_path") == 0) {
            if (*val) {
                snprintf(cfg->term_font_path, sizeof(cfg->term_font_path), "%s", val);
                cfg->term_font_path_explicit = 1;
            }
        } else if (strcmp(key, "term_font_path_bold") == 0) {
            if (*val) snprintf(cfg->term_font_path_bold, sizeof(cfg->term_font_path_bold), "%s", val);
        } else if (strcmp(key, "term_font_path_italic") == 0) {
            if (*val) snprintf(cfg->term_font_path_italic, sizeof(cfg->term_font_path_italic), "%s", val);
        } else if (strcmp(key, "term_font_path_bold_italic") == 0) {
            if (*val) snprintf(cfg->term_font_path_bold_italic, sizeof(cfg->term_font_path_bold_italic), "%s", val);
        } else if (strcmp(key, "bg_image") == 0) {
            snprintf(cfg->bg_image, sizeof(cfg->bg_image), "%s", val);
            cfg->background_image_explicit = 1;
        } else if (strcmp(key, "shell") == 0) {
            snprintf(cfg->shell, sizeof(cfg->shell), "%s", val);
        } else if (strcmp(key, "osk_layout_path") == 0) {
            if (*val) snprintf(cfg->osk_layout_path, sizeof(cfg->osk_layout_path), "%s", val);
        } else if (strcmp(key, "key_repeat_delay") == 0) {
            int v = atoi(val);
            if (v > 0) cfg->key_repeat_delay = v;
        } else if (strcmp(key, "key_repeat_rate") == 0) {
            int v = atoi(val);
            if (v > 0) cfg->key_repeat_rate = v;
        } else if (strcmp(key, "dpad_repeat_delay") == 0) {
            int v = atoi(val);
            if (v > 0) cfg->dpad_repeat_delay = v;
        } else if (strcmp(key, "dpad_repeat_rate") == 0) {
            int v = atoi(val);
            if (v > 0) cfg->dpad_repeat_rate = v;
        } else if (strcmp(key, "force_redraw") == 0) {
            cfg->force_redraw = atoi(val) != 0;
        } else if (strcmp(key, "font_hinting") == 0) {
            if (strcmp(val, "none") == 0) {
                cfg->font_hinting = 3;
            } else if (strcmp(val, "light") == 0) {
                cfg->font_hinting = 1;
            } else if (strcmp(val, "mono") == 0) {
                cfg->font_hinting = 2;
            } else {
                cfg->font_hinting = 0;
            }
            cfg->font_hinting_explicit = 1;
        } else if (strcmp(key, "bg_colour") == 0) {
            SDL_Color c = {0, 0, 0, 255};
            if (*val && parse_hex_colour(val, &c)) {
                cfg->solid_bg = c;
                cfg->use_solid_bg = 1;
                cfg->background_explicit = 1;
            }
        } else if (strcmp(key, "fg_colour") == 0) {
            SDL_Color c = {255, 255, 255, 255};
            if (*val && parse_hex_colour(val, &c)) {
                cfg->solid_fg = c;
                cfg->use_solid_fg = 1;
                cfg->foreground_explicit = 1;
            }
        }
    }

    fclose(f);
}

static int read_muos_file(const char *base, const char *rel, char *buf) {
    char path[512];
    snprintf(path, sizeof(path), "%s%s%s", base, base[strlen(base) - 1] == '/' ? "" : "/", rel);

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    size_t n = fread(buf, 1, 255, f);
    fclose(f);

    buf[n] = '\0';
    while (n > 0 && (unsigned char) buf[n - 1] <= ' ')
        buf[--n] = '\0';

    fprintf(stderr, "[CFG] muOS %s = \"%s\"\n", path, buf);
    return n > 0;
}

static void read_muos_device_config(const char *base, MuxtermConfig *cfg) {
    char val[256];

    if (read_muos_file(base, "screen/width", val)) {
        int v = atoi(val);
        if (v > 0) cfg->width = v;
    }

    if (read_muos_file(base, "screen/height", val)) {
        int v = atoi(val);
        if (v > 0) cfg->height = v;
    }

    if (read_muos_file(base, "screen/zoom", val)) {
        float v = (float) atof(val);
        if (v > 0.0f) cfg->zoom = v;
    }

    if (read_muos_file(base, "screen/rotate", val)) {
        int v = atoi(val);
        if (v >= 0 && v <= 3) cfg->rotate = v;
    }

    if (read_muos_file(base, "board/name", val)) {
        if (strcasecmp(val, "tui-brick") == 0) cfg->term_font_size = 28;
        if (strcasecmp(val, "rg-vita-pro") == 0) cfg->term_font_size = 32;
    }
}

static void read_muos_global_config(const char *base, MuxtermConfig *cfg) {
    char val[256];
    if (read_muos_file(base, "settings/hdmi/scan", val)) cfg->underscan = (atoi(val) == 1);
}

static void apply_muos_terminal_config(MuxtermConfig *cfg, const struct mux_config *muos_config) {
    if (!muos_config) return;

    if (muos_config->terminal.font_size > 0) {
        cfg->term_font_size = muos_config->terminal.font_size;
        cfg->term_font_size_explicit = 1;
    }
    if (muos_config->terminal.font_hinting >= 0) {
        cfg->font_hinting = muos_config->terminal.font_hinting;
        cfg->font_hinting_explicit = 1;
    }

    cfg->scrollback = muos_config->terminal.scrollback;
    cfg->readonly = muos_config->terminal.readonly;
    cfg->key_repeat_delay = muos_config->terminal.key_repeat_delay;
    cfg->key_repeat_rate = muos_config->terminal.key_repeat_rate;
    cfg->dpad_repeat_delay = muos_config->terminal.dpad_repeat_delay;
    cfg->dpad_repeat_rate = muos_config->terminal.dpad_repeat_rate;
    cfg->force_redraw = muos_config->terminal.force_redraw;

    if (muos_config->terminal.font_path[0]) {
        snprintf(cfg->term_font_path, sizeof(cfg->term_font_path), "%s", muos_config->terminal.font_path);
        cfg->term_font_path_explicit = 1;
    }
    if (muos_config->terminal.font_path_bold[0])
        snprintf(
            cfg->term_font_path_bold, sizeof(cfg->term_font_path_bold), "%s", muos_config->terminal.font_path_bold
        );
    if (muos_config->terminal.font_path_italic[0])
        snprintf(
            cfg->term_font_path_italic, sizeof(cfg->term_font_path_italic), "%s", muos_config->terminal.font_path_italic
        );
    if (muos_config->terminal.font_path_bold_italic[0])
        snprintf(
            cfg->term_font_path_bold_italic, sizeof(cfg->term_font_path_bold_italic), "%s",
            muos_config->terminal.font_path_bold_italic
        );
    if (parse_hex_colour(muos_config->terminal.foreground, &cfg->solid_fg)) {
        cfg->use_solid_fg = 1;
        cfg->foreground_explicit = 1;
    }
    if (parse_hex_colour(muos_config->terminal.background, &cfg->solid_bg)) {
        cfg->use_solid_bg = 1;
        cfg->background_explicit = 1;
    }
    if (muos_config->terminal.background_image[0]) {
        snprintf(cfg->bg_image, sizeof(cfg->bg_image), "%s", muos_config->terminal.background_image);
        cfg->background_image_explicit = 1;
    }
    if (muos_config->terminal.shell[0]) snprintf(cfg->shell, sizeof(cfg->shell), "%s", muos_config->terminal.shell);
    if (muos_config->terminal.osk_layout[0])
        snprintf(cfg->osk_layout_path, sizeof(cfg->osk_layout_path), "%s", muos_config->terminal.osk_layout);
    if (muos_config->terminal.scrollback_path[0])
        snprintf(cfg->scrollback_path, sizeof(cfg->scrollback_path), "%s", muos_config->terminal.scrollback_path);
}

void config_load(
    MuxtermConfig *cfg, const struct mux_config *muos_config, int ignore_muos, const char *custom_config_path
) {
    memset(cfg, 0, sizeof(*cfg));

    cfg->width = MUXTERM_DEFAULT_WIDTH;
    cfg->height = MUXTERM_DEFAULT_HEIGHT;

    cfg->term_font_size = MUXTERM_DEFAULT_TERM_SIZE;
    cfg->menu_font_size = MUXTERM_DEFAULT_MENU_SIZE;

    cfg->scrollback = MUXTERM_DEFAULT_SCROLLBACK;

    cfg->zoom = 1.0f;
    cfg->ignore_muos = ignore_muos;

    cfg->solid_bg = (SDL_Color) {0, 0, 0, 255};
    cfg->solid_fg = (SDL_Color) {255, 200, 0, 255};
    cfg->use_solid_fg = 1;
    cfg->font_hinting = 2;

    snprintf(cfg->term_font_path, sizeof(cfg->term_font_path), "%s", MUXTERM_DEFAULT_FONT_PATH);
    snprintf(cfg->scrollback_path, sizeof(cfg->scrollback_path), "%s", MUXTERM_DEFAULT_SB_PATH);
    cfg->key_repeat_delay = MUXTERM_DEFAULT_KEY_DELAY;
    cfg->key_repeat_rate = MUXTERM_DEFAULT_KEY_RATE;
    cfg->dpad_repeat_delay = MUXTERM_DEFAULT_DPAD_DELAY;
    cfg->dpad_repeat_rate = MUXTERM_DEFAULT_DPAD_RATE;

    if (!ignore_muos) {
        read_muos_device_config(MUOS_DEVICE_CONFIG, cfg);
        read_muos_global_config(CONF_CONFIG_PATH, cfg);
        apply_muos_terminal_config(cfg, muos_config);
    } else {
        fprintf(stderr, "[CFG] --ignore-muos: skipping muOS device, global, and system configs\n");
    }

    const char *home = getenv("HOME");
    if (ignore_muos && home && *home) {
        char user_conf[PATH_MAX];
        snprintf(user_conf, sizeof(user_conf), "%s/%s", home, MUXTERM_USR_CONF);
        parse_muxterm_conf(user_conf, cfg);
    }

    if (custom_config_path && custom_config_path[0]) {
        snprintf(cfg->custom_config_path, sizeof(cfg->custom_config_path), "%s", custom_config_path);

        FILE *probe = fopen(custom_config_path, "r");
        if (probe) {
            fclose(probe);
        } else {
            ensure_parent_dir(custom_config_path);
            FILE *create = fopen(custom_config_path, "w");
            if (create) {
                fprintf(create, "# muxterm.conf\n");
                fclose(create);
                fprintf(stderr, "[CFG] created custom config: %s\n", custom_config_path);
            } else {
                fprintf(stderr, "[CFG] cannot create custom config: %s\n", custom_config_path);
            }
        }

        parse_muxterm_conf(custom_config_path, cfg);
        fprintf(stderr, "[CFG] custom config applied: %s\n", custom_config_path);
    }
}

void config_dump(const MuxtermConfig *cfg) {
    fprintf(
        stderr, "[CFG] width=%d height=%d font=%s size=%d menu_font_size=%d\n", cfg->width, cfg->height,
        cfg->term_font_path, cfg->term_font_size, cfg->menu_font_size
    );

    fprintf(
        stderr, "[CFG] scroll=%d zoom=%.2f rotate=%d underscan=%d readonly=%d ignore_muos=%d\n", cfg->scrollback,
        cfg->zoom, cfg->rotate, cfg->underscan, cfg->readonly, cfg->ignore_muos
    );

    if (cfg->use_solid_bg)
        fprintf(stderr, "[CFG] solid_bg=#%02X%02X%02X\n", cfg->solid_bg.r, cfg->solid_bg.g, cfg->solid_bg.b);
    if (cfg->use_solid_fg)
        fprintf(stderr, "[CFG] solid_fg=#%02X%02X%02X\n", cfg->solid_fg.r, cfg->solid_fg.g, cfg->solid_fg.b);

    if (cfg->bg_image[0]) fprintf(stderr, "[CFG] bg_image=%s\n", cfg->bg_image);

    if (cfg->shell[0]) fprintf(stderr, "[CFG] shell=%s\n", cfg->shell);

    if (cfg->term_font_path_bold[0]) fprintf(stderr, "[CFG] font_path_bold=%s\n", cfg->term_font_path_bold);
    if (cfg->term_font_path_italic[0]) fprintf(stderr, "[CFG] font_path_italic=%s\n", cfg->term_font_path_italic);
    if (cfg->term_font_path_bold_italic[0])
        fprintf(stderr, "[CFG] font_path_bold_italic=%s\n", cfg->term_font_path_bold_italic);

    if (cfg->osk_layout_path[0]) fprintf(stderr, "[CFG] osk_layout_path=%s\n", cfg->osk_layout_path);
    fprintf(stderr, "[CFG] scrollback_path=%s\n", cfg->scrollback_path);

    fprintf(
        stderr, "[CFG] key_repeat delay=%d rate=%d dpad_repeat delay=%d rate=%d\n", cfg->key_repeat_delay,
        cfg->key_repeat_rate, cfg->dpad_repeat_delay, cfg->dpad_repeat_rate
    );

    if (cfg->force_redraw) fprintf(stderr, "[CFG] force_redraw=1\n");

    {
        static const char *const hint_names[] = {"normal", "light", "mono", "none"};
        int h = cfg->font_hinting;
        if (h < 0 || h > 3) h = 0;
        fprintf(stderr, "[CFG] font_hinting=%s\n", hint_names[h]);
    }
}

static const char *config_save_path(const MuxtermConfig *cfg, char *buf, size_t buf_sz) {
    if (cfg->custom_config_path[0]) {
        snprintf(buf, buf_sz, "%s", cfg->custom_config_path);
        return buf;
    }

    const char *home = getenv("HOME");
    if (home && *home) {
        snprintf(buf, buf_sz, "%s/%s", home, MUXTERM_USR_CONF);
        return buf;
    }

    snprintf(buf, buf_sz, "%s", MUXTERM_USR_CONF);
    return buf;
}

static int managed_key(const char *line) {
    char work[PATH_MAX];
    snprintf(work, sizeof(work), "%s", line);

    char *key = ltrim(work);
    if (!*key || *key == '#') return 0;

    char *eq = strchr(key, '=');
    if (!eq) return 0;
    *eq = '\0';
    rtrim(key);

    return strcmp(key, "term_font_size") == 0 || strcmp(key, "menu_font_size") == 0 || strcmp(key, "font_hinting") == 0
           || strcmp(key, "fg_colour") == 0 || strcmp(key, "bg_colour") == 0;
}

static int rewrite_managed(const MuxtermConfig *cfg, const int append_values) {
    char path[PATH_MAX];
    config_save_path(cfg, path, sizeof(path));
    ensure_parent_dir(path);

    char temporary[PATH_MAX];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path) >= (int) sizeof(temporary)) return 0;

    const int fd = mkstemp(temporary);
    if (fd < 0) return 0;

    FILE *output = fdopen(fd, "w");
    if (!output) {
        close(fd);
        unlink(temporary);
        return 0;
    }

    FILE *input = fopen(path, "r");
    if (input) {
        char line[PATH_MAX];
        while (fgets(line, sizeof(line), input)) {
            if (!managed_key(line) && fputs(line, output) == EOF) {
                fclose(input);
                fclose(output);
                unlink(temporary);
                return 0;
            }
        }
        fclose(input);
    }

    if (append_values) {
        static const char *const hint_names[] = {"normal", "light", "mono", "none"};
        int hint = cfg->font_hinting;
        if (hint < 0 || hint > 3) hint = 0;

        if (fprintf(
                output,
                "term_font_size = %d\nfont_hinting = %s\nfg_colour = %02X%02X%02X\n"
                "bg_colour = %02X%02X%02X\n",
                cfg->term_font_size, hint_names[hint], cfg->solid_fg.r, cfg->solid_fg.g, cfg->solid_fg.b,
                cfg->solid_bg.r, cfg->solid_bg.g, cfg->solid_bg.b
            )
            < 0) {
            fclose(output);
            unlink(temporary);
            return 0;
        }
    }

    int failed = fflush(output) != 0;
    if (!failed) failed = fsync(fd) != 0;
    if (fclose(output) != 0) failed = 1;
    if (!failed && rename(temporary, path) != 0) failed = 1;
    if (failed) {
        unlink(temporary);
        return 0;
    }

    return 1;
}

int config_save_managed(const MuxtermConfig *cfg) {
    if (!cfg->ignore_muos && !cfg->custom_config_path[0]) {
        int hint = cfg->font_hinting;
        if (hint < 0 || hint > 3) hint = 0;
        char foreground[7];
        char background[7];
        snprintf(foreground, sizeof(foreground), "%02X%02X%02X", cfg->solid_fg.r, cfg->solid_fg.g, cfg->solid_fg.b);
        snprintf(background, sizeof(background), "%02X%02X%02X", cfg->solid_bg.r, cfg->solid_bg.g, cfg->solid_bg.b);

        ensure_parent_dir(CONF_CONFIG_PATH "terminal/font_size");
        return write_text_to_file_atomic(CONF_CONFIG_PATH "terminal/font_size", INT, cfg->term_font_size)
               && write_text_to_file_atomic(CONF_CONFIG_PATH "terminal/font_hinting", INT, hint)
               && write_text_to_file_atomic(CONF_CONFIG_PATH "terminal/foreground", CHAR, foreground)
               && write_text_to_file_atomic(CONF_CONFIG_PATH "terminal/background", CHAR, background);
    }
    return rewrite_managed(cfg, 1);
}

int config_reset_managed(MuxtermConfig *cfg, const struct mux_config *muos_config) {
    char custom_path[PATH_MAX];
    snprintf(custom_path, sizeof(custom_path), "%s", cfg->custom_config_path);
    const int ignore_muos = cfg->ignore_muos;

    if (!ignore_muos && !custom_path[0]) {
        unlink(CONF_CONFIG_PATH "terminal/font_size");
        unlink(CONF_CONFIG_PATH "terminal/font_hinting");
        unlink(CONF_CONFIG_PATH "terminal/foreground");
        unlink(CONF_CONFIG_PATH "terminal/background");

        config_load(cfg, muos_config, 0, NULL);
        cfg->term_font_size = MUXTERM_DEFAULT_TERM_SIZE;
        cfg->font_hinting = 2;
        cfg->solid_fg = (SDL_Color) {255, 200, 0, 255};
        cfg->solid_bg = (SDL_Color) {0, 0, 0, 255};
        cfg->term_font_size_explicit = 0;
        cfg->font_hinting_explicit = 0;
        cfg->foreground_explicit = 0;
        cfg->background_explicit = 0;
        return 1;
    }

    if (!rewrite_managed(cfg, 0)) return 0;

    config_load(cfg, muos_config, ignore_muos, custom_path[0] ? custom_path : NULL);
    return 1;
}
