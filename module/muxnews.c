#include "muxshare.h"
#include "../common/download.h"

#define NEWS_TEXT_BUF (MAX_BUFFER_SIZE * 4)

#define NEWS_URL           "https://community.muos.dev"
#define NEWS_INDEX_URL     NEWS_URL "/c/device/32.json"
#define NEWS_TOPIC_URL_FMT NEWS_URL "/t/%d.json"
#define NEWS_POST_URL_FMT  NEWS_URL "/posts/%d.json"

#define NEWS_CACHE_DIR      OPT_SHARE_PATH "news"
#define NEWS_INDEX_PATH     NEWS_CACHE_DIR "/index.json"
#define NEWS_TOPIC_PATH_FMT NEWS_CACHE_DIR "/topic_%d.json"
#define NEWS_POST_PATH_FMT  NEWS_CACHE_DIR "/post_%d.json"

#define NEWS_TOPIC_LIST     "topic_list.topics"
#define NEWS_POST_STREAM_ID "post_stream.posts.0.id"

static int *topic_ids;
static size_t topic_id_count;

static int pending_rebuild;
static int pending_show_topic;
static int pending_error;
static int pending_show_post;
static char *pending_index_json;

static int requested_post_id;
static int requested_topic_id;
static char requested_topic_title[256];

static char cache_path[MAX_BUFFER_SIZE];
static char text_buffer[NEWS_TEXT_BUF];

static int extract_ini_block(const char *raw, char *out) {
    const char *start = strstr(raw, "```ini");
    if (!start) return 0;

    start += 6;
    while (*start && *start != '\n')
        start++;
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

    const char *unk = "Unknown";

    snprintf(c_title, sizeof(c_title), "%s", get_ini_string(ini, "current", "title", unk));
    snprintf(c_system, sizeof(c_system), "%s", get_ini_string(ini, "current", "system", unk));
    snprintf(c_region, sizeof(c_region), "%s", get_ini_string(ini, "current", "region", unk));
    snprintf(c_year, sizeof(c_year), "%s", get_ini_string(ini, "current", "year", unk));
    snprintf(c_emu, sizeof(c_emu), "%s", get_ini_string(ini, "current", "emulator", unk));

    snprintf(l_title, sizeof(l_title), "%s", get_ini_string(ini, "last", "title", unk));
    snprintf(l_system, sizeof(l_system), "%s", get_ini_string(ini, "last", "system", unk));
    snprintf(l_winner, sizeof(l_winner), "%s", get_ini_string(ini, "last", "winner", unk));
    snprintf(l_score, sizeof(l_score), "%s", get_ini_string(ini, "last", "score", unk));

    snprintf(
        text_buffer, sizeof(text_buffer),
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
        intro, c_title, c_region, c_system, c_year, c_emu, l_title, l_system, l_winner, l_score
    );

    show_info_box("GAME OF THE MONTH", text_buffer, 1);
}

static void load_post_finished(const int result) {
    if (result != 0) {
        pending_error = 1;
        return;
    }

    pending_show_post = 1;
}

static void free_topics(void) {
    if (topic_ids) {
        free(topic_ids);
        topic_ids = NULL;
    }

    topic_id_count = 0;
}

static void create_news_entries(void) {
    lv_obj_clean(ui_pnl_content);

    if (ui_group) lv_group_del(ui_group);
    if (ui_group_glyph) lv_group_del(ui_group_glyph);
    if (ui_group_panel) lv_group_del(ui_group_panel);

    ui_group = ui_group_glyph = ui_group_panel = NULL;
    ui_count_static = current_item_index = nav_moved = 0;

    reset_ui_groups();

    for (size_t i = 0; i < item_count; i++) {
        ui_count_static++;

        lv_obj_t *pnl = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(pnl);

        lv_obj_t *lbl = lv_label_create(pnl);
        apply_theme_list_item(&theme, lbl, items[i].display_name);

        lv_obj_t *glyph = lv_img_create(pnl);
        apply_theme_list_glyph(&theme, glyph, mux_module, items[i].extra_data);

        lv_group_add_obj(ui_group, lbl);
        lv_group_add_obj(ui_group_glyph, glyph);
        lv_group_add_obj(ui_group_panel, pnl);

        apply_size_to_content(&theme, ui_pnl_content, lbl, glyph, items[i].display_name);
        apply_text_long_dot(&theme, lbl);
    }

    if (ui_count_static > 0) {
        lv_obj_update_layout(ui_pnl_content);
        lv_label_set_text(ui_lbl_screen_message, "");
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_label_set_text(ui_lbl_screen_message, lang.muxnews.none);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    nav_moved = 1;
}

static void parse_index_json(const char *json_data) {
    free_topics();

    free_items(&items, &item_count);
    items = NULL;
    item_count = 0;

    const struct json topics = json_get(json_data, NEWS_TOPIC_LIST);
    if (!json_exists(topics) || json_type(topics) != JSON_ARRAY) return;

    size_t count = json_array_count(topics);
    if (count == 0) return;
    if (count > MAX_MANIFEST_ITEMS) {
        LOG_WARN(mux_module, "News index has %zu items, capping at %d", count, MAX_MANIFEST_ITEMS);
        count = MAX_MANIFEST_ITEMS;
    }

    topic_ids = calloc(count, sizeof(int));
    if (!topic_ids) return;

    size_t out_i = 0;

    for (size_t i = 0; i < count; i++) {
        const struct json t = json_array_get(topics, i);
        if (!json_exists(t) || json_type(t) != JSON_OBJECT) continue;

        const struct json post_id = json_object_get(t, "id");
        const struct json post_title = json_object_get(t, "title");
        const struct json post_tags = json_object_get(t, "tags");

        if (!json_exists(post_id) || !json_exists(post_title)) continue;

        char title_buf[256];
        json_string_copy(post_title, title_buf, sizeof(title_buf));

        if (strcmp(title_buf, "On-Device News") == 0) continue;

        const int id = json_int(post_id);
        if (id <= 0) continue;

        char tag_buf[128];
        char key_buf[64];

        if (json_exists(post_tags) && json_type(post_tags) == JSON_ARRAY && json_array_count(post_tags) > 0) {
            const struct json t0 = json_array_get(post_tags, 0);
            if (json_type(t0) == JSON_OBJECT) {
                const struct json slug = json_object_get(t0, "slug");
                if (json_exists(slug) && json_type(slug) == JSON_STRING) {
                    json_string_copy(slug, tag_buf, sizeof(tag_buf));
                } else {
                    snprintf(tag_buf, sizeof(tag_buf), "%s", "news");
                }
            } else if (json_type(t0) == JSON_STRING) {
                json_string_copy(t0, tag_buf, sizeof(tag_buf));
            } else {
                snprintf(tag_buf, sizeof(tag_buf), "%s", "news");
            }
        } else {
            snprintf(tag_buf, sizeof(tag_buf), "%s", "news");
        }

        snprintf(key_buf, sizeof(key_buf), "topic_%d", id);

        add_item(&items, &item_count, key_buf, title_buf, tag_buf, ITEM);
        topic_ids[out_i++] = id;
    }

    topic_id_count = out_i;
}

static void load_post_from_cache(const int post_id, const char *title) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), NEWS_POST_PATH_FMT, post_id);

    LOG_INFO(mux_module, "Loading Post: %s", path);

    char *json_data = read_all_char_from(path);
    if (!json_data) {
        play_sound(snd_error);
        toast_message(lang.muxnews.error, tst_wait_m);
        return;
    }

    const struct json raw = json_get(json_data, "raw");
    if (!json_exists(raw) || json_type(raw) != JSON_STRING) {
        free(json_data);
        play_sound(snd_error);
        toast_message(lang.muxnews.error, tst_wait_m);
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

static void load_topic_from_cache(const int topic_id, const char *title) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), NEWS_TOPIC_PATH_FMT, topic_id);

    LOG_INFO(mux_module, "Loading Topic: %s", path);

    char *json_data = read_all_char_from(path);
    if (!json_data) {
        play_sound(snd_error);
        toast_message(lang.muxnews.error, tst_wait_m);
        return;
    }

    const struct json post_id = json_get(json_data, NEWS_POST_STREAM_ID);
    if (!json_exists(post_id)) {
        free(json_data);
        play_sound(snd_error);
        toast_message(lang.muxnews.error, tst_wait_m);
        return;
    }

    const int pid = json_int(post_id);
    free(json_data);

    char post_path[MAX_BUFFER_SIZE];
    snprintf(post_path, sizeof(post_path), NEWS_POST_PATH_FMT, pid);

    if (file_exist(post_path)) {
        load_post_from_cache(pid, title);
        return;
    }

    requested_post_id = pid;

    char url[MAX_BUFFER_SIZE];
    snprintf(url, sizeof(url), NEWS_POST_URL_FMT, pid);

    set_download_callbacks(load_post_finished);
    initiate_download(url, post_path, 0, lang.muxnews.download);
}

static void load_index_finished(const int result) {
    if (result != 0) {
        pending_error = 1;
        return;
    }

    char *json_data = read_all_char_from(NEWS_INDEX_PATH);
    if (!json_data) return;

    if (strlen(json_data) > MAX_MANIFEST_BYTES) {
        LOG_ERROR(mux_module, "News index exceeds size limit, rejecting");
        free(json_data);
        pending_error = 1;
        return;
    }

    if (pending_index_json) {
        free(pending_index_json);
        pending_index_json = NULL;
    }

    pending_index_json = json_data;
    pending_rebuild = 1;
}

static void load_topic_finished(const int result) {
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
    initiate_download(NEWS_INDEX_URL, cache_path, 1, lang.muxnews.download);
}

static void refresh_topic(const int topic_id) {
    create_directories(NEWS_CACHE_DIR, 0);

    snprintf(cache_path, sizeof(cache_path), NEWS_TOPIC_PATH_FMT, topic_id);
    if (file_exist(cache_path)) remove(cache_path);

    char url[MAX_BUFFER_SIZE];
    snprintf(url, sizeof(url), NEWS_TOPIC_URL_FMT, topic_id);

    set_download_callbacks(load_topic_finished);
    initiate_download(url, cache_path, 0, lang.muxnews.download);
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 1, 0, 1);
}

static void list_nav_prev(const int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    if (download_in_progress) return;
    list_nav_move(steps, +1);
}

static void handle_a(void) {
    if (download_in_progress || msgbox_active || hold_call) return;
    if (!ui_count_static || (size_t) current_item_index >= topic_id_count) return;

    requested_topic_id = topic_ids[current_item_index];
    snprintf(requested_topic_title, sizeof(requested_topic_title), "%s", items[current_item_index].display_name);

    snprintf(cache_path, sizeof(cache_path), NEWS_TOPIC_PATH_FMT, requested_topic_id);
    if (file_exist(cache_path)) {
        play_sound(snd_confirm);
        load_topic_from_cache(requested_topic_id, requested_topic_title);
        return;
    }

    if (device.board.has_network && is_network_connected()) {
        play_sound(snd_confirm);
        toast_message(lang.muxnews.open, tst_wait_m);
        refresh_topic(requested_topic_id);
    } else {
        play_sound(snd_error);
        toast_message(lang.generic.need_connect, tst_wait_m);
    }
}

static void handle_b(void) {
    if (download_in_progress || hold_call) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "news");

    mux_input_stop();
}

static void handle_x(void) {
    if (download_in_progress || msgbox_active || hold_call) return;

    if (device.board.has_network && is_network_connected()) {
        play_sound(snd_confirm);

        delete_files_of_type(NEWS_CACHE_DIR, ".ini", NULL, 0);
        delete_files_of_type(NEWS_CACHE_DIR, ".json", NULL, 0);

        refresh_index();
    } else {
        play_sound(snd_error);
        toast_message(lang.generic.need_connect, tst_wait_m);
    }

    if (ui_count_static > 0) {
        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.read, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    if (device.board.has_network) {
        setup_nav(
            (struct nav_bar[]) {{ui_lbl_nav_x_glyph, "", 0}, {ui_lbl_nav_x, lang.generic.refresh, 0}, {NULL, NULL, 0}}
        );
    }

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    download_poll();

    if (pending_error) {
        pending_error = 0;
        play_sound(snd_error);
        toast_message(lang.muxnews.error, tst_wait_m);
    }

    if (pending_rebuild) {
        pending_rebuild = 0;

        if (pending_index_json) {
            parse_index_json(pending_index_json);
            free(pending_index_json);
            pending_index_json = NULL;
        }

        create_news_entries();

        if (ui_count_static > 0) {
            current_item_index = 0;
            list_nav_next(0);
            lv_obj_update_layout(ui_pnl_content);
        }
    }

    if (pending_show_topic) {
        pending_show_topic = 0;

        snprintf(cache_path, sizeof(cache_path), NEWS_TOPIC_PATH_FMT, requested_topic_id);
        if (file_exist(cache_path)) {
            load_topic_from_cache(requested_topic_id, requested_topic_title);
        } else {
            play_sound(snd_error);
            toast_message(lang.muxnews.error, tst_wait_m);
        }
    }

    if (pending_show_post) {
        pending_show_post = 0;

        char post_path[MAX_BUFFER_SIZE];
        snprintf(post_path, sizeof(post_path), NEWS_POST_PATH_FMT, requested_post_id);

        if (file_exist(post_path)) {
            load_post_from_cache(requested_post_id, requested_topic_title);
        } else {
            play_sound(snd_error);
            toast_message(lang.muxnews.error, tst_wait_m);
        }
    }

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, wall_general);
        adjust_gen_panel();

        if (overlay_image) lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnl_content);
        nav_moved = 0;
    }
}

int muxnews_main(void) {
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxnews.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    init_elements();

    if (file_exist(NEWS_INDEX_PATH)) {
        char *json_data = read_all_char_from(NEWS_INDEX_PATH);
        if (json_data) {
            parse_index_json(json_data);
            free(json_data);
        }
    }

    if (device.board.has_network && is_network_connected()) refresh_index();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_list_nav_up,
                [mux_input_dpad_down] = handle_list_nav_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler = {},
        .hold_handler = {
            [mux_input_dpad_up] = handle_list_nav_up_hold,
            [mux_input_dpad_down] = handle_list_nav_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    free_topics();
    free_items(&items, &item_count);

    return 0;
}
