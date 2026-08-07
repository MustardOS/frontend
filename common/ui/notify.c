#include <string.h>
#include "notify.h"
#include "common.h"
#include "nav.h"
#include "../config.h"
#include "../language.h"
#include "../init.h"
#include "../options.h"
#include <stdio.h>

#define NOTIFY_QUEUE_MAX 8
#define NOTIFY_STACK_MAX 3

typedef struct {
    notify_level level;
    char text[MAX_BUFFER_SIZE];
    uint32_t hold_ms;
} notify_entry;

static notify_entry queue[NOTIFY_QUEUE_MAX];
static int queue_count = 0;

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *label;
    char text[MAX_BUFFER_SIZE];
    uint32_t until;
    int active;
} notify_slot;

static notify_slot stack[NOTIFY_STACK_MAX];
static int stack_ready = 0;

static int slot_usable(const int i) {
    return stack[i].panel && lv_obj_is_valid(stack[i].panel) && stack[i].label && lv_obj_is_valid(stack[i].label);
}

void notify_screen_reset(void) {
    stack_ready = 0;

    for (int i = 0; i < NOTIFY_STACK_MAX; i++) {
        stack[i].panel = NULL;
        stack[i].label = NULL;
        stack[i].active = 0;
        stack[i].until = 0;
        stack[i].text[0] = '\0';
    }
}

static void stack_build(void) {
    if (stack_ready && slot_usable(0)) return;

    notify_screen_reset();

    if (!ui_pnl_message || !lv_obj_is_valid(ui_pnl_message)) return;

    stack[0].panel = ui_pnl_message;
    stack[0].label = ui_lbl_message;

    for (int i = 1; i < NOTIFY_STACK_MAX; i++)
        stack[i].panel = build_message_panel(&theme, &device, &stack[i].label);

    stack_ready = 1;
}

static void stack_layout(void) {
    const lv_coord_t base = -theme.footer.height - 5;
    lv_coord_t offset = 0;

    for (int i = 0; i < NOTIFY_STACK_MAX; i++) {
        if (!stack[i].active || !slot_usable(i)) continue;

        lv_obj_update_layout(stack[i].panel);
        lv_obj_set_y(stack[i].panel, base - offset);
        lv_obj_move_foreground(stack[i].panel);

        offset = offset + lv_obj_get_height(stack[i].panel) + 4;
    }
}

static void slot_clear(const int i) {
    stack[i].active = 0;
    stack[i].text[0] = '\0';
    stack[i].until = 0;

    if (slot_usable(i)) lv_obj_set_style_opa(stack[i].panel, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
}

static const uint32_t hold_choices[] = {tst_wait_s, tst_wait_m, tst_wait_l, 4000};

uint32_t notify_hold_ms(void) {
    const int choice = config.visual.notify_time;
    if (choice < 0 || choice >= (int) (sizeof(hold_choices) / sizeof(hold_choices[0]))) return tst_wait_m;

    return hold_choices[choice];
}

static uint32_t hold_for(const notify_level level) {
    const uint32_t base = notify_hold_ms();

    return level == notify_warning ? base + base / 2 : base;
}

static uint32_t level_colour(const notify_level level) {
    switch (level) {
        case notify_success:
            return theme.message.level_success;
        case notify_warning:
            return theme.message.level_warning;
        case notify_error:
            return theme.message.level_error;
        default:
            return theme.message.border;
    }
}

static void present(const notify_entry *entry) {
    if (entry->level == notify_error) {
        show_info_box(lang.generic.warning, entry->text, 0);
        return;
    }

    stack_build();
    if (!stack_ready) return;

    if (stack[NOTIFY_STACK_MAX - 1].active) slot_clear(NOTIFY_STACK_MAX - 1);

    for (int i = NOTIFY_STACK_MAX - 1; i > 0; i--) {
        stack[i].active = stack[i - 1].active;
        stack[i].until = stack[i - 1].until;
        snprintf(stack[i].text, sizeof(stack[i].text), "%s", stack[i - 1].text);

        if (!stack[i].active || !slot_usable(i)) continue;

        lv_label_set_text(stack[i].label, stack[i].text);
        lv_label_set_recolor(stack[i].label, 1);
        lv_obj_clear_flag(stack[i].panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(stack[i].panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_color(
            stack[i].panel, lv_obj_get_style_border_color(stack[i - 1].panel, LV_PART_MAIN), MU_OBJ_MAIN_DEFAULT
        );
    }

    snprintf(stack[0].text, sizeof(stack[0].text), "%s", entry->text);
    stack[0].active = 1;
    stack[0].until = entry->hold_ms ? lv_tick_get() + entry->hold_ms : 0;

    lv_label_set_text(stack[0].label, stack[0].text);
    lv_label_set_recolor(stack[0].label, 1);
    lv_obj_clear_flag(stack[0].panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(stack[0].panel, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(stack[0].panel, lv_color_hex(level_colour(entry->level)), MU_OBJ_MAIN_DEFAULT);

    stack_layout();
}

static int already_pending(const char *msg) {
    for (int i = 0; i < NOTIFY_STACK_MAX; i++)
        if (stack[i].active && strcmp(stack[i].text, msg) == 0) return 1;

    for (int i = 0; i < queue_count; i++)
        if (strcmp(queue[i].text, msg) == 0) return 1;

    return 0;
}

void notify_send_for(const notify_level level, const char *msg, const uint32_t hold_ms) {
    if (!msg || !*msg) return;
    if (already_pending(msg)) return;

    if (level == notify_error) {
        notify_entry entry = {level, {0}, hold_ms};
        snprintf(entry.text, sizeof(entry.text), "%s", msg);
        present(&entry);

        return;
    }

    if (!stack[0].active || !stack[NOTIFY_STACK_MAX - 1].active) {
        notify_entry entry = {level, {0}, hold_ms};
        snprintf(entry.text, sizeof(entry.text), "%s", msg);
        present(&entry);

        return;
    }

    if (queue_count >= NOTIFY_QUEUE_MAX) {
        memmove(&queue[0], &queue[1], sizeof(queue[0]) * (NOTIFY_QUEUE_MAX - 1));
        queue_count--;
    }

    queue[queue_count].level = level;
    queue[queue_count].hold_ms = hold_ms;

    snprintf(queue[queue_count].text, sizeof(queue[queue_count].text), "%s", msg);
    queue_count++;
}

void notify_send(const notify_level level, const char *msg) {
    notify_send_for(level, msg, hold_for(level));
}

static notify_level level_from_name(const char *name) {
    if (!strcasecmp(name, "success")) return notify_success;
    if (!strcasecmp(name, "warning")) return notify_warning;
    if (!strcasecmp(name, "error")) return notify_error;

    return notify_info;
}

static void drain_drop_file(void) {
    static unsigned seen = 0;

    if (notify_drop_changes == seen) return;
    seen = notify_drop_changes;

    if (!notify_drop_exists) return;

    FILE *f = fopen(NOTIFY_DROP, "r");
    if (!f) return;

    char line[MAX_BUFFER_SIZE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;

        char *text = strchr(line, '\t');

        if (!text) {
            notify_send(notify_info, line);
            continue;
        }

        *text++ = '\0';
        while (*text == ' ')
            text++;

        if (*text) notify_send(level_from_name(line), text);
    }

    fclose(f);
    remove(NOTIFY_DROP);
}

void notify_tick(void) {
    drain_drop_file();

    if (stack_ready && !slot_usable(0)) notify_screen_reset();

    if (stack_ready) {
        const uint32_t now = lv_tick_get();
        int expired = 0;

        for (int i = 0; i < NOTIFY_STACK_MAX; i++) {
            if (!stack[i].active || !stack[i].until) continue;
            if ((int32_t) (now - stack[i].until) < 0) continue;

            slot_clear(i);
            expired = 1;
        }

        if (expired) stack_layout();
    }

    if (queue_count == 0 || stack[0].active) return;

    const notify_entry next = queue[0];

    memmove(&queue[0], &queue[1], sizeof(queue[0]) * (NOTIFY_QUEUE_MAX - 1));
    queue_count--;

    present(&next);
}

void notify_reset(void) {
    queue_count = 0;

    if (!stack_ready) return;

    for (int i = 0; i < NOTIFY_STACK_MAX; i++)
        slot_clear(i);
}
