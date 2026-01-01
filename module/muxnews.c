#include "muxshare.h"
#include "../common/download.h"

#define NEWS_TEXT_BUF (MAX_BUFFER_SIZE * 4)

struct news_entry {
    const char *item;
    const char *name;
    const char *post;
    const char *help;
};

static const struct news_entry news_entries[] = {
        {
                .item = "general",
                .name = lang.MUXNEWS.GENERAL,
                .post = "https://community.muos.dev/posts/5076.json",
                .help = lang.MUXNEWS.HELP.GENERAL
        },
        {
                .item = "gotm",
                .name = lang.MUXNEWS.GOTM.TITLE,
                .post = "https://community.muos.dev/posts/5077.json",
                .help = lang.MUXNEWS.HELP.GOTM
        }
};

static const size_t news_entry_count = sizeof(news_entries) / sizeof(news_entries[0]);

static char cache_path[MAX_BUFFER_SIZE];
static char text_buffer[NEWS_TEXT_BUF];

static void show_help() {
    show_info_box(news_entries[current_item_index].name, news_entries[current_item_index].help, 0);
}

static int extract_ini_block(const char *raw, char *out) {
    const char *start = strstr(raw, "```ini");
    if (!start) return 0;

    start += 6;
    while (*start && *start != '\n') start++;
    if (*start == '\n') start++;

    const char *end = strstr(start, "```");
    if (!end) return 0;

    size_t len = end - start;
    if (len >= NEWS_TEXT_BUF) len = NEWS_TEXT_BUF - 1;

    memcpy(out, start, len);
    out[len] = '\0';

    return 1;
}

static void render_gotm(mini_t *ini) {
    static char intro[MAX_BUFFER_SIZE];
    static char c_title[MAX_BUFFER_SIZE];
    static char c_system[MAX_BUFFER_SIZE];
    static char c_region[MAX_BUFFER_SIZE];
    static char c_year[MAX_BUFFER_SIZE];
    static char l_title[MAX_BUFFER_SIZE];
    static char l_system[MAX_BUFFER_SIZE];
    static char l_winner[MAX_BUFFER_SIZE];
    static char l_score[MAX_BUFFER_SIZE];

    snprintf(intro, sizeof(intro), "%s", get_ini_string(ini, "global", "intro", ""));

    snprintf(c_title, sizeof(c_title), "%s", get_ini_string(ini, "current", "title", lang.GENERIC.UNKNOWN));
    snprintf(c_system, sizeof(c_system), "%s", get_ini_string(ini, "current", "system", lang.GENERIC.UNKNOWN));
    snprintf(c_region, sizeof(c_region), "%s", get_ini_string(ini, "current", "region", lang.GENERIC.UNKNOWN));
    snprintf(c_year, sizeof(c_year), "%s", get_ini_string(ini, "current", "year", lang.GENERIC.UNKNOWN));

    snprintf(l_title, sizeof(l_title), "%s", get_ini_string(ini, "last", "title", lang.GENERIC.UNKNOWN));
    snprintf(l_system, sizeof(l_system), "%s", get_ini_string(ini, "last", "system", lang.GENERIC.UNKNOWN));
    snprintf(l_winner, sizeof(l_winner), "%s", get_ini_string(ini, "last", "winner", lang.GENERIC.UNKNOWN));
    snprintf(l_score, sizeof(l_score), "%s", get_ini_string(ini, "last", "score", lang.GENERIC.UNKNOWN));

    snprintf(text_buffer, sizeof(text_buffer),
             "%s\n\n"
             "%s\n"
             "%s: %s (%s)\n"
             "%s: %s\n"
             "%s: %s\n\n"
             "%s\n"
             "%s: %s\n"
             "%s: %s\n"
             "%s: %s\n"
             "%s: %s\n",
             intro,
             lang.MUXNEWS.GOTM.CURRENT.TITLE,
             lang.MUXNEWS.GOTM.CURRENT.CONTENT, c_title, c_region,
             lang.MUXNEWS.GOTM.CURRENT.SYSTEM, c_system,
             lang.MUXNEWS.GOTM.CURRENT.YEAR, c_year,
             lang.MUXNEWS.GOTM.LAST.TITLE,
             lang.MUXNEWS.GOTM.LAST.CONTENT, l_title,
             lang.MUXNEWS.GOTM.LAST.SYSTEM, l_system,
             lang.MUXNEWS.GOTM.LAST.WINNER, l_winner,
             lang.MUXNEWS.GOTM.LAST.SCORE, l_score
    );

    show_info_box(lang.MUXNEWS.GOTM.TITLE, text_buffer, 1);
}

static void load_cached_entry(int result) {
    if (result != 0) {
        play_sound(SND_ERROR);
        toast_message(lang.MUXNEWS.ERROR, MEDIUM);
        return;
    }

    char *json_data = read_all_char_from(cache_path);
    if (!json_data) {
        play_sound(SND_ERROR);
        toast_message(lang.MUXNEWS.ERROR, MEDIUM);
        return;
    }

    struct json root = json_parse(json_data);
    if (!json_exists(root)) {
        free(json_data);
        play_sound(SND_ERROR);
        return;
    }

    struct json raw = json_object_get(root, "raw");
    if (!json_exists(raw) || json_type(raw) != JSON_STRING) {
        free(json_data);
        play_sound(SND_ERROR);
        return;
    }

    char raw_buf[NEWS_TEXT_BUF];
    json_string_copy(raw, raw_buf, sizeof(raw_buf));

    char ini_buf[NEWS_TEXT_BUF];
    if (extract_ini_block(raw_buf, ini_buf)) {
        const char *news_ini = OPT_SHARE_PATH "news/gotm.ini";

        write_text_to_file(news_ini, "w", CHAR, ini_buf);
        mini_t *ini = mini_try_load(news_ini);

        if (ini) {
            render_gotm(ini);
            mini_free(ini);
            remove(news_ini);
            free(json_data);
            return;
        }
    }

    show_info_box(news_entries[current_item_index].name, raw_buf, 1);
    free(json_data);
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, +1);
}

static void create_news_items(void) {
    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    for (size_t i = 0; i < news_entry_count; i++) {
        add_item(&items, &item_count, news_entries[i].name, news_entries[i].name, news_entries[i].item, ITEM);
    }

    for (size_t i = 0; i < item_count; i++) {
        ui_count++;

        lv_obj_t *pnl = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(pnl);

        lv_obj_t *lbl = lv_label_create(pnl);
        apply_theme_list_item(&theme, lbl, items[i].display_name);

        lv_obj_t *glyph = lv_img_create(pnl);
        apply_theme_list_glyph(&theme, glyph, mux_module, items[i].extra_data);

        lv_group_add_obj(ui_group, lbl);
        lv_group_add_obj(ui_group_glyph, glyph);
        lv_group_add_obj(ui_group_panel, pnl);

        apply_size_to_content(&theme, ui_pnlContent, lbl, glyph, items[i].name);
        apply_text_long_dot(&theme, ui_pnlContent, lbl);
    }
}

static void handle_a(void) {
    if (msgbox_active || !ui_count || download_in_progress) return;

    play_sound(SND_CONFIRM);

    create_directories(OPT_SHARE_PATH "news");

    snprintf(cache_path, sizeof(cache_path), OPT_SHARE_PATH "news/%s.json",
             news_entries[current_item_index].item);

    if (file_exist(cache_path)) remove(cache_path);

    set_download_callbacks(load_cached_entry);
    initiate_download(news_entries[current_item_index].post, cache_path, false, lang.MUXNEWS.DOWNLOAD);
}

static void handle_b(void) {
    if (download_in_progress || hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "news");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (download_in_progress || msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                1},
            {ui_lblNavA,      lang.GENERIC.READ, 1},
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {NULL, NULL,                         0}
    });

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxnews_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXNEWS.TITLE);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    create_news_items();

    init_elements();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    free_items(&items, &item_count);

    return 0;
}
