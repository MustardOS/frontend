#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "../../common/audio.h"
#include "../../common/input.h"
#include "../../common/randname.h"
#include "../../common/ui/osk.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"
#include "deck.h"

typedef enum {
    prow_controller = 0,
    prow_layout,
    prow_controls,
    prow_priority,
    prow_core_device,
    prow_stick_dpad,
    prow_button_mapping,
    prow_macros,
    prow_reset_port,
    prow_kind_count
} port_row_kind;

enum { port_row_limit = 7 };

static const char *row_labels[port_row_limit];
static const char *row_glyphs[port_row_limit];
static const char *row_help[port_row_limit];
static port_row_kind row_kinds[port_row_limit];
static int row_count;

static int active_port = 0;

static submenu port_self[MUX_INPUT_PORT_COUNT];
static submenu_def port_def;

typedef enum { deck_naming_off = 0, deck_naming_new, deck_naming_rename } deck_naming_mode;

static deck_naming_mode deck_naming = deck_naming_off;
static mux_dialogue deck_manage_dlg;
static mux_dialogue deck_delete_dlg;
static lv_obj_t *deck_entry_panel;
static lv_obj_t *deck_entry_text;
static uint64_t deck_modal_prev_mask;
static nav_repeat_t rpt_deck_osk_up;
static nav_repeat_t rpt_deck_osk_down;
static nav_repeat_t rpt_deck_osk_left;
static nav_repeat_t rpt_deck_osk_right;
static nav_repeat_t rpt_deck_osk_backspace;

enum { deck_manage_rename = 0, deck_manage_delete };

static int port_wears_deck(void) {
    return session_settings_port_deck_position(active_port) >= 0;
}

static void add_row(const port_row_kind kind, const char *label, const char *glyph, const char *help) {
    if (row_count >= port_row_limit) return;

    row_kinds[row_count] = kind;
    row_labels[row_count] = label;
    row_glyphs[row_count] = glyph;
    row_help[row_count] = help;
    row_count++;
}

static void build_rows(void) {
    row_count = 0;

    add_row(
        prow_controller, lang.muxretro.settings_screen.controller, "controller", lang.muxretro.help.port.controller
    );

    if (session_settings_port_is_deck(active_port)) {
        add_row(prow_layout, lang.muxretro.settings_screen.port_deck, "deck", lang.muxretro.help.port.deck);

        if (port_wears_deck()) {
            add_row(
                prow_controls, lang.muxretro.settings_screen.port_deck_route, "deckroute",
                lang.muxretro.help.port.deck_route
            );
            add_row(
                prow_priority, lang.muxretro.settings_screen.port_deck_priority, "deckpriority",
                lang.muxretro.help.port.deck_priority
            );
        }
    } else {
        add_row(
            prow_core_device, lang.muxretro.settings_screen.core_device, "coredevice",
            lang.muxretro.help.port.core_device
        );
        add_row(
            prow_stick_dpad, lang.muxretro.settings_screen.stick_dpad, "sticksensitivity",
            lang.muxretro.help.port.stick_dpad
        );
    }

    add_row(
        prow_button_mapping, lang.muxretro.settings_screen.button_mapping, "buttonmapping",
        lang.muxretro.help.port.button_mapping
    );
    add_row(prow_macros, lang.muxretro.settings_screen.macros, "macro", lang.muxretro.help.port.macros);
    add_row(prow_reset_port, lang.muxretro.settings_screen.reset_port, "portreset", lang.muxretro.help.port.reset_port);

    port_def.row_count = row_count;
}

static port_row_kind row_kind(const int index) {
    return index >= 0 && index < row_count ? row_kinds[index] : prow_kind_count;
}

static int row_index_of(const port_row_kind kind) {
    for (int i = 0; i < row_count; i++) {
        if (row_kinds[i] == kind) return i;
    }

    return 0;
}

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (row_kind(index)) {
        case prow_controller:
            session_settings_port_summary(active_port, buf, buf_len);
            break;
        case prow_layout:
            session_settings_port_deck_summary(active_port, buf, buf_len);
            break;
        case prow_controls:
            session_settings_port_deck_route_summary(active_port, buf, buf_len);
            break;
        case prow_priority:
            session_settings_port_deck_priority_summary(active_port, buf, buf_len);
            break;
        case prow_core_device:
            session_settings_port_device_summary(active_port, buf, buf_len);
            break;
        case prow_stick_dpad:
            session_settings_stick_dpad_summary(active_port, buf, buf_len);
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (row_kind(index)) {
        case prow_controller: {
            const int was_deck = session_settings_port_is_deck(active_port);
            session_settings_cycle_port_controller(active_port, direction);

            if (session_settings_port_is_deck(active_port) != was_deck) {
                build_rows();
                submenu_reopen_at(&port_self[active_port], row_index_of(prow_controller));
            }
            break;
        }
        case prow_layout: {
            const int was_worn = port_wears_deck();
            session_settings_cycle_port_deck(active_port, direction);

            if (port_wears_deck() != was_worn) {
                build_rows();
                submenu_reopen_at(&port_self[active_port], row_index_of(prow_layout));
                break;
            }

            submenu_refresh_values(&port_self[active_port]);
            break;
        }
        case prow_controls:
            session_settings_cycle_port_deck_route(active_port, direction);
            break;
        case prow_priority:
            session_settings_cycle_port_deck_priority(active_port, direction);
            break;
        case prow_core_device:
            session_settings_cycle_port_device(active_port, direction);
            break;
        case prow_stick_dpad:
            session_settings_cycle_stick_dpad(active_port, direction);
            break;
        default:
            break;
    }
}

static int row_is_action(const int index) {
    const port_row_kind kind = row_kind(index);
    return kind == prow_button_mapping || kind == prow_macros || kind == prow_reset_port;
}

static int row_can_cycle(const int index) {
    switch (row_kind(index)) {
        case prow_layout:
            return deck_count > 0;
        case prow_button_mapping:
        case prow_macros:
        case prow_reset_port:
            return 0;
        default:
            return 1;
    }
}

static const char *row_extra_label(const int index) {
    return row_kind(index) == prow_layout ? lang.muxretro.deck.new : NULL;
}

static const char *row_y_label(const int index) {
    if (row_kind(index) != prow_layout || !port_wears_deck()) return NULL;
    return lang.muxretro.deck.manage;
}

static uint64_t deck_modal_nav_mask(void) {
    return nav_dir_bits() | (mux_input_pressed(mux_input_a) ? BIT(4) : 0)
           | (mux_input_pressed(mux_input_b) ? BIT(5) : 0) | (mux_input_pressed(mux_input_x) ? BIT(6) : 0)
           | (mux_input_pressed(mux_input_y) ? BIT(7) : 0) | (mux_input_pressed(mux_input_select) ? BIT(8) : 0)
           | (mux_input_pressed(mux_input_start) ? BIT(9) : 0) | nav_mask_page();
}

static void deck_osk_objects(void) {
    deck_entry_panel = lv_obj_create(ui_screen);
    lv_obj_set_width(deck_entry_panel, device.mux.width);
    lv_obj_set_height(deck_entry_panel, device.mux.height);
    lv_obj_set_align(deck_entry_panel, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(deck_entry_panel, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(deck_entry_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(deck_entry_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(deck_entry_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(deck_entry_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(deck_entry_panel, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(deck_entry_panel, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(deck_entry_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(deck_entry_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(deck_entry_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(deck_entry_panel, 5, MU_OBJ_MAIN_DEFAULT);

    deck_entry_text = lv_textarea_create(deck_entry_panel);
    lv_obj_set_width(deck_entry_text, device.mux.width * 5 / 6);
    lv_obj_set_height(deck_entry_text, LV_SIZE_CONTENT);
    lv_obj_set_align(deck_entry_text, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(deck_entry_text, DECK_NAME_MAX - 1);
    lv_textarea_set_one_line(deck_entry_text, 1);
    lv_obj_set_style_radius(deck_entry_text, theme.osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(deck_entry_text, lv_color_hex(theme.osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(deck_entry_text, theme.osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(deck_entry_text, 2, MU_OBJ_MAIN_DEFAULT);
}

static void deck_open_osk(const deck_naming_mode mode, const char *seed) {
    deck_naming = mode;
    init_osk(deck_entry_panel, deck_entry_text, 0, 0, DECK_NAME_MAX - 1);
    key_show = 1;
    osk_show(deck_entry_panel);
    lv_textarea_set_text(deck_entry_text, seed ? seed : "");
    deck_modal_prev_mask = deck_modal_nav_mask();
}

static void deck_close_osk(void) {
    deck_naming = deck_naming_off;
    key_show = 0;
    reset_osk(key_entry);
    lv_textarea_set_text(deck_entry_text, "");
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(deck_entry_panel);
}

static void deck_cancel_osk(void) {
    deck_naming = deck_naming_off;
    close_osk(key_entry, ui_group, deck_entry_text, deck_entry_panel);
    port_self[active_port].prev_nav_mask = deck_modal_nav_mask();
}

static void deck_start_new(const int index) {
    (void) index;

    if (deck_count >= DECK_MAX) {
        pause_menu_show_toast(lang.muxretro.deck.limit_reached);
        return;
    }

    char seed[RANDNAME_MAX_LEN];
    randname_generate_with_separator(seed, sizeof(seed), " ");
    deck_open_osk(deck_naming_new, seed);
}

static void deck_start_manage(const int index) {
    (void) index;

    if (!port_wears_deck()) return;
    dialogue_open(&deck_manage_dlg, &theme);
    deck_modal_prev_mask = deck_modal_nav_mask();
}

static void deck_trim(char *name) {
    const char *start = name;
    while (*start == ' ' || *start == '\t')
        start++;
    if (start != name) memmove(name, start, strlen(start) + 1);

    size_t length = strlen(name);
    while (length > 0 && (name[length - 1] == ' ' || name[length - 1] == '\t'))
        name[--length] = '\0';
}

static void deck_reopen_at_layout(void) {
    build_rows();
    submenu_reopen_at(&port_self[active_port], row_index_of(prow_layout));
}

static void deck_finish_name(void) {
    const deck_naming_mode mode = deck_naming;
    char entered[DECK_NAME_MAX];
    const char *text = lv_textarea_get_text(deck_entry_text);

    snprintf(entered, sizeof(entered), "%s", text ? text : "");
    deck_trim(entered);
    deck_close_osk();

    if (!entered[0]) {
        pause_menu_show_toast(lang.muxretro.deck.name_needed);
        port_self[active_port].prev_nav_mask = deck_modal_nav_mask();
        return;
    }

    if (mode == deck_naming_new) {
        const int position = decks_create(entered);
        if (position < 0) {
            pause_menu_show_toast(lang.muxretro.deck.limit_reached);
        } else {
            session_settings_set_port_deck(active_port, deck_list[position].index);
            pause_menu_show_toast(lang.muxretro.deck.created);
        }
    } else {
        const int position = session_settings_port_deck_position(active_port);
        if (position >= 0 && decks_rename(position, entered) == 0) pause_menu_show_toast(lang.muxretro.deck.renamed);
    }

    deck_reopen_at_layout();
}

static void deck_tick_osk(const uint64_t edge, const uint64_t mask) {
    if (edge & BIT(9)) {
        deck_finish_name();
        return;
    }

    if (edge & BIT(4)) {
        play_sound(snd_keypress);
        const char *key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
        if (key && strcasecmp(key, OSK_DONE) == 0)
            deck_finish_name();
        else
            lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
        return;
    }

    if (edge & BIT(6)) {
        deck_cancel_osk();
        return;
    }

    if (edge & BIT(7)) {
        key_space(deck_entry_text);
        return;
    }

    if (edge & BIT(8)) {
        key_clear(deck_entry_text);
        return;
    }

    if (edge & NAV_PAGE_UP_BIT) {
        key_swap_back();
        return;
    }

    if (edge & NAV_PAGE_DOWN_BIT) {
        key_swap();
        return;
    }

    const uint32_t now = SDL_GetTicks();
    const int up = nav_repeat_step(&rpt_deck_osk_up, edge & BIT(0), mask & BIT(0), 1, now);
    const int down = nav_repeat_step(&rpt_deck_osk_down, edge & BIT(1), mask & BIT(1), 1, now);
    const int left = nav_repeat_step(&rpt_deck_osk_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int right = nav_repeat_step(&rpt_deck_osk_right, edge & BIT(3), mask & BIT(3), 1, now);
    const int backspace = nav_repeat_step(&rpt_deck_osk_backspace, edge & BIT(5), mask & BIT(5), 1, now);

    if (up)
        key_up();
    else if (down)
        key_down();
    else if (left)
        key_left();
    else if (right)
        key_right();
    else if (backspace)
        key_backspace(deck_entry_text);
}

static void deck_apply_delete(void) {
    const int position = session_settings_port_deck_position(active_port);
    if (position < 0) return;

    session_settings_clear_deck_references(deck_list[position].index);
    decks_delete(position);
    pause_menu_show_toast(lang.muxretro.deck.deleted);
    deck_reopen_at_layout();
}

static int deck_modal_tick(void) {
    if (deck_naming == deck_naming_off && !dialogue_active(&deck_manage_dlg) && !dialogue_active(&deck_delete_dlg))
        return 0;

    const uint64_t mask = deck_modal_nav_mask();
    const uint64_t edge = mask & ~deck_modal_prev_mask;
    deck_modal_prev_mask = mask;

    if (deck_naming != deck_naming_off) {
        deck_tick_osk(edge, mask);
        return 1;
    }

    if (dialogue_active(&deck_manage_dlg)) {
        if (edge & (BIT(0) | BIT(1))) {
            dialogue_handle_dpad(&deck_manage_dlg, &theme, edge & BIT(1) ? 1 : -1, 1);
        } else if (edge & BIT(4)) {
            const int choice = deck_manage_dlg.selected;
            dialogue_dismiss(&deck_manage_dlg);

            if (choice == deck_manage_rename) {
                const int position = session_settings_port_deck_position(active_port);
                deck_open_osk(deck_naming_rename, position >= 0 ? deck_list[position].name : NULL);
            } else {
                dialogue_open(&deck_delete_dlg, &theme);
                deck_modal_prev_mask = deck_modal_nav_mask();
            }
        } else if (edge & BIT(5)) {
            dialogue_mark_cancelled(&deck_manage_dlg);
            dialogue_dismiss(&deck_manage_dlg);
            port_self[active_port].prev_nav_mask = mask;
        }
        return 1;
    }

    if (edge & (BIT(0) | BIT(1))) {
        dialogue_handle_dpad(&deck_delete_dlg, &theme, edge & BIT(1) ? 1 : -1, 1);
    } else if (edge & BIT(4)) {
        const mux_confirm_opt option = (mux_confirm_opt) deck_delete_dlg.selected;
        dialogue_dismiss(&deck_delete_dlg);

        if (option == mux_confirm_yep) {
            deck_apply_delete();
        } else {
            port_self[active_port].prev_nav_mask = mask;
        }
    } else if (edge & BIT(5)) {
        dialogue_mark_cancelled(&deck_delete_dlg);
        dialogue_dismiss(&deck_delete_dlg);
        port_self[active_port].prev_nav_mask = mask;
    }

    return 1;
}

static void row_action(const int index) {
    switch (row_kind(index)) {
        case prow_button_mapping:
            button_mapping_menu_open(active_port);
            break;
        case prow_macros:
            macros_menu_open(active_port);
            break;
        case prow_reset_port:
            session_settings_reset_input_port(active_port);
            build_rows();
            submenu_reopen_at(&port_self[active_port], row_index_of(prow_controller));
            break;
        default:
            break;
    }
}

static int child_tick(void) {
    if (deck_modal_tick()) return 1;

    if (button_mapping_menu_is_active()) {
        button_mapping_menu_tick();
        return 1;
    }

    if (macros_menu_is_active()) {
        macros_menu_tick();
        return 1;
    }

    return 0;
}

static void closed(void) {
    decks_flush();
    input_menu_reopen_port(active_port);
}

static submenu_def port_def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .help = row_help,
    .row_count = port_row_limit,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .row_can_cycle = row_can_cycle,
    .row_is_action = row_is_action,
    .extra_label = row_extra_label,
    .extra_action = deck_start_new,
    .y_label = row_y_label,
    .y_action = deck_start_manage,
    .action = row_action,
    .child_tick = child_tick,
    .closed = closed,
    .save_title = lang.muxretro.save.input_port_title,
    .save_desc = lang.muxretro.save.input_port_desc,
};

void input_port_menu_init_all(void) {
    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++)
        submenu_init(&port_self[i], &port_def);

    static const char *manage_options[2];
    manage_options[deck_manage_rename] = lang.muxretro.deck.rename;
    manage_options[deck_manage_delete] = lang.muxretro.deck.delete;
    dialogue_init(
        &deck_manage_dlg, &theme, ui_screen, lang.muxretro.deck.manage_title, lang.muxretro.deck.manage_desc,
        manage_options, 2, lang.generic.select, lang.generic.cancel
    );

    dialogue_init_confirm(
        &deck_delete_dlg, &theme, ui_screen, lang.muxretro.deck.delete_title, lang.muxretro.deck.delete_desc,
        lang.muxretro.deck.delete, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );
    deck_delete_dlg.safe_default = mux_confirm_nah;

    deck_osk_objects();

    button_mapping_menu_init_all();
    macros_menu_init();
}

void input_port_menu_open(const int port) {
    if (port < 0 || port >= MUX_INPUT_PORT_COUNT) return;

    active_port = port;
    build_rows();
    submenu_open(&port_self[port]);
}

int input_port_menu_is_active(void) {
    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++)
        if (submenu_is_active(&port_self[i])) return 1;
    return 0;
}

void input_port_menu_tick(void) {
    for (int i = 0; i < MUX_INPUT_PORT_COUNT; i++) {
        if (submenu_is_active(&port_self[i])) {
            submenu_tick(&port_self[i]);
            return;
        }
    }
}

void input_port_menu_reopen_button_mapping(const int port) {
    active_port = port;
    build_rows();
    submenu_reopen_at(&port_self[port], row_index_of(prow_button_mapping));
}

void input_port_menu_reopen_macros(const int port) {
    active_port = port;
    build_rows();
    submenu_reopen_at(&port_self[port], row_index_of(prow_macros));
}
