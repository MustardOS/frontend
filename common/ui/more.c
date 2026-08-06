#include "more.h"
#include "../language.h"

#define MORE_DISABLED_OPA 120

static const char *more_label(const more_id id) {
    switch (id) {
        case more_information:
            return lang.generic.information;
        case more_search:
            return lang.generic.search;
        case more_sort:
            return lang.generic.sort_order;
        case more_filter:
            return lang.generic.tag_sort;
        case more_location:
            return lang.generic.location;
        case more_top_level:
            return lang.generic.top_level;
        case more_remove:
            return lang.generic.remove;
        case more_help:
            return lang.generic.help;
        default:
            return NULL;
    }
}

void more_init(
    mux_dialogue *dlg, struct theme_config *t, lv_obj_t *parent, const more_entry *entries, const int count
) {
    if (!dlg || !entries) return;

    if (dlg->panel) {
        if (dlg->dim && lv_obj_is_valid(dlg->dim)) lv_obj_del(dlg->dim);
        if (lv_obj_is_valid(dlg->panel)) lv_obj_del(dlg->panel);

        dlg->dim = NULL;
        dlg->panel = NULL;
        dlg->title_label = NULL;
        dlg->description_label = NULL;
        dlg->option_count = 0;

        for (int i = 0; i < MUX_DIALOGUE_MAX_OPTIONS; i++)
            dlg->options[i] = NULL;
    }

    const char *labels[MUX_DIALOGUE_MAX_OPTIONS];
    int packed[MUX_DIALOGUE_MAX_OPTIONS];
    int used = 0;

    for (int id = 0; id < more_count && used < MUX_DIALOGUE_MAX_OPTIONS; id++) {
        for (int e = 0; e < count; e++) {
            if (entries[e].id != id) continue;

            const char *label = more_label(entries[e].id);
            if (!label) break;

            packed[used] = entries[e].enabled ? id : id + more_count;
            labels[used] = label;

            used++;
            break;
        }
    }

    dialogue_init(dlg, t, parent, lang.generic.actions, NULL, labels, used, lang.generic.select, lang.generic.cancel);

    for (int i = 0; i < used; i++) {
        dlg->option_data[i] = packed[i];

        if (packed[i] >= more_count && dlg->options[i])
            lv_obj_set_style_text_opa(dlg->options[i], MORE_DISABLED_OPA, MU_OBJ_MAIN_DEFAULT);
    }
}

more_id more_selected(const mux_dialogue *dlg) {
    if (!dlg || dlg->option_count <= 0) return more_count;
    if (dlg->selected < 0 || dlg->selected >= dlg->option_count) return more_count;

    return (more_id) (dlg->option_data[dlg->selected] % more_count);
}

int more_is_enabled(const mux_dialogue *dlg, const int index) {
    if (!dlg || index < 0 || index >= dlg->option_count) return 0;
    return dlg->option_data[index] < more_count;
}

void more_open(mux_more *m, struct theme_config *t, lv_obj_t *parent, const more_entry *entries, const int count) {
    if (!m) return;

    more_init(&m->dlg, t, parent, entries, count);
    if (m->dlg.option_count <= 0) return;

    m->active = 1;
    m->dlg.selected = 0;

    dialogue_show(&m->dlg);
    dialogue_refresh(&m->dlg, t);
}

int more_active(const mux_more *m) {
    return m && m->active;
}

void more_close(mux_more *m) {
    if (!m || !m->active) return;

    m->active = 0;
    dialogue_hide(&m->dlg);
}

more_id more_current(const mux_more *m) {
    return m ? more_selected(&m->dlg) : more_count;
}

int more_dpad(mux_more *m, struct theme_config *t, const int direction, const int should_fire) {
    if (!more_active(m)) return 0;

    dialogue_handle_dpad(&m->dlg, t, direction, should_fire);
    return 1;
}

int more_dpad_hold(mux_more *m, struct theme_config *t, const int direction, const int should_fire) {
    if (!more_active(m)) return 0;

    dialogue_handle_dpad_hold(&m->dlg, t, direction, should_fire);
    return 1;
}
