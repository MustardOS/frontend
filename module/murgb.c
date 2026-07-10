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
#include "../common/colour.h"
#include "../common/config.h"
#include "../common/fileio.h"
#include "../common/strutil.h"
#include "../common/theme.h"

#define MUOS_CONFIG_PATH "/opt/muos/config/"

#define LED_SYS "/sys/class/led_anim"
#define JOY_SYS "/sys/devices/platform/singleadc-joypad"
#define SER_DEV "/dev/ttyS5"
#define MCU_DEV "/sys/class/power_supply/axp2202-battery/mcu_pwr"

#define SER_BRI 60
#define MCU_BRI 255
#define JOY_BRI 100

typedef enum { be_auto, be_sysfs, be_serial, be_joypad } backend_t;

typedef struct {
    int dur_all, dur_l, dur_r, dur_m, dur_f1, dur_f2;
    int cyc_all, cyc_l, cyc_r, cyc_m, cyc_f1, cyc_f2;
} flags_t;

static void die(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);

    va_end(ap);
    fputc('\n', stderr);

    exit(1);
}

static int clamp(const int v, const int max) {
    if (v < 0) return 0;
    if (v > max) return max;

    return v;
}

static int parse_int(const char *s, const char *what) {
    if (!s || !*s) die("Error: %s is required", what);

    char *end;

    const long v = strtol(s, &end, 10);
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

    const int n = fputs(value, f);
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

static int sysfs_write_int(const char *leaf, const int v) {
    char buf[32];
    snprintf(buf, sizeof buf, "%d\n", v);

    return sysfs_write(leaf, buf);
}

static int joypad_writable() {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", JOY_SYS, "led_set");

    return access(path, W_OK) == 0;
}

static int joypad_write(const char *leaf, const char *value) {
    char path[512];
    snprintf(path, sizeof path, "%s/%s", JOY_SYS, leaf);

    if (access(path, W_OK) != 0) return -1;

    return write_string(path, value);
}

static int joypad_write_int(const char *leaf, const int v) {
    char buf[32];
    snprintf(buf, sizeof buf, "%d\n", v);

    return joypad_write(leaf, buf);
}

static int joypad_level_from_byte(const int brightness) {
    return clamp((clamp(brightness, MCU_BRI) * JOY_BRI + MCU_BRI / 2) / MCU_BRI, JOY_BRI);
}

static int joypad_mode_from_protocol(const int mode) {
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

static int joypad_breathing_speed_pct_for_protocol(const int mode) {
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
    if (joypad_writable()) joypad_write_int("led_set", 1);

    return 0;
}

static backend_t detect_device_backend(void) {
    static const struct {
        const char *code;
        backend_t backend;
    } map[] = {
        {"gcs-h36s", be_serial},    {"rg40xx-h", be_serial}, {"rg40xx-v", be_serial}, {"rgcubexx-h", be_serial},
        {"rg-vita-pro", be_joypad}, {"tui-brick", be_sysfs}, {"tui-spoon", be_sysfs},
    };

    const char *board = read_line_char_from(CONF_DEVICE_PATH "board/name", 1);
    if (!board || !*board) return be_auto;

    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++) {
        if (strcmp(board, map[i].code) == 0) return map[i].backend;
    }

    return be_auto;
}

static backend_t detect_backend(const backend_t requested) {
    if (requested != be_auto) return requested;

    const backend_t from_device = detect_device_backend();

    if (from_device == be_joypad && dir_exists(JOY_SYS)) return be_joypad;
    if (from_device == be_sysfs && dir_exists(LED_SYS)) return be_sysfs;
    if (from_device == be_serial && char_dev_exists(SER_DEV)) return be_serial;

    if (dir_exists(JOY_SYS) && joypad_writable()) return be_joypad;
    if (dir_exists(LED_SYS)) return be_sysfs;
    if (char_dev_exists(SER_DEV)) return be_serial;

    die("Error: no supported LED backend found (missing %s, %s and %s)", JOY_SYS, LED_SYS, SER_DEV);

    return be_auto;
}

static int serial_fd = -1;

static int mcu_already_powered(void) {
    FILE *f = fopen(MCU_DEV, "r");
    if (!f) return 0;

    char buf[8] = {0};
    const char *got = fgets(buf, sizeof buf, f);
    fclose(f);

    return got && buf[0] == '1';
}

static void serial_prepare(void) {
    const int mcu_was_on = mcu_already_powered();

    if (access(MCU_DEV, W_OK) == 0) write_string(MCU_DEV, "1\n");

    serial_fd = open(SER_DEV, O_WRONLY | O_NOCTTY);
    if (serial_fd < 0) die("Error: cannot open %s: %s", SER_DEV, strerror(errno));

    struct termios tio;
    if (tcgetattr(serial_fd, &tio) == 0) {
        cfmakeraw(&tio);

        cfsetispeed(&tio, B115200);
        cfsetospeed(&tio, B115200);

        tio.c_cflag |= CLOCAL | CREAD | CS8;
        tio.c_cflag &= ~(PARENB | CSTOPB);
        tio.c_lflag &= ~(ICANON | ECHO | ISIG);
        tio.c_oflag &= ~OPOST;

        tcsetattr(serial_fd, TCSANOW, &tio);
    }

    if (!mcu_was_on) sleep(1);
}

static uint8_t checksum_u8(const uint8_t *bytes, const size_t n) {
    unsigned sum = 0;
    for (size_t i = 0; i < n; i++)
        sum += bytes[i];
    return (uint8_t) (sum & 0xff);
}

static void serial_write_bytes(const uint8_t *bytes, const size_t n) {
    if (serial_fd < 0) return;

    const ssize_t w = write(serial_fd, bytes, n);
    (void) w;
}

static void serial_send_static(
    const int brightness, const int rr, const int rg, const int rb, const int lr, const int lg, const int lb
) {
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

static void serial_send_breath(const int mode, const int brightness, const int r, const int g, const int b) {
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

static void serial_send_rainbow(const int mode, const int brightness, const int speed) {
    uint8_t buf[6];

    buf[0] = (uint8_t) mode;
    buf[1] = (uint8_t) clamp(brightness, MCU_BRI);
    buf[2] = 1;
    buf[3] = 1;
    buf[4] = (uint8_t) clamp(speed, MCU_BRI);
    buf[5] = checksum_u8(buf, 5);

    serial_write_bytes(buf, 6);
}

static int effect_map_sysfs(const int mode) {
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

static int duration_map_sysfs(const int mode, const int fallback) {
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

static int env_int(const char *name, const int fallback) {
    const char *v = getenv(name);

    if (!v || !*v) return fallback;

    char *end;
    const long n = strtol(v, &end, 10);

    return *end ? fallback : (int) n;
}

static void hex3(char *out, const int r, const int g, const int b) {
    snprintf(out, 16, "%02X%02X%02X ", clamp(r, MCU_BRI), clamp(g, MCU_BRI), clamp(b, MCU_BRI));
}

static int apply_sysfs(int mode, int brightness_raw, int argc, char **argv, const flags_t *fl);

static int apply_serial(int mode, int brightness, int argc, char **argv);

static int apply_joypad(int mode, int brightness, int argc, char **argv);

static int read_config_int(const char *leaf, const int fallback) {
    char path[512];
    snprintf(path, sizeof path, "%s%s", MUOS_CONFIG_PATH, leaf);

    FILE *f = fopen(path, "r");
    if (!f) return fallback;

    char buf[32];
    char *got = fgets(buf, sizeof buf, f);
    fclose(f);

    if (!got) return fallback;

    char *end;
    const long v = strtol(buf, &end, 10);
    if (end == buf) return fallback;

    return (int) v;
}

static int dispatch_off(const backend_t requested) {
    const backend_t use = detect_backend(requested);
    flags_t fl;

    fl.dur_all = fl.dur_l = fl.dur_r = fl.dur_m = fl.dur_f1 = fl.dur_f2 = -1;
    fl.cyc_all = fl.cyc_l = fl.cyc_r = fl.cyc_m = fl.cyc_f1 = fl.cyc_f2 = INT32_MIN;

    if (use == be_joypad) {
        if (!dir_exists(JOY_SYS)) die("Error: JOYPAD backend selected but %s not present.", JOY_SYS);

        joypad_write_int("led_switch", 0);
        joypad_commit();

        return 0;
    }

    if (use == be_sysfs) {
        if (!dir_exists(LED_SYS)) die("Error: SYSFS backend selected but %s not present.", LED_SYS);

        char *zero_argv[] = {
            "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
        };

        return apply_sysfs(1, 0, 15, zero_argv, &fl);
    }

    if (!char_dev_exists(SER_DEV)) die("Error: SERIAL backend selected but %s not present.", SER_DEV);

    char *zero_argv[] = {"0", "0", "0", "0", "0", "0"};
    const int rc = apply_serial(1, 0, 6, zero_argv);

    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }

    return rc;
}

static backend_t restore_backend_from_config(const int saved) {
    switch (saved) {
        case 1:
            return be_sysfs;
        case 2:
            return be_serial;
        case 3:
            return be_joypad;
        default:
            return be_auto;
    }
}

static int restore_breath_wire_mode(const int saved) {
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

static int restore_brightness_to_byte(const int b) {
    return clamp(b, MCU_BRI);
}

static int get_rgb_path(char *rgb_path) {
    theme_base = get_theme_base();
    char active_path[MAX_BUFFER_SIZE];
    snprintf(active_path, sizeof(active_path), "%s/active.txt", theme_base);
    if (file_exist(active_path)) {
        snprintf(
            rgb_path, MAX_BUFFER_SIZE, "%s/alternate/rgb/%s/", theme_base,
            str_replace(read_line_char_from(active_path, 1), "\r", "")
        );
        return dir_exist(rgb_path);
    }
    snprintf(rgb_path, MAX_BUFFER_SIZE, "%s/rgb/", theme_base);
    return dir_exist(rgb_path);
}

typedef struct {
    int mode;
    int backend;
    int bright_raw;
    rgb_colour_t col_l, col_r, col_m, col_f1, col_f2;
} rgb_restore_state_t;

static void load_saved_rgb_state(rgb_restore_state_t *st) {
    st->mode = read_config_int("settings/rgb/mode", 0);
    st->backend = read_config_int("settings/rgb/backend", 0);

    if (st->mode == 4) {
        char settings_path[MAX_BUFFER_SIZE];

        char base_path[MAX_BUFFER_SIZE];
        get_rgb_path(base_path);

        snprintf(settings_path, sizeof(settings_path), "%smode", base_path);
        st->mode = read_line_int_from(settings_path, 1);

        snprintf(settings_path, sizeof(settings_path), "%sbackend", base_path);
        st->backend = read_line_int_from(settings_path, 1);

        snprintf(settings_path, sizeof(settings_path), "%sbright", base_path);
        st->bright_raw = read_line_int_from(settings_path, 1);

        snprintf(settings_path, sizeof(settings_path), "%scolour_l", base_path);
        read_rgb_colour_from_file(settings_path, &st->col_l, &rgb_colours[0]);

        snprintf(settings_path, sizeof(settings_path), "%scolour_r", base_path);
        read_rgb_colour_from_file(settings_path, &st->col_r, &st->col_l);

        snprintf(settings_path, sizeof(settings_path), "%scolour_m", base_path);
        read_rgb_colour_from_file(settings_path, &st->col_m, &st->col_l);

        snprintf(settings_path, sizeof(settings_path), "%scolour_f1", base_path);
        read_rgb_colour_from_file(settings_path, &st->col_f1, &st->col_l);

        snprintf(settings_path, sizeof(settings_path), "%scolour_f2", base_path);
        read_rgb_colour_from_file(settings_path, &st->col_f2, &st->col_r);
    } else {
        st->bright_raw = read_config_int("settings/rgb/bright", 6);
        read_rgb_colour_from_file(MUOS_CONFIG_PATH "settings/rgb/colour_l", &st->col_l, &rgb_colours[0]);
        st->col_r = *rgb_colour_or_fallback(read_config_int("settings/rgb/colour_r", 0), &st->col_l);
        st->col_m = *rgb_colour_or_fallback(read_config_int("settings/rgb/colour_m", 0), &st->col_l);
        st->col_f1 = *rgb_colour_or_fallback(read_config_int("settings/rgb/colour_f1", 0), &st->col_l);
        st->col_f2 = *rgb_colour_or_fallback(read_config_int("settings/rgb/colour_f2", 0), &st->col_r);
    }
}

static int dispatch_wire_command(
    const backend_t use, const int wire_mode, const int bright_byte, const int n, char **argv_buf, const flags_t *fl
) {
    if (use == be_joypad) {
        if (!dir_exists(JOY_SYS)) die("Error: JOYPAD backend selected but %s not present!", JOY_SYS);
        return apply_joypad(wire_mode, bright_byte, n, argv_buf);
    }

    if (use == be_sysfs) {
        if (!dir_exists(LED_SYS)) die("Error: SYSFS backend selected but %s not present!", LED_SYS);
        return apply_sysfs(wire_mode, bright_byte, n, argv_buf, fl);
    }

    if (!char_dev_exists(SER_DEV)) die("Error: SERIAL backend selected but %s not present!", SER_DEV);
    const int rc = apply_serial(wire_mode, bright_byte, n, argv_buf);

    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }

    return rc;
}

static int dispatch_restore(void) {
    rgb_restore_state_t st;
    load_saved_rgb_state(&st);

    int saved_mode = st.mode;
    int saved_backend = st.backend;
    int saved_bright_raw = st.bright_raw;
    rgb_colour_t col_l = st.col_l, col_r = st.col_r, col_m = st.col_m, col_f1 = st.col_f1, col_f2 = st.col_f2;

    if (saved_mode == 10) {
        // Screen React: the rgb_react daemon drives the LEDs itself. Start it
        // if it is not already running (it records its PID and exits on its
        // own when the mode changes).
        system(
            "P=$(cat /run/muos/rgb_react.pid 2>/dev/null); "
            "{ [ -n \"$P\" ] && kill -0 \"$P\" 2>/dev/null; } || "
            "setsid -f /opt/muos/script/device/rgb_react.sh </dev/null >/dev/null 2>&1"
        );
        return 0;
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

#define PUSH(v)                                                                                                        \
    do {                                                                                                               \
        snprintf(num_buf[n], sizeof num_buf[n], "%d", (v));                                                            \
        argv_buf[n] = num_buf[n];                                                                                      \
        n++;                                                                                                           \
    } while (0)

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

        if (use == be_joypad) {
            int sec_r, sec_g, sec_b;
            //
            // TODO: by Xongle
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

            if (use == be_sysfs) {
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

        if (use == be_sysfs) {
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
            PUSH(col_r.r);
            PUSH(col_r.g);
            PUSH(col_r.b);
        }
    }

#undef PUSH

    return dispatch_wire_command(use, wire_mode, bright_byte, n, argv_buf, &fl);
}

static int apply_sysfs(const int mode, const int brightness_raw, const int argc, char **argv, const flags_t *fl) {
    if (effect_map_sysfs(mode) < 0) die("Invalid mode for SYSFS: %d (1-9)", mode);
    const int bri = clamp(brightness_raw * SER_BRI / MCU_BRI, SER_BRI);

    if (argc < 3) {
        fprintf(
            stderr, "SYSFS usage: murgb -b sysfs <mode 1-9> <brightness 0-255> "
                    "<L_r L_g L_b> [<R_r R_g R_b>] [M_r M_g M_b] [F1_r F1_g F1_b] [F2_r F2_g F2_b]\n"
        );
        return 1;
    }

    const int lr = parse_int(argv[0], "L_r");
    const int lg = parse_int(argv[1], "L_g");
    const int lb = parse_int(argv[2], "L_b");

    const int rr = argc >= 6 ? parse_int(argv[3], "R_r") : lr;
    const int rg = argc >= 6 ? parse_int(argv[4], "R_g") : lg;
    const int rb = argc >= 6 ? parse_int(argv[5], "R_b") : lb;

    const int mr = argc >= 9 ? parse_int(argv[6], "M_r") : lr;
    const int mg = argc >= 9 ? parse_int(argv[7], "M_g") : lg;
    const int mb = argc >= 9 ? parse_int(argv[8], "M_b") : lb;

    const int f1_r = argc >= 12 ? parse_int(argv[9], "F1_r") : lr;
    const int f1_g = argc >= 12 ? parse_int(argv[10], "F1_g") : lg;
    const int f1_b = argc >= 12 ? parse_int(argv[11], "F1_b") : lb;

    const int f2_r = argc >= 15 ? parse_int(argv[12], "F2_r") : rr;
    const int f2_g = argc >= 15 ? parse_int(argv[13], "F2_g") : rg;
    const int f2_b = argc >= 15 ? parse_int(argv[14], "F2_b") : rb;

    char hex_l[16];
    char hex_r[16];
    char hex_m[16];
    char hex_f1[16];
    char hex_f2[16];

    hex3(hex_l, lr, lg, lb);
    hex3(hex_r, rr, rg, rb);
    hex3(hex_m, mr, mg, mb);
    hex3(hex_f1, f1_r, f1_g, f1_b);
    hex3(hex_f2, f2_r, f2_g, f2_b);

    sysfs_write_int("max_scale", bri);

    const int same_lr = strcmp(hex_l, hex_r) == 0;
    if (same_lr && sysfs_writable("effect_rgb_hex_lr")) {
        sysfs_write("effect_rgb_hex_lr", hex_l);
    } else {
        sysfs_write("effect_rgb_hex_l", hex_l);
        sysfs_write("effect_rgb_hex_r", hex_r);
    }

    sysfs_write("effect_rgb_hex_m", hex_m);
    sysfs_write("effect_rgb_hex_f1", hex_f1);
    sysfs_write("effect_rgb_hex_f2", hex_f2);

    int dur_all = fl->dur_all >= 0 ? fl->dur_all : env_int("LED_DUR", 1000);
    dur_all = duration_map_sysfs(mode, dur_all);

    const int dur_l = fl->dur_l >= 0 ? fl->dur_l : env_int("LED_DUR_L", dur_all);
    const int dur_r = fl->dur_r >= 0 ? fl->dur_r : env_int("LED_DUR_R", dur_all);
    const int dur_m = fl->dur_m >= 0 ? fl->dur_m : env_int("LED_DUR_M", dur_all);
    const int dur_f1 = fl->dur_f1 >= 0 ? fl->dur_f1 : env_int("LED_DUR_F1", dur_all);
    const int dur_f2 = fl->dur_f2 >= 0 ? fl->dur_f2 : env_int("LED_DUR_F2", dur_all);

    const int cyc_all = fl->cyc_all != INT32_MIN ? fl->cyc_all : env_int("LED_CYCLES", -1);
    const int cyc_l = fl->cyc_l != INT32_MIN ? fl->cyc_l : env_int("LED_CYCLES_L", cyc_all);
    const int cyc_r = fl->cyc_r != INT32_MIN ? fl->cyc_r : env_int("LED_CYCLES_R", cyc_all);
    const int cyc_m = fl->cyc_m != INT32_MIN ? fl->cyc_m : env_int("LED_CYCLES_M", cyc_all);
    const int cyc_f1 = fl->cyc_f1 != INT32_MIN ? fl->cyc_f1 : env_int("LED_CYCLES_F1", cyc_all);
    const int cyc_f2 = fl->cyc_f2 != INT32_MIN ? fl->cyc_f2 : env_int("LED_CYCLES_F2", cyc_all);

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

    const int effect = effect_map_sysfs(mode);
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

static int apply_joypad(const int mode, const int brightness, const int argc, char **argv) {
    if (mode < 1 || mode > 8) die("Invalid mode for JOYPAD: %d (1-8)", mode);

    const int level = joypad_level_from_byte(brightness);

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
            const int sr = clamp(parse_int(argv[0], "R"), MCU_BRI);
            const int sg = clamp(parse_int(argv[1], "G"), MCU_BRI);
            const int sb = clamp(parse_int(argv[2], "B"), MCU_BRI);

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
            const int speed_pct = joypad_speed_pct_to_reg(parse_int(argv[0], "speed"));
            joypad_write_int("led_speed", speed_pct);
        }

        joypad_write_int("led_mode", joypad_mode_from_protocol(mode));
        joypad_commit();

        printf("LED mode %d set with brightness %d (JOYPAD, rainbow)\n", mode, level);
        return 0;
    }

    if (mode == 7 || mode == 8) {
        const int mode8_argc_ok = mode == 8 && (argc == 4 || argc == 7);
        if (!(mode == 7 && argc == 1) && !mode8_argc_ok) {
            fprintf(
                stderr, "JOYPAD usage:\n"
                        "  murgb -b joypad 7 <brightness> <speed 0-100>\n"
                        "  murgb -b joypad 8 <brightness> <speed 0-100> <R1> <G1> <B1> [<R2> <G2> <B2>]\n"
            );
            return 1;
        }

        const int ui_speed_pct = joypad_speed_pct_to_reg(parse_int(argv[0], "speed"));
        const int reg_speed_pct = mode == 8 ? joypad_breathing_speed_invert(ui_speed_pct) : ui_speed_pct;

        joypad_write_int("led_switch", 1);
        joypad_write_int("led_level", level);
        joypad_write_int("led_sync_colour", 1);
        joypad_write_int("led_speed", reg_speed_pct);

        if (mode == 8) {
            const int r1 = clamp(parse_int(argv[1], "R1"), MCU_BRI);
            const int g1 = clamp(parse_int(argv[2], "G1"), MCU_BRI);
            const int b1 = clamp(parse_int(argv[3], "B1"), MCU_BRI);

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

    const int lr = clamp(parse_int(argv[0], "R"), MCU_BRI);
    const int lg = clamp(parse_int(argv[1], "G"), MCU_BRI);
    const int lb = clamp(parse_int(argv[2], "B"), MCU_BRI);

    const int enabled = lr || lg || lb;

    if (!enabled) {
        joypad_write_int("led_switch", 0);
        joypad_commit();

        printf("LED off (JOYPAD)\n");

        return 0;
    }

    joypad_write_int("led_switch", 1);
    joypad_write_int("led_level", level);
    joypad_write_int("led_sync_colour", mode == 1 ? 0 : 1);

    if (mode >= 2 && mode <= 4) {
        const int ui_pct = joypad_breathing_speed_pct_for_protocol(mode);
        joypad_write_int("led_speed", joypad_breathing_speed_invert(ui_pct));
    }

    joypad_write_int("Led_rgb_r1", lr);
    joypad_write_int("Led_rgb_g1", lg);
    joypad_write_int("Led_rgb_b1", lb);

    const int lr2 = argc >= 6 ? clamp(parse_int(argv[3], "R2"), MCU_BRI) : lr;
    const int lg2 = argc >= 6 ? clamp(parse_int(argv[4], "G2"), MCU_BRI) : lg;
    const int lb2 = argc >= 6 ? clamp(parse_int(argv[5], "B2"), MCU_BRI) : lb;

    joypad_write_int("Led_rgb_r2", lr2);
    joypad_write_int("Led_rgb_g2", lg2);
    joypad_write_int("Led_rgb_b2", lb2);

    joypad_write_int("led_mode", joypad_mode_from_protocol(mode));
    joypad_commit();

    printf("LED mode %d set with brightness %d (JOYPAD)\n", mode, level);

    return 0;
}

static int apply_serial(const int mode, const int brightness, const int argc, char **argv) {
    if (mode < 1 || mode > 6) die("Invalid mode for SERIAL: %d (1-6)", mode);

    const int bri = clamp(brightness, MCU_BRI);
    serial_prepare();

    if (mode >= 5 && mode <= 6) {
        if (argc != 1) {
            fprintf(stderr, "SERIAL usage (5-6): murgb -b serial <5|6> <brightness> <speed 0-255>\n");
            return 1;
        }

        const int speed = clamp(parse_int(argv[0], "speed"), MCU_BRI);
        serial_send_rainbow(mode, bri, speed);

        printf("LED mode %d set with brightness %d (SERIAL)\n", mode, bri);

        return 0;
    }

    if (mode == 1) {
        if (argc != 6) {
            fprintf(
                stderr, "SERIAL usage (1): murgb -b serial 1 <brightness> "
                        "<right_r> <right_g> <right_b> <left_r> <left_g> <left_b>\n"
            );
            return 1;
        }

        const int rr = parse_int(argv[0], "right_r");
        const int rg = parse_int(argv[1], "right_g");
        const int rb = parse_int(argv[2], "right_b");

        const int lr = parse_int(argv[3], "left_r");
        const int lg = parse_int(argv[4], "left_g");
        const int lb = parse_int(argv[5], "left_b");

        serial_send_static(bri, rr, rg, rb, lr, lg, lb);
        printf("LED mode %d set with brightness %d (SERIAL)\n", mode, bri);

        return 0;
    }

    if (argc != 3) {
        fprintf(stderr, "SERIAL usage (2-4): murgb -b serial <2|3|4> <brightness> <r> <g> <b>\n");
        return 1;
    }

    const int r = parse_int(argv[0], "r");
    const int g = parse_int(argv[1], "g");
    const int b = parse_int(argv[2], "b");

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
        stderr
    );
}

static backend_t parse_backend(const char *s) {
    if (!s) die("Error: -b requires a value");

    if (!strcasecmp(s, "auto")) return be_auto;
    if (!strcasecmp(s, "sysfs")) return be_sysfs;
    if (!strcasecmp(s, "serial")) return be_serial;
    if (!strcasecmp(s, "joypad")) return be_joypad;

    die("Invalid backend: %s", s);
    return be_auto;
}

int main(const int argc, char **argv) {
    load_config(&config);

    if (argc >= 2 && (strcmp(argv[1], "off") == 0 || strcmp(argv[1], "restore") == 0)) {
        if (argc != 2) {
            fprintf(stderr, "Error: '%s' takes no other arguments\n", argv[1]);
            return 1;
        }
        if (strcmp(argv[1], "off") == 0) return dispatch_off(be_auto);
        return dispatch_restore();
    }

    flags_t fl;
    fl.dur_all = fl.dur_l = fl.dur_r = fl.dur_m = fl.dur_f1 = fl.dur_f2 = -1;
    fl.cyc_all = fl.cyc_l = fl.cyc_r = fl.cyc_m = fl.cyc_f1 = fl.cyc_f2 = INT32_MIN;

    backend_t be = be_auto;
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

#define OPT_INT(flag, slot)                                                                                            \
    if (strcmp(a, flag) == 0) {                                                                                        \
        if (i + 1 >= argc) die("Error: %s requires a value", flag);                                                    \
        (slot) = parse_int(argv[i + 1], flag);                                                                         \
        i += 2;                                                                                                        \
        continue;                                                                                                      \
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

    const int mode = parse_int(argv[i], "mode");
    const int brightness = parse_int(argv[i + 1], "brightness");
    const int rest_argc = argc - i - 2;
    char **rest_argv = argv + i + 2;

    const backend_t use = detect_backend(be);

    return dispatch_wire_command(use, mode, brightness, rest_argc, rest_argv, &fl);
}
