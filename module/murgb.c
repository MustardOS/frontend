#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <../common/colour.h>
#include "../common/config.h"
#include <../common/common.h>
#include <../common/options.h>

#define MUOS_CONFIG_PATH "/opt/muos/config/"

#define LED_SYS "/sys/class/led_anim"
#define JOY_SYS "/sys/devices/platform/singleadc-joypad"
#define SER_DEV "/dev/ttyS5"
#define MCU_DEV "/sys/class/power_supply/axp2202-battery/mcu_pwr"

#define SER_BRI 60
#define MCU_BRI 255
#define JOY_BRI 100

typedef enum {
    BE_AUTO,
    BE_SYSFS,
    BE_SERIAL,
    BE_JOYPAD
} backend_t;

typedef struct {
    int dur_all, dur_l, dur_r, dur_m, dur_f1, dur_f2;
    int cyc_all, cyc_l, cyc_r, cyc_m, cyc_f1, cyc_f2;
} flags_t;

static int g_debug = 0;

static void die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);

    va_end(ap);
    fputc('\n', stderr);

    exit(1);
}

static int clamp(int v, int max) {
    if (v < 0) return 0;
    if (v > max) return max;

    return v;
}

static int parse_int(const char *s, const char *what) {
    if (!s || !*s) die("Error: %s is required", what);

    char *end;

    long v = strtol(s, &end, 10);
    if (*end) die("Error: %s must be numeric (got '%s')", what, s);

    return (int) v;
}

static int dir_exists(const char *path) {
    struct stat st;

    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int char_dev_exists(const char *path) {
    struct stat st;

    return stat(path, &st) == 0 && S_ISCHR(st.st_mode);
}

static int write_string(const char *path, const char *value) {
    FILE *f = fopen(path, "w");

    if (!f) return -1;

    int n = fputs(value, f);
    fclose(f);

    return n < 0 ? -1 : 0;
}

static int sysfs_writable(const char *leaf) {
    char path[512];

    snprintf(path, sizeof path, "%s/%s", LED_SYS, leaf);

    return access(path, W_OK) == 0;
}

static int sysfs_write(const char *leaf, const char *value) {
    char path[512];

    snprintf(path, sizeof path, "%s/%s", LED_SYS, leaf);
    if (access(path, W_OK) != 0) return -1;

    return write_string(path, value);
}

static int sysfs_write_int(const char *leaf, int v) {
    char buf[32];
    snprintf(buf, sizeof buf, "%d\n", v);

    return sysfs_write(leaf, buf);
}

static int joypad_writable(const char *leaf) {
    char path[512];

    snprintf(path, sizeof path, "%s/%s", JOY_SYS, leaf);

    return access(path, W_OK) == 0;
}

static int joypad_write(const char *leaf, const char *value) {
    char path[512];

    snprintf(path, sizeof path, "%s/%s", JOY_SYS, leaf);
    if (access(path, W_OK) != 0) return -1;

    return write_string(path, value);
}

static int joypad_write_int(const char *leaf, int v) {
    char buf[32];

    snprintf(buf, sizeof buf, "%d\n", v);

    return joypad_write(leaf, buf);
}

static int joypad_level_from_byte(int brightness) {
    return clamp((clamp(brightness, MCU_BRI) * JOY_BRI + (MCU_BRI / 2)) / MCU_BRI, JOY_BRI);
}

static int joypad_mode_from_protocol(int mode) {
    switch (mode) {
        case 1:
            return 1;
        case 2:
        case 3:
        case 4:
        case 8:
            return 2;
        case 7:
            return 3;
        case 5:
            return 4;
        case 6:
            return 5;
        default:
            return 1;
    }
}

static int joypad_speed_pct_to_reg(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

static int joypad_breathing_speed_invert(int ui_pct) {
    if (ui_pct < 0) ui_pct = 0;
    if (ui_pct > 100) ui_pct = 100;
    return 100 - ui_pct;
}

static int joypad_breathing_speed_pct_for_protocol(int mode) {
    switch (mode) {
        case 2:
            return 80;
        case 3:
            return 50;
        case 4:
            return 20;
        default:
            return 50;
    }
}

static int joypad_commit(void) {
    if (joypad_writable("led_set")) joypad_write_int("led_set", 1);

    return 0;
}

static backend_t detect_backend(backend_t requested) {
    if (requested != BE_AUTO) return requested;

    if (dir_exists(JOY_SYS)) return BE_JOYPAD;
    if (dir_exists(LED_SYS)) return BE_SYSFS;
    if (char_dev_exists(SER_DEV)) return BE_SERIAL;

    die("Error: no supported LED backend found (missing %s, %s and %s)", JOY_SYS, LED_SYS, SER_DEV);

    return BE_AUTO;
}

static int serial_fd = -1;

static void serial_prepare(void) {
    if (access(MCU_DEV, W_OK) == 0) write_string(MCU_DEV, "1\n");

    serial_fd = open(SER_DEV, O_WRONLY | O_NOCTTY);
    if (serial_fd < 0) die("Error: cannot open %s: %s", SER_DEV, strerror(errno));

    struct termios tio;
    if (tcgetattr(serial_fd, &tio) == 0) {
        cfmakeraw(&tio);

        cfsetispeed(&tio, B115200);
        cfsetospeed(&tio, B115200);

        tio.c_cflag |= (CLOCAL | CREAD | CS8);
        tio.c_cflag &= ~(PARENB | CSTOPB);
        tio.c_lflag &= ~(ICANON | ECHO | ISIG);
        tio.c_oflag &= ~OPOST;

        tcsetattr(serial_fd, TCSANOW, &tio);
    }

    sleep(1);
}

static uint8_t checksum_u8(const uint8_t *bytes, size_t n) {
    unsigned sum = 0;
    for (size_t i = 0; i < n; i++) sum += bytes[i];
    return (uint8_t) (sum & 0xff);
}

static void serial_write_bytes(const uint8_t *bytes, size_t n) {
    if (g_debug) {
        fputs("TX:", stderr);
        for (size_t i = 0; i < n; i++) fprintf(stderr, " %02X", bytes[i]);
        fputc('\n', stderr);
    }

    if (serial_fd < 0) return;

    ssize_t w = write(serial_fd, bytes, n);
    (void) w;
}

static void serial_send_static(int brightness, int rr, int rg, int rb, int lr, int lg, int lb) {
    uint8_t buf[51];
    size_t i = 0;

    buf[i++] = 1;
    buf[i++] = (uint8_t) clamp(brightness, MCU_BRI);

    for (int k = 0; k < 8; k++) {
        buf[i++] = (uint8_t) clamp(rr, MCU_BRI);
        buf[i++] = (uint8_t) clamp(rg, MCU_BRI);
        buf[i++] = (uint8_t) clamp(rb, MCU_BRI);
    }

    for (int k = 0; k < 8; k++) {
        buf[i++] = (uint8_t) clamp(lr, MCU_BRI);
        buf[i++] = (uint8_t) clamp(lg, MCU_BRI);
        buf[i++] = (uint8_t) clamp(lb, MCU_BRI);
    }

    buf[i] = checksum_u8(buf, i);
    serial_write_bytes(buf, i + 1);
}

static void serial_send_breath(int mode, int brightness, int r, int g, int b) {
    uint8_t buf[51];
    size_t i = 0;

    buf[i++] = (uint8_t) mode;
    buf[i++] = (uint8_t) clamp(brightness, MCU_BRI);

    for (int k = 0; k < 16; k++) {
        buf[i++] = (uint8_t) clamp(r, MCU_BRI);
        buf[i++] = (uint8_t) clamp(g, MCU_BRI);
        buf[i++] = (uint8_t) clamp(b, MCU_BRI);
    }

    buf[i] = checksum_u8(buf, i);
    serial_write_bytes(buf, i + 1);
}

static void serial_send_rainbow(int mode, int brightness, int speed) {
    uint8_t buf[6];

    buf[0] = (uint8_t) mode;
    buf[1] = (uint8_t) clamp(brightness, MCU_BRI);
    buf[2] = 1;
    buf[3] = 1;
    buf[4] = (uint8_t) clamp(speed, MCU_BRI);
    buf[5] = checksum_u8(buf, 5);

    serial_write_bytes(buf, 6);
}

static int effect_map_sysfs(int mode) {
    switch (mode) {
        case 1:
            return 4;
        case 2:
        case 3:
        case 4:
            return 2;
        case 5:
            return 5;
        case 6:
            return 6;
        case 7:
            return 7;
        case 8:
            return 1;
        case 9:
            return 3;
        default:
            return -1;
    }
}

static int duration_map_sysfs(int mode, int fallback) {
    switch (mode) {
        case 2:
            return 3000;
        case 3:
            return 5000;
        case 4:
            return 10000;
        default:
            return fallback;
    }
}

static int env_int(const char *name, int fallback) {
    const char *v = getenv(name);

    if (!v || !*v) return fallback;

    char *end;
    long n = strtol(v, &end, 10);

    return *end ? fallback : (int) n;
}

static void hex3(char *out, int r, int g, int b) {
    snprintf(out, 16, "%02X%02X%02X ", clamp(r, MCU_BRI), clamp(g, MCU_BRI), clamp(b, MCU_BRI));
}

static int apply_sysfs(int mode, int brightness_raw, int argc, char **argv, const flags_t *fl);

static int apply_serial(int mode, int brightness, int argc, char **argv);

static int apply_joypad(int mode, int brightness, int argc, char **argv);

static int read_config_int(const char *leaf, int fallback) {
    char path[512];
    snprintf(path, sizeof path, "%s%s", MUOS_CONFIG_PATH, leaf);

    FILE *f = fopen(path, "r");
    if (!f) return fallback;

    char buf[32];
    char *got = fgets(buf, sizeof buf, f);
    fclose(f);

    if (!got) return fallback;

    char *end;
    long v = strtol(buf, &end, 10);
    if (end == buf) return fallback;

    return (int) v;
}

static int dispatch_off(backend_t requested) {
    backend_t use = detect_backend(requested);
    flags_t fl;

    fl.dur_all = fl.dur_l = fl.dur_r = fl.dur_m = fl.dur_f1 = fl.dur_f2 = -1;
    fl.cyc_all = fl.cyc_l = fl.cyc_r = fl.cyc_m = fl.cyc_f1 = fl.cyc_f2 = INT32_MIN;

    if (use == BE_JOYPAD) {
        if (!dir_exists(JOY_SYS)) die("Error: JOYPAD backend selected but %s not present.", JOY_SYS);

        joypad_write_int("led_switch", 0);
        joypad_commit();

        return 0;
    }

    if (use == BE_SYSFS) {
        if (!dir_exists(LED_SYS)) die("Error: SYSFS backend selected but %s not present.", LED_SYS);

        char *zero_argv[] = {
                "0", "0", "0",
                "0", "0", "0",
                "0", "0", "0",
                "0", "0", "0",
                "0", "0", "0",
        };

        return apply_sysfs(1, 0, 15, zero_argv, &fl);
    }

    if (!char_dev_exists(SER_DEV)) die("Error: SERIAL backend selected but %s not present.", SER_DEV);

    char *zero_argv[] = {"0", "0", "0", "0", "0", "0"};
    int rc = apply_serial(1, 0, 6, zero_argv);

    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }

    return rc;
}

static backend_t restore_backend_from_config(int saved) {
    switch (saved) {
        case 1:
            return BE_SYSFS;
        case 2:
            return BE_SERIAL;
        case 3:
            return BE_JOYPAD;
        default:
            return BE_AUTO;
    }
}

static int restore_breath_wire_mode(int saved) {
    switch (saved) {
        case 0:
            return 2;
        case 2:
            return 4;
        default:
            return 3;
    }
}

static int joypad_speed_pct_from_saved(int bs) {
    if (bs > 2) {
        if (bs > 100) bs = 100;
        return bs;
    }
    switch (bs) {
        case 0:
            return 80;
        case 2:
            return 20;
        default:
            return 50;
    }
}

static int restore_brightness_to_byte(int b) {
    return clamp(b, MCU_BRI);
}

static int get_rgb_path(char *rgb_path, size_t rgb_path_size) {
    theme_base = get_theme_base();
    char active_path[MAX_BUFFER_SIZE];
    snprintf(active_path, sizeof(active_path), "%s/active.txt", theme_base);
    if (file_exist(active_path)) {
        snprintf(rgb_path, rgb_path_size, "%s/alternate/rgb/%s/", theme_base,
                 str_replace(read_line_char_from(active_path, 1), "\r", ""));
        return dir_exist(rgb_path);
    }
    snprintf(rgb_path, rgb_path_size, "%s/rgb/", theme_base);
    return dir_exist(rgb_path);
}

static int dispatch_restore(void) {
    int saved_mode = read_config_int("settings/rgb/mode", 0);
    int saved_backend = read_config_int("settings/rgb/backend", 0);
    int saved_bright_raw = 6;
    rgb_colour_t col_l, col_r, col_m, col_f1, col_f2;

    if (saved_mode == 4) {
        char base_path[MAX_BUFFER_SIZE];
        get_rgb_path(base_path, sizeof(base_path));
        char settings_path[MAX_BUFFER_SIZE];

        snprintf(settings_path, sizeof(settings_path), "%smode", base_path);
        saved_mode = read_line_int_from(settings_path, 1);

        snprintf(settings_path, sizeof(settings_path), "%sbackend", base_path);
        saved_backend = read_line_int_from(settings_path, 1);

        snprintf(settings_path, sizeof(settings_path), "%sbright", base_path);
        saved_bright_raw = read_line_int_from(settings_path, 1);

        snprintf(settings_path, sizeof(settings_path), "%scolour_l", base_path);
        read_rgb_colour_from_file(settings_path, &col_l, &RGB_COLOURS[0]);
        
        snprintf(settings_path, sizeof(settings_path), "%scolour_r", base_path);
        read_rgb_colour_from_file(settings_path, &col_r, &col_l);

        snprintf(settings_path, sizeof(settings_path), "%scolour_m", base_path);
        read_rgb_colour_from_file(settings_path, &col_m, &col_l);

        snprintf(settings_path, sizeof(settings_path), "%scolour_f1", base_path);
        read_rgb_colour_from_file(settings_path, &col_f1, &col_l);

        snprintf(settings_path, sizeof(settings_path), "%scolour_f2", base_path);
        read_rgb_colour_from_file(settings_path, &col_f2, &col_r);
    } else {
        saved_bright_raw = read_config_int("settings/rgb/bright", 6);
        read_rgb_colour_from_file(MUOS_CONFIG_PATH "settings/rgb/colour_l", &col_l, &RGB_COLOURS[0]);
        read_rgb_colour_from_file(MUOS_CONFIG_PATH "settings/rgb/colour_r", &col_r, &col_l);
        read_rgb_colour_from_file(MUOS_CONFIG_PATH "settings/rgb/colour_m", &col_m, &col_l);
        read_rgb_colour_from_file(MUOS_CONFIG_PATH "settings/rgb/colour_f1", &col_f1, &col_l);
        read_rgb_colour_from_file(MUOS_CONFIG_PATH "settings/rgb/colour_f2", &col_f2, &col_r);
    }

    if (saved_mode == 6) return 0;
    if (saved_mode == 5) saved_mode = 0;

    backend_t use = detect_backend(restore_backend_from_config(saved_backend));

    if (saved_mode == 0) return dispatch_off(restore_backend_from_config(saved_backend));

    flags_t fl;
    fl.dur_all = fl.dur_l = fl.dur_r = fl.dur_m = fl.dur_f1 = fl.dur_f2 = -1;
    fl.cyc_all = fl.cyc_l = fl.cyc_r = fl.cyc_m = fl.cyc_f1 = fl.cyc_f2 = INT32_MIN;

    int bright_byte = restore_brightness_to_byte(saved_bright_raw);

    int combo_idx = read_config_int("settings/rgb/combo", 0);
    const rgb_colour_combo_t *kc = rgb_colour_combo_at(combo_idx);

    char num_buf[15][8];
    char *argv_buf[15];

    int wire_mode;
    int n = 0;

#define PUSH(v) do { snprintf(num_buf[n], sizeof num_buf[n], "%d", (v)); argv_buf[n] = num_buf[n]; n++; } while (0)

    if (saved_mode == 3) {
        wire_mode = 1;
        PUSH(kc->a_r);
        PUSH(kc->a_g);
        PUSH(kc->a_b);
        PUSH(kc->b_r);
        PUSH(kc->b_g);
        PUSH(kc->b_b);
    } else if (saved_mode == 2) {
        int bs = read_config_int("settings/rgb/breath_speed", 1);

        if (use == BE_JOYPAD) {
            int sec_r, sec_g, sec_b;
            //
            //TODO: by Xongle
            //
            // if (idx_r <= 0) {
            //     sec_r = sec_g = sec_b = 0;
            // } else {
                sec_r = col_r.r;
                sec_g = col_r.g;
                sec_b = col_r.b;
            // }

            wire_mode = 8;
            PUSH(joypad_speed_pct_from_saved(bs));
            PUSH(col_l.r);
            PUSH(col_l.g);
            PUSH(col_l.b);
            PUSH(sec_r);
            PUSH(sec_g);
            PUSH(sec_b);
        } else {
            wire_mode = restore_breath_wire_mode(bs);

            if (use == BE_SYSFS) {
                PUSH(col_l.r);
                PUSH(col_l.g);
                PUSH(col_l.b);
                PUSH(col_r.r);
                PUSH(col_r.g);
                PUSH(col_r.b);
                PUSH(col_m.r);
                PUSH(col_m.g);
                PUSH(col_m.b);
                PUSH(col_f1.r);
                PUSH(col_f1.g);
                PUSH(col_f1.b);
                PUSH(col_f2.r);
                PUSH(col_f2.g);
                PUSH(col_f2.b);
            } else {
                PUSH(col_l.r);
                PUSH(col_l.g);
                PUSH(col_l.b);
            }
        }
    } else if (saved_mode == 7) {
        int bs = read_config_int("settings/rgb/breath_speed", 50);
        wire_mode = 7;
        PUSH(joypad_speed_pct_from_saved(bs));
    } else if (saved_mode == 8) {
        int bs = read_config_int("settings/rgb/breath_speed", 50);
        wire_mode = 5;
        PUSH(joypad_speed_pct_from_saved(bs));
    } else if (saved_mode == 9) {
        if (col_l.r || col_l.g || col_l.b) {
            wire_mode = 6;
            PUSH(col_l.r);
            PUSH(col_l.g);
            PUSH(col_l.b);
        } else {
            return dispatch_off(restore_backend_from_config(saved_backend));
        }
    } else {
        wire_mode = 1;

        if (use == BE_SYSFS) {
            PUSH(col_l.r);
            PUSH(col_l.g);
            PUSH(col_l.b);
            PUSH(col_r.r);
            PUSH(col_r.g);
            PUSH(col_r.b);
            PUSH(col_m.r);
            PUSH(col_m.g);
            PUSH(col_m.b);
            PUSH(col_f1.r);
            PUSH(col_f1.g);
            PUSH(col_f1.b);
            PUSH(col_f2.r);
            PUSH(col_f2.g);
            PUSH(col_f2.b);
        } else {
            PUSH(col_r.r);
            PUSH(col_r.g);
            PUSH(col_r.b);
            PUSH(col_l.r);
            PUSH(col_l.g);
            PUSH(col_l.b);
        }
    }

#undef PUSH

    if (use == BE_JOYPAD) {
        if (!dir_exists(JOY_SYS)) die("Error: JOYPAD backend selected but %s not present.", JOY_SYS);
        return apply_joypad(wire_mode, bright_byte, n, argv_buf);
    }

    if (use == BE_SYSFS) {
        if (!dir_exists(LED_SYS)) die("Error: SYSFS backend selected but %s not present.", LED_SYS);
        return apply_sysfs(wire_mode, bright_byte, n, argv_buf, &fl);
    }

    if (!char_dev_exists(SER_DEV)) die("Error: SERIAL backend selected but %s not present.", SER_DEV);
    int rc = apply_serial(wire_mode, bright_byte, n, argv_buf);

    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }

    return rc;
}

static int apply_sysfs(int mode, int brightness_raw, int argc, char **argv, const flags_t *fl) {
    if (effect_map_sysfs(mode) < 0) die("Invalid mode for SYSFS: %d (1-9)", mode);
    int bri = clamp((brightness_raw * SER_BRI) / MCU_BRI, SER_BRI);

    if (argc < 3) {
        fprintf(stderr, "SYSFS usage: murgb -b sysfs <mode 1-9> <brightness 0-255> "
                        "<L_r L_g L_b> [<R_r R_g R_b>] [M_r M_g M_b] [F1_r F1_g F1_b] [F2_r F2_g F2_b]\n");
        return 1;
    }

    int lr = parse_int(argv[0], "L_r");
    int lg = parse_int(argv[1], "L_g");
    int lb = parse_int(argv[2], "L_b");

    int rr = (argc >= 6) ? parse_int(argv[3], "R_r") : lr;
    int rg = (argc >= 6) ? parse_int(argv[4], "R_g") : lg;
    int rb = (argc >= 6) ? parse_int(argv[5], "R_b") : lb;

    int mr = (argc >= 9) ? parse_int(argv[6], "M_r") : lr;
    int mg = (argc >= 9) ? parse_int(argv[7], "M_g") : lg;
    int mb = (argc >= 9) ? parse_int(argv[8], "M_b") : lb;

    int f1r = (argc >= 12) ? parse_int(argv[9], "F1_r") : lr;
    int f1g = (argc >= 12) ? parse_int(argv[10], "F1_g") : lg;
    int f1b = (argc >= 12) ? parse_int(argv[11], "F1_b") : lb;

    int f2r = (argc >= 15) ? parse_int(argv[12], "F2_r") : rr;
    int f2g = (argc >= 15) ? parse_int(argv[13], "F2_g") : rg;
    int f2b = (argc >= 15) ? parse_int(argv[14], "F2_b") : rb;

    char hex_l[16];
    char hex_r[16];
    char hex_m[16];
    char hex_f1[16];
    char hex_f2[16];

    hex3(hex_l, lr, lg, lb);
    hex3(hex_r, rr, rg, rb);
    hex3(hex_m, mr, mg, mb);
    hex3(hex_f1, f1r, f1g, f1b);
    hex3(hex_f2, f2r, f2g, f2b);

    sysfs_write_int("max_scale", bri);

    int same_lr = (strcmp(hex_l, hex_r) == 0);
    if (same_lr && sysfs_writable("effect_rgb_hex_lr")) {
        sysfs_write("effect_rgb_hex_lr", hex_l);
    } else {
        sysfs_write("effect_rgb_hex_l", hex_l);
        sysfs_write("effect_rgb_hex_r", hex_r);
    }

    sysfs_write("effect_rgb_hex_m", hex_m);
    sysfs_write("effect_rgb_hex_f1", hex_f1);
    sysfs_write("effect_rgb_hex_f2", hex_f2);

    int dur_all = (fl->dur_all >= 0) ? fl->dur_all : env_int("LED_DUR", 1000);
    dur_all = duration_map_sysfs(mode, dur_all);

    int dur_l = (fl->dur_l >= 0) ? fl->dur_l : env_int("LED_DUR_L", dur_all);
    int dur_r = (fl->dur_r >= 0) ? fl->dur_r : env_int("LED_DUR_R", dur_all);
    int dur_m = (fl->dur_m >= 0) ? fl->dur_m : env_int("LED_DUR_M", dur_all);
    int dur_f1 = (fl->dur_f1 >= 0) ? fl->dur_f1 : env_int("LED_DUR_F1", dur_all);
    int dur_f2 = (fl->dur_f2 >= 0) ? fl->dur_f2 : env_int("LED_DUR_F2", dur_all);

    int cyc_all = (fl->cyc_all != INT32_MIN) ? fl->cyc_all : env_int("LED_CYCLES", -1);
    int cyc_l = (fl->cyc_l != INT32_MIN) ? fl->cyc_l : env_int("LED_CYCLES_L", cyc_all);
    int cyc_r = (fl->cyc_r != INT32_MIN) ? fl->cyc_r : env_int("LED_CYCLES_R", cyc_all);
    int cyc_m = (fl->cyc_m != INT32_MIN) ? fl->cyc_m : env_int("LED_CYCLES_M", cyc_all);
    int cyc_f1 = (fl->cyc_f1 != INT32_MIN) ? fl->cyc_f1 : env_int("LED_CYCLES_F1", cyc_all);
    int cyc_f2 = (fl->cyc_f2 != INT32_MIN) ? fl->cyc_f2 : env_int("LED_CYCLES_F2", cyc_all);

    if (sysfs_writable("effect_duration_lr") && dur_l == dur_r) {
        sysfs_write_int("effect_duration_lr", dur_l);
    } else {
        sysfs_write_int("effect_duration_l", dur_l);
        sysfs_write_int("effect_duration_r", dur_r);
    }

    sysfs_write_int("effect_duration_m", dur_m);
    sysfs_write_int("effect_duration_f1", dur_f1);
    sysfs_write_int("effect_duration_f2", dur_f2);

    if (sysfs_writable("effect_cycles_lr") && cyc_l == cyc_r) {
        sysfs_write_int("effect_cycles_lr", cyc_l);
    } else {
        sysfs_write_int("effect_cycles_l", cyc_l);
        sysfs_write_int("effect_cycles_r", cyc_r);
    }

    sysfs_write_int("effect_cycles_m", cyc_m);
    sysfs_write_int("effect_cycles_f1", cyc_f1);
    sysfs_write_int("effect_cycles_f2", cyc_f2);

    int effect = effect_map_sysfs(mode);
    if (sysfs_writable("effect_lr")) {
        sysfs_write_int("effect_lr", effect);
    } else {
        sysfs_write_int("effect_l", effect);
        sysfs_write_int("effect_r", effect);
    }

    sysfs_write_int("effect_m", effect);
    sysfs_write_int("effect_f1", effect);
    sysfs_write_int("effect_f2", effect);

    sysfs_write("effect_enable", "1\n");

    printf("LED mode %d applied (SYSFS) brightness=%d\n", mode, bri);

    return 0;
}

static int apply_joypad(int mode, int brightness, int argc, char **argv) {
    if (mode < 1 || mode > 8) die("Invalid mode for JOYPAD: %d (1-8)", mode);

    int level = joypad_level_from_byte(brightness);

    if (level <= 0) {
        joypad_write_int("led_switch", 0);
        joypad_commit();

        printf("LED off (JOYPAD)\n");

        return 0;
    }

    if (mode == 6) {
        joypad_write_int("led_switch", 1);
        joypad_write_int("led_level", level);

        if (argc >= 3) {
            int sr = clamp(parse_int(argv[0], "R"), MCU_BRI);
            int sg = clamp(parse_int(argv[1], "G"), MCU_BRI);
            int sb = clamp(parse_int(argv[2], "B"), MCU_BRI);

            joypad_write_int("Led_rgb_r1", sr);
            joypad_write_int("Led_rgb_g1", sg);
            joypad_write_int("Led_rgb_b1", sb);

            joypad_write_int("Led_rgb_r2", 0);
            joypad_write_int("Led_rgb_g2", 0);
            joypad_write_int("Led_rgb_b2", 0);
        }

        joypad_write_int("led_mode", joypad_mode_from_protocol(mode));
        joypad_commit();

        printf("LED mode %d set with brightness %d (JOYPAD, stick-follow)\n", mode, level);
        return 0;
    }

    if (mode == 5) {
        joypad_write_int("led_switch", 1);
        joypad_write_int("led_level", level);

        if (argc >= 1) {
            int speed_pct = joypad_speed_pct_to_reg(parse_int(argv[0], "speed"));
            joypad_write_int("led_speed", speed_pct);
        }

        joypad_write_int("led_mode", joypad_mode_from_protocol(mode));
        joypad_commit();

        printf("LED mode %d set with brightness %d (JOYPAD, rainbow)\n", mode, level);
        return 0;
    }

    if (mode == 7 || mode == 8) {
        int mode8_argc_ok = (mode == 8 && (argc == 4 || argc == 7));
        if (!(mode == 7 && argc == 1) && !mode8_argc_ok) {
            fprintf(stderr, "JOYPAD usage:\n"
                            "  murgb -b joypad 7 <brightness> <speed 0-100>\n"
                            "  murgb -b joypad 8 <brightness> <speed 0-100> <R1> <G1> <B1> [<R2> <G2> <B2>]\n");
            return 1;
        }

        int ui_speed_pct = joypad_speed_pct_to_reg(parse_int(argv[0], "speed"));
        int reg_speed_pct = (mode == 8) ? joypad_breathing_speed_invert(ui_speed_pct) : ui_speed_pct;

        joypad_write_int("led_switch", 1);
        joypad_write_int("led_level", level);
        joypad_write_int("led_sync_colour", 1);
        joypad_write_int("led_speed", reg_speed_pct);

        if (mode == 8) {
            int r1 = clamp(parse_int(argv[1], "R1"), MCU_BRI);
            int g1 = clamp(parse_int(argv[2], "G1"), MCU_BRI);
            int b1 = clamp(parse_int(argv[3], "B1"), MCU_BRI);

            int r2, g2, b2;
            if (argc == 7) {
                r2 = clamp(parse_int(argv[4], "R2"), MCU_BRI);
                g2 = clamp(parse_int(argv[5], "G2"), MCU_BRI);
                b2 = clamp(parse_int(argv[6], "B2"), MCU_BRI);
            } else {
                r2 = r1;
                g2 = g1;
                b2 = b1;
            }

            joypad_write_int("Led_rgb_r1", r1);
            joypad_write_int("Led_rgb_g1", g1);
            joypad_write_int("Led_rgb_b1", b1);

            joypad_write_int("Led_rgb_r2", r2);
            joypad_write_int("Led_rgb_g2", g2);
            joypad_write_int("Led_rgb_b2", b2);
        }

        joypad_write_int("led_mode", joypad_mode_from_protocol(mode));
        joypad_commit();

        printf("LED mode %d set with brightness %d speed %d%% (JOYPAD)\n", mode, level, ui_speed_pct);
        return 0;
    }

    if (argc < 3) {
        fprintf(stderr, "JOYPAD usage: murgb -b joypad <mode 1-4> <brightness> <R G B>\n");
        return 1;
    }

    int lr = clamp(parse_int(argv[0], "R"), MCU_BRI);
    int lg = clamp(parse_int(argv[1], "G"), MCU_BRI);
    int lb = clamp(parse_int(argv[2], "B"), MCU_BRI);

    int enabled = lr || lg || lb;

    if (!enabled) {
        joypad_write_int("led_switch", 0);
        joypad_commit();

        printf("LED off (JOYPAD)\n");

        return 0;
    }

    joypad_write_int("led_switch", 1);
    joypad_write_int("led_level", level);
    joypad_write_int("led_sync_colour", 1);

    if (mode >= 2 && mode <= 4) {
        int ui_pct = joypad_breathing_speed_pct_for_protocol(mode);
        joypad_write_int("led_speed", joypad_breathing_speed_invert(ui_pct));
    }

    if (mode == 1) {
        joypad_write_int("custum_rgb_r", lr);
        joypad_write_int("custum_rgb_g", lg);
        joypad_write_int("custum_rgb_b", lb);
    } else {
        joypad_write_int("Led_rgb_r1", lr);
        joypad_write_int("Led_rgb_g1", lg);
        joypad_write_int("Led_rgb_b1", lb);

        joypad_write_int("Led_rgb_r2", 0);
        joypad_write_int("Led_rgb_g2", 0);
        joypad_write_int("Led_rgb_b2", 0);
    }

    joypad_write_int("led_mode", joypad_mode_from_protocol(mode));
    joypad_commit();

    printf("LED mode %d set with brightness %d (JOYPAD)\n", mode, level);

    return 0;
}

static int apply_serial(int mode, int brightness, int argc, char **argv) {
    if (mode < 1 || mode > 6) die("Invalid mode for SERIAL: %d (1-6)", mode);

    int bri = clamp(brightness, MCU_BRI);
    serial_prepare();

    if (mode >= 5 && mode <= 6) {
        if (argc != 1) {
            fprintf(stderr, "SERIAL usage (5-6): murgb -b serial <5|6> <brightness> <speed 0-255>\n");
            return 1;
        }

        int speed = clamp(parse_int(argv[0], "speed"), MCU_BRI);
        serial_send_rainbow(mode, bri, speed);

        printf("LED mode %d set with brightness %d (SERIAL)\n", mode, bri);

        return 0;
    }

    if (mode == 1) {
        if (argc != 6) {
            fprintf(stderr, "SERIAL usage (1): murgb -b serial 1 <brightness> "
                            "<right_r> <right_g> <right_b> <left_r> <left_g> <left_b>\n");
            return 1;
        }

        int rr = parse_int(argv[0], "right_r");
        int rg = parse_int(argv[1], "right_g");
        int rb = parse_int(argv[2], "right_b");

        int lr = parse_int(argv[3], "left_r");
        int lg = parse_int(argv[4], "left_g");
        int lb = parse_int(argv[5], "left_b");

        serial_send_static(bri, rr, rg, rb, lr, lg, lb);
        printf("LED mode %d set with brightness %d (SERIAL)\n", mode, bri);

        return 0;
    }

    if (argc != 3) {
        fprintf(stderr, "SERIAL usage (2-4): murgb -b serial <2|3|4> <brightness> <r> <g> <b>\n");
        return 1;
    }

    int r = parse_int(argv[0], "r");
    int g = parse_int(argv[1], "g");
    int b = parse_int(argv[2], "b");

    serial_send_breath(mode, bri, r, g, b);
    printf("LED mode %d set with brightness %d (SERIAL)\n", mode, bri);

    return 0;
}

static void usage(void) {
    fputs(
            "Usage:\n"
            "  murgb [-b auto|sysfs|serial|joypad] [--dur MS] [--dur-l MS] [--dur-r MS]\n"
            "        [--dur-m MS] [--dur-f1 MS] [--dur-f2 MS]\n"
            "        [--cycles N] [--cycles-l N] [--cycles-r N] [--cycles-m N]\n"
            "        [--cycles-f1 N] [--cycles-f2 N]\n"
            "        <mode> <brightness> [args...]\n"
            "\n"
            "  murgb off       Blank the lights.  Persistent settings are kept\n"
            "                  intact for a later `restore`.  Takes no other args.\n"
            "  murgb restore   Re-apply whatever the muxrgb frontend last saved\n"
            "                  (read from " MUOS_CONFIG_PATH "settings/rgb/).\n"
            "                  Useful for boot, suspend resume, etc.  Takes no\n"
            "                  other args.\n"
            "\nBackends:\n"
            "  JOYPAD : " JOY_SYS "\n"
            "  SYSFS  : " LED_SYS "\n"
            "  SERIAL : " SER_DEV "\n"
            "  AUTO   : prefer JOYPAD if present, else SYSFS, else SERIAL\n",
            stderr);
}

static backend_t parse_backend(const char *s) {
    if (!s) die("Error: -b requires a value");

    if (!strcasecmp(s, "auto")) return BE_AUTO;
    if (!strcasecmp(s, "sysfs")) return BE_SYSFS;
    if (!strcasecmp(s, "serial")) return BE_SERIAL;
    if (!strcasecmp(s, "joypad")) return BE_JOYPAD;

    die("Invalid backend: %s", s);
    return BE_AUTO;
}

int main(int argc, char **argv) {
    load_config(&config);

    const char *dbg = getenv("RGB_DEBUG");
    g_debug = (dbg && *dbg && strcmp(dbg, "0") != 0);

    if (argc >= 2 && (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "restore") == 0)) {
        if (argc != 2) {
            fprintf(stderr, "Error: '%s' takes no other arguments\n", argv[1]);
            return 1;
        }
        if (strcmp(argv[1], "off") == 0) return dispatch_off(BE_AUTO);
        return dispatch_restore();
    }

    flags_t fl;
    fl.dur_all = fl.dur_l = fl.dur_r = fl.dur_m = fl.dur_f1 = fl.dur_f2 = -1;
    fl.cyc_all = fl.cyc_l = fl.cyc_r = fl.cyc_m = fl.cyc_f1 = fl.cyc_f2 = INT32_MIN;

    backend_t be = BE_AUTO;
    int i = 1;

    while (i < argc) {
        const char *a = argv[i];
        if (a[0] != '-' || a[1] == '\0') break;
        if (strcmp(a, "--") == 0) {
            i++;
            break;
        }
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            usage();
            return 0;
        }
        if (strcmp(a, "-b") == 0) {
            if (i + 1 >= argc) die("Error: -b requires a value");
            be = parse_backend(argv[i + 1]);
            i += 2;
            continue;
        }

#define OPT_INT(flag, slot)                                             \
        if (strcmp(a, flag) == 0) {                                     \
            if (i + 1 >= argc) die("Error: %s requires a value", flag); \
            slot = parse_int(argv[i + 1], flag);                        \
            i += 2;                                                     \
            continue;                                                   \
        }
        OPT_INT("--dur", fl.dur_all)
        OPT_INT("--dur-l", fl.dur_l)
        OPT_INT("--dur-r", fl.dur_r)
        OPT_INT("--dur-m", fl.dur_m)
        OPT_INT("--dur-f1", fl.dur_f1)
        OPT_INT("--dur-f2", fl.dur_f2)
        OPT_INT("--cycles", fl.cyc_all)
        OPT_INT("--cycles-l", fl.cyc_l)
        OPT_INT("--cycles-r", fl.cyc_r)
        OPT_INT("--cycles-m", fl.cyc_m)
        OPT_INT("--cycles-f1", fl.cyc_f1)
        OPT_INT("--cycles-f2", fl.cyc_f2)
#undef OPT_INT

        fprintf(stderr, "Unknown option: %s\n", a);
        usage();
        return 1;
    }

    if (argc - i < 2) {
        usage();
        return 1;
    }

    int mode = parse_int(argv[i], "mode");
    int brightness = parse_int(argv[i + 1], "brightness");
    int rest_argc = argc - i - 2;
    char **rest_argv = argv + i + 2;

    backend_t use = detect_backend(be);

    if (use == BE_JOYPAD) {
        if (!dir_exists(JOY_SYS)) die("Error: JOYPAD backend selected but %s not present.", JOY_SYS);
        return apply_joypad(mode, brightness, rest_argc, rest_argv);
    }

    if (use == BE_SYSFS) {
        if (!dir_exists(LED_SYS)) die("Error: SYSFS backend selected but %s not present.", LED_SYS);
        return apply_sysfs(mode, brightness, rest_argc, rest_argv, &fl);
    }

    if (!char_dev_exists(SER_DEV)) die("Error: SERIAL backend selected but %s not present.", SER_DEV);

    int rc = apply_serial(mode, brightness, rest_argc, rest_argv);

    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }

    return rc;
}
