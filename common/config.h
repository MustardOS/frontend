#pragma once

#include <stddef.h>
#include <stdint.h>
#include "options.h"

extern struct mux_config config;

#define CFG_DIR_MAX   64
#define CFG_NAME_MAX  64
#define CFG_VALUE_MAX 1024

typedef struct {
    char name[CFG_NAME_MAX];
    char value[CFG_VALUE_MAX];
} cfg_kv_t;

typedef struct {
    cfg_kv_t entries[CFG_DIR_MAX];
    int count;
} cfg_dir_t;

void cfg_dir_scan(cfg_dir_t *d, const char *dir_path);

const char *cfg_dir_get(const cfg_dir_t *d, const char *name);

int cfg_dir_int(const cfg_dir_t *d, const char *name, int fallback);

double cfg_dir_flo(const cfg_dir_t *d, const char *name, double fallback);

struct mux_config {
    struct {
        char build[MAX_BUFFER_SIZE];
        char version[MAX_BUFFER_SIZE];
    } system;

    struct {
        int16_t factory_reset;
        int16_t device_mode;
    } boot;

    struct {
        int16_t apps;
        int16_t bios;
        int16_t catalogue;
        int16_t cheats;
        int16_t collection;
        int16_t config;
        int16_t content;
        int16_t history;
        int16_t init;
        int16_t merge;
        int16_t music;
        int16_t name;
        int16_t network;
        int16_t overlays;
        int16_t override;
        int16_t package;
        int16_t save;
        int16_t screenshot;
        int16_t shaders;
        int16_t syncthing;
        int16_t theme;
        int16_t track;
        int16_t start;
        int16_t target;
    } backup;

    struct {
        int16_t notation;
        char pool[MAX_BUFFER_SIZE];
        char custom[MAX_BUFFER_SIZE];
    } clock;

    struct {
        char interface[MAX_BUFFER_SIZE];
        int16_t type;
        char ssid[MAX_BUFFER_SIZE];
        char pass[MAX_BUFFER_SIZE];
        char address[MAX_BUFFER_SIZE];
        char gateway[MAX_BUFFER_SIZE];
        char subnet[MAX_BUFFER_SIZE];
        char dns[MAX_BUFFER_SIZE];
    } network;

    struct {
        struct {
            char data[MAX_BUFFER_SIZE];
        } download;
        struct {
            char data[MAX_BUFFER_SIZE];
        } language;
    } extra;

    struct {
        char active[MAX_BUFFER_SIZE];
        char storage_theme[MAX_BUFFER_SIZE];
        char theme_cat_path[MAX_BUFFER_SIZE];

        struct {
            int16_t all_themes;
            int16_t resolution_640_x480;
            int16_t resolution_720_x480;
            int16_t resolution_720_x720;
            int16_t resolution_1024_x768;
            int16_t resolution_1280_x720;
            int16_t resolution_1920_x1080;
            int16_t grid;
            int16_t hdmi;
            int16_t language;
            char lookup[MAX_BUFFER_SIZE];
        } filter;

        struct {
            char data[MAX_BUFFER_SIZE];
            char preview[MAX_BUFFER_SIZE];
        } download;
    } theme;

    struct {
        struct {
            int16_t accelerate;
            int16_t repeat_delay;
            int16_t stick_nav;
            int16_t thermal;
            int16_t font;
            int16_t volume;
            int16_t brightness;
            int16_t led;
            int16_t random_theme;
            int16_t retro_wait;
            int16_t retro_free;
            int16_t retro_cache;
            int16_t activity;
            int16_t usb_function;
            int16_t verbose;
            int16_t debug_log;
            int16_t rumble;
            int16_t user_init;
            int16_t dpad_swap;
            int16_t overdrive;
            int16_t swapfile;
            int16_t zramfile;
            int16_t lid_switch;
            int16_t disp_suspend;
            int16_t stage_overlay;
            int16_t inc_bright;
            int16_t inc_volume;
            int16_t max_gpu;
            int16_t audio_ready;
            int16_t audio_swap;
            int16_t audio_suspend;
            int16_t bt_scan_timeout;
            int16_t second_part;
            int16_t usb_part;
            int16_t trust_modify;
            int16_t trust_power;
            int16_t trust_remove;
        } advanced;
        struct {
            int16_t schedule_mode;
            int16_t sunrise_temp;
            int16_t sunset_temp;
            int16_t sunrise_time;
            int16_t sunset_time;
        } colour;
        struct {
            int16_t sound;
            int16_t soundvol;
            int16_t chime;
            int16_t bgm;
            int16_t bgmvol;
            char startup[MAX_BUFFER_SIZE];
            int16_t brightness;
            int16_t volume;
            int16_t rgb;
            char language[MAX_BUFFER_SIZE];
            int16_t theme_scaling;
            int16_t theme_resolution;
            int16_t theme_resolution_width;
            int16_t theme_resolution_height;
            int16_t hkdpad;
            int16_t hkshot;
            int16_t audiosink;
        } general;
        struct {
            int16_t resolution;
            int16_t space;
            int16_t depth;
            int16_t range;
            int16_t scan;
            int16_t audio;
        } hdmi;
        struct {
            int16_t monitor;
            int16_t boot;
            int16_t wake;
            int16_t compat;
            int16_t async_load;
            int16_t con_retry;
            int16_t wait;
            int16_t mod_retry;
            int16_t proxy_enabled;
            int16_t proxy_type;
            char proxy_server[MAX_BUFFER_SIZE];
            char proxy_noproxy[MAX_BUFFER_SIZE];
        } network;
        struct {
            int16_t gen_alpha;
            int16_t gen_anchor;
            int16_t gen_scale;
            int16_t bat_alpha;
            int16_t bat_anchor;
            int16_t bat_scale;
            int16_t vol_alpha;
            int16_t vol_anchor;
            int16_t vol_scale;
            int16_t bri_alpha;
            int16_t bri_anchor;
            int16_t bri_scale;
        } overlay;
        struct {
            int16_t low_battery;
            int16_t shutdown;
            int16_t saver_type;
            int16_t saver_speed;
            struct {
                int16_t display;
                int16_t sleep;
                int16_t mute;
            } idle;
            struct {
                char dflt[MAX_BUFFER_SIZE];
                char idle[MAX_BUFFER_SIZE];
            } gov;
        } power;
        struct {
            int16_t mode;
            int16_t bright;
            int16_t breath_speed;
            int16_t colour_l;
            int16_t colour_r;
            int16_t colour_m;
            int16_t colour_f1;
            int16_t colour_f2;
            int16_t combo;
            int16_t backend;
        } rgb;
        struct {
            char name[MAX_BUFFER_SIZE];
            int16_t list_size;
            int16_t header_size;
            int16_t footer_size;
            int16_t panel_size;
        } font;
        struct {
            int16_t header_height;
            int16_t footer_height;
            int16_t content_item_count;
            int16_t glyph_size_list;
            int16_t glyph_size_footer;
            int16_t glyph_size_header;
            int16_t glyph_size_grid;
            int16_t label_width;
        } themeopt;
        struct {
            int16_t layout;
        } remap;
    } settings;

    struct {
        int16_t dflt;
        int16_t collection;
        int16_t history;
    } sort;

    struct {
        int16_t sort;
        int16_t battery;
        int16_t network;
        int16_t header_title;
        int16_t dialogue_transition;
        int16_t bluetooth;
        int16_t clock;
        int16_t overlay_image;
        int16_t overlay_transparency;
        int16_t grid_mode_content;
        int16_t box_art;
        int16_t box_art_align;
        int16_t box_art_hide;
        int16_t box_art_scale;
        int16_t box_art_transition;
        int16_t video_preview;
        int16_t content_width;
        int16_t name;
        int16_t dash;
        int16_t launch_swap;
        int16_t shuffle;
        int16_t hidden;
        int16_t friendly_folder;
        int16_t the_title_format;
        int16_t title_include_root_drive;
        int16_t folder_item_count;
        int16_t display_empty_folder;
        int16_t menu_counter_folder;
        int16_t menu_counter_file;
        int16_t video_wallpaper;
        int16_t background_scale;
        int16_t launchsplash;
        int16_t blackfade;
        int16_t content_collect;
        int16_t content_history;
        int16_t mixed_content;
        int16_t forward_history;
        int16_t name_scroll;
        int16_t label_scroll_speed;
        int16_t list_glyph;
        int16_t selection_animation;
        int16_t selection_style;
        int16_t render_shadows;
    } visual;

    struct {
        int16_t auto_connect;
    } bluetooth;

    struct {
        int16_t sshd;
        int16_t sftp_go;
        int16_t ttyd;
        int16_t syncthing;
        int16_t tailscaled;
    } web;

    struct {
        int16_t vm_swap;
        int16_t dirty_ratio;
        int16_t dirty_back;
        int16_t cache;
        int16_t merge;
        int16_t requests;
        int16_t io_stats;
        int16_t read_ahead;
        int16_t idle_flush;
        int16_t page_cluster;
        int16_t child_first;
        int16_t time_slice;
        int16_t tune_scale;
        char card_mode[MAX_BUFFER_SIZE];
        char state[MAX_BUFFER_SIZE];
    } danger;
};

void load_config(struct mux_config *config);
