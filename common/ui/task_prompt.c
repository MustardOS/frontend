#include <string.h>
#include "task_prompt.h"
#include "../audio.h"
#include "../language.h"
#include "../log.h"

static struct theme_config *theme_ref = NULL;
static lv_obj_t *parent_ref = NULL;

static mux_dialogue dlg;
static int active = 0;

static char prompt_id[TASK_FIELD_MAX];
static char options[TASK_PROMPT_MAX_OPTIONS][TASK_FIELD_MAX];
static int option_count = 0;

void task_prompt_init(struct theme_config *t, lv_obj_t *parent) {
    theme_ref = t;
    parent_ref = parent;

    active = 0;
    option_count = 0;
    prompt_id[0] = '\0';

    memset(&dlg, 0, sizeof(dlg));
}

void task_prompt_reset(void) {
    theme_ref = NULL;
    parent_ref = NULL;

    active = 0;
    option_count = 0;
    prompt_id[0] = '\0';

    memset(&dlg, 0, sizeof(dlg));
}

int task_prompt_active(void) {
    return active;
}

static int safe_default(const char *fallback) {
    if (fallback && fallback[0]) {
        for (int i = 0; i < option_count; i++)
            if (strcasecmp(options[i], fallback) == 0) return i;
    }

    return option_count > 0 ? option_count - 1 : 0;
}

int task_prompt_show(const task_event *prompt) {
    if (!prompt || !theme_ref || !parent_ref) return -1;

    if (strcasecmp(prompt->prompt_type, "confirm") != 0 && strcasecmp(prompt->prompt_type, "choice") != 0) {
        LOG_WARN("task", "unsupported prompt type '%s'", prompt->prompt_type);
        return -1;
    }

    if (prompt->option_count <= 0) {
        LOG_WARN("task", "prompt '%s' arrived without any options", prompt->id);
        return -1;
    }

    option_count = prompt->option_count < TASK_PROMPT_MAX_OPTIONS ? prompt->option_count : TASK_PROMPT_MAX_OPTIONS;
    for (int i = 0; i < option_count; i++)
        snprintf(options[i], sizeof(options[i]), "%s", prompt->options[i]);

    snprintf(prompt_id, sizeof(prompt_id), "%s", prompt->id);

    if (dlg.panel) {
        if (dlg.dim && lv_obj_is_valid(dlg.dim)) lv_obj_del(dlg.dim);
        if (lv_obj_is_valid(dlg.panel)) lv_obj_del(dlg.panel);

        memset(&dlg, 0, sizeof(dlg));
    }

    const char *labels[TASK_PROMPT_MAX_OPTIONS];
    for (int i = 0; i < option_count; i++)
        labels[i] = options[i];

    dialogue_init(
        &dlg, theme_ref, parent_ref, prompt->title[0] ? prompt->title : lang.generic.confirm, prompt->message, labels,
        option_count, lang.generic.select, lang.generic.cancel
    );

    dialogue_show(&dlg);
    lv_obj_move_foreground(dlg.dim);
    lv_obj_move_foreground(dlg.panel);

    dlg.selected = safe_default(prompt->fallback);
    dialogue_refresh(&dlg, theme_ref);

    active = 1;
    return 0;
}

void task_prompt_hide(void) {
    if (!active) return;

    active = 0;
    dialogue_mark_silent(&dlg);
    dialogue_hide(&dlg);
}

int task_prompt_handle_a(void) {
    if (!active) return 0;

    play_sound(snd_confirm);

    const int index = dlg.selected >= 0 && dlg.selected < option_count ? dlg.selected : 0;
    task_exec_respond(prompt_id, options[index]);

    task_prompt_hide();
    return 1;
}

int task_prompt_handle_b(void) {
    if (!active) return 0;

    play_sound(snd_back);
    task_exec_respond(prompt_id, options[safe_default(NULL)]);

    task_prompt_hide();
    return 1;
}

int task_prompt_handle_dpad(const int direction) {
    if (!active) return 0;

    dialogue_handle_dpad(&dlg, theme_ref, direction, 1);
    return 1;
}

int task_prompt_handle_dpad_hold(const int direction) {
    if (!active) return 0;

    dialogue_handle_dpad_hold(&dlg, theme_ref, direction, 1);
    return 1;
}
