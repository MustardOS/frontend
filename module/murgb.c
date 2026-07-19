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

static int joypad_commit(void) {
    if (joypad_writable()) joypad_write_int("led_set", 1);

    return 0;
}

static backend_t detect_device_backend(void) {
    static const struct {
        const char *code;
        backend_t backend;
    } map[] = {
        {"gcs-h36s", be_serial},    {"rg40xx-h", be_serial}, {"rg40xx-v", be_serial},     {"rgcubexx-h", be_serial},
        {"rg-vita-pro", be_joypad}, {"tui-brick", be_sysfs}, {"tui-brick-pro", be_sysfs}, {"tui-spoon", be_sysfs},
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

#define LEDC_RAW_TOTAL 23

#define BRICK_F2_LED 8
#define BRICK_F1_LED 9

#define BRICK_PRO_RSTICK1_LED_LO 14
#define BRICK_PRO_RSTICK1_LED_HI 15
#define BRICK_PRO_RSTICK2_LED_LO 16
#define BRICK_PRO_RSTICK2_LED_HI 17

#define BRICK_PRO_SHOULDER_L1_LED 18
#define BRICK_PRO_SHOULDER_L2_LED 19
#define BRICK_PRO_SHOULDER_R2_LED 20
#define BRICK_PRO_SHOULDER_R1_LED 21

#define BRICK_SHOULDER_L1_LED 10
#define BRICK_SHOULDER_L2_LED 11
#define BRICK_SHOULDER_R2_LED 12
#define BRICK_SHOULDER_R1_LED 13

static int board_is_tui_brick_pro(void) {
    const char *board = read_line_char_from(CONF_DEVICE_PATH "board/name", 1);
    return board && strcmp(board, "tui-brick-pro") == 0;
}

static int board_is_tui_brick(void) {
    const char *board = read_line_char_from(CONF_DEVICE_PATH "board/name", 1);
    return board && strcmp(board, "tui-brick") == 0;
}

static void swap_rg(int *r, int *g) {
    const int t = *r;
    *r = *g;
    *g = t;
}

static int scale_channel(const int v, const int brightness_raw) {
    return clamp(clamp(v, MCU_BRI) * clamp(brightness_raw, MCU_BRI) / MCU_BRI, MCU_BRI);
}

static void write_raw_zone_frame(
    const int rs1_r, const int rs1_g, const int rs1_b, const int rs2_r, const int rs2_g, const int rs2_b,
    const int shoulder_rgb[4][3], const int shoulder_led[4]
) {
    char frame[LEDC_RAW_TOTAL * 7 + 1];
    char *p = frame;

    for (int i = 0; i < LEDC_RAW_TOTAL; i++) {
        int r = 0, g = 0, b = 0;

        if (i >= BRICK_PRO_RSTICK1_LED_LO && i <= BRICK_PRO_RSTICK1_LED_HI) {
            r = rs1_r;
            g = rs1_g;
            b = rs1_b;
        } else if (i >= BRICK_PRO_RSTICK2_LED_LO && i <= BRICK_PRO_RSTICK2_LED_HI) {
            r = rs2_r;
            g = rs2_g;
            b = rs2_b;
        } else {
            for (int k = 0; k < 4; k++) {
                if (i != shoulder_led[k]) continue;
                r = shoulder_rgb[k][0];
                g = shoulder_rgb[k][1];
                b = shoulder_rgb[k][2];
                break;
            }
        }

        p += snprintf(p, 8, "%02X%02X%02X ", clamp(r, MCU_BRI), clamp(g, MCU_BRI), clamp(b, MCU_BRI));
    }

    sysfs_write("frame_hex", frame);
}

static void write_brick_raw_frame(const int f1_rgb[3], const int f2_rgb[3], const int shoulder_rgb[4][3]) {
    static const int led_index[6] = {BRICK_F2_LED,          BRICK_F1_LED,          BRICK_SHOULDER_L1_LED,
                                     BRICK_SHOULDER_L2_LED, BRICK_SHOULDER_R2_LED, BRICK_SHOULDER_R1_LED};
    const int *const rgb[6] = {f2_rgb, f1_rgb, shoulder_rgb[0], shoulder_rgb[1], shoulder_rgb[2], shoulder_rgb[3]};

    char frame[LEDC_RAW_TOTAL * 7 + 1];
    char *p = frame;

    for (int i = 0; i < LEDC_RAW_TOTAL; i++) {
        int r = 0, g = 0, b = 0;

        for (int k = 0; k < 6; k++) {
            if (i != led_index[k]) continue;
            r = rgb[k][0];
            g = rgb[k][1];
            b = rgb[k][2];
            break;
        }

        p += snprintf(p, 8, "%02X%02X%02X ", clamp(r, MCU_BRI), clamp(g, MCU_BRI), clamp(b, MCU_BRI));
    }

    sysfs_write("frame_hex", frame);
}

static void hex3(char *out, int r, int g, int b);

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

static int effect_map_sysfs(const int mode) {
    return mode == 1 ? 4 : -1;
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

static void dispatch_off(void) {
    const backend_t use = detect_backend(be_auto);
    flags_t fl;

    fl.dur_all = fl.dur_l = fl.dur_r = fl.dur_m = fl.dur_f1 = fl.dur_f2 = -1;
    fl.cyc_all = fl.cyc_l = fl.cyc_r = fl.cyc_m = fl.cyc_f1 = fl.cyc_f2 = INT32_MIN;

    if (use == be_joypad) {
        if (!dir_exists(JOY_SYS)) die("Error: JOYPAD backend selected but %s not present.", JOY_SYS);

        joypad_write_int("led_switch", 0);
        joypad_commit();

        return;
    }

    if (use == be_sysfs) {
        if (!dir_exists(LED_SYS)) die("Error: SYSFS backend selected but %s not present.", LED_SYS);

        char *zero_argv[] = {
            "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
        };

        apply_sysfs(1, 0, 15, zero_argv, &fl);
        return;
    }

    if (!char_dev_exists(SER_DEV)) die("Error: SERIAL backend selected but %s not present.", SER_DEV);

    char *zero_argv[] = {"0", "0", "0", "0", "0", "0"};
    apply_serial(1, 0, 6, zero_argv);

    if (serial_fd >= 0) {
        close(serial_fd);
        serial_fd = -1;
    }
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

typedef struct {
    int backend;
    rgb_colour_t col_l, col_r, col_m, col_f1, col_f2, col_rs1, col_rs2;
    rgb_colour_t col_shl1, col_shl2, col_shr2, col_shr1;
    int bright_l, bright_r, bright_m, bright_f1, bright_f2, bright_rs1, bright_rs2;
    int bright_shl1, bright_shl2, bright_shr2, bright_shr1;
} rgb_restore_state_t;

static void load_saved_rgb_state(rgb_restore_state_t *st) {
    st->backend = read_config_int("settings/rgb/backend", 0);

    st->col_l = *rgb_colour_at(read_config_int("settings/rgb/colour_l", 0));
    st->bright_l = read_config_int("settings/rgb/bright_l", 255);

    st->col_r = *rgb_colour_at(read_config_int("settings/rgb/colour_r", 0));
    st->bright_r = read_config_int("settings/rgb/bright_r", 255);

    st->col_m = *rgb_colour_at(read_config_int("settings/rgb/colour_m", 0));
    st->bright_m = read_config_int("settings/rgb/bright_m", 255);

    st->col_f1 = *rgb_colour_at(read_config_int("settings/rgb/colour_f1", 0));
    st->bright_f1 = read_config_int("settings/rgb/bright_f1", 255);

    st->col_f2 = *rgb_colour_at(read_config_int("settings/rgb/colour_f2", 0));
    st->bright_f2 = read_config_int("settings/rgb/bright_f2", 255);

    st->col_rs1 = *rgb_colour_at(read_config_int("settings/rgb/colour_rs1", 0));
    st->bright_rs1 = read_config_int("settings/rgb/bright_rs1", 255);

    st->col_rs2 = *rgb_colour_at(read_config_int("settings/rgb/colour_rs2", 0));
    st->bright_rs2 = read_config_int("settings/rgb/bright_rs2", 255);

    st->col_shl1 = *rgb_colour_at(read_config_int("settings/rgb/colour_shl1", 0));
    st->bright_shl1 = read_config_int("settings/rgb/bright_shl1", 255);

    st->col_shl2 = *rgb_colour_at(read_config_int("settings/rgb/colour_shl2", 0));
    st->bright_shl2 = read_config_int("settings/rgb/bright_shl2", 255);

    st->col_shr2 = *rgb_colour_at(read_config_int("settings/rgb/colour_shr2", 0));
    st->bright_shr2 = read_config_int("settings/rgb/bright_shr2", 255);

    st->col_shr1 = *rgb_colour_at(read_config_int("settings/rgb/colour_shr1", 0));
    st->bright_shr1 = read_config_int("settings/rgb/bright_shr1", 255);
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

    backend_t use = detect_backend(restore_backend_from_config(st.backend));

    flags_t fl;
    fl.dur_all = fl.dur_l = fl.dur_r = fl.dur_m = fl.dur_f1 = fl.dur_f2 = -1;
    fl.cyc_all = fl.cyc_l = fl.cyc_r = fl.cyc_m = fl.cyc_f1 = fl.cyc_f2 = INT32_MIN;

    char num_buf[33][8];
    char *argv_buf[33];
    int n = 0;

#define PUSH(v)                                                                                                        \
    do {                                                                                                               \
        snprintf(num_buf[n], sizeof num_buf[n], "%d", (v));                                                            \
        argv_buf[n] = num_buf[n];                                                                                      \
        n++;                                                                                                           \
    } while (0)
#define PUSH_ZONE(col, bri)                                                                                            \
    do {                                                                                                               \
        PUSH(scale_channel((col).r, (bri)));                                                                           \
        PUSH(scale_channel((col).g, (bri)));                                                                           \
        PUSH(scale_channel((col).b, (bri)));                                                                           \
    } while (0)

    if (use == be_sysfs) {
        PUSH_ZONE(st.col_l, st.bright_l);
        PUSH_ZONE(st.col_r, st.bright_r);
        PUSH_ZONE(st.col_m, st.bright_m);
        PUSH_ZONE(st.col_f1, st.bright_f1);
        PUSH_ZONE(st.col_f2, st.bright_f2);
        PUSH_ZONE(st.col_rs1, st.bright_rs1);
        PUSH_ZONE(st.col_rs2, st.bright_rs2);
        PUSH_ZONE(st.col_shl1, st.bright_shl1);
        PUSH_ZONE(st.col_shl2, st.bright_shl2);
        PUSH_ZONE(st.col_shr2, st.bright_shr2);
        PUSH_ZONE(st.col_shr1, st.bright_shr1);
    } else {
        PUSH_ZONE(st.col_l, st.bright_l);
        PUSH_ZONE(st.col_r, st.bright_r);
    }

#undef PUSH_ZONE
#undef PUSH

    return dispatch_wire_command(use, 1, MCU_BRI, n, argv_buf, &fl);
}

static int apply_sysfs(const int mode, const int brightness_raw, const int argc, char **argv, const flags_t *fl) {
    if (effect_map_sysfs(mode) < 0) die("Invalid mode for SYSFS: %d (1)", mode);
    const int bri = clamp(brightness_raw * SER_BRI / MCU_BRI, SER_BRI);

    if (argc < 3) {
        fprintf(
            stderr, "SYSFS usage: murgb -b sysfs 1 <brightness 0-255> "
                    "<L_r L_g L_b> [<R_r R_g R_b>] [M_r M_g M_b] [F1_r F1_g F1_b] [F2_r F2_g F2_b] "
                    "[RS1_r RS1_g RS1_b] [RS2_r RS2_g RS2_b] "
                    "[SHL1_r SHL1_g SHL1_b] [SHL2_r SHL2_g SHL2_b] [SHR2_r SHR2_g SHR2_b] [SHR1_r SHR1_g SHR1_b]\n"
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

    const int rs1_r = argc >= 18 ? parse_int(argv[15], "RS1_r") : rr;
    const int rs1_g = argc >= 18 ? parse_int(argv[16], "RS1_g") : rg;
    const int rs1_b = argc >= 18 ? parse_int(argv[17], "RS1_b") : rb;

    const int rs2_r = argc >= 21 ? parse_int(argv[18], "RS2_r") : rs1_r;
    const int rs2_g = argc >= 21 ? parse_int(argv[19], "RS2_g") : rs1_g;
    const int rs2_b = argc >= 21 ? parse_int(argv[20], "RS2_b") : rs1_b;

    const int shl1_r = argc >= 24 ? parse_int(argv[21], "SHL1_r") : f1_r;
    const int shl1_g = argc >= 24 ? parse_int(argv[22], "SHL1_g") : f1_g;
    const int shl1_b = argc >= 24 ? parse_int(argv[23], "SHL1_b") : f1_b;

    const int shl2_r = argc >= 27 ? parse_int(argv[24], "SHL2_r") : shl1_r;
    const int shl2_g = argc >= 27 ? parse_int(argv[25], "SHL2_g") : shl1_g;
    const int shl2_b = argc >= 27 ? parse_int(argv[26], "SHL2_b") : shl1_b;

    const int shr2_r = argc >= 30 ? parse_int(argv[27], "SHR2_r") : f2_r;
    const int shr2_g = argc >= 30 ? parse_int(argv[28], "SHR2_g") : f2_g;
    const int shr2_b = argc >= 30 ? parse_int(argv[29], "SHR2_b") : f2_b;

    const int shr1_r = argc >= 33 ? parse_int(argv[30], "SHR1_r") : shr2_r;
    const int shr1_g = argc >= 33 ? parse_int(argv[31], "SHR1_g") : shr2_g;
    const int shr1_b = argc >= 33 ? parse_int(argv[32], "SHR1_b") : shr2_b;

    char hex_l[16];
    char hex_r[16];
    char hex_m[16];
    char hex_f1[16];
    char hex_f2[16];

    const int tui_brick_pro = board_is_tui_brick_pro();
    const int tui_brick = board_is_tui_brick();

    int hex_lr_r = lr, hex_lr_g = lg;
    int hex_rr_r = rr, hex_rr_g = rg;
    if (tui_brick_pro) {
        swap_rg(&hex_lr_r, &hex_lr_g);
        swap_rg(&hex_rr_r, &hex_rr_g);
    }

    hex3(hex_l, hex_lr_r, hex_lr_g, lb);
    hex3(hex_r, hex_rr_r, hex_rr_g, rb);
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

    const int dur_all = fl->dur_all >= 0 ? fl->dur_all : env_int("LED_DUR", 1000);

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

    if (tui_brick) {
        sysfs_write_int("effect_l", 0);
        sysfs_write_int("effect_r", 0);
        sysfs_write_int("effect_f1", 0);
        sysfs_write_int("effect_f2", 0);
    } else if (same_lr && sysfs_writable("effect_lr")) {
        sysfs_write_int("effect_lr", effect);
    } else {
        sysfs_write_int("effect_l", effect);
        sysfs_write_int("effect_r", effect);
    }

    sysfs_write_int("effect_m", effect);

    if (!tui_brick) {
        sysfs_write_int("effect_f1", effect);
        sysfs_write_int("effect_f2", effect);
    }

    sysfs_write("effect_enable", "1\n");

    if (tui_brick_pro || tui_brick) {
        const int shoulder_rgb[4][3] = {
            {scale_channel(shl1_r, brightness_raw), scale_channel(shl1_g, brightness_raw),
             scale_channel(shl1_b, brightness_raw)},
            {scale_channel(shl2_r, brightness_raw), scale_channel(shl2_g, brightness_raw),
             scale_channel(shl2_b, brightness_raw)},
            {scale_channel(shr2_r, brightness_raw), scale_channel(shr2_g, brightness_raw),
             scale_channel(shr2_b, brightness_raw)},
            {scale_channel(shr1_r, brightness_raw), scale_channel(shr1_g, brightness_raw),
             scale_channel(shr1_b, brightness_raw)},
        };

        if (tui_brick_pro) {
            static const int pro_shoulder_led[4] = {
                BRICK_PRO_SHOULDER_L1_LED, BRICK_PRO_SHOULDER_L2_LED, BRICK_PRO_SHOULDER_R2_LED,
                BRICK_PRO_SHOULDER_R1_LED
            };

            int sc_rs1_r = scale_channel(rs1_r, brightness_raw);
            int sc_rs1_g = scale_channel(rs1_g, brightness_raw);
            const int sc_rs1_b = scale_channel(rs1_b, brightness_raw);
            swap_rg(&sc_rs1_r, &sc_rs1_g);

            int sc_rs2_r = scale_channel(rs2_r, brightness_raw);
            int sc_rs2_g = scale_channel(rs2_g, brightness_raw);
            const int sc_rs2_b = scale_channel(rs2_b, brightness_raw);
            swap_rg(&sc_rs2_r, &sc_rs2_g);

            write_raw_zone_frame(
                sc_rs1_r, sc_rs1_g, sc_rs1_b, sc_rs2_r, sc_rs2_g, sc_rs2_b, shoulder_rgb, pro_shoulder_led
            );
        } else {
            const int f1_rgb[3] = {
                scale_channel(f1_r, brightness_raw), scale_channel(f1_g, brightness_raw),
                scale_channel(f1_b, brightness_raw)
            };
            const int f2_rgb[3] = {
                scale_channel(f2_r, brightness_raw), scale_channel(f2_g, brightness_raw),
                scale_channel(f2_b, brightness_raw)
            };

            write_brick_raw_frame(f1_rgb, f2_rgb, shoulder_rgb);
        }
    }

    printf("LED mode %d applied (SYSFS) brightness=%d\n", mode, bri);

    return 0;
}

static int apply_joypad(const int mode, const int brightness, const int argc, char **argv) {
    if (mode != 1) die("Invalid mode for JOYPAD: %d (1)", mode);

    const int level = joypad_level_from_byte(brightness);

    if (level <= 0) {
        joypad_write_int("led_switch", 0);
        joypad_commit();

        printf("LED off (JOYPAD)\n");

        return 0;
    }

    if (argc < 3) {
        fprintf(stderr, "JOYPAD usage: murgb -b joypad 1 <brightness> <R G B>\n");
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
    joypad_write_int("led_sync_colour", 0);

    joypad_write_int("Led_rgb_r1", lr);
    joypad_write_int("Led_rgb_g1", lg);
    joypad_write_int("Led_rgb_b1", lb);

    const int lr2 = argc >= 6 ? clamp(parse_int(argv[3], "R2"), MCU_BRI) : lr;
    const int lg2 = argc >= 6 ? clamp(parse_int(argv[4], "G2"), MCU_BRI) : lg;
    const int lb2 = argc >= 6 ? clamp(parse_int(argv[5], "B2"), MCU_BRI) : lb;

    joypad_write_int("Led_rgb_r2", lr2);
    joypad_write_int("Led_rgb_g2", lg2);
    joypad_write_int("Led_rgb_b2", lb2);

    joypad_write_int("led_mode", 1);
    joypad_commit();

    printf("LED mode %d set with brightness %d (JOYPAD)\n", mode, level);

    return 0;
}

static int apply_serial(const int mode, const int brightness, const int argc, char **argv) {
    if (mode != 1) die("Invalid mode for SERIAL: %d (1)", mode);

    const int bri = clamp(brightness, MCU_BRI);
    serial_prepare();

    if (argc != 6) {
        fprintf(
            stderr, "SERIAL usage: murgb -b serial 1 <brightness> "
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
        if (strcmp(argv[1], "off") == 0) {
            dispatch_off();
            return 0;
        }
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
