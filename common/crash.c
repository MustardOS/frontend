#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "crash.h"
#include "options.h"
#include "fileio.h"

static char crash_module_name[MAX_BUFFER_SIZE];
static char crash_label_module[MAX_BUFFER_SIZE];
static char crash_label_fault[MAX_BUFFER_SIZE];

static mux_dialogue crash_dlg;
static int crash_dlg_active = 0;

static mux_dialogue power_loss_dlg;
static int power_loss_dlg_active = 0;

static const char *crash_sig_name(const int sig) {
    switch (sig) {
        case SIGSEGV:
            return "SIGSEGV";
        case SIGABRT:
            return "SIGABRT";
        case SIGBUS:
            return "SIGBUS";
        case SIGFPE:
            return "SIGFPE";
        default:
            return "SIGUNKNOWN";
    }
}

static void write_hex_addr(const int fd, const uintptr_t val) {
    const size_t digits = sizeof(uintptr_t) * 2;
    char buf[2 + sizeof(uintptr_t) * 2];

    buf[0] = '0';
    buf[1] = 'x';

    for (size_t i = 0; i < digits; i++) {
        const char hex[] = "0123456789abcdef";
        buf[2 + i] = hex[val >> ((digits - 1 - i) * 4) & 0xf];
    }

    const ssize_t r = write(fd, buf, 2 + digits);
    (void) r;
}

static void crash_signal_handler(int sig, siginfo_t *info, void *ucontext) {
    (void) ucontext;

    uintptr_t fault_addr = 0;
    if (info) fault_addr = (uintptr_t) info->si_addr;

    int fd = open(MUOS_CRS_LOAD, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const size_t ml = strlen(crash_label_module);
        const size_t n = strlen(crash_module_name);

        const char *sname = crash_sig_name(sig);

        const size_t slen = strlen(sname);
        const size_t fl = strlen(crash_label_fault);
        ssize_t r = write(fd, crash_label_module, ml);

        (void) r;
        if (n) {
            r = write(fd, crash_module_name, n);
            (void) r;
        }

        r = write(fd, " (", 2);
        (void) r;
        r = write(fd, sname, slen);

        (void) r;
        r = write(fd, ")", 1);

        (void) r;
        r = write(fd, crash_label_fault, fl);

        (void) r;
        write_hex_addr(fd, fault_addr);

        close(fd);
    }

    fd = open(MUOS_ACT_LOAD, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        const ssize_t r = write(fd, "launcher", 8);
        (void) r;
        close(fd);
    }

    fd = open(SAFE_QUIT, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) close(fd);

    _exit(1);
}

void crash_init(const char *module_name) {
    snprintf(crash_module_name, sizeof(crash_module_name), "%s", module_name);
    snprintf(crash_label_module, sizeof(crash_label_module), "%s: ", lang.generic.crash_module);
    snprintf(crash_label_fault, sizeof(crash_label_fault), "\n%s: ", lang.generic.crash_fault);

    struct sigaction sa = {0};
    sa.sa_sigaction = crash_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
}

void crash_ui_check(struct theme_config *t, const struct mux_lang *l, lv_obj_t *layer, int *msgbox_active) {
    if (!file_exist(MUOS_CRS_LOAD)) return;

    char *crashed = read_all_char_from(MUOS_CRS_LOAD);
    remove(MUOS_CRS_LOAD);

    if (crashed && *crashed) {
        increment_counter_file(CONF_CONFIG_PATH "count/crash");

        dialogue_init_message(
            &crash_dlg, t, layer, l->generic.crash_title, crashed, l->generic.crash_message, l->generic.confirm
        );
        dialogue_show(&crash_dlg);

        *msgbox_active = 1;
        crash_dlg_active = 1;
    }

    free(crashed);
}

void crash_ui_apply_font(const lv_obj_t *source) {
    if (!crash_dlg_active) return;

    const lv_font_t *font = lv_obj_get_style_text_font(source, LV_PART_MAIN);
    if (!font) return;

    lv_obj_set_style_text_font(crash_dlg.panel, font, MU_OBJ_MAIN_DEFAULT);

    lv_anim_del(crash_dlg.panel, NULL);
    lv_obj_set_style_opa(crash_dlg.panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_x(crash_dlg.panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_update_layout(lv_obj_get_parent(crash_dlg.panel));
    const lv_coord_t panel_h = lv_obj_get_height(crash_dlg.panel);
    lv_obj_set_y(crash_dlg.panel, (LV_VER_RES - panel_h) / 2);
}

int crash_ui_dismiss(void) {
    if (crash_dlg_active) {
        crash_dlg_active = 0;
        dialogue_hide(&crash_dlg);
        return 1;
    }

    return 0;
}

void power_loss_ui_apply_font(const lv_obj_t *source) {
    if (!power_loss_dlg_active) return;

    const lv_font_t *font = lv_obj_get_style_text_font(source, LV_PART_MAIN);
    if (!font) return;

    lv_obj_set_style_text_font(power_loss_dlg.panel, font, MU_OBJ_MAIN_DEFAULT);

    lv_anim_del(power_loss_dlg.panel, NULL);
    lv_obj_set_style_opa(power_loss_dlg.panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_translate_x(power_loss_dlg.panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_update_layout(lv_obj_get_parent(power_loss_dlg.panel));
    const lv_coord_t panel_h = lv_obj_get_height(power_loss_dlg.panel);
    lv_obj_set_y(power_loss_dlg.panel, (LV_VER_RES - panel_h) / 2);
}

void power_loss_ui_check(struct theme_config *t, const struct mux_lang *l, lv_obj_t *layer, int *msgbox_active) {
    if (!file_exist(MUOS_PWR_LOSS)) return;
    remove(MUOS_PWR_LOSS);

    increment_counter_file(CONF_CONFIG_PATH "count/power_loss");

    dialogue_init_message(
        &power_loss_dlg, t, layer, l->generic.power_loss_title, NULL, l->generic.power_loss_message,
        l->generic.understand
    );
    dialogue_show(&power_loss_dlg);

    *msgbox_active = 1;
    power_loss_dlg_active = 1;
}

int power_loss_ui_dismiss(void) {
    if (power_loss_dlg_active) {
        power_loss_dlg_active = 0;
        dialogue_hide(&power_loss_dlg);
        return 1;
    }

    return 0;
}
