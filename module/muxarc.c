#include <math.h>

#include "muxshare.h"

#define ARC_VARIANT_COUNT  6
#define ARC_VISIBLE_COUNT  5
#define ARC_MENU_ROWS      7
#define ARC_TRAIL_COUNT    12
#define ARC_AIM_DOTS       9
#define ARC_TERRAIN_STRIPS 320
#define ARC_TICK_MS        16
#define ARC_SECRET_MS      1500

enum arc_screen {
    arc_screen_setup,
    arc_screen_play,
    arc_screen_pause,
    arc_screen_round,
    arc_screen_match,
};

enum arc_setup_row {
    arc_setup_start,
    arc_setup_players,
    arc_setup_length,
    arc_setup_gravity,
    arc_setup_player_one,
    arc_setup_player_two,
    arc_setup_quit,
    arc_setup_count,
};

enum arc_difficulty {
    arc_difficulty_pushover,
    arc_difficulty_easy,
    arc_difficulty_medium,
    arc_difficulty_hard,
    arc_difficulty_insane,
    arc_difficulty_count,
};

typedef struct {
    const char *name;
    const char *glyph;
    uint32_t colour;
    uint32_t cap;
    uint32_t ink;
    const char *trait;
    int health;
    int damage;
    int defence;
    int blast;
    int launch;
    int weight;
    int guide;
} arc_variant;

typedef struct {
    float x;
    float y;
    int visible;
} arc_trail_sample;

static const arc_variant arc_variants[ARC_VARIANT_COUNT] = {
    {"Ketchup", "K", 0xD73B32, 0xF4E8D1, 0xFFFFFF, "Hits hardest, but falls short", 100, 125, 100, 100, 92, 100, 6},
    {"Mustard", "M", 0xE3B817, 0xF4E8D1, 0x3A2400, "Longest aim guide, but a small blast", 100, 100, 100, 80, 100, 100, 9},
    {"Wasabi", "W", 0x6CA83D, 0xE6D4B0, 0x10260A, "Huge blast, but fragile", 80, 105, 100, 135, 100, 95, 6},
    {"Pickles", "P", 0x2F8F6B, 0xD7B85C, 0x031510, "Tough as brine, but soft hits", 130, 80, 85, 100, 95, 105, 6},
    {"Relish", "R", 0x7C9F35, 0xE6D4B0, 0x13220A, "Longest reach, but a short aim guide", 95, 95, 100, 95, 115, 95, 3},
    {"Mayo", "Y", 0xF1E5C8, 0xC9B99B, 0x4A3A1C, "Strong all round, but heavy shots drop fast", 120, 115, 90, 115, 105, 125, 7},
};

typedef struct {
    const char *name;
    const char *trait;
    int pull;
} arc_planet;

static const arc_planet arc_planets[] = {
    {"Mercury", "38% of Earth, shots sail a long way", 61},
    {"Venus", "90% of Earth, a touch lighter than home", 95},
    {"Earth", "The usual pull", 100},
    {"Mars", "38% of Earth, long and lazy arcs", 62},
    {"Jupiter", "253% of Earth, shots drop like stones", 159},
    {"Saturn", "106% of Earth, a touch heavier than home", 103},
    {"Uranus", "89% of Earth, a little lighter than home", 94},
    {"Neptune", "114% of Earth, heavier shots and shorter reach", 107},
};

#define ARC_PLANET_COUNT ((int) (sizeof(arc_planets) / sizeof(arc_planets[0])))
#define ARC_PLANET_EARTH 2

static const char *arc_difficulty_traits[arc_difficulty_count] = {
    "Barely aims at all",
    "Misses more often than not",
    "A fair fight",
    "Rarely misses",
    "Hardly ever misses",
};

static enum arc_screen arc_screen;
static enum arc_screen arc_previous_screen;

static lv_obj_t *arc_root;
static lv_obj_t *arc_terrain[ARC_TERRAIN_STRIPS];
static lv_obj_t *arc_menu;
static lv_obj_t *arc_menu_title;
static lv_obj_t *arc_menu_subtitle;
static lv_obj_t *arc_menu_row[ARC_MENU_ROWS];
static lv_obj_t *arc_menu_label[ARC_MENU_ROWS];
static lv_obj_t *arc_menu_value[ARC_MENU_ROWS];
static lv_obj_t *arc_hud;
static lv_obj_t *arc_hud_score;
static lv_obj_t *arc_hud_values;
static lv_obj_t *arc_hud_round;
static lv_obj_t *arc_actor[2];
static lv_obj_t *arc_actor_cap[2];
static lv_obj_t *arc_actor_glyph[2];
static lv_obj_t *arc_health_bar[2];
static lv_obj_t *arc_projectile;
static lv_obj_t *arc_trail[ARC_TRAIL_COUNT];
static lv_obj_t *arc_aim[ARC_AIM_DOTS];
static lv_obj_t *arc_explosion;
static lv_timer_t *arc_timer;

static int16_t *arc_ground;
static int arc_width;
static int arc_height;
static int arc_terrain_count;
static int arc_menu_count;
static int arc_menu_selected;
static const char *arc_menu_text[ARC_MENU_ROWS];

static int arc_players = 1;
static int arc_best_of = 3;
static int arc_planet_selected = ARC_PLANET_EARTH;
static int arc_difficulty = arc_difficulty_medium;
static int arc_wins_needed = 2;
static int arc_variant_selected[2] = {1, 0};
static int arc_score[2];
static int arc_health[2];
static int arc_angle[2] = {45, 45};
static int arc_power[2] = {60, 60};
static int arc_turn;
static int arc_round_number;
static int arc_mayo_player = -1;
static uint32_t arc_mayo_since;

static float arc_actor_x[2];
static float arc_actor_y[2];
static int arc_actor_w;
static int arc_actor_h;

static int arc_shot_active;
static float arc_shot_x;
static float arc_shot_y;
static float arc_shot_vx;
static float arc_shot_vy;
static float arc_shot_age;
static float arc_shot_gravity;
static uint32_t arc_shot_tick;
static arc_trail_sample arc_trail_points[ARC_TRAIL_COUNT];
static int arc_trail_head;

static int arc_explosion_active;
static uint32_t arc_explosion_started;
static uint32_t arc_cpu_deadline;
static int arc_round_winner = -2;

static int arc_rand(const int maximum) {
    if (maximum <= 1) return 0;
    return (int) (random() % maximum);
}

static int arc_clamp(const int value, const int low, const int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int arc_scale(const int value) {
    const int scaled = value * arc_height / 400;
    return scaled > 0 ? scaled : 1;
}

static uint32_t arc_panel_colour(void) {
    if (theme.list_default.background_alpha > 0) return theme.list_default.background;
    return theme.footer.background;
}

static uint32_t arc_focus_colour(void) {
    if (theme.dialogue.selection_alpha > 0) return theme.dialogue.selection;
    return theme.list_focus.text;
}

static void arc_set_hidden(lv_obj_t *object, const int hidden) {
    if (!object) return;
    if (hidden)
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static void arc_set_terrain_hidden(const int hidden) {
    for (int index = 0; index < ARC_TERRAIN_STRIPS; index++)
        arc_set_hidden(arc_terrain[index], hidden || index >= arc_terrain_count);
}

static void arc_style_clear(lv_obj_t *object) {
    lv_obj_remove_style_all(object);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *arc_label(lv_obj_t *parent, const char *text, const uint32_t colour) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_color(label, lv_color_hex(colour), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    return label;
}

static void arc_refresh_footer(void) {
    nav_hide_all();

    if (arc_screen == arc_screen_play) {
        setup_nav((struct nav_bar[]) {
            {ui_lbl_nav_lr_glyph, "", 0},
            {ui_lbl_nav_lr, TRS("Aim / Power"), 0},
            {ui_lbl_nav_a_glyph, "", 0},
            {ui_lbl_nav_a, TRS("Fire"), 0},
            {ui_lbl_nav_b_glyph, "", 0},
            {ui_lbl_nav_b, TRS("Pause"), 0},
            {NULL, NULL, 0},
        });
        return;
    }

    setup_nav((struct nav_bar[]) {
        {ui_lbl_nav_lr_glyph, "", 0},
        {ui_lbl_nav_lr, arc_screen == arc_screen_setup ? TRS("Change") : TRS("Choose"), 0},
        {ui_lbl_nav_a_glyph, "", 0},
        {ui_lbl_nav_a, lang.generic.select, 0},
        {ui_lbl_nav_b_glyph, "", 0},
        {ui_lbl_nav_b, lang.generic.back, 0},
        {NULL, NULL, 0},
    });
}

static void arc_style_menu(void) {
    for (int index = 0; index < ARC_MENU_ROWS; index++) {
        const int focused = index == arc_menu_selected && index < arc_menu_count;
        lv_obj_set_style_bg_color(arc_menu_row[index], lv_color_hex(theme.dialogue.selection), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(
            arc_menu_row[index], focused ? theme.dialogue.selection_alpha : LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_set_style_text_color(
            arc_menu_label[index], lv_color_hex(focused ? theme.dialogue.option : theme.dialogue.content),
            MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_set_style_text_color(
            arc_menu_value[index], lv_color_hex(focused ? theme.dialogue.option : theme.dialogue.content),
            MU_OBJ_MAIN_DEFAULT
        );
        if (focused) {
            lv_obj_add_state(arc_menu_label[index], LV_STATE_CHECKED);
            lv_obj_add_state(arc_menu_value[index], LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(arc_menu_label[index], LV_STATE_CHECKED);
            lv_obj_clear_state(arc_menu_value[index], LV_STATE_CHECKED);
        }
    }
}

static int arc_trait_width(void) {
    const lv_font_t *font = lv_obj_get_style_text_font(arc_menu_subtitle, LV_PART_MAIN);
    const lv_coord_t letter_space = lv_obj_get_style_text_letter_space(arc_menu_subtitle, LV_PART_MAIN);
    int widest = 0;

    for (int index = 0; index < ARC_VARIANT_COUNT + arc_difficulty_count + ARC_PLANET_COUNT; index++) {
        const char *text = index < ARC_VARIANT_COUNT ? arc_variants[index].trait
                         : index < ARC_VARIANT_COUNT + arc_difficulty_count
                             ? arc_difficulty_traits[index - ARC_VARIANT_COUNT]
                             : arc_planets[index - ARC_VARIANT_COUNT - arc_difficulty_count].trait;
        lv_point_t size;
        lv_txt_get_size(&size, TRS((char *) text), font, letter_space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > widest) widest = size.x;
    }

    return widest;
}

static void arc_show_menu(
    const enum arc_screen screen, const char *title, const char *subtitle, const char *const *options, const int count
) {
    arc_screen = screen;
    arc_menu_count = arc_clamp(count, 0, ARC_MENU_ROWS);
    arc_menu_selected = 0;

    lv_label_set_text(arc_menu_title, title ? title : "");
    lv_label_set_text(arc_menu_subtitle, subtitle ? subtitle : "");

    const int row_h = arc_clamp(arc_scale(42), 34, 58);
    const int gap = arc_clamp(arc_scale(6), 4, 10);
    const int title_h = title && title[0] ? row_h : 0;
    const int subtitle_h = subtitle && subtitle[0] ? row_h : 0;
    const int panel_h = 32 + title_h + subtitle_h + arc_menu_count * row_h + (arc_menu_count - 1) * gap;
    int panel_w = arc_clamp(arc_width * 3 / 5, 280, 660);
    if (screen == arc_screen_setup) {
        const int trait_w = arc_trait_width() + 48;
        if (trait_w > panel_w) panel_w = arc_clamp(trait_w, 280, arc_width - 16);
    }

    lv_obj_set_size(arc_menu, panel_w, arc_clamp(panel_h, 160, arc_height - 16));
    lv_obj_center(arc_menu);
    lv_obj_set_y(arc_menu_title, 10);
    lv_obj_set_y(arc_menu_subtitle, 10 + title_h);
    lv_obj_set_height(
        arc_menu_subtitle, lv_font_get_line_height(lv_obj_get_style_text_font(arc_menu_subtitle, LV_PART_MAIN))
    );

    int row_y = 16 + title_h + subtitle_h;
    for (int index = 0; index < ARC_MENU_ROWS; index++) {
        if (index < arc_menu_count) {
            arc_menu_text[index] = options[index];
            lv_label_set_text(arc_menu_label[index], arc_menu_text[index]);
            lv_obj_set_size(arc_menu_row[index], panel_w - 32, row_h);
            lv_obj_set_pos(arc_menu_row[index], 16, row_y);
            lv_obj_set_width(arc_menu_label[index], LV_PCT(100));
            lv_obj_set_style_text_align(arc_menu_label[index], LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
            lv_obj_center(arc_menu_label[index]);
            arc_set_hidden(arc_menu_value[index], 1);
            arc_set_hidden(arc_menu_row[index], 0);
            row_y += row_h + gap;
        } else {
            arc_menu_text[index] = NULL;
            arc_set_hidden(arc_menu_row[index], 1);
        }
    }

    const int keep_arena = screen == arc_screen_pause || screen == arc_screen_round || screen == arc_screen_match;
    arc_set_terrain_hidden(!keep_arena);
    arc_set_hidden(arc_hud, !keep_arena);
    for (int index = 0; index < 2; index++)
        arc_set_hidden(arc_actor[index], !keep_arena);
    for (int index = 0; index < ARC_AIM_DOTS; index++)
        arc_set_hidden(arc_aim[index], 1);
    arc_set_hidden(arc_menu, 0);
    arc_style_menu();
    arc_refresh_footer();
}

static void arc_refresh_setup(void) {
    static const char *difficulty_names[arc_difficulty_count] = {
        "Pushover",
        "Easy",
        "Medium",
        "Hard",
        "Insane",
    };
    const char *labels[arc_setup_count] = {
        TRS("Start"),
        TRS("Players"),
        TRS("Match Length"),
        TRS("Gravity"),
        TRS("Player 1"),
        arc_players == 1 ? TRS("Difficulty") : TRS("Player 2"),
        TRS("Quit"),
    };
    const char *values[arc_setup_count] = {
        "",
        arc_players == 1 ? TRS("1 Player") : TRS("2 Players"),
        arc_best_of == 3   ? TRS("Best of 3")
        : arc_best_of == 5 ? TRS("Best of 5")
                           : TRS("Best of 7"),
        TRS((char *) arc_planets[arc_planet_selected].name),
        TRS((char *) arc_variants[arc_variant_selected[0]].name),
        arc_players == 1 ? TRS((char *) difficulty_names[arc_difficulty])
                         : TRS((char *) arc_variants[arc_variant_selected[1]].name),
        "",
    };

    const char *trait = " ";
    if (arc_menu_selected == arc_setup_gravity)
        trait = arc_planets[arc_planet_selected].trait;
    else if (arc_menu_selected == arc_setup_player_one)
        trait = arc_variants[arc_variant_selected[0]].trait;
    else if (arc_menu_selected == arc_setup_player_two)
        trait = arc_players == 1 ? arc_difficulty_traits[arc_difficulty] : arc_variants[arc_variant_selected[1]].trait;
    lv_label_set_text(arc_menu_subtitle, TRS((char *) trait));

    for (int index = 0; index < arc_setup_count; index++) {
        lv_label_set_text(arc_menu_label[index], labels[index]);
        if (index == arc_setup_start || index == arc_setup_quit) {
            lv_obj_set_width(arc_menu_label[index], LV_PCT(100));
            lv_obj_set_style_text_align(arc_menu_label[index], LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
            lv_obj_center(arc_menu_label[index]);
            arc_set_hidden(arc_menu_value[index], 1);
            continue;
        }

        lv_obj_set_width(arc_menu_label[index], LV_PCT(48));
        lv_obj_set_style_text_align(arc_menu_label[index], LV_TEXT_ALIGN_LEFT, MU_OBJ_MAIN_DEFAULT);
        lv_obj_align(arc_menu_label[index], LV_ALIGN_LEFT_MID, arc_scale(10), 0);
        lv_label_set_text(arc_menu_value[index], values[index]);
        lv_obj_set_width(arc_menu_value[index], LV_PCT(48));
        lv_obj_set_style_text_align(arc_menu_value[index], LV_TEXT_ALIGN_RIGHT, MU_OBJ_MAIN_DEFAULT);
        lv_obj_align(arc_menu_value[index], LV_ALIGN_RIGHT_MID, -arc_scale(10), 0);
        arc_set_hidden(arc_menu_value[index], 0);
    }

    arc_style_menu();
}

static void arc_show_setup(void) {
    static const char *options[arc_setup_count] = {"Start", "Players", "Match Length", "Gravity", "Player 1", "Player 2", "Quit"};
    arc_show_menu(arc_screen_setup, NULL, " ", options, arc_setup_count);
    arc_refresh_setup();
}

static void arc_update_aim(void) {
    if (arc_screen != arc_screen_play || arc_shot_active || arc_explosion_active) {
        for (int index = 0; index < ARC_AIM_DOTS; index++)
            arc_set_hidden(arc_aim[index], 1);
        return;
    }

    const float radians = (float) arc_angle[arc_turn] * (float) M_PI / 180.0f;
    const float direction = arc_turn ? -1.0f : 1.0f;
    const float start_x = arc_actor_x[arc_turn] + arc_actor_w * 0.5f;
    const float start_y = arc_actor_y[arc_turn] - arc_scale(2);
    const float spacing = (float) arc_scale(5) + (float) arc_scale(12) * (float) arc_power[arc_turn] / 100.0f;
    const int guide = arc_variants[arc_variant_selected[arc_turn]].guide;

    for (int index = 0; index < ARC_AIM_DOTS; index++) {
        if (index >= guide) {
            arc_set_hidden(arc_aim[index], 1);
            continue;
        }
        const float distance = spacing * (float) (index + 1);
        const int x = (int) lroundf(start_x + direction * cosf(radians) * distance);
        const int y = (int) lroundf(start_y - sinf(radians) * distance);
        lv_obj_set_pos(arc_aim[index], x - arc_scale(2), y - arc_scale(2));
        arc_set_hidden(arc_aim[index], 0);
    }
}

static void arc_update_hud(void) {
    if (arc_variant_selected[0] < 0 || arc_variant_selected[1] < 0) return;

    char score[256];
    snprintf(
        score, sizeof(score), "%s  %d  -  %d  %s", TRS((char *) arc_variants[arc_variant_selected[0]].name),
        arc_score[0], arc_score[1], TRS((char *) arc_variants[arc_variant_selected[1]].name)
    );
    lv_label_set_text(arc_hud_score, score);

    char values[256];
    snprintf(
        values, sizeof(values), "%s %d   %s %d%%", TRS("Angle"), arc_angle[arc_turn], TRS("Power"), arc_power[arc_turn]
    );
    lv_label_set_text(arc_hud_values, values);

    char round_text[64];
    snprintf(round_text, sizeof(round_text), "%s %d", TRS("Round"), arc_round_number);
    lv_label_set_text(arc_hud_round, round_text);

    const int right_full = arc_variants[arc_variant_selected[1]].health;
    lv_bar_set_value(arc_health_bar[0], arc_health[0], LV_ANIM_OFF);
    lv_bar_set_value(arc_health_bar[1], right_full, LV_ANIM_OFF);
    lv_bar_set_start_value(arc_health_bar[1], right_full - arc_health[1], LV_ANIM_OFF);
    arc_update_aim();
}

static void arc_position_actor(const int player) {
    const int centre = arc_clamp((int) lroundf(arc_actor_x[player] + arc_actor_w * 0.5f), 0, arc_width - 1);
    arc_actor_y[player] = arc_ground[centre] - arc_actor_h;
    lv_obj_set_pos(arc_actor[player], (int) lroundf(arc_actor_x[player]), (int) lroundf(arc_actor_y[player]));
}

static void arc_draw_ground(void) {
    if (!arc_ground) return;

    const lv_color_t ground_colour =
        lv_color_mix(lv_color_hex(arc_focus_colour()), lv_color_hex(arc_panel_colour()), LV_OPA_30);
    const int strip_count = arc_clamp(arc_width / 3, 64, ARC_TERRAIN_STRIPS);
    arc_terrain_count = strip_count;
    for (int index = 0; index < ARC_TERRAIN_STRIPS; index++) {
        if (index >= strip_count) {
            arc_set_hidden(arc_terrain[index], 1);
            continue;
        }

        const int left = index * arc_width / strip_count;
        const int right = (index + 1) * arc_width / strip_count;
        const int sample = arc_clamp((left + right) / 2, 0, arc_width - 1);
        const int top = arc_clamp(arc_ground[sample], 0, arc_height - 1);
        lv_obj_set_pos(arc_terrain[index], left, top);
        lv_obj_set_size(arc_terrain[index], right - left + 1, arc_height - top);
        lv_obj_set_style_bg_color(arc_terrain[index], ground_colour, MU_OBJ_MAIN_DEFAULT);
        arc_set_hidden(arc_terrain[index], 0);
    }
}

static void arc_flatten_pad(const int centre) {
    const int core = arc_actor_w;
    const int blend = arc_clamp(arc_width / 12, arc_actor_w, arc_width / 6);
    const int core_start = arc_clamp(centre - core, 0, arc_width - 1);
    const int core_end = arc_clamp(centre + core, 0, arc_width - 1);

    int total = 0;
    for (int x = core_start; x <= core_end; x++)
        total += arc_ground[x];
    const float level = (float) total / (float) (core_end - core_start + 1);

    const int start = arc_clamp(centre - core - blend, 0, arc_width - 1);
    const int end = arc_clamp(centre + core + blend, 0, arc_width - 1);
    for (int x = start; x <= end; x++) {
        const int distance = abs(x - centre);
        float weight = 1.0f;
        if (distance > core) weight = 0.5f * (1.0f + cosf((float) (distance - core) / (float) blend * (float) M_PI));
        arc_ground[x] = (int16_t) lroundf((float) arc_ground[x] * (1.0f - weight) + level * weight);
    }
}

static void arc_generate_ground(void) {
    const int top = arc_clamp(arc_height * 38 / 100, arc_scale(90), arc_height - arc_scale(120));
    const int bottom = arc_height - arc_scale(34);
    const int range = bottom - top;
    const float phase = (float) arc_rand(628) / 100.0f;
    const float cycles = 1.6f + (float) arc_rand(140) / 100.0f;
    const float feature_x = (float) (arc_width / 2 + arc_rand(arc_width / 10 + 1) - arc_width / 20);
    const float feature_spread = (float) arc_width / 9.0f;
    const float feature_size = (float) range * (0.24f + (float) arc_rand(15) / 100.0f) * (arc_rand(4) ? -1.0f : 1.0f);
    int drift = 0;

    for (int x = 0; x < arc_width; x++) {
        drift = arc_clamp(drift + arc_rand(3) - 1, -range / 12, range / 12);
        const float progress = (float) x / (float) arc_width;
        const float primary = sinf(progress * cycles * 2.0f * (float) M_PI + phase) * (float) range * 0.27f;
        const float detail =
            sinf(progress * cycles * 2.3f * 2.0f * (float) M_PI + phase * 0.63f) * (float) range * 0.08f;
        const float offset = (float) x - feature_x;
        const float feature =
            feature_size * expf(-(offset * offset) / (2.0f * feature_spread * feature_spread));
        const float half = (float) range * 0.5f;
        const float shape = tanhf((primary + detail + feature + (float) drift) / half * 0.9f) / tanhf(0.9f);
        const int height = top + range / 2 + (int) lroundf(shape * half);
        arc_ground[x] = (int16_t) arc_clamp(height, top, bottom);
    }

    int16_t *smooth = malloc((size_t) arc_width * sizeof(*smooth));
    if (smooth) {
        const int reach = arc_clamp(arc_width / 48, 4, 20);
        for (int pass = 0; pass < 3; pass++) {
            for (int x = 0; x < arc_width; x++) {
                const int start = arc_clamp(x - reach, 0, arc_width - 1);
                const int end = arc_clamp(x + reach, 0, arc_width - 1);
                int total = 0;
                for (int sample = start; sample <= end; sample++)
                    total += arc_ground[sample];
                smooth[x] = (int16_t) (total / (end - start + 1));
            }
            memcpy(arc_ground, smooth, (size_t) arc_width * sizeof(*smooth));
        }
        free(smooth);
    }

    const int left = arc_width * 14 / 100;
    const int right = arc_width * 86 / 100;
    arc_flatten_pad(left);
    arc_flatten_pad(right);

    arc_actor_x[0] = left - arc_actor_w * 0.5f;
    arc_actor_x[1] = right - arc_actor_w * 0.5f;
    arc_position_actor(0);
    arc_position_actor(1);
    arc_draw_ground();
}

static void arc_apply_variant(const int player) {
    const arc_variant *variant = &arc_variants[arc_variant_selected[player]];
    lv_obj_set_style_bg_color(arc_actor[player], lv_color_hex(variant->colour), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(arc_actor_cap[player], lv_color_hex(variant->cap), MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(arc_actor_glyph[player], variant->glyph);
    lv_obj_set_style_text_color(arc_actor_glyph[player], lv_color_hex(variant->ink), MU_OBJ_MAIN_DEFAULT);
    lv_bar_set_range(arc_health_bar[player], 0, variant->health);
    lv_obj_set_style_bg_color(
        arc_health_bar[player], lv_color_hex(variant->colour), LV_PART_INDICATOR | MU_OBJ_MAIN_DEFAULT
    );
}

static void arc_clear_shot(void) {
    arc_shot_active = 0;
    arc_set_hidden(arc_projectile, 1);
    for (int index = 0; index < ARC_TRAIL_COUNT; index++) {
        arc_trail_points[index].visible = 0;
        arc_set_hidden(arc_trail[index], 1);
    }
}

static void arc_prepare_cpu(void) {
    static const int angle_error[arc_difficulty_count] = {16, 10, 5, 2, 0};
    static const int power_error[arc_difficulty_count] = {30, 20, 11, 5, 1};
    static const int delay[arc_difficulty_count] = {1100, 900, 700, 550, 400};
    arc_cpu_deadline = 0;
    if (arc_players != 1 || arc_turn != 1 || arc_screen != arc_screen_play) return;

    const float distance = fabsf(arc_actor_x[1] - arc_actor_x[0]);
    const int angle = 36 + arc_rand(26);
    const float radians = (float) angle * (float) M_PI / 180.0f;
    const arc_variant *cpu = &arc_variants[arc_variant_selected[1]];
    const float gravity = (float) arc_height * 1.2f * (float) cpu->weight / 100.0f
                        * (float) arc_planets[arc_planet_selected].pull / 100.0f;
    const float sine = sinf(2.0f * radians);
    const float velocity = sine > 0.05f ? sqrtf(distance * gravity / sine) : (float) arc_width * 0.65f;
    const float launch = (float) cpu->launch / 100.0f;
    int power = (int) lroundf((velocity / launch / (float) arc_width - 0.25f) / 0.008f);

    const int angle_spread = angle_error[arc_difficulty];
    const int power_spread = power_error[arc_difficulty];
    arc_angle[1] = arc_clamp(angle + arc_rand(angle_spread * 2 + 1) - angle_spread, 15, 80);
    arc_power[1] = arc_clamp(power + arc_rand(power_spread * 2 + 1) - power_spread, 20, 100);
    arc_cpu_deadline = lv_tick_get() + (uint32_t) delay[arc_difficulty];
    arc_update_hud();
}

static void arc_show_play(void) {
    arc_screen = arc_screen_play;
    arc_set_hidden(arc_menu, 1);
    arc_set_hidden(arc_hud, arc_shot_active || arc_explosion_active);
    arc_set_hidden(arc_actor[0], 0);
    arc_set_hidden(arc_actor[1], 0);
    arc_set_hidden(arc_explosion, !arc_explosion_active);
    arc_refresh_footer();
    arc_update_hud();
    arc_prepare_cpu();
}

static void arc_begin_round(void) {
    arc_round_number++;
    arc_health[0] = arc_variants[arc_variant_selected[0]].health;
    arc_health[1] = arc_variants[arc_variant_selected[1]].health;
    arc_angle[0] = 45;
    arc_angle[1] = 45;
    arc_power[0] = 60;
    arc_power[1] = 60;
    arc_turn = (arc_round_number - 1) & 1;
    arc_round_winner = -2;
    arc_explosion_active = 0;
    arc_set_hidden(arc_explosion, 1);
    arc_clear_shot();
    arc_apply_variant(0);
    arc_apply_variant(1);
    arc_generate_ground();
    arc_show_play();
}

static void arc_begin_match(void) {
    arc_score[0] = 0;
    arc_score[1] = 0;
    arc_round_number = 0;
    arc_wins_needed = arc_best_of / 2 + 1;
    arc_begin_round();
}

static void arc_pick_cpu(void) {
    int choices[ARC_VARIANT_COUNT];
    int count = 0;
    for (int index = 0; index < ARC_VISIBLE_COUNT; index++)
        if (index != arc_variant_selected[0]) choices[count++] = index;

    arc_variant_selected[1] = choices[arc_rand(count)];
}

static void arc_finish_round(void) {
    static const char *next_options[] = {"Next Round", "Setup"};
    static const char *match_options[] = {"Rematch", "Setup", "Exit"};
    char title[192];

    if (arc_round_winner >= 0) {
        arc_score[arc_round_winner]++;
        snprintf(
            title, sizeof(title), "%s %s", TRS((char *) arc_variants[arc_variant_selected[arc_round_winner]].name),
            TRS("wins the round!")
        );
    } else {
        snprintf(title, sizeof(title), "%s", TRS("Draw"));
    }

    arc_update_hud();

    if (arc_score[0] >= arc_wins_needed || arc_score[1] >= arc_wins_needed) {
        const int winner = arc_score[1] > arc_score[0] ? 1 : 0;
        snprintf(
            title, sizeof(title), "%s %s", TRS((char *) arc_variants[arc_variant_selected[winner]].name), TRS("wins!")
        );
        for (size_t index = 0; index < A_SIZE(match_options); index++)
            match_options[index] = TRS((char *) match_options[index]);
        arc_show_menu(arc_screen_match, title, NULL, match_options, A_SIZE(match_options));
    } else {
        for (size_t index = 0; index < A_SIZE(next_options); index++)
            next_options[index] = TRS((char *) next_options[index]);
        arc_show_menu(arc_screen_round, title, NULL, next_options, A_SIZE(next_options));
    }
}

static void arc_damage_ground(const int centre, const int radius) {
    const int start = arc_clamp(centre - radius, 0, arc_width - 1);
    const int end = arc_clamp(centre + radius, 0, arc_width - 1);
    for (int x = start; x <= end; x++) {
        const int dx = x - centre;
        const float depth = sqrtf((float) (radius * radius - dx * dx)) * 0.55f;
        arc_ground[x] = (int16_t) arc_clamp(arc_ground[x] + (int) lroundf(depth), 0, arc_height - 1);
    }
    arc_position_actor(0);
    arc_position_actor(1);
    arc_draw_ground();
}

static void arc_explode(const float x, const float y, const int direct_player) {
    const arc_variant *shooter = &arc_variants[arc_variant_selected[arc_turn]];
    const int base = arc_clamp(arc_width / 13, arc_scale(28), arc_scale(70));
    const int radius = arc_clamp(base * shooter->blast / 100, arc_scale(16), arc_scale(100));
    arc_shot_active = 0;
    arc_set_hidden(arc_projectile, 1);

    arc_explosion_active = 1;
    arc_explosion_started = lv_tick_get();
    lv_obj_set_size(arc_explosion, radius * 2, radius * 2);
    lv_obj_set_pos(arc_explosion, (int) lroundf(x) - radius, (int) lroundf(y) - radius);
    arc_set_hidden(arc_explosion, 0);

    for (int player = 0; player < 2; player++) {
        const float px = arc_actor_x[player] + arc_actor_w * 0.5f;
        const float py = arc_actor_y[player] + arc_actor_h * 0.5f;
        const float dx = px - x;
        const float dy = py - y;
        const float distance = sqrtf(dx * dx + dy * dy);
        const int scale = shooter->damage * arc_variants[arc_variant_selected[player]].defence;
        int damage = distance < radius ? (int) lroundf((1.0f - distance / radius) * 72.0f) : 0;
        if (player == direct_player && damage < 85) damage = 85;
        damage = damage * scale / 10000;
        arc_health[player] =
            arc_clamp(arc_health[player] - damage, 0, arc_variants[arc_variant_selected[player]].health);
    }

    arc_damage_ground(arc_clamp((int) lroundf(x), 0, arc_width - 1), radius);
    arc_update_hud();

    if (arc_health[0] <= 0 && arc_health[1] <= 0)
        arc_round_winner = -1;
    else if (arc_health[0] <= 0)
        arc_round_winner = 1;
    else if (arc_health[1] <= 0)
        arc_round_winner = 0;
    else
        arc_round_winner = -2;
}

static int arc_projectile_hit_player(void) {
    if (arc_shot_age < 0.20f) return -1;
    for (int player = 0; player < 2; player++) {
        const float inset_x = arc_actor_w * 0.20f;
        const float inset_y = arc_actor_h * 0.10f;
        if (arc_shot_x >= arc_actor_x[player] + inset_x && arc_shot_x <= arc_actor_x[player] + arc_actor_w - inset_x
            && arc_shot_y >= arc_actor_y[player] + inset_y && arc_shot_y <= arc_actor_y[player] + arc_actor_h - inset_y)
            return player;
    }
    return -1;
}

static void arc_push_trail(void) {
    arc_trail_head = (arc_trail_head + 1) % ARC_TRAIL_COUNT;
    arc_trail_points[arc_trail_head].x = arc_shot_x;
    arc_trail_points[arc_trail_head].y = arc_shot_y;
    arc_trail_points[arc_trail_head].visible = 1;

    for (int offset = 0; offset < ARC_TRAIL_COUNT; offset++) {
        const int index = (arc_trail_head - offset + ARC_TRAIL_COUNT) % ARC_TRAIL_COUNT;
        if (!arc_trail_points[index].visible) {
            arc_set_hidden(arc_trail[index], 1);
            continue;
        }
        const int size = arc_clamp(arc_scale(7) - offset / 3, 2, arc_scale(7));
        lv_obj_set_size(arc_trail[index], size, size);
        lv_obj_set_pos(
            arc_trail[index], (int) lroundf(arc_trail_points[index].x) - size / 2,
            (int) lroundf(arc_trail_points[index].y) - size / 2
        );
        lv_obj_set_style_bg_opa(arc_trail[index], arc_clamp(190 - offset * 12, 35, 190), MU_OBJ_MAIN_DEFAULT);
        arc_set_hidden(arc_trail[index], 0);
    }
}

static void arc_fire(void) {
    if (arc_screen != arc_screen_play || arc_shot_active || arc_explosion_active) return;
    if (arc_players == 1 && arc_turn == 1 && !arc_cpu_deadline) return;

    const float radians = (float) arc_angle[arc_turn] * (float) M_PI / 180.0f;
    const float direction = arc_turn ? -1.0f : 1.0f;
    const arc_variant *shooter = &arc_variants[arc_variant_selected[arc_turn]];
    const float velocity =
        (float) arc_width * (0.25f + 0.008f * (float) arc_power[arc_turn]) * (float) shooter->launch / 100.0f;

    arc_shot_gravity = (float) arc_height * 1.2f * (float) shooter->weight / 100.0f
                     * (float) arc_planets[arc_planet_selected].pull / 100.0f;
    arc_shot_x = arc_actor_x[arc_turn] + arc_actor_w * 0.5f;
    arc_shot_y = arc_actor_y[arc_turn] - arc_scale(3);
    arc_shot_vx = direction * cosf(radians) * velocity;
    arc_shot_vy = -sinf(radians) * velocity;
    arc_shot_age = 0.0f;
    arc_shot_tick = lv_tick_get();
    arc_shot_active = 1;
    arc_cpu_deadline = 0;
    arc_set_hidden(arc_hud, 1);
    arc_trail_head = 0;
    memset(arc_trail_points, 0, sizeof(arc_trail_points));
    lv_obj_set_style_bg_color(
        arc_projectile, lv_color_hex(arc_variants[arc_variant_selected[arc_turn]].colour), MU_OBJ_MAIN_DEFAULT
    );
    arc_set_hidden(arc_projectile, 0);
    arc_update_aim();
    play_sound(snd_confirm);
}

static void arc_step_shot(const uint32_t now) {
    float dt = (float) (now - arc_shot_tick) / 1000.0f;
    arc_shot_tick = now;
    if (dt > 0.05f) dt = 0.05f;
    if (dt <= 0.0f) return;

    arc_shot_age += dt;
    const float drag = 1.0f - 0.045f * dt;
    arc_shot_vx *= drag;
    arc_shot_vy = arc_shot_vy * drag + arc_shot_gravity * dt;
    arc_shot_x += arc_shot_vx * dt;
    arc_shot_y += arc_shot_vy * dt;

    const int dot = arc_clamp(arc_scale(9), 6, 14);
    lv_obj_set_size(arc_projectile, dot, dot);
    lv_obj_set_pos(arc_projectile, (int) lroundf(arc_shot_x) - dot / 2, (int) lroundf(arc_shot_y) - dot / 2);
    arc_push_trail();

    const int hit = arc_projectile_hit_player();
    if (hit >= 0) {
        arc_explode(arc_shot_x, arc_shot_y, hit);
        return;
    }

    if (arc_shot_x < 0.0f || arc_shot_x >= arc_width || arc_shot_y >= arc_height) {
        arc_clear_shot();
        arc_turn ^= 1;
        arc_update_hud();
        arc_set_hidden(arc_hud, 0);
        arc_prepare_cpu();
        return;
    }

    if (arc_shot_y >= 0.0f) {
        const int x = arc_clamp((int) lroundf(arc_shot_x), 0, arc_width - 1);
        if (arc_shot_y >= arc_ground[x]) arc_explode(arc_shot_x, arc_shot_y, -1);
    }
}

static void arc_timer_tick(lv_timer_t *timer __attribute__((unused))) {
    if (arc_screen != arc_screen_play) return;
    const uint32_t now = lv_tick_get();

    if (arc_shot_active) {
        arc_step_shot(now);
        return;
    }

    if (arc_explosion_active) {
        const uint32_t age = now - arc_explosion_started;
        const int fade = arc_clamp(255 - (int) (age * 255 / 650), 0, 255);
        lv_obj_set_style_bg_opa(arc_explosion, fade, MU_OBJ_MAIN_DEFAULT);

        if (age < 650) return;
        arc_explosion_active = 0;
        arc_set_hidden(arc_explosion, 1);
        arc_set_hidden(arc_hud, 0);
        if (arc_round_winner != -2) {
            arc_finish_round();
            return;
        }
        arc_turn ^= 1;
        arc_update_hud();
        arc_prepare_cpu();
        return;
    }

    if (arc_cpu_deadline && (int32_t) (now - arc_cpu_deadline) >= 0) arc_fire();
}

static void arc_move_menu(const int direction) {
    if (arc_menu_count < 1) return;
    arc_menu_selected += direction;
    if (arc_menu_selected < 0) arc_menu_selected = arc_menu_count - 1;
    if (arc_menu_selected >= arc_menu_count) arc_menu_selected = 0;
    if (arc_screen == arc_screen_setup)
        arc_refresh_setup();
    else
        arc_style_menu();
    play_sound(snd_navigate);
}

static int arc_cycle(const int value, const int direction, const int count) {
    return (value + direction + count) % count;
}

static void arc_change_setup(const int direction) {
    const int variant_count = ARC_VISIBLE_COUNT;

    switch (arc_menu_selected) {
        case arc_setup_players:
            arc_players = arc_players == 1 ? 2 : 1;
            if (arc_players == 2 && arc_variant_selected[1] == arc_variant_selected[0])
                arc_variant_selected[1] = arc_cycle(arc_variant_selected[1], 1, variant_count);
            break;
        case arc_setup_length:
            arc_best_of = 3 + arc_cycle((arc_best_of - 3) / 2, direction, 3) * 2;
            break;
        case arc_setup_gravity:
            arc_planet_selected = arc_cycle(arc_planet_selected, direction, ARC_PLANET_COUNT);
            break;
        case arc_setup_player_one:
            arc_variant_selected[0] = arc_cycle(arc_variant_selected[0], direction, variant_count);
            if (arc_players == 2 && arc_variant_selected[1] == arc_variant_selected[0])
                arc_variant_selected[1] = arc_cycle(arc_variant_selected[1], direction, variant_count);
            break;
        case arc_setup_player_two:
            if (arc_players == 1) {
                arc_difficulty = arc_cycle(arc_difficulty, direction, arc_difficulty_count);
                break;
            }
            do {
                arc_variant_selected[1] = arc_cycle(arc_variant_selected[1], direction, variant_count);
            } while (arc_variant_selected[1] == arc_variant_selected[0]);
            break;
        default:
            return;
    }

    arc_refresh_setup();
    play_sound(snd_option);
}

static void arc_show_pause(void) {
    static const char *options[] = {"Resume", "Restart Match", "Exit"};
    for (size_t index = 0; index < A_SIZE(options); index++)
        options[index] = TRS((char *) options[index]);
    arc_previous_screen = arc_screen_play;
    arc_show_menu(arc_screen_pause, TRS("Paused"), NULL, options, A_SIZE(options));
}

static void arc_quit(void) {
    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "apps");
    mux_input_stop();
}

static void arc_select_menu(void) {
    switch (arc_screen) {
        case arc_screen_setup:
            if (arc_menu_selected == arc_setup_start) {
                if (arc_players == 1) arc_pick_cpu();
                play_sound(snd_confirm);
                arc_begin_match();
            } else if (arc_menu_selected == arc_setup_quit) {
                arc_quit();
            } else {
                arc_change_setup(1);
            }
            break;
        case arc_screen_pause:
            play_sound(snd_confirm);
            if (arc_menu_selected == 0) {
                arc_show_play();
            } else if (arc_menu_selected == 1) {
                arc_begin_match();
            } else {
                arc_quit();
            }
            break;
        case arc_screen_round:
            play_sound(snd_confirm);
            if (arc_menu_selected == 0)
                arc_begin_round();
            else
                arc_show_setup();
            break;
        case arc_screen_match:
            play_sound(snd_confirm);
            if (arc_menu_selected == 0)
                arc_begin_match();
            else if (arc_menu_selected == 1)
                arc_show_setup();
            else
                arc_quit();
            break;
        default:
            break;
    }
}

static void arc_handle_a(void) {
    if (arc_screen == arc_screen_play) {
        if (arc_players == 1 && arc_turn == 1) return;
        arc_fire();
        return;
    }
    arc_select_menu();
}

static void arc_handle_b(void) {
    switch (arc_screen) {
        case arc_screen_setup:
            arc_quit();
            break;
        case arc_screen_play:
            arc_show_pause();
            break;
        case arc_screen_pause:
            if (arc_previous_screen == arc_screen_play) arc_show_play();
            break;
        case arc_screen_round:
        case arc_screen_match:
            arc_show_setup();
            break;
    }
}

static void arc_adjust_angle(const int direction) {
    if (arc_screen != arc_screen_play || arc_shot_active || arc_explosion_active) return;
    if (arc_players == 1 && arc_turn == 1) return;
    arc_angle[arc_turn] = arc_clamp(arc_angle[arc_turn] + direction, 15, 80);
    arc_update_hud();
    play_sound(snd_navigate);
}

static void arc_adjust_power(const int direction) {
    if (arc_screen != arc_screen_play || arc_shot_active || arc_explosion_active) return;
    if (arc_players == 1 && arc_turn == 1) return;
    arc_power[arc_turn] = arc_clamp(arc_power[arc_turn] + direction, 20, 100);
    arc_update_hud();
    play_sound(snd_navigate);
}

static void arc_handle_up(void) {
    if (arc_screen == arc_screen_play)
        arc_adjust_power(1);
    else
        arc_move_menu(-1);
}

static void arc_handle_down(void) {
    if (arc_screen == arc_screen_play)
        arc_adjust_power(-1);
    else
        arc_move_menu(1);
}

static void arc_handle_left(void) {
    if (arc_screen == arc_screen_play)
        arc_adjust_angle(arc_turn ? -1 : 1);
    else if (arc_screen == arc_screen_setup)
        arc_change_setup(-1);
}

static void arc_handle_right(void) {
    if (arc_screen == arc_screen_play)
        arc_adjust_angle(arc_turn ? 1 : -1);
    else if (arc_screen == arc_screen_setup)
        arc_change_setup(1);
}

static void arc_handle_menu(void) {
    if (arc_screen == arc_screen_play) arc_show_pause();
}

static void arc_mayo_press(void) {
    arc_mayo_player = -1;
    if (arc_screen == arc_screen_setup && arc_menu_selected == arc_setup_player_one)
        arc_mayo_player = 0;
    else if (arc_screen == arc_screen_setup && arc_players == 2 && arc_menu_selected == arc_setup_player_two)
        arc_mayo_player = 1;
    arc_mayo_since = arc_mayo_player >= 0 ? mux_input_tick() : 0;
}

static void arc_mayo_hold(void) {
    if (!arc_mayo_since || arc_mayo_player < 0) return;
    if (mux_input_tick() - arc_mayo_since < ARC_SECRET_MS) return;
    arc_variant_selected[arc_mayo_player] = ARC_VARIANT_COUNT - 1;
    if (arc_players == 2 && arc_variant_selected[0] == arc_variant_selected[1])
        arc_variant_selected[arc_mayo_player ^ 1] = arc_mayo_player == 0 ? 0 : 1;
    arc_mayo_since = 0;
    arc_mayo_player = -1;
    arc_refresh_setup();
    play_sound(snd_option);
}

static void arc_mayo_release(void) {
    arc_mayo_since = 0;
    arc_mayo_player = -1;
}

static void arc_create_menu(void) {
    arc_menu = lv_obj_create(arc_root);
    arc_style_clear(arc_menu);
    lv_obj_set_style_bg_color(arc_menu, lv_color_hex(theme.dialogue.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(arc_menu, theme.dialogue.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(arc_menu, lv_color_hex(theme.dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(arc_menu, theme.dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(arc_menu, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(arc_menu, theme.dialogue.radius.main, MU_OBJ_MAIN_DEFAULT);
    lv_shadow_zone_register(
        arc_menu, lv_color_hex(theme.dialogue.shadow_colour), (lv_opa_t) theme.dialogue.shadow_alpha,
        (int8_t) theme.dialogue.shadow_x_offset, (int8_t) theme.dialogue.shadow_y_offset,
        lv_color_hex(theme.dialogue.shadow_colour_focus), (lv_opa_t) theme.dialogue.shadow_alpha_focus,
        (int8_t) theme.dialogue.shadow_x_offset_focus, (int8_t) theme.dialogue.shadow_y_offset_focus
    );

    arc_menu_title = arc_label(arc_menu, "", theme.dialogue.title);
    lv_obj_set_width(arc_menu_title, LV_PCT(100));
    lv_obj_align(arc_menu_title, LV_ALIGN_TOP_MID, 0, 0);

    arc_menu_subtitle = arc_label(arc_menu, "", theme.dialogue.content);
    lv_label_set_long_mode(arc_menu_subtitle, LV_LABEL_LONG_DOT);
    lv_obj_set_width(arc_menu_subtitle, LV_PCT(100));
    lv_obj_align(arc_menu_subtitle, LV_ALIGN_TOP_MID, 0, 0);

    for (int index = 0; index < ARC_MENU_ROWS; index++) {
        arc_menu_row[index] = lv_obj_create(arc_menu);
        arc_style_clear(arc_menu_row[index]);
        lv_obj_set_style_radius(arc_menu_row[index], theme.dialogue.radius.selected, MU_OBJ_MAIN_DEFAULT);
        arc_menu_label[index] = arc_label(arc_menu_row[index], "", theme.dialogue.content);
        arc_menu_value[index] = arc_label(arc_menu_row[index], "", theme.dialogue.content);
        arc_set_hidden(arc_menu_value[index], 1);
    }
}

static void arc_create_actor(const int player) {
    arc_actor[player] = lv_obj_create(arc_root);
    arc_style_clear(arc_actor[player]);
    lv_obj_add_flag(arc_actor[player], LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_size(arc_actor[player], arc_actor_w, arc_actor_h);
    lv_obj_set_style_radius(arc_actor[player], arc_actor_w / 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(arc_actor[player], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(arc_actor[player], lv_color_hex(theme.header.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(arc_actor[player], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(arc_actor[player], arc_clamp(arc_scale(2), 1, 4), MU_OBJ_MAIN_DEFAULT);

    arc_actor_cap[player] = lv_obj_create(arc_actor[player]);
    arc_style_clear(arc_actor_cap[player]);
    lv_obj_set_size(arc_actor_cap[player], arc_actor_w / 2, arc_clamp(arc_actor_h / 5, 4, 10));
    lv_obj_align(arc_actor_cap[player], LV_ALIGN_TOP_MID, 0, -arc_clamp(arc_actor_h / 7, 3, 8));
    lv_obj_set_style_radius(arc_actor_cap[player], 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(arc_actor_cap[player], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);

    arc_actor_glyph[player] = arc_label(arc_actor[player], "", theme.header.text);
    lv_obj_set_width(arc_actor_glyph[player], LV_PCT(100));
    lv_obj_center(arc_actor_glyph[player]);
    lv_shadow_zone_register(
        arc_actor_glyph[player], lv_color_black(), LV_OPA_TRANSP, 0, 0, lv_color_black(), LV_OPA_TRANSP, 0, 0
    );

}

static int arc_create_scene(void) {
    arc_root = lv_obj_create(ui_pnl_content);
    arc_style_clear(arc_root);
    lv_obj_set_size(arc_root, LV_PCT(100), LV_PCT(100));
    lv_obj_center(arc_root);
    lv_obj_update_layout(arc_root);

    arc_width = lv_obj_get_content_width(arc_root);
    arc_height = lv_obj_get_content_height(arc_root);
    if (arc_width < 240) arc_width = device.mux.width;
    if (arc_height < 180) arc_height = device.mux.height - theme.header.height - theme.footer.height;
    if (arc_height < 180) arc_height = device.mux.height;

    arc_ground = calloc((size_t) arc_width, sizeof(*arc_ground));
    if (!arc_ground) return 0;

    for (int index = 0; index < ARC_TERRAIN_STRIPS; index++) {
        arc_terrain[index] = lv_obj_create(arc_root);
        arc_style_clear(arc_terrain[index]);
        lv_obj_set_style_radius(arc_terrain[index], 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(arc_terrain[index], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_side(arc_terrain[index], LV_BORDER_SIDE_TOP, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_width(arc_terrain[index], arc_clamp(arc_scale(2), 1, 4), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_color(arc_terrain[index], lv_color_hex(arc_focus_colour()), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_opa(arc_terrain[index], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        arc_set_hidden(arc_terrain[index], 1);
    }

    arc_hud = lv_obj_create(arc_root);
    arc_style_clear(arc_hud);
    const int hud_padding = arc_clamp(arc_scale(10), 8, 14);
    const int bar_height = arc_clamp(arc_scale(12), 8, 16);
    const int bar_gap = arc_clamp(arc_scale(6), 4, 8);
    const int hud_width = arc_width - 16;
    lv_obj_set_width(arc_hud, hud_width);
    lv_obj_set_pos(arc_hud, 8, 6);
    lv_obj_set_style_bg_color(arc_hud, lv_color_hex(theme.header.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(arc_hud, arc_clamp(theme.header.background_alpha * 3 / 4, 96, 220), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(arc_hud, theme.dialogue.radius.main, MU_OBJ_MAIN_DEFAULT);

    arc_hud_round = arc_label(arc_hud, "", theme.header.text);
    char round_sample[64];
    snprintf(round_sample, sizeof(round_sample), "%s 99", TRS("Round"));
    lv_point_t round_size;
    lv_txt_get_size(
        &round_size, round_sample, lv_obj_get_style_text_font(arc_hud_round, LV_PART_MAIN),
        lv_obj_get_style_text_letter_space(arc_hud_round, LV_PART_MAIN), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE
    );
    const int round_width = round_size.x + hud_padding * 2;
    const int top_row = round_size.y > bar_height ? round_size.y : bar_height;
    const int line_height = round_size.y;
    lv_obj_set_width(arc_hud_round, round_width);
    lv_obj_align(arc_hud_round, LV_ALIGN_TOP_MID, 0, hud_padding + (top_row - round_size.y) / 2);
    lv_obj_set_height(arc_hud, hud_padding * 2 + top_row + (bar_gap + line_height) * 2);

    const int bar_width = (hud_width - hud_padding * 2 - round_width) / 2;
    for (int player = 0; player < 2; player++) {
        arc_health_bar[player] = lv_bar_create(arc_hud);
        lv_obj_set_size(arc_health_bar[player], bar_width, bar_height);
        lv_obj_set_pos(
            arc_health_bar[player], player ? hud_width - hud_padding - bar_width : hud_padding,
            hud_padding + (top_row - bar_height) / 2
        );
        lv_bar_set_mode(arc_health_bar[player], player ? LV_BAR_MODE_RANGE : LV_BAR_MODE_NORMAL);
        lv_bar_set_range(arc_health_bar[player], 0, 100);
        lv_obj_set_style_radius(arc_health_bar[player], bar_height / 3, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_radius(arc_health_bar[player], bar_height / 3, LV_PART_INDICATOR | MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_color(arc_health_bar[player], lv_color_hex(theme.footer.background), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(arc_health_bar[player], LV_OPA_70, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(arc_health_bar[player], LV_OPA_COVER, LV_PART_INDICATOR | MU_OBJ_MAIN_DEFAULT);
    }

    arc_hud_score = arc_label(arc_hud, "", theme.header.text);
    lv_obj_set_width(arc_hud_score, LV_PCT(100));
    lv_obj_align(arc_hud_score, LV_ALIGN_TOP_MID, 0, hud_padding + top_row + bar_gap);
    arc_hud_values = arc_label(arc_hud, "", arc_focus_colour());
    lv_obj_set_width(arc_hud_values, LV_PCT(100));
    lv_obj_align(arc_hud_values, LV_ALIGN_TOP_MID, 0, hud_padding + top_row + bar_gap * 2 + line_height);

    arc_actor_w = arc_clamp(arc_width / 18, 30, 58);
    arc_actor_h = arc_clamp(arc_height / 12, 28, 52);
    arc_create_actor(0);
    arc_create_actor(1);

    for (int index = 0; index < ARC_AIM_DOTS; index++) {
        arc_aim[index] = lv_obj_create(arc_root);
        arc_style_clear(arc_aim[index]);
        const int size = arc_clamp(arc_scale(4), 3, 7);
        lv_obj_set_size(arc_aim[index], size, size);
        lv_obj_set_style_radius(arc_aim[index], LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_color(arc_aim[index], lv_color_hex(arc_focus_colour()), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(arc_aim[index], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    }

    for (int index = 0; index < ARC_TRAIL_COUNT; index++) {
        arc_trail[index] = lv_obj_create(arc_root);
        arc_style_clear(arc_trail[index]);
        lv_obj_set_style_radius(arc_trail[index], LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_color(arc_trail[index], lv_color_hex(arc_focus_colour()), MU_OBJ_MAIN_DEFAULT);
        arc_set_hidden(arc_trail[index], 1);
    }

    arc_projectile = lv_obj_create(arc_root);
    arc_style_clear(arc_projectile);
    lv_obj_set_style_radius(arc_projectile, LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
    arc_set_hidden(arc_projectile, 1);

    arc_explosion = lv_obj_create(arc_root);
    arc_style_clear(arc_explosion);
    lv_obj_set_style_radius(arc_explosion, LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(arc_explosion, lv_color_hex(arc_focus_colour()), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(arc_explosion, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    arc_set_hidden(arc_explosion, 1);

    arc_create_menu();
    lv_obj_move_foreground(arc_menu);
    return 1;
}

static void arc_cleanup(void) {
    if (arc_timer) {
        lv_timer_del(arc_timer);
        arc_timer = NULL;
    }
    if (arc_root && lv_obj_is_valid(arc_root)) lv_obj_del(arc_root);
    arc_root = NULL;
    free(arc_ground);
    arc_ground = NULL;
    arc_terrain_count = 0;
}

int muxarc_main(void) {
    init_module(__func__);
    init_theme(1, 0);
    init_ui_common_screen(&theme, &device, &lang, "");
    header_and_footer_setup();
    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());
    init_fonts();

    srandom((unsigned) time(NULL) ^ (unsigned) getpid() ^ (uintptr_t) &arc_root);

    if (!arc_create_scene()) {
        arc_cleanup();
        return 0;
    }

    init_timer(NULL, NULL);
    arc_timer = lv_timer_create(arc_timer_tick, ARC_TICK_MS, NULL);
    arc_show_setup();

    mux_input_options input_opts = {
        .hold_disabled = 1,
        .press_handler =
            {
                [mux_input_a] = arc_handle_a,
                [mux_input_b] = arc_handle_b,
                [mux_input_y] = arc_mayo_press,
                [mux_input_dpad_up] = arc_handle_up,
                [mux_input_dpad_down] = arc_handle_down,
                [mux_input_dpad_left] = arc_handle_left,
                [mux_input_dpad_right] = arc_handle_right,
            },
        .hold_handler =
            {
                [mux_input_y] = arc_mayo_hold,
                [mux_input_dpad_up] = arc_handle_up,
                [mux_input_dpad_down] = arc_handle_down,
                [mux_input_dpad_left] = arc_handle_left,
                [mux_input_dpad_right] = arc_handle_right,
            },
        .release_handler =
            {
                [mux_input_y] = arc_mayo_release,
                [mux_input_menu] = arc_handle_menu,
            },
        .idle_handler = ui_common_handle_idle,
    };

    init_input(&input_opts, 1);
    input_opts.remap_to_dpad = 1;
    mux_input_task(&input_opts);

    arc_cleanup();
    return 0;
}
