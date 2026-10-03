#include "library.h"
#include "player.h"
#include "paths.h"
#include "state.h"

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <module/muxshare.h>
#include <common/platform/audio.h>
#include <common/platform/battery.h>
#include <common/storage/download.h>
#include <common/ui/orientation.h>

#define WASABI_VERSION "1.0.0"

typedef enum {
    screen_home,
    screen_history,
    screen_bookmarks,
    screen_collection,
    screen_videos,
    screen_audio,
    screen_live,
    screen_playlist,
} video_screen;

static video_screen active_screen = screen_home;
static char selected_uri[PATH_MAX];
static char selected_title[PATH_MAX];
static int selected_live;
static double selected_start;
static int launch_requested;
static video_library_entry *playlist_entries;
static size_t playlist_entry_count;
static size_t playlist_window_start;
static size_t playlist_selected_index;
static int playlist_channels;
static int playlist_audio;
static char playlist_name[PATH_MAX];
static char playlist_source[PATH_MAX];
static char logo_desired[MAX_BUFFER_SIZE];
static char logo_downloading[MAX_BUFFER_SIZE];
static char logo_download_path[PATH_MAX];
static char logo_failed[MAX_BUFFER_SIZE];
static int logo_refresh;
static uint32_t logo_ready_at;

static uint64_t logo_key(const char *value) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (const unsigned char *cursor = (const unsigned char *) value; cursor && *cursor; cursor++) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static const char *logo_extension(const char *source) {
    const char *query = strchr(source, '?');
    const char *dot = strrchr(source, '.');
    if (!dot || (query && dot > query)) return ".png";
    const size_t length = query ? (size_t) (query - dot) : strlen(dot);
    if ((length == 4 && (!strncasecmp(dot, ".png", 4) || !strncasecmp(dot, ".jpg", 4) || !strncasecmp(dot, ".svg", 4)))
        || (length == 5 && (!strncasecmp(dot, ".jpeg", 5) || !strncasecmp(dot, ".webp", 5))))
        return dot;
    return ".png";
}

static int logo_cache_path(const char *source, char *path, const size_t size) {
    const char *suffix = logo_extension(source);
    const char *query = strchr(suffix, '?');
    const size_t suffix_length = query ? (size_t) (query - suffix) : strlen(suffix);
    const int written = snprintf(
        path, size, WASABI_SHARE_PATH "logo/%016llx%.*s", (unsigned long long) logo_key(source), (int) suffix_length,
        suffix
    );
    return written > 0 && (size_t) written < size;
}

static void logo_download_complete(const int result) {
    if (result == 0 && file_exist(logo_download_path)) {
        logo_failed[0] = '\0';
        logo_refresh = 1;
    } else {
        snprintf(logo_failed, sizeof(logo_failed), "%s", logo_downloading);
    }
    logo_downloading[0] = '\0';
    logo_download_path[0] = '\0';
}

static void logo_download_start(void) {
    if (!logo_desired[0] || logo_downloading[0] || atomic_load_explicit(&download_in_progress, memory_order_acquire)
        || strcmp(logo_desired, logo_failed) == 0 || !SDL_TICKS_PASSED(SDL_GetTicks(), logo_ready_at))
        return;

    char path[PATH_MAX];
    if (!logo_cache_path(logo_desired, path, sizeof(path))) return;
    if (file_exist(path)) {
        logo_desired[0] = '\0';
        logo_refresh = 1;
        return;
    }

    snprintf(logo_downloading, sizeof(logo_downloading), "%s", logo_desired);
    snprintf(logo_download_path, sizeof(logo_download_path), "%s", path);
    set_download_callbacks(logo_download_complete);
    if (initiate_download_limited(logo_downloading, logo_download_path, 2U * 1024U * 1024U, 0, "") < 0) {
        logo_downloading[0] = '\0';
        logo_download_path[0] = '\0';
    }
}

static int resolve_logo(const char *source, char *path, const size_t size) {
    if (!source || !source[0]) {
        logo_desired[0] = '\0';
        if (logo_downloading[0]) atomic_store_explicit(&cancel_download, 1, memory_order_release);
        return 0;
    }
    if (!strstr(source, "://")) {
        logo_desired[0] = '\0';
        if (logo_downloading[0]) atomic_store_explicit(&cancel_download, 1, memory_order_release);
        if (!file_exist(source)) return 0;
        return snprintf(path, size, "%s", source) < (int) size;
    }
    if (strncasecmp(source, "http://", 7) && strncasecmp(source, "https://", 8)) return 0;
    if (!logo_cache_path(source, path, size)) return 0;
    if (file_exist(path)) {
        logo_desired[0] = '\0';
        if (logo_downloading[0] && strcmp(logo_downloading, source) != 0)
            atomic_store_explicit(&cancel_download, 1, memory_order_release);
        return 1;
    }

    if (strcmp(logo_desired, source) != 0) {
        snprintf(logo_desired, sizeof(logo_desired), "%s", source);
        logo_failed[0] = '\0';
        logo_ready_at = SDL_GetTicks() + 750;
        if (logo_downloading[0] && strcmp(logo_downloading, source) != 0)
            atomic_store_explicit(&cancel_download, 1, memory_order_release);
    }
    logo_download_start();
    return 0;
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 1, 0, 1);
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static size_t playlist_window_capacity(void) {
    size_t capacity = theme.mux.item.count > 0 ? (size_t) theme.mux.item.count * LIST_WINDOW_SCALE : LIST_WINDOW_MIN;
    if (capacity < LIST_WINDOW_MIN) capacity = LIST_WINDOW_MIN;
    if (capacity > LIST_WINDOW_MAX) capacity = LIST_WINDOW_MAX;
    return capacity < playlist_entry_count ? capacity : playlist_entry_count;
}

static void playlist_position_window(void) {
    const size_t capacity = playlist_window_capacity();
    if (!capacity || playlist_selected_index >= playlist_entry_count) {
        playlist_window_start = 0;
        playlist_selected_index = 0;
        return;
    }

    size_t start = playlist_selected_index > capacity / 2U ? playlist_selected_index - capacity / 2U : 0;
    if (start + capacity > playlist_entry_count) start = playlist_entry_count - capacity;
    playlist_window_start = start;
}

static void add_menu_item(const char *key, const char *label, const char *glyph) {
    content_item *item = add_item(&items, &item_count, key, label, "", content_type_menu);
    if (!item) return;
    item->glyph_icon = strdup(glyph);
}

static void add_media_item(const char *uri, const char *title, const char *logo, const int live) {
    content_item *item = add_item(&items, &item_count, title, title, uri, content_type_item);
    if (!item) return;
    item->glyph_icon = strdup(live ? "network" : video_path_is_audio(uri) ? "audio" : "video");
    if (logo && logo[0]) item->grid_image = strdup(logo);
    item->folder_item_count = live;
}

static void populate_home(void) {
    add_menu_item("history", lang.muxmedia.continue_watching, "history");
    add_menu_item("bookmarks", lang.muxmedia.bookmarks, "bookmark");
    add_menu_item("collection", lang.muxretro.catalogue_screen.collection, "collection");
    add_menu_item("videos", lang.muxmedia.videos, "video");
    add_menu_item("audio", lang.muxmedia.audio, "audio");
    add_menu_item("live", lang.muxmedia.live_tv, "network");
}

static void format_position(const double seconds, char *buffer, const size_t size) {
    const int value = seconds > 0.0 ? (int) seconds : 0;
    const int hours = value / 3600;
    const int minutes = value / 60 % 60;
    const int remainder = value % 60;
    if (hours > 0)
        snprintf(buffer, size, "%d:%02d:%02d", hours, minutes, remainder);
    else
        snprintf(buffer, size, "%02d:%02d", minutes, remainder);
}

static void populate_bookmarks(void) {
    video_state_entry *state = NULL;
    size_t count = 0;
    if (video_bookmark_load(&state, &count) < 0) LOG_ERROR(mux_module, "Unable to read video bookmarks");

    for (size_t i = 0; i < count; ++i) {
        char position[32];
        char label[PATH_MAX];
        format_position(state[i].position, position, sizeof(position));
        if (state[i].name && state[i].name[0])
            snprintf(label, sizeof(label), "%s - %s - %s", state[i].title, position, state[i].name);
        else
            snprintf(label, sizeof(label), "%s - %s", state[i].title, position);
        add_media_item(state[i].uri, label, NULL, 0);
        if (item_count) {
            items[item_count - 1].order.play_time = (size_t) (state[i].position * 1000.0 + 0.5);
            items[item_count - 1].help = strdup(state[i].title);
            if (!video_path_is_audio(state[i].uri) && state[i].thumbnail && state[i].thumbnail[0])
                items[item_count - 1].grid_image = strdup(state[i].thumbnail);
        }
    }

    video_state_free(state, count);
}

static void populate_state(const int collection) {
    video_state_entry *state = NULL;
    size_t count = 0;
    const int result = collection ? video_collection_load(&state, &count) : video_history_load(&state, &count);
    if (result < 0) LOG_ERROR(mux_module, "Unable to read video %s", collection ? "collection" : "history");

    for (size_t i = 0; i < count; ++i) {
        const char *target =
            !collection && state[i].live && state[i].name && state[i].name[0] ? state[i].name : state[i].uri;
        add_media_item(target, state[i].title, NULL, state[i].live);
        if (!collection && item_count && !video_path_is_audio(target) && state[i].thumbnail && state[i].thumbnail[0])
            items[item_count - 1].grid_image = strdup(state[i].thumbnail);
    }

    video_state_free(state, count);
}

static void populate_library(const int audio) {
    video_library_entry *library = NULL;
    size_t count = 0;
    char root[PATH_MAX];

    static const char *const video_directories[] = {"VIDEOS", "Videos", "videos"};
    static const char *const audio_directories[] = {"AUDIO", "Audio", "audio"};
    const char *const *directories = audio ? audio_directories : video_directories;
    const size_t directory_count = audio ? A_SIZE(audio_directories) : A_SIZE(video_directories);
    for (size_t i = 0; i < directory_count; ++i) {
        snprintf(root, sizeof(root), "%s/%s", device.storage.rom.mount, directories[i]);
        if (!dir_exist(root)) continue;
        video_library_scan(root, audio, &library, &count);
        break;
    }

    for (size_t i = 0; i < count; ++i)
        add_media_item(library[i].uri, library[i].title, library[i].logo, 0);
    video_library_free(library, count);
}

static void populate_live(void) {
    video_library_entry *library = NULL;
    size_t count = 0;
    video_live_scan(INFO_VID_PATH "/live", &library, &count);
    for (size_t i = 0; i < count; ++i)
        add_media_item(library[i].uri, library[i].title, library[i].logo, 1);
    video_library_free(library, count);
}

static void populate_playlist(void) {
    playlist_position_window();
    const size_t end = playlist_window_start + playlist_window_capacity();
    for (size_t index = playlist_window_start; index < end; index++)
        add_media_item(
            playlist_entries[index].uri, playlist_entries[index].title, playlist_entries[index].logo,
            playlist_entries[index].live
        );
}

static const char *screen_title(void) {
    switch (active_screen) {
        case screen_history:
            return lang.muxmedia.continue_watching;
        case screen_bookmarks:
            return lang.muxmedia.bookmarks;
        case screen_collection:
            return lang.muxretro.catalogue_screen.collection;
        case screen_videos:
            return lang.muxmedia.videos;
        case screen_audio:
            return lang.muxmedia.audio;
        case screen_live:
            return lang.muxmedia.live_tv;
        case screen_playlist:
            return playlist_name;
        default:
            return lang.muxmedia.title;
    }
}

static const char *empty_message(void) {
    switch (active_screen) {
        case screen_history:
            return lang.muxmedia.empty_history;
        case screen_bookmarks:
            return lang.muxmedia.empty_bookmarks;
        case screen_collection:
            return lang.muxmedia.empty_collection;
        case screen_videos:
            return lang.muxmedia.empty_videos;
        case screen_audio:
            return lang.muxmedia.empty_audio;
        case screen_live:
            return lang.muxmedia.empty_live_tv;
        case screen_playlist:
            return playlist_channels ? lang.muxmedia.empty_live_tv
                   : playlist_audio  ? lang.muxmedia.empty_audio
                                     : lang.muxmedia.empty_videos;
        default:
            return "";
    }
}

static void add_standard_row(const content_item *item) {
    ui_count_static++;
    gen_label(mux_module, item->glyph_icon ? item->glyph_icon : "video", item->display_name);
}

static void update_nav(void) {
    const int media = active_screen != screen_home && ui_count_static > 0;
    const int bookmark = active_screen == screen_bookmarks;
    const int collection_action = media && active_screen != screen_playlist;
    const int collected =
        collection_action && !bookmark && video_collection_contains(items[current_item_index].extra_data);

    nav_hide_all();
    setup_nav((struct nav_bar[]) {
        {ui_lbl_nav_b_glyph, "", 0},
        {ui_lbl_nav_b, lang.generic.back, 0},
        {NULL, NULL, 0},
    });
    nav_show_a(ui_count_static > 0, lang.generic.select);
    if (collection_action) {
        lv_label_set_text(ui_lbl_nav_x, bookmark || collected ? lang.generic.remove : lang.generic.collect);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void update_playlist_counter(void) {
    update_item_counter(
        ui_lbl_counter_explore, playlist_selected_index, playlist_entry_count,
        active_screen == screen_playlist && config.visual.menu_counter_file
    );
}

static void update_preview(void) {
    const int visual_state = active_screen == screen_history || active_screen == screen_bookmarks;
    const int channel = active_screen == screen_live || (active_screen == screen_playlist && playlist_channels);
    const char *source = (visual_state || channel) && ui_count_static && current_item_index < (int) item_count
                             ? items[current_item_index].grid_image
                             : NULL;
    char logo_path[PATH_MAX];
    const char *thumbnail = source;
    if (channel) thumbnail = resolve_logo(source, logo_path, sizeof(logo_path)) ? logo_path : NULL;

    if (!thumbnail || !thumbnail[0] || !file_exist(thumbnail)) {
        clear_image(ui_img_box);
        lv_obj_add_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_img_cache_invalidate_src(NULL);
    lv_obj_clear_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    static const int divisors[] = {3, 2, 3};
    const int divisor = channel ? 4 : divisors[config.video.thumbnail_size];
    const int multiplier = !channel && config.video.thumbnail_size == 2 ? 2 : 1;
    const int16_t max_height =
        channel
            ? (int16_t) ((device.mux.height - theme.header.height - theme.footer.height) / 4)
            : (int16_t) ((device.mux.height - theme.header.height - theme.footer.height - 8) * multiplier / divisor);
    const struct image_settings settings = {
        .image_path = (char *) thumbnail,
        .align = LV_ALIGN_BOTTOM_RIGHT,
        .max_width = (int16_t) (device.mux.width * multiplier / divisor),
        .max_height = max_height,
        .pad_right = channel ? 12 : 0,
        .pad_bottom = channel ? 8 : 0,
    };
    update_image(ui_img_box, settings);
}

static void rebuild_screen(void) {
    free_items(&items, &item_count);
    clear_image(ui_img_box);
    lv_obj_add_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(ui_pnl_content);
    reset_ui_groups();

    ui_count_static = 0;
    current_item_index = 0;
    first_open = 1;

    switch (active_screen) {
        case screen_home:
            populate_home();
            break;
        case screen_history:
            populate_state(0);
            break;
        case screen_bookmarks:
            populate_bookmarks();
            break;
        case screen_collection:
            populate_state(1);
            break;
        case screen_videos:
            populate_library(0);
            break;
        case screen_audio:
            populate_library(1);
            break;
        case screen_live:
            populate_live();
            break;
        case screen_playlist:
            populate_playlist();
            break;
    }

    for (size_t i = 0; i < item_count; ++i) {
        add_standard_row(&items[i]);
    }

    lv_label_set_text(ui_lbl_title, screen_title());
    lv_label_set_text(ui_lbl_screen_message, ui_count_static ? "" : empty_message());
    update_nav();

    if (ui_count_static) {
        if (active_screen == screen_playlist) {
            const int focus = (int) (playlist_selected_index - playlist_window_start);
            if (focus > 0)
                gen_step_movement(focus, 1, 1, 0, 0);
            else
                list_nav_next(0);
        } else {
            list_nav_next(0);
        }
        lv_obj_update_layout(ui_pnl_content);
    }
    update_preview();
    update_playlist_counter();
}

static void playlist_select(const size_t target) {
    if (target >= playlist_entry_count || target == playlist_selected_index) return;

    const size_t previous = playlist_selected_index;
    playlist_selected_index = target;
    if (target >= playlist_window_start && target < playlist_window_start + item_count) {
        const int direction = target < previous ? -1 : 1;
        const size_t distance = target < previous ? previous - target : target - previous;
        gen_step_movement((int) distance, direction, 1, 0, 1);
        update_playlist_counter();
        return;
    }

    gen_step_movement(0, target < previous ? -1 : 1, 1, 0, 1);
    toast_message(lang.muxmedia.loading_more, tst_wait_s);
    rebuild_screen();
}

static void playlist_step(const int direction, const int wrap) {
    if (msgbox_active) {
        if (direction < 0)
            handle_list_nav_up();
        else
            handle_list_nav_down();
        return;
    }
    if (block_input || playlist_entry_count < 2) return;
    playlist_select(
        video_playlist_step(playlist_entries, playlist_entry_count, playlist_selected_index, direction, 1, wrap)
    );
}

static void playlist_skip(const int direction) {
    if (msgbox_active) {
        if (direction < 0)
            handle_list_nav_page_up();
        else
            handle_list_nav_page_down();
        return;
    }
    if (block_input || page_nav_blocked || playlist_entry_count < 2) return;
    playlist_select(video_playlist_skip(
        playlist_entries, playlist_entry_count, playlist_selected_index, direction,
        theme.mux.item.count > 0 ? (size_t) theme.mux.item.count : 1U, config.visual.page_skip != 0
    ));
}

static void handle_playlist_up(void) {
    playlist_step(-1, 1);
}

static void handle_playlist_down(void) {
    playlist_step(1, 1);
}

static void handle_playlist_up_hold(void) {
    playlist_step(-1, 0);
}

static void handle_playlist_down_hold(void) {
    playlist_step(1, 0);
}

static void handle_playlist_page_up(void) {
    playlist_skip(-1);
}

static void handle_playlist_page_down(void) {
    playlist_skip(1);
}

static void enter_screen(const char *key) {
    if (strcmp(key, "history") == 0)
        active_screen = screen_history;
    else if (strcmp(key, "bookmarks") == 0)
        active_screen = screen_bookmarks;
    else if (strcmp(key, "collection") == 0)
        active_screen = screen_collection;
    else if (strcmp(key, "videos") == 0)
        active_screen = screen_videos;
    else if (strcmp(key, "audio") == 0)
        active_screen = screen_audio;
    else if (strcmp(key, "live") == 0)
        active_screen = screen_live;
    rebuild_screen();
}

static void request_playback(const content_item *item) {
    snprintf(selected_uri, sizeof(selected_uri), "%s", item->extra_data);
    snprintf(
        selected_title, sizeof(selected_title), "%s", item->help && item->help[0] ? item->help : item->display_name
    );
    selected_live = item->folder_item_count;
    selected_start = active_screen == screen_bookmarks ? (double) item->order.play_time / 1000.0 : 0.0;
    launch_requested = 1;
    mux_input_stop();
}

static void handle_a(void) {
    if (msgbox_active || hold_call || !ui_count_static) return;
    play_sound(snd_confirm);

    if (active_screen == screen_home) {
        enter_screen(items[current_item_index].name);
    } else {
        request_playback(&items[current_item_index]);
    }
}

static void handle_b(void) {
    if (hold_call) return;
    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    if (active_screen == screen_home || active_screen == screen_playlist) {
        mux_input_stop();
    } else {
        active_screen = screen_home;
        rebuild_screen();
    }
}

static void handle_x(void) {
    if (orientation_handle_skip()) return;
    if (msgbox_active || hold_call || !ui_count_static || active_screen == screen_home
        || active_screen == screen_playlist)
        return;

    int collected = 0;
    const content_item *item = &items[current_item_index];
    if (active_screen == screen_bookmarks) {
        if (video_bookmark_remove(item->extra_data, (double) item->order.play_time / 1000.0) < 0) {
            play_sound(snd_error);
            return;
        }
        play_sound(snd_confirm);
        rebuild_screen();
        return;
    }
    if (video_collection_toggle(item->extra_data, item->display_name, item->folder_item_count, &collected) < 0) {
        play_sound(snd_error);
        return;
    }

    play_sound(snd_confirm);
    if (active_screen == screen_collection && !collected) {
        rebuild_screen();
    } else {
        update_nav();
    }
}

static void handle_help(void) {
    if (msgbox_active || hold_call) return;
    play_sound(snd_info_open);
    show_info_box(lang.muxmedia.title, lang.muxmedia.overview, 0);
}

static void ui_refresh_task(lv_timer_t *timer) {
    ui_gen_refresh_task(timer);
    download_poll();
    logo_download_start();
    if (logo_refresh) {
        logo_refresh = 0;
        update_preview();
    }
    static int previous_index = -1;
    if (previous_index != current_item_index) {
        previous_index = current_item_index;
        update_nav();
        update_preview();
    }
}

static int play_folder(const char *uri, const char *title, const video_player_options *base);

static int play_selected(void) {
    init_timer(NULL, NULL);
    int result = 0;
    if (active_screen == screen_playlist && playlist_entry_count > 0) {
        size_t selected = playlist_selected_index;
        do {
            const video_library_entry *entry = &playlist_entries[selected];
            video_player_options options = {
                .hardware_decode = config.video.hardware_decode,
                .resume = config.video.resume,
                .deinterlace = config.video.deinterlace,
                .keep_history = config.video.keep_history,
                .live = playlist_channels,
                .start_position = 0.0,
                .playlist = playlist_entries,
                .playlist_count = playlist_entry_count,
                .playlist_index = selected,
                .playlist_channels = playlist_channels,
                .playlist_selection = &selected,
                .container_uri = playlist_source,
            };
            result = video_player_run(entry->uri, entry->title, &options);
        } while (result == 1 && selected < playlist_entry_count);
        if (selected < playlist_entry_count) playlist_selected_index = selected;
    } else {
        const video_player_options options = {
            .hardware_decode = config.video.hardware_decode,
            .resume = config.video.resume,
            .deinterlace = config.video.deinterlace,
            .keep_history = config.video.keep_history,
            .live = selected_live,
            .start_position = selected_start,
        };
        result = selected_live || strstr(selected_uri, "://") ? video_player_run(selected_uri, selected_title, &options)
                                                              : play_folder(selected_uri, selected_title, &options);
    }
    init_timer(ui_refresh_task, NULL);
    return result;
}

static void initialise_audio(void) {
    static const int rates[] = {32000, 44100, 48000};
    static const int periods[] = {256, 512, 1024, 2048};
    const useconds_t backoff[] = {10000, 25000, 50000, 100000, 200000, 400000, 800000};
    for (size_t attempt = 0; attempt < A_SIZE(backoff); attempt++) {
        if (init_audio_backend_spec(rates[config.video.sample_rate], periods[config.video.audio_period])) {
            init_fe_snd(&fe_snd, config.settings.general.sound, 0);
            return;
        }
        usleep(backoff[attempt]);
    }
}

static void initialise_ui(void) {
    init_theme(1, 0);
    init_display();
    init_ui_common_screen(&theme, &device, &lang, lang.muxmedia.title);
    init_ui_item_counter(&theme);
    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());
    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();
    header_and_footer_setup();
    overlay_display();
    initialise_audio();
    inotify_init();
    battery_init();
    init_timer(NULL, NULL);
}

static int playlist_history_selection(const char *path, size_t *selected) {
    video_state_entry entry = {0};
    if (!path || !selected || !video_history_find(path, &entry) || !entry.live || !entry.name || !entry.name[0]) {
        free(entry.uri);
        free(entry.title);
        free(entry.thumbnail);
        free(entry.name);
        return 0;
    }

    int found = 0;
    for (size_t index = 0; index < playlist_entry_count; index++) {
        if (strcmp(playlist_entries[index].uri, entry.name) != 0) continue;
        *selected = index;
        found = 1;
        break;
    }
    free(entry.uri);
    free(entry.title);
    free(entry.thumbnail);
    free(entry.name);
    return found;
}

static int run_playlist_ui(const char *path, const int channels, const int resume_history) {
    int content_switch_requested = 0;
    int history_launch = 0;
    playlist_channels = channels;
    snprintf(playlist_source, sizeof(playlist_source), "%s", path);
    if (video_playlist_load(path, channels, &playlist_entries, &playlist_entry_count) < 0) return -1;
    playlist_audio = !channels && playlist_entry_count > 0;
    for (size_t index = 0; playlist_audio && index < playlist_entry_count; index++)
        playlist_audio = video_path_is_audio(playlist_entries[index].uri);
    video_title_from_uri(path, playlist_name, sizeof(playlist_name));
    playlist_window_start = 0;
    playlist_selected_index = 0;
    if (resume_history && playlist_entry_count) {
        size_t selected = 0;
        if (playlist_history_selection(path, &selected)) {
            playlist_selected_index = selected;
            history_launch = 1;
        }
    }
    active_screen = screen_playlist;
    initialise_ui();
    rebuild_screen();
    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_up] = handle_playlist_up,
                [mux_input_dpad_down] = handle_playlist_down,
                [mux_input_l1] = handle_playlist_page_up,
                [mux_input_r1] = handle_playlist_page_down,
            },
        .release_handler = {[mux_input_menu] = handle_help},
        .hold_handler = {
            [mux_input_dpad_up] = handle_playlist_up_hold,
            [mux_input_dpad_down] = handle_playlist_down_hold,
            [mux_input_l1] = handle_playlist_page_up,
            [mux_input_r1] = handle_playlist_page_down,
        },
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    orientation_introduce(mux_module, lang.muxmedia.title, lang.muxmedia.overview);
    do {
        if (history_launch) {
            history_launch = 0;
            launch_requested = 1;
        } else {
            launch_requested = 0;
            init_input(&input_opts, 1);
            mux_input_task(&input_opts);
        }
        if (launch_requested) {
            const int result = play_selected();
            if (result == video_player_content_switch) {
                content_switch_requested = 1;
                launch_requested = 0;
            } else if (result == video_player_stopped) {
                launch_requested = 0;
            } else {
                rebuild_screen();
            }
            if (result == video_player_failed) {
                play_sound(snd_error);
                toast_message(lang.muxmedia.playback_failed, tst_wait_m);
            }
        }
    } while (launch_requested);

    free_items(&items, &item_count);
    video_library_free(playlist_entries, playlist_entry_count);
    playlist_entries = NULL;
    playlist_entry_count = 0;
    sdl_cleanup();
    return content_switch_requested ? video_player_content_switch : video_player_stopped;
}

static int play_folder(const char *uri, const char *title, const video_player_options *base) {
    video_library_entry *entries = NULL;
    size_t count = 0;
    size_t selected = 0;
    if (video_folder_playlist(uri, &entries, &count, &selected) <= 0) return video_player_run(uri, title, base);

    video_player_options options = *base;
    options.playlist = entries;
    options.playlist_count = count;
    options.playlist_selection = &selected;
    options.folder_playlist = 1;

    int result;
    const char *current_uri = uri;
    const char *current_title = title;
    do {
        options.playlist_index = selected;
        result = video_player_run(current_uri, current_title, &options);
        options.start_position = 0.0;
        if (selected < count) {
            current_uri = entries[selected].uri;
            current_title = entries[selected].title;
        }
    } while (result == video_player_playlist_switch && selected < count);

    video_library_free(entries, count);
    return result;
}

static void print_help(const char *program) {
    printf("Usage: %s [--live] [--history] [--title TITLE] [--link-file FILE] [URI]\n", program);
}

int main(const int argc, char **argv) {
    setlocale(LC_CTYPE, "");

    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)) {
        print_help(argv[0]);
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("Wasabi %s\n", WASABI_VERSION);
        return EXIT_SUCCESS;
    }

    load_device(&device);
    load_config(&config);
    load_wasabi_defaults(&config);
    init_module("muxmedia");
    if (video_state_init() < 0) LOG_WARN(mux_module, "Video state storage is unavailable");

    const char *uri = NULL;
    const char *title = NULL;
    const char *link_file = NULL;
    int live = 0;
    int history = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--live") == 0) {
            live = 1;
        } else if (strcmp(argv[i], "--history") == 0) {
            history = 1;
        } else if (strcmp(argv[i], "--title") == 0 && i + 1 < argc) {
            title = argv[++i];
        } else if (strcmp(argv[i], "--link-file") == 0 && i + 1 < argc) {
            link_file = argv[++i];
            live = 1;
        } else if (argv[i][0] != '-') {
            uri = argv[i];
        } else {
            print_help(argv[0]);
            return EXIT_FAILURE;
        }
    }

    char *link_value = NULL;
    if (link_file) {
        link_value = read_line_char_from(link_file, 1);
        uri = link_value;
    }

    if (uri && uri[0]) {
        const int remote = strstr(uri, "://") != NULL;
        const int m3u = video_path_extension_is(uri, ".m3u");
        const int m3u8 = video_path_extension_is(uri, ".m3u8");
        if (!remote && (video_path_extension_is(uri, ".pls") || video_path_extension_is(uri, ".json"))) {
            const int result = run_playlist_ui(uri, 1, history);
            free(link_value);
            if (result == video_player_content_switch) return CONTENT_SWITCH_EXIT_STATUS;
            return result == video_player_stopped ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        if (m3u || m3u8) {
            const video_playlist_type playlist_type = video_playlist_probe(uri);
            if (playlist_type == video_playlist_m3u) {
                const int result = run_playlist_ui(uri, live || m3u8, history);
                free(link_value);
                if (result == video_player_content_switch) return CONTENT_SWITCH_EXIT_STATUS;
                return result == video_player_stopped ? EXIT_SUCCESS : EXIT_FAILURE;
            }
            if (playlist_type == video_playlist_hls) live = 1;
        }
        char derived_title[PATH_MAX];
        if (!title || !title[0]) title = video_title_from_uri(uri, derived_title, sizeof(derived_title));
        const video_player_options options = {
            .hardware_decode = config.video.hardware_decode,
            .resume = config.video.resume,
            .deinterlace = config.video.deinterlace,
            .keep_history = config.video.keep_history,
            .live = live,
            .start_position = 0.0,
        };
        initialise_ui();
        const int result = live || remote ? video_player_run(uri, title, &options) : play_folder(uri, title, &options);
        sdl_cleanup();
        free(link_value);
        if (result == video_player_content_switch) return CONTENT_SWITCH_EXIT_STATUS;
        return result >= video_player_stopped ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    free(link_value);

    print_help(argv[0]);
    return EXIT_FAILURE;
}
