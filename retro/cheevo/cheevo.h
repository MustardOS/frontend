#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../core/libretro.h"

typedef enum {
    cheevo_status_disabled = 0,
    cheevo_status_signed_out,
    cheevo_status_signing_in,
    cheevo_status_identifying,
    cheevo_status_active_softcore,
    cheevo_status_active_hardcore,
    cheevo_status_offline,
    cheevo_status_unsupported,
    cheevo_status_failed
} cheevo_status;

typedef enum {
    cheevo_notifications_disabled = 0,
    cheevo_notifications_basic,
    cheevo_notifications_detailed
} cheevo_notification_mode;

typedef enum {
    cheevo_sort_alphanumeric_ascending = 0,
    cheevo_sort_alphanumeric_descending,
    cheevo_sort_points_highest,
    cheevo_sort_points_lowest,
    cheevo_sort_percentage_common,
    cheevo_sort_percentage_rarest,
    cheevo_sort_unlocked,
    cheevo_sort_easy_points,
    cheevo_sort_count
} cheevo_achievement_sort;

typedef enum { cheevo_view_achievements = 0, cheevo_view_leaderboards, cheevo_view_count } cheevo_achievement_view;

typedef struct {
    int enabled;
    int hardcore;
    int unofficial;
    int notifications;
    char username[128];
    char display_name[128];
    uint32_t score;
    char game_title[256];
    char rich_presence[256];
    char failure[256];
    uint32_t unlocked;
    uint32_t total;
} cheevo_info;

typedef enum { cheevo_game_entry_achievement = 0, cheevo_game_entry_leaderboard } cheevo_game_entry_type;

typedef struct {
    cheevo_game_entry_type type;
    uint32_t id;
    char title[128];
    char description[256];
    char progress[32];
    char preview_path[256];
    uint32_t points;
    float rarity;
    int unlocked;
    int active;
} cheevo_game_entry;

typedef enum {
    cheevo_leaderboard_idle = 0,
    cheevo_leaderboard_loading,
    cheevo_leaderboard_ready,
    cheevo_leaderboard_failed
} cheevo_leaderboard_state;

typedef struct {
    uint32_t rank;
    char user[128];
    char score[32];
    int current_user;
} cheevo_leaderboard_rank;

int cheevo_init(const char *content_path);

int cheevo_hash_content(const char *content_path, char out[33]);

void cheevo_shutdown(void);

void cheevo_tick(void);

int cheevo_needs_tick(void);

void cheevo_present_tick(void);

int cheevo_needs_present_tick(void);

void cheevo_do_frame(void);

int cheevo_needs_frame(void);

void cheevo_idle(void);

int cheevo_is_starting(void);

void cheevo_play_without(void);

void cheevo_reset(void);

void cheevo_set_memory_map(const struct retro_memory_map *map);

void cheevo_refresh_memory(void);

void cheevo_set_core_support(int supported);

int cheevo_core_support(void);

void cheevo_set_netplay_active(int active);

int cheevo_netplay_active(void);

cheevo_status cheevo_get_status(void);

int cheevo_is_configured(void);

void cheevo_get_info(cheevo_info *info);

unsigned cheevo_game_entries(cheevo_game_entry_type type, cheevo_game_entry *entries, unsigned capacity);

int cheevo_has_game_entries(void);

int cheevo_leaderboard_fetch(uint32_t leaderboard_id);

cheevo_leaderboard_state cheevo_leaderboard_get_state(void);

unsigned cheevo_leaderboard_ranks(cheevo_leaderboard_rank *entries, unsigned capacity, unsigned *total);

int cheevo_login(const char *username, const char *password);

void cheevo_logout(void);

int cheevo_set_enabled(int enabled);

int cheevo_set_hardcore(int enabled);

int cheevo_set_unofficial(int enabled);

int cheevo_set_notifications(cheevo_notification_mode mode);

cheevo_achievement_sort cheevo_get_achievement_sort(void);

cheevo_achievement_view cheevo_get_achievement_view(void);

int cheevo_set_achievement_preferences(cheevo_achievement_sort sort, cheevo_achievement_view view);

int cheevo_refresh_data(void);

int cheevo_hardcore_active(void);

int cheevo_restricted(void);

int cheevo_can_pause(uint32_t *frames_remaining);

size_t cheevo_progress_size(void);

int cheevo_progress_save(void *data, size_t size);

int cheevo_progress_load(const void *data, size_t size);

void cheevo_progress_reset(void);
