#include "muxshare.h"
#include "../common/download.h"

#define NEWS_TEXT_BUF (MAX_BUFFER_SIZE * 4)

#define NEWS_URL "https://community.muos.dev"
#define NEWS_INDEX_URL NEWS_URL "/c/device/32.json"
#define NEWS_TOPIC_URL_FMT NEWS_URL "/t/%d.json"
#define NEWS_POST_URL_FMT NEWS_URL "/posts/%d.json"

#define NEWS_CACHE_DIR OPT_SHARE_PATH "news"
#define NEWS_INDEX_PATH NEWS_CACHE_DIR "/index.json"
#define NEWS_TOPIC_PATH_FMT NEWS_CACHE_DIR "/topic_%d.json"
#define NEWS_POST_PATH_FMT NEWS_CACHE_DIR "/post_%d.json"

#define NEWS_TOPIC_LIST "topic_list.topics"
#define NEWS_POST_STREAM_ID "post_stream.posts.0.id"

static int *topic_ids;
static size_t topic_id_count;

static int pending_rebuild;
static int pending_show_topic;
static int pending_error;

static int requested_topic_id;
static char requested_topic_title[256];

static char *pending_index_json;

static char cache_path[MAX_BUFFER_SIZE];
static char text_buffer[NEWS_TEXT_BUF];

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
    static char c_emu[MAX_BUFFER_SIZE];
    static char l_title[MAX_BUFFER_SIZE];
    static char l_system[MAX_BUFFER_SIZE];
    static char l_winner[MAX_BUFFER_SIZE];
    static char l_score[MAX_BUFFER_SIZE];

    snprintf(intro, sizeof(intro), "%s", get_ini_string(ini, "global", "intro", ""));

    char *unk = "Unknown";

    snprintf(c_title, sizeof(c_title), "%s", get_ini_string(ini, "current", "title", unk));
    snprintf(c_system, sizeof(c_system), "%s", get_ini_string(ini, "current", "system", unk));
    snprintf(c_region, sizeof(c_region), "%s", get_ini_string(ini, "current", "region", unk));
    snprintf(c_year, sizeof(c_year), "%s", get_ini_string(ini, "current", "year", unk));
    snprintf(c_emu, sizeof(c_emu), "%s", get_ini_string(ini, "current", "emulator", unk));

    snprintf(l_title, sizeof(l_title), "%s", get_ini_string(ini, "last", "title", unk));
    snprintf(l_system, sizeof(l_system), "%s", get_ini_string(ini, "last", "system", unk));
    snprintf(l_winner, sizeof(l_winner), "%s", get_ini_string(ini, "last", "winner", unk));
    snprintf(l_score, sizeof(l_score), "%s", get_ini_string(ini, "last", "score", unk));

    snprintf(text_buffer, sizeof(text_buffer),
             "%s\n\n"
             "Current Month\n"
             "Title: %s (%s)\n"
             "System: %s\n"
             "Year: %s\n"
             "Emulator: %s\n\n"
             "Last Month\n"
             "Title: %s\n"
             "System: %s\n"
             "Winner: %s\n"
             "Score: %s\n",
             intro,
             c_title, c_region,
             c_system,
             c_year,
             c_emu,
             l_title,
             l_system,
             l_winner,
             l_score
    );

    show_info_box("GAME OF THE MONTH", text_buffer, 1);
}

static void free_topics(void) {
    if (topic_ids) {
        free(topic_ids);
        topic_ids = NULL;
    }

    topic_id_count = 0;
}

static void create_news_entries(void) {
    lv_obj_clean(ui_pnlContent);

    if (ui_group) lv_group_del(ui_group);
    if (ui_group_glyph) lv_group_del(ui_group_glyph);
    if (ui_group_panel) lv_group_del(ui_group_panel);

    ui_group = ui_group_glyph = ui_group_panel = NULL;
    ui_count = current_item_index = nav_moved = 0;

    reset_ui_groups();

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

    if (ui_count > 0) {
        lv_obj_update_layout(ui_pnlContent);
        lv_label_set_text(ui_lblScreenMessage, "");
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXNEWS.NONE);
        lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    nav_moved = 1;
}

static void parse_index_json(const char *json_data) {
    free_topics();

    free_items(&items, &item_count);
    items = NULL;
    item_count = 0;

    struct json topics = json_get(json_data, NEWS_TOPIC_LIST);
    if (!json_exists(topics) || json_type(topics) != JSON_ARRAY) return;

    size_t count = json_array_count(topics);
    if (count == 0) return;

    topic_ids = calloc(count, sizeof(int));
    if (!topic_ids) return;

    size_t out_i = 0;

    for (size_t i = 0; i < count; i++) {
        struct json t = json_array_get(topics, i);
        if (!json_exists(t) || json_type(t) != JSON_OBJECT) continue;

        struct json post_id = json_object_get(t, "id");
        struct json post_title = json_object_get(t, "title");
        struct json post_tags = json_object_get(t, "tags");

        if (!json_exists(post_id) || !json_exists(post_title)) continue;

        char title_buf[256];
        json_string_copy(post_title, title_buf, sizeof(title_buf));

        if (strcmp(title_buf, "On-Device News") == 0) continue;

        int id = json_int(post_id);
        if (id <= 0) continue;

        char tag_buf[128];
        char key_buf[64];

        if (json_exists(post_tags) &&
            json_type(post_tags) == JSON_ARRAY &&
            json_array_count(post_tags) > 0) {
            struct json t0 = json_array_get(post_tags, 0);
            json_string_copy(t0, tag_buf, sizeof(tag_buf));
        } else {
            snprintf(tag_buf, sizeof(tag_buf), "news");
        }

        snprintf(key_buf, sizeof(key_buf), "topic_%d", id);

        add_item(&items, &item_count, key_buf, title_buf, tag_buf, ITEM);
        topic_ids[out_i++] = id;
    }

    topic_id_count = out_i;
}

static void load_post_from_cache(int post_id, char *title) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), NEWS_POST_PATH_FMT, post_id);

    LOG_INFO(mux_module, "Loading Post: %s", path);

    char *json_data = read_all_char_from(path);
    if (!json_data) {
        play_sound(SND_ERROR);
        toast_message(lang.MUXNEWS.ERROR, MEDIUM);
        return;
    }

    struct json raw = json_get(json_data, "raw");
    if (!json_exists(raw) || json_type(raw) != JSON_STRING) {
        free(json_data);
        play_sound(SND_ERROR);
        toast_message(lang.MUXNEWS.ERROR, MEDIUM);
        return;
    }

    char raw_buf[NEWS_TEXT_BUF];
    json_string_copy(raw, raw_buf, sizeof(raw_buf));

    char ini_buf[NEWS_TEXT_BUF];
    if (extract_ini_block(raw_buf, ini_buf)) {
        const char *news_ini = NEWS_CACHE_DIR "/gotm.ini";

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

    show_info_box(str_toupper(title), raw_buf, 1);
    free(json_data);
}

static void load_topic_from_cache(int topic_id, char *title) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), NEWS_TOPIC_PATH_FMT, topic_id);

    LOG_INFO(mux_module, "Loading Topic: %s", path);

    char *json_data = read_all_char_from(path);
    if (!json_data) {
        play_sound(SND_ERROR);
        toast_message(lang.MUXNEWS.ERROR, MEDIUM);
        return;
    }

    struct json post_id = json_get(json_data, NEWS_POST_STREAM_ID);
    if (!json_exists(post_id)) {
        free(json_data);
        play_sound(SND_ERROR);
        toast_message(lang.MUXNEWS.ERROR, MEDIUM);
        return;
    }

    int pid = json_int(post_id);
    free(json_data);

    char post_path[MAX_BUFFER_SIZE];
    snprintf(post_path, sizeof(post_path), NEWS_POST_PATH_FMT, pid);

    if (file_exist(post_path)) {
        load_post_from_cache(pid, title);
        return;
    }

    char url[MAX_BUFFER_SIZE];
    snprintf(url, sizeof(url), NEWS_POST_URL_FMT, pid);

    initiate_download(url, post_path, false, lang.MUXNEWS.DOWNLOAD);
}

static void load_index_finished(int result) {
    if (result != 0) {
        pending_error = 1;
        return;
    }

    char *json_data = read_all_char_from(NEWS_INDEX_PATH);
    if (!json_data) return;

    if (pending_index_json) {
        free(pending_index_json);
        pending_index_json = NULL;
    }

    pending_index_json = json_data;
    pending_rebuild = 1;
}

static void load_topic_finished(int result) {
    if (result != 0) {
        pending_error = 1;
        return;
    }

    pending_show_topic = 1;
}

static void refresh_index(void) {
    create_directories(NEWS_CACHE_DIR, 0);

    snprintf(cache_path, sizeof(cache_path), "%s", NEWS_INDEX_PATH);
    if (file_exist(cache_path)) remove(cache_path);

    set_download_callbacks(load_index_finished);
    initiate_download(NEWS_INDEX_URL, cache_path, true, lang.MUXNEWS.DOWNLOAD);
}

static void refresh_topic(int topic_id) {
    create_directories(NEWS_CACHE_DIR, 0);

    snprintf(cache_path, sizeof(cache_path), NEWS_TOPIC_PATH_FMT, topic_id);
    if (file_exist(cache_path)) remove(cache_path);

    char url[MAX_BUFFER_SIZE];
    snprintf(url, sizeof(url), NEWS_TOPIC_URL_FMT, topic_id);

    set_download_callbacks(load_topic_finished);
    initiate_download(url, cache_path, false, lang.MUXNEWS.DOWNLOAD);
}

static void list_nav_move(int steps, int direction) {
    gen_step_movement(steps, direction, true, 0);
}

static void list_nav_prev(int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (download_in_progress || msgbox_active || hold_call) return;
    if (!ui_count || (size_t) current_item_index >= topic_id_count) return;

    requested_topic_id = topic_ids[current_item_index];
    snprintf(requested_topic_title, sizeof(requested_topic_title), "%s",
             items[current_item_index].display_name);

    snprintf(cache_path, sizeof(cache_path), NEWS_TOPIC_PATH_FMT, requested_topic_id);
    if (file_exist(cache_path)) {
        play_sound(SND_CONFIRM);
        load_topic_from_cache(requested_topic_id, requested_topic_title);
        return;
    }

    if (device.BOARD.HAS_NETWORK && is_network_connected()) {
        play_sound(SND_CONFIRM);
        toast_message(lang.MUXNEWS.OPEN, MEDIUM);
        refresh_topic(requested_topic_id);
    } else {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.NEED_CONNECT, MEDIUM);
    }
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

static void handle_x(void) {
    if (download_in_progress || msgbox_active || hold_call) return;

    if (device.BOARD.HAS_NETWORK && is_network_connected()) {
        play_sound(SND_CONFIRM);

        delete_files_of_type(NEWS_CACHE_DIR, ".ini", NULL, 0);
        delete_files_of_type(NEWS_CACHE_DIR, ".json", NULL, 0);

        refresh_index();
    } else {
        play_sound(SND_ERROR);
        toast_message(lang.GENERIC.NEED_CONNECT, MEDIUM);
    }

    if (ui_count > 0) {
        lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void init_elements(void) {
    adjust_gen_panel();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                0},
            {ui_lblNavA,      lang.GENERIC.READ, 0},
            {ui_lblNavBGlyph, "",                0},
            {ui_lblNavB,      lang.GENERIC.BACK, 0},
            {NULL, NULL,                         0}
    });

    if (device.BOARD.HAS_NETWORK) {
        setup_nav((struct nav_bar[]) {
                {ui_lblNavXGlyph, "",                   0},
                {ui_lblNavX,      lang.GENERIC.REFRESH, 0},
                {NULL, NULL,                            0}
        });
    }

    overlay_display();
}

static void ui_refresh_task() {
    if (pending_error) {
        pending_error = 0;
        play_sound(SND_ERROR);
        toast_message(lang.MUXNEWS.ERROR, MEDIUM);
    }

    if (pending_rebuild) {
        pending_rebuild = 0;

        if (pending_index_json) {
            parse_index_json(pending_index_json);
            free(pending_index_json);
            pending_index_json = NULL;
        }

        create_news_entries();

        if (ui_count > 0) {
            current_item_index = 0;
            list_nav_next(0);
            lv_obj_update_layout(ui_pnlContent);
        }
    }

    if (pending_show_topic) {
        pending_show_topic = 0;

        snprintf(cache_path, sizeof(cache_path), NEWS_TOPIC_PATH_FMT, requested_topic_id);
        if (file_exist(cache_path)) {
            load_topic_from_cache(requested_topic_id, requested_topic_title);
        } else {
            play_sound(SND_ERROR);
            toast_message(lang.MUXNEWS.ERROR, MEDIUM);
        }
    }

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_gen_panel();

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
    init_elements();

    if (file_exist(NEWS_INDEX_PATH)) {
        char *json_data = read_all_char_from(NEWS_INDEX_PATH);
        if (json_data) {
            parse_index_json(json_data);
            free(json_data);
        }
    }

    if (device.BOARD.HAS_NETWORK && is_network_connected()) refresh_index();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
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

    free_topics();
    free_items(&items, &item_count);

    return 0;
}
