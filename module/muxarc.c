#include <float.h>
#include <math.h>

#include "muxshare.h"

#define ARC_VARIANT_COUNT  6
#define ARC_VISIBLE_COUNT  5
#define ARC_MENU_ROWS      7
#define ARC_TRAIL_COUNT    12
#define ARC_AIM_DOTS       9
#define ARC_PATH_DOTS      18
#define ARC_STAR_COUNT     18
#define ARC_CLOUD_COUNT    4
#define ARC_DROP_COUNT     40
#define ARC_SHELL_COUNT    5
#define ARC_METEOR_DOTS    6
#define ARC_PARTICLE_COUNT 30
#define ARC_BOLT_DOTS      40
#define ARC_HAZARD_SLOTS   3
#define ARC_TICK_MS        16
#define ARC_SECRET_MS      1500
#define ARC_EXPLOSION_MS   650
#define ARC_SHAKE_MS       340
#define ARC_TOAST_MS       1500
#define ARC_BUBBLE_MS      2300
#define ARC_DAMAGE_MS      1000
#define ARC_SQUASH_MS      320
#define ARC_HIT_MS         260
#define ARC_HAZARD_WAIT_MS 700
#define ARC_VOLLEY_CAP     60
#define ARC_HAZARD_CAP     40
#define ARC_WALL_SPARKS    6
#define ARC_REGEN_TURNS    3
#define ARC_RECOIL_MS      150
#define ARC_STEP           (1.0f / 120.0f)
#define ARC_SAVE_FILE      RUN_STORAGE_PATH "info/arc.ini"

#define ARC_PICK(list) ((list)[arc_rand((int) A_SIZE(list))])

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
    arc_setup_world,
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

enum arc_walls {
    arc_walls_open,
    arc_walls_rubber,
    arc_walls_wrap,
    arc_walls_concrete,
    arc_walls_random,
};

enum arc_surface {
    arc_surface_craters,
    arc_surface_volcanic,
    arc_surface_hills,
    arc_surface_mesas,
    arc_surface_bands,
    arc_surface_ice,
};

enum arc_cloud_style {
    arc_cloud_cumulus,
    arc_cloud_stratus,
    arc_cloud_cirrus,
    arc_cloud_streak,
};

enum arc_weather {
    arc_weather_meteor,
    arc_weather_virga,
    arc_weather_rain,
    arc_weather_dust,
    arc_weather_snow,
    arc_weather_diamond,
};

enum arc_body {
    arc_body_none,
    arc_body_earth,
    arc_body_jupiter,
    arc_body_galaxy,
};

enum arc_hazard_kind {
    arc_hazard_none,
    arc_hazard_meteor,
    arc_hazard_lightning,
    arc_hazard_eruption,
    arc_hazard_geyser,
    arc_hazard_quake,
    arc_hazard_gust,
    arc_hazard_calm,
    arc_hazard_flare,
    arc_hazard_haze,
};

enum arc_weapon_kind {
    arc_kind_main,
    arc_kind_spread,
    arc_kind_rail,
    arc_kind_bomb,
    arc_kind_roller,
    arc_kind_cluster,
    arc_kind_bounce,
    arc_kind_wall,
    arc_kind_shield,
    arc_kind_heal,
    arc_kind_hop,
    arc_kind_trench,
};

enum arc_event {
    arc_event_fly,
    arc_event_ground,
    arc_event_actor,
    arc_event_lost,
    arc_event_split,
};

enum arc_particle_kind {
    arc_particle_splat,
    arc_particle_debris,
    arc_particle_confetti,
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
    int kind;
    const char *name;
} arc_hazard;

typedef struct {
    const char *name;
    const char *trait;
    const char *rules;
    const char *watch;
    int pull;
    int wind;
    int gusts;
    int air;
    int distance;
    int walls;
    int falls;
    int crater;
    uint32_t sky_top;
    uint32_t sky_bottom;
    uint32_t ground;
    uint32_t cloud;
    int cloud_opa;
    int star_opa;
    int surface;
    int cloud_style;
    int weather;
    uint32_t halo;
    uint32_t precip;
    int body;
    int body_size;
    int chance;
    arc_hazard hazards[ARC_HAZARD_SLOTS];
} arc_planet;

typedef struct {
    float x;
    float y;
    int visible;
} arc_trail_sample;

typedef struct {
    const char *name;
    const char *about;
    int kind;
    int ammo;
    int blast;
    int damage;
    int gravity;
    int launch;
    int size;
    int strength;
    int guard;
    int fade;
    int regen;
} arc_weapon;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float age;
    float peak;
    float roll;
    int bounces;
    int rolling;
    int split;
    int hit;
    int active;
} arc_shell;

typedef struct {
    lv_obj_t *obj;
    float x;
    float y;
    float vx;
    float vy;
    float sway;
    uint32_t born;
    int life;
    int kind;
    int active;
} arc_particle;

typedef struct {
    lv_obj_t *obj;
    int x;
    int y;
    int size;
    int base_opa;
    float phase;
    float speed;
} arc_star;

typedef struct {
    lv_obj_t *obj;
    lv_color32_t *buf;
    float x;
    int y;
    int width;
    int height;
    uint32_t seed;
    float drift;
} arc_cloud;

typedef struct {
    lv_obj_t *obj;
    float x;
    float y;
    float speed;
    float phase;
    int length;
} arc_drop;

typedef struct {
    int won;
    int lost;
    int rounds;
    int shots;
    int hits;
    int best_damage;
    int longest;
} arc_record;

static const arc_variant arc_variants[ARC_VARIANT_COUNT] = {
    {"Ketchup", "K", 0xD73B32, 0xF4E8D1, 0xFFFFFF, "Hits hardest, but falls short", 100, 125, 100, 100, 92, 100, 6},
    {"Mustard", "M", 0xE3B817, 0xF4E8D1, 0x3A2400, "Longest aim guide, but a small blast", 100, 100, 100, 80, 100, 100,
     9},
    {"Wasabi", "W", 0x6CA83D, 0xE6D4B0, 0x10260A, "Huge blast, but fragile", 80, 95, 100, 130, 100, 95, 6},
    {"Pickles", "P", 0x2F8F6B, 0xD7B85C, 0x031510, "Tough as brine, but soft hits", 130, 80, 85, 100, 95, 105, 6},
    {"Relish", "R", 0x7C9F35, 0xE6D4B0, 0x13220A, "Longest reach, but a short aim guide", 95, 95, 100, 95, 115, 95, 3},
    {"Mayo", "Y", 0xF1E5C8, 0xC9B99B, 0x4A3A1C, "Strong all round, but heavy shots drop fast", 120, 115, 90, 115, 105,
     125, 7},
};

static const char *arc_wall_names[] = {"Open Edges", "Rubber Walls", "Wraparound", "Concrete Walls"};

static const arc_weapon arc_main_weapon = {"Main Shot", "", arc_kind_main, 0, 100, 100, 100, 100, 100};

static const arc_weapon arc_specials[ARC_VARIANT_COUNT] = {
    {"Squirt Spread", "Special: Squirt Spread, three shots at once", arc_kind_spread, 1, 75, 38, 100, 100, 80},
    {"Hot Rail", "Special: Hot Rail, a fast and flat shot", arc_kind_rail, 1, 70, 95, 30, 112, 80},
    {"Wasabi Bomb", "Special: Wasabi Bomb, one enormous blast", arc_kind_bomb, 1, 140, 90, 125, 92, 150},
    {"Brine Roller", "Special: Brine Roller, rolls along the ground", arc_kind_roller, 1, 100, 85, 100, 100, 110},
    {"Relish Cluster", "Special: Relish Cluster, splits into four", arc_kind_cluster, 1, 55, 30, 100, 100, 100},
    {"Mayo Bounce", "Special: Mayo Bounce, bounces before it bursts", arc_kind_bounce, 1, 105, 90, 100, 100, 120},
};

static const arc_weapon arc_guards[ARC_VARIANT_COUNT] = {
    {"Tomato Wall", "Defence: Tomato Wall, raises a mound where it lands", arc_kind_wall, 1, 100, 0, 100, 100, 120,
     100},
    {"Mustard Shield", "Defence: Mustard Shield, blocks 60% and fades each turn", arc_kind_shield, 1, 0, 0, 0, 0, 0, 0,
     60, 15},
    {"Green Shield", "Defence: Green Shield, blocks 70% but fades quickly", arc_kind_shield, 1, 0, 0, 0, 0, 0, 0, 70,
     20},
    {"Brine Soak", "Defence: Brine Soak, heals 20 now and 8 more for three turns", arc_kind_heal, 1, 0, 0, 0, 0, 0, 20,
     0, 0, 8},
    {"Relish Hop", "Defence: Relish Hop, jumps back and dodges 30% for a while", arc_kind_hop, 1, 0, 0, 0, 0, 0, 10, 30,
     15},
    {"Mayo Trench", "Defence: Mayo Trench, digs in to block 40% for many turns", arc_kind_trench, 1, 0, 0, 0, 0, 0, 100,
     40, 10},
};

static const arc_planet arc_planets[] = {
    {
        .name = "Mercury",
        .trait = "38% of Earth, with no air and no wind",
        .rules = "Open edges, and shots bake deep craters",
        .watch = "Watch for solar flares and meteorite strikes",
        .pull = 61,
        .wind = 0,
        .gusts = 0,
        .air = 0,
        .distance = 39,
        .walls = arc_walls_open,
        .falls = 0,
        .crater = 135,
        .sky_top = 0x04050A,
        .sky_bottom = 0x1B1B26,
        .ground = 0x8C8680,
        .cloud = 0xFFFFFF,
        .cloud_opa = 0,
        .star_opa = 255,
        .surface = arc_surface_craters,
        .cloud_style = arc_cloud_cumulus,
        .weather = arc_weather_meteor,
        .halo = 0xFFF2D0,
        .precip = 0xFFFFFF,
        .body = arc_body_galaxy,
        .body_size = 56,
        .chance = 35,
        .hazards = {{arc_hazard_flare, "Solar flare!"}, {arc_hazard_meteor, "Meteorite strike!"}},
    },
    {
        .name = "Venus",
        .trait = "90% of Earth, and the crushing air slows every shot",
        .rules = "Concrete edges, and every fall hurts",
        .watch = "Watch for eruptions, sulphuric haze and gusts",
        .pull = 95,
        .wind = 2,
        .gusts = 1,
        .air = 300,
        .distance = 72,
        .walls = arc_walls_concrete,
        .falls = 1,
        .crater = 100,
        .sky_top = 0xC98F35,
        .sky_bottom = 0xF2D58A,
        .ground = 0xA0672F,
        .cloud = 0xF5E2A8,
        .cloud_opa = 150,
        .star_opa = 0,
        .surface = arc_surface_volcanic,
        .cloud_style = arc_cloud_stratus,
        .weather = arc_weather_virga,
        .halo = 0xFFD27A,
        .precip = 0xF0DC8C,
        .body = arc_body_none,
        .body_size = 0,
        .chance = 40,
        .hazards =
            {{arc_hazard_eruption, "Volcanic eruption!"},
             {arc_hazard_haze, "Sulphuric haze!"},
             {arc_hazard_gust, "Superrotation gust!"}},
    },
    {
        .name = "Earth",
        .trait = "The usual pull and a gentle breeze",
        .rules = "Open edges, and falling never hurts",
        .watch = "Watch for lightning, sudden gusts and calm spells",
        .pull = 100,
        .wind = 4,
        .gusts = 2,
        .air = 100,
        .distance = 100,
        .walls = arc_walls_open,
        .falls = 0,
        .crater = 100,
        .sky_top = 0x2D6FB5,
        .sky_bottom = 0x9CC9EE,
        .ground = 0x5F8F3A,
        .cloud = 0xFFFFFF,
        .cloud_opa = 190,
        .star_opa = 0,
        .surface = arc_surface_hills,
        .cloud_style = arc_cloud_cumulus,
        .weather = arc_weather_rain,
        .halo = 0xFFE9A8,
        .precip = 0xD6E8FF,
        .body = arc_body_none,
        .body_size = 0,
        .chance = 30,
        .hazards =
            {{arc_hazard_lightning, "Lightning strike!"},
             {arc_hazard_gust, "Sudden gust!"},
             {arc_hazard_calm, "The wind drops"}},
    },
    {
        .name = "Moon",
        .trait = "17% of Earth, with no air and no wind",
        .rules = "Open edges, and shots dig wide craters",
        .watch = "Watch for meteoroids, moonquakes and solar flares",
        .pull = 41,
        .wind = 0,
        .gusts = 0,
        .air = 0,
        .distance = 100,
        .walls = arc_walls_open,
        .falls = 0,
        .crater = 120,
        .sky_top = 0x020308,
        .sky_bottom = 0x10131C,
        .ground = 0x9A9A96,
        .cloud = 0xFFFFFF,
        .cloud_opa = 0,
        .star_opa = 255,
        .surface = arc_surface_craters,
        .cloud_style = arc_cloud_cumulus,
        .weather = arc_weather_meteor,
        .halo = 0xFFF6E0,
        .precip = 0xFFFFFF,
        .body = arc_body_earth,
        .body_size = 22,
        .chance = 35,
        .hazards =
            {{arc_hazard_meteor, "Meteoroid impact!"},
             {arc_hazard_quake, "Moonquake!"},
             {arc_hazard_flare, "Solar flare!"}},
    },
    {
        .name = "Mars",
        .trait = "38% of Earth, with thin air and dusty gusts",
        .rules = "Open edges, deeper craters, and every fall hurts",
        .watch = "Watch for dust storms, marsquakes and meteorites",
        .pull = 62,
        .wind = 4,
        .gusts = 3,
        .air = 30,
        .distance = 152,
        .walls = arc_walls_open,
        .falls = 1,
        .crater = 120,
        .sky_top = 0xB8754A,
        .sky_bottom = 0xE8BE93,
        .ground = 0xB5532B,
        .cloud = 0xE0A27A,
        .cloud_opa = 90,
        .star_opa = 40,
        .surface = arc_surface_mesas,
        .cloud_style = arc_cloud_cirrus,
        .weather = arc_weather_dust,
        .halo = 0x9CC4FF,
        .precip = 0xD9935E,
        .body = arc_body_none,
        .body_size = 0,
        .chance = 35,
        .hazards =
            {{arc_hazard_gust, "Dust storm!"},
             {arc_hazard_quake, "Marsquake!"},
             {arc_hazard_meteor, "Meteorite strike!"}},
    },
    {
        .name = "Jupiter",
        .trait = "253% of Earth, so shots drop like stones",
        .rules = "Rubber edges, and the wind shifts hard each turn",
        .watch = "Watch for superbolts, Great Red Spot gusts and calm spells",
        .pull = 159,
        .wind = 8,
        .gusts = 4,
        .air = 150,
        .distance = 520,
        .walls = arc_walls_rubber,
        .falls = 0,
        .crater = 100,
        .sky_top = 0x7E5F42,
        .sky_bottom = 0xD9B98F,
        .ground = 0xB08A5E,
        .cloud = 0xEBD3B0,
        .cloud_opa = 170,
        .star_opa = 0,
        .surface = arc_surface_bands,
        .cloud_style = arc_cloud_streak,
        .weather = arc_weather_snow,
        .halo = 0xFFE2B0,
        .precip = 0xF4EEDF,
        .body = arc_body_none,
        .body_size = 0,
        .chance = 45,
        .hazards =
            {{arc_hazard_lightning, "Superbolt!"},
             {arc_hazard_gust, "Great Red Spot gust!"},
             {arc_hazard_calm, "Between the jet streams"}},
    },
    {
        .name = "Io",
        .trait = "18% of Earth, with no air and restless volcanoes",
        .rules = "Concrete edges, soft sulphur craters, and every fall hurts",
        .watch = "Watch for erupting volcanoes and tidal quakes",
        .pull = 43,
        .wind = 0,
        .gusts = 0,
        .air = 0,
        .distance = 520,
        .walls = arc_walls_concrete,
        .falls = 1,
        .crater = 150,
        .sky_top = 0x040306,
        .sky_bottom = 0x1A140C,
        .ground = 0xD9C24A,
        .cloud = 0xFFF1A0,
        .cloud_opa = 0,
        .star_opa = 220,
        .surface = arc_surface_volcanic,
        .cloud_style = arc_cloud_cumulus,
        .weather = arc_weather_snow,
        .halo = 0xFFF0C8,
        .precip = 0xF2E27A,
        .body = arc_body_jupiter,
        .body_size = 46,
        .chance = 50,
        .hazards =
            {{arc_hazard_eruption, "Loki erupts!"},
             {arc_hazard_eruption, "Pele erupts!"},
             {arc_hazard_quake, "Tidal quake!"}},
    },
    {
        .name = "Europa",
        .trait = "13% of Earth, with no air over a frozen ocean",
        .rules = "Rubber edges, shallow craters in hard ice, and every fall hurts",
        .watch = "Watch for water plumes, cracking ice and radiation bursts",
        .pull = 37,
        .wind = 0,
        .gusts = 0,
        .air = 0,
        .distance = 520,
        .walls = arc_walls_rubber,
        .falls = 1,
        .crater = 80,
        .sky_top = 0x03050C,
        .sky_bottom = 0x141C2E,
        .ground = 0xC8B8A0,
        .cloud = 0xFFFFFF,
        .cloud_opa = 0,
        .star_opa = 230,
        .surface = arc_surface_ice,
        .cloud_style = arc_cloud_cirrus,
        .weather = arc_weather_snow,
        .halo = 0xE8F0FF,
        .precip = 0xEAF6FF,
        .body = arc_body_jupiter,
        .body_size = 30,
        .chance = 40,
        .hazards =
            {{arc_hazard_geyser, "Water plume!"},
             {arc_hazard_quake, "The ice cracks!"},
             {arc_hazard_flare, "Radiation burst!"}},
    },
    {
        .name = "Saturn",
        .trait = "106% of Earth, with blustery winds",
        .rules = "Shells leaving one edge loop back in on the other",
        .watch = "Watch for jet stream gusts and storm lightning",
        .pull = 103,
        .wind = 8,
        .gusts = 3,
        .air = 150,
        .distance = 954,
        .walls = arc_walls_wrap,
        .falls = 0,
        .crater = 100,
        .sky_top = 0xA88D55,
        .sky_bottom = 0xE6D5A3,
        .ground = 0xC7A86A,
        .cloud = 0xF1E4BE,
        .cloud_opa = 150,
        .star_opa = 20,
        .surface = arc_surface_bands,
        .cloud_style = arc_cloud_streak,
        .weather = arc_weather_snow,
        .halo = 0xFFE8BC,
        .precip = 0xF6EBCB,
        .body = arc_body_none,
        .body_size = 0,
        .chance = 35,
        .hazards =
            {{arc_hazard_gust, "Jet stream gust!"},
             {arc_hazard_lightning, "Storm lightning!"},
             {arc_hazard_calm, "The wind drops"}},
    },
    {
        .name = "Titan",
        .trait = "14% of Earth, with thick air that slows every shot",
        .rules = "Open edges, and the wind shifts slowly",
        .watch = "Watch for thick haze and methane storms",
        .pull = 37,
        .wind = 3,
        .gusts = 1,
        .air = 250,
        .distance = 954,
        .walls = arc_walls_open,
        .falls = 0,
        .crater = 100,
        .sky_top = 0x8A5A20,
        .sky_bottom = 0xD9A453,
        .ground = 0x7A5A35,
        .cloud = 0xE8C27A,
        .cloud_opa = 120,
        .star_opa = 0,
        .surface = arc_surface_hills,
        .cloud_style = arc_cloud_stratus,
        .weather = arc_weather_rain,
        .halo = 0xFFD58A,
        .precip = 0xD9A85C,
        .body = arc_body_none,
        .body_size = 0,
        .chance = 35,
        .hazards =
            {{arc_hazard_haze, "Thick haze!"},
             {arc_hazard_gust, "Methane storm!"},
             {arc_hazard_calm, "The wind drops"}},
    },
    {
        .name = "Uranus",
        .trait = "89% of Earth, with steady winds",
        .rules = "Different edges every round",
        .watch = "Watch for polar storms, methane haze and calm spells",
        .pull = 94,
        .wind = 6,
        .gusts = 2,
        .air = 130,
        .distance = 1920,
        .walls = arc_walls_random,
        .falls = 0,
        .crater = 100,
        .sky_top = 0x4E9FB0,
        .sky_bottom = 0xB5E3EA,
        .ground = 0x7FBFC7,
        .cloud = 0xE2F6F8,
        .cloud_opa = 140,
        .star_opa = 30,
        .surface = arc_surface_ice,
        .cloud_style = arc_cloud_cirrus,
        .weather = arc_weather_snow,
        .halo = 0xDDF6FF,
        .precip = 0xD8FAFF,
        .body = arc_body_none,
        .body_size = 0,
        .chance = 30,
        .hazards =
            {{arc_hazard_gust, "Polar storm!"},
             {arc_hazard_haze, "Methane haze!"},
             {arc_hazard_calm, "The wind drops"}},
    },
    {
        .name = "Neptune",
        .trait = "114% of Earth, with the wildest winds",
        .rules = "Open edges, and the wind shifts wildly each turn",
        .watch = "Watch for supersonic gusts, diamond hail and calm spells",
        .pull = 107,
        .wind = 10,
        .gusts = 5,
        .air = 130,
        .distance = 3007,
        .walls = arc_walls_open,
        .falls = 0,
        .crater = 100,
        .sky_top = 0x16307E,
        .sky_bottom = 0x5C7FD6,
        .ground = 0x3E5BB0,
        .cloud = 0xC9D6FF,
        .cloud_opa = 130,
        .star_opa = 70,
        .surface = arc_surface_ice,
        .cloud_style = arc_cloud_cirrus,
        .weather = arc_weather_diamond,
        .halo = 0xDDE6FF,
        .precip = 0xFFFFFF,
        .body = arc_body_none,
        .body_size = 0,
        .chance = 45,
        .hazards =
            {{arc_hazard_gust, "Supersonic gust!"},
             {arc_hazard_meteor, "Diamond hail!"},
             {arc_hazard_calm, "Dark spot calm"}},
    },
    {
        .name = "Pluto",
        .trait = "6% of Earth, with frozen haze and no wind",
        .rules = "Huge craters in soft ice, and every fall hurts",
        .watch = "Watch for nitrogen geysers, glacier slumps and impacts",
        .pull = 25,
        .wind = 0,
        .gusts = 0,
        .air = 5,
        .distance = 3948,
        .walls = arc_walls_open,
        .falls = 1,
        .crater = 180,
        .sky_top = 0x05060C,
        .sky_bottom = 0x1C2640,
        .ground = 0xC9A98A,
        .cloud = 0x9FB4E0,
        .cloud_opa = 0,
        .star_opa = 255,
        .surface = arc_surface_ice,
        .cloud_style = arc_cloud_cirrus,
        .weather = arc_weather_snow,
        .halo = 0xB8C8F0,
        .precip = 0xF2EEE6,
        .body = arc_body_galaxy,
        .body_size = 48,
        .chance = 35,
        .hazards = {
            {arc_hazard_geyser, "Nitrogen geyser!"},
            {arc_hazard_quake, "Glacier slump!"},
            {arc_hazard_meteor, "Kuiper belt impact!"}
        },
    },
};

#define ARC_PLANET_COUNT ((int) (sizeof(arc_planets) / sizeof(arc_planets[0])))
#define ARC_PLANET_EARTH 2

static const char *arc_legacy_planets[] = {
    "Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn", "Uranus", "Neptune", "Pluto",
};

static const char *arc_difficulty_traits[arc_difficulty_count] = {
    "Barely aims at all", "Misses more often than not", "A fair fight", "Rarely misses", "Hardly ever misses",
};

static const char *arc_say_turn[] = {
    "Hold still, this will only sting a little",
    "Watch and learn",
    "I have been saving this one",
    "This one has your name on it",
    "Brace yourself",
    "Incoming!",
    "Say goodbye to your lid",
};

static const char *arc_say_variant[ARC_VARIANT_COUNT][2] = {
    {"Time to paint the town red", "Fresh from the bottle"},
    {"Can you cut the mustard?", "Hot dog, here it comes"},
    {"A little goes a long way", "This will clear your sinuses"},
    {"I am kind of a big dill", "In a bit of a pickle?"},
    {"I will relish this", "Try to relish the moment"},
    {"Smooth, creamy and deadly", "Nobody puts Mayo in a corner"},
};

static const char *arc_say_gloat[] = {
    "Right on the label!", "Too easy", "Did that sting?", "Smeared!", "And stay down", "Wipe that off",
};

static const char *arc_say_excuse[] = {
    "The wind took it",      "I meant to do that",     "That was a warm-up shot",
    "The nozzle is clogged", "Next one will not miss", "Hmm, a little off",
};

static const char *arc_say_whine[] = {
    "Hey, that is my good side!", "Ow, my lid!",    "Lucky shot",
    "You will pay for that",      "Now I am cross", "That will stain",
};

static const char *arc_say_mock[] = {
    "Not even close",      "Were you aiming at the clouds?", "My turn, then", "Swing and a miss", "Are your eyes open?",
    "Nice shot... for me",
};

static const char *arc_say_special[] = {
    "Time for something special",
    "Try this on for size",
    "Saving the best for last",
    "You will not see this coming",
};

static const char *arc_say_hazard[] = {
    "Even the planet is on my side",
    "Not my doing, honest",
    "Did you feel that?",
    "The universe has spoken",
};

static const char *arc_toast_direct[] = {"Direct hit!", "Bullseye!", "Splat!", "Right on target!"};
static const char *arc_toast_heavy[] = {"Massive hit!", "Huge splat!", "What a shot!"};
static const char *arc_toast_glance[] = {"Clipped them!", "Glancing blow", "Splash damage!"};
static const char *arc_toast_self[] = {"Friendly fire!", "Oops, own goal", "Wrong way!"};
static const char *arc_toast_both[] = {"Everybody gets some!", "Splash all round!"};
static const char *arc_toast_close[] = {"So close!", "Nearly!", "A whisker away!"};
static const char *arc_toast_miss[] = {"Missed!", "Wide of the mark", "Nothing but dirt"};
static const char *arc_toast_out[] = {"Out of bounds!", "Gone for good", "See you later"};
static const char *arc_toast_orbit[] = {"Lost in orbit!", "Houston, we have a problem"};

static enum arc_screen arc_screen;
static enum arc_screen arc_previous_screen;

static lv_obj_t *arc_root;
static lv_obj_t *arc_sky;
static lv_obj_t *arc_sun;
static lv_color32_t *arc_sun_buf;
static int arc_sun_box;
static arc_star arc_stars[ARC_STAR_COUNT];
static arc_cloud arc_clouds[ARC_CLOUD_COUNT];
static arc_drop arc_drops[ARC_DROP_COUNT];
static int arc_drop_level;
static uint32_t arc_meteor_at;
static float arc_meteor_x;
static float arc_meteor_y;
static float arc_meteor_vx;
static float arc_meteor_vy;
static lv_obj_t *arc_terrain;
static lv_color32_t *arc_terrain_buf;
static lv_obj_t *arc_menu;
static lv_obj_t *arc_menu_title;
static lv_obj_t *arc_menu_subtitle;
static lv_obj_t *arc_menu_rule;
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
static lv_obj_t *arc_health_lag_bar[2];
static lv_obj_t *arc_trail[ARC_TRAIL_COUNT];
static lv_obj_t *arc_aim[ARC_AIM_DOTS];
static lv_obj_t *arc_path[ARC_PATH_DOTS];
static lv_obj_t *arc_explosion;
static lv_obj_t *arc_blast_ring;
static arc_particle arc_particles[ARC_PARTICLE_COUNT];
static lv_obj_t *arc_damage_label[2];
static lv_obj_t *arc_toast;
static lv_obj_t *arc_toast_label;
static lv_obj_t *arc_bubble;
static lv_obj_t *arc_bubble_label;
static lv_timer_t *arc_timer;

static int16_t *arc_ground;
static uint8_t *arc_scorch;
static uint8_t *arc_stain;
static uint32_t *arc_stain_colour;
static uint32_t *arc_row_colour;
static uint32_t arc_lip_colour;
static int arc_width;
static int arc_height;
static int arc_pad_left;
static int arc_pad_right;
static int arc_pad_top;
static int arc_pad_bottom;
static int arc_canvas_w;
static int arc_canvas_h;
static int arc_ground_top;
static int arc_hud_bottom;
static int arc_menu_count;
static int arc_menu_selected;
static const char *arc_menu_text[ARC_MENU_ROWS];

static int arc_players = 1;
static int arc_best_of = 3;
static int arc_planet_selected = ARC_PLANET_EARTH;
static int arc_difficulty = arc_difficulty_medium;
static int arc_walls;
static int arc_mayo_known;
static arc_record arc_records[ARC_VARIANT_COUNT];
static int arc_streak;
static int arc_best_streak;
static int arc_match_best[2];
static int arc_match_long[2];
static lv_obj_t *arc_wall_obj[2];
static lv_color32_t *arc_wall_buf[2];
static lv_obj_t *arc_wall_spark[2][ARC_WALL_SPARKS];
static float arc_wall_spark_y[2][ARC_WALL_SPARKS];
static int arc_wall_w;
static int arc_wall_h;
static int arc_wall_live;
static int arc_wall_pending[2];
static int arc_wall_pending_y[2];
static uint32_t arc_wall_hit_at[2];
static int arc_wall_shift[2];
static float arc_actor_fall[2];
static uint32_t arc_actor_fall_at[2];
static int arc_wins_needed = 2;
static int arc_variant_selected[2] = {1, 0};
static int arc_score[2];
static int arc_health[2];
static float arc_health_lag[2];
static uint32_t arc_health_hit_at[2];
static int arc_angle[2] = {45, 45};
static int arc_power[2] = {60, 60};
static int arc_turn;
static int arc_round_number;
static int arc_wind;
static int arc_shots[2];
static int arc_hits[2];
static int arc_mayo_player = -1;
static uint32_t arc_mayo_since;
static uint32_t arc_repeat_since;

static float arc_actor_x[2];
static float arc_actor_y[2];
static int arc_actor_w;
static int arc_actor_h;
static uint32_t arc_actor_hit_at[2];
static uint32_t arc_actor_recoil_at[2];
static uint32_t arc_actor_ko_at[2];
static int arc_actor_shift_x[2];
static int arc_actor_shift_y[2];
static int arc_actor_flash[2] = {-1, -1};
static int arc_actor_size_w[2];

static int arc_shot_active;
static float arc_shot_x;
static float arc_shot_y;
static float arc_shot_gravity;
static float arc_shot_wind;
static uint32_t arc_shot_tick;
static float arc_shot_accum;
static arc_shell arc_shells[ARC_SHELL_COUNT];
static lv_obj_t *arc_shell_obj[ARC_SHELL_COUNT];
static const arc_weapon *arc_volley_weapon;
static int arc_volley_damage[2];
static int arc_volley_direct;
static int arc_volley_bursts;
static int arc_volley_orbit;
static int arc_volley_radius;
static float arc_volley_x;
static int arc_lead_shell;
static int arc_ammo[2];
static int arc_armed[2];
static int arc_guard_ammo[2];
static int arc_shield[2];
static int arc_shield_fade[2];
static const char *arc_shield_name[2];
static int arc_regen[2];
static int arc_turns_taken[2];
static int arc_regen_amount[2];
static int arc_volley_absorbed[2];
static lv_obj_t *arc_shield_obj[2];
static uint32_t arc_shield_hit_at[2];
static int arc_volley_built;
static uint32_t arc_aim_tint;
static int arc_setup_flip;
static uint32_t arc_setup_flip_at;
static arc_trail_sample arc_trail_points[ARC_TRAIL_COUNT];
static int arc_trail_head;
static float arc_trail_last_x;
static float arc_trail_last_y;
static arc_trail_sample arc_path_points[2][ARC_PATH_DOTS];
static int arc_path_count[2];
static int arc_path_step;

static int arc_explosion_active;
static uint32_t arc_explosion_started;
static int arc_explosion_radius;
static int arc_explosion_x;
static int arc_explosion_y;
static uint32_t arc_shake_started;
static int arc_shake_strength;
static int arc_shake_x;
static int arc_shake_y;

static lv_obj_t *arc_body;
static lv_color32_t *arc_body_buf;
static int arc_body_box;
static lv_obj_t *arc_bolt[ARC_BOLT_DOTS];
static int arc_hazard_kind;
static const char *arc_hazard_name;
static int arc_hazard_stage;
static int arc_hazard_struck;
static int arc_hazard_rolled;
static uint32_t arc_hazard_at;
static float arc_hazard_x;
static float arc_hazard_from_x;
static float arc_hazard_from_y;
static int arc_condition;
static int arc_condition_next;
static uint32_t arc_shake_ms = ARC_SHAKE_MS;

static uint32_t arc_cpu_deadline;
static uint32_t arc_cpu_started;
static int arc_cpu_from_angle;
static int arc_cpu_from_power;
static int arc_cpu_angle;
static int arc_cpu_power;
static int arc_cpu_misses;
static int arc_round_winner = -2;

static uint32_t arc_toast_started;
static uint32_t arc_bubble_started;
static uint32_t arc_damage_started[2];
static int arc_damage_x[2];
static int arc_damage_y[2];
static uint32_t arc_last_tick;
static uint32_t arc_frame;

static int arc_rand(const int maximum) {
    if (maximum <= 1) return 0;
    return (int) (random() % maximum);
}

static float arc_randf(void) {
    return (float) (random() % 10000) / 10000.0f;
}

static int arc_clamp(const int value, const int low, const int high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static float arc_clampf(const float value, const float low, const float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static int arc_scale(const int value) {
    const int scaled = value * arc_height / 400;
    return scaled > 0 ? scaled : 1;
}

static uint32_t arc_mix(const uint32_t first, const uint32_t second, const int amount) {
    const int keep = 255 - amount;
    const uint32_t red = (((first >> 16) & 0xFF) * amount + ((second >> 16) & 0xFF) * keep) / 255;
    const uint32_t green = (((first >> 8) & 0xFF) * amount + ((second >> 8) & 0xFF) * keep) / 255;
    const uint32_t blue = ((first & 0xFF) * amount + (second & 0xFF) * keep) / 255;
    return (red << 16) | (green << 8) | blue;
}

static uint32_t arc_shade(const uint32_t colour, const int delta) {
    const int red = arc_clamp((int) ((colour >> 16) & 0xFF) + delta, 0, 255);
    const int green = arc_clamp((int) ((colour >> 8) & 0xFF) + delta, 0, 255);
    const int blue = arc_clamp((int) (colour & 0xFF) + delta, 0, 255);
    return ((uint32_t) red << 16) | ((uint32_t) green << 8) | (uint32_t) blue;
}

static uint32_t arc_panel_colour(void) {
    if (theme.list_default.background_alpha > 0) return theme.list_default.background;
    return theme.footer.background;
}

static uint32_t arc_focus_colour(void) {
    if (theme.dialogue.selection_alpha > 0) return theme.dialogue.selection;
    return theme.list_focus.text;
}

static void arc_save(void) {
    FILE *file = fopen(ARC_SAVE_FILE, "w");
    if (!file) return;

    fprintf(
        file, "players=%d\nbest_of=%d\nworld=%s\ndifficulty=%d\n", arc_players, arc_best_of,
        arc_planets[arc_planet_selected].name, arc_difficulty
    );
    fprintf(
        file, "first=%d\nsecond=%d\nknown=%d\nstreak=%d\nbest_streak=%d\n", arc_variant_selected[0],
        arc_variant_selected[1], arc_mayo_known, arc_streak, arc_best_streak
    );
    for (int index = 0; index < ARC_VARIANT_COUNT; index++) {
        const arc_record *record = &arc_records[index];
        fprintf(
            file, "record%d=%d %d %d %d %d %d %d\n", index, record->won, record->lost, record->rounds, record->shots,
            record->hits, record->best_damage, record->longest
        );
    }

    fclose(file);
}

static int arc_find_planet(const char *name) {
    for (int index = 0; index < ARC_PLANET_COUNT; index++)
        if (!strcmp(arc_planets[index].name, name)) return index;
    return ARC_PLANET_EARTH;
}

static void arc_load(void) {
    FILE *file = fopen(ARC_SAVE_FILE, "r");
    if (!file) return;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char key[32];
        int value = 0;
        int index = 0;
        arc_record record;

        if (sscanf(line, "world=%31s", key) == 1) {
            arc_planet_selected = arc_find_planet(key);
            continue;
        }

        if (sscanf(
                line, "record%d=%d %d %d %d %d %d %d", &index, &record.won, &record.lost, &record.rounds, &record.shots,
                &record.hits, &record.best_damage, &record.longest
            )
            == 8) {
            if (index >= 0 && index < ARC_VARIANT_COUNT) arc_records[index] = record;
            continue;
        }
        if (sscanf(line, "%31[^=]=%d", key, &value) != 2) continue;

        if (!strcmp(key, "players"))
            arc_players = value == 2 ? 2 : 1;
        else if (!strcmp(key, "best_of"))
            arc_best_of = value == 5 || value == 7 ? value : 3;
        else if (!strcmp(key, "planet"))
            arc_planet_selected =
                arc_find_planet(arc_legacy_planets[arc_clamp(value, 0, (int) A_SIZE(arc_legacy_planets) - 1)]);
        else if (!strcmp(key, "difficulty"))
            arc_difficulty = arc_clamp(value, 0, arc_difficulty_count - 1);
        else if (!strcmp(key, "first"))
            arc_variant_selected[0] = arc_clamp(value, 0, ARC_VARIANT_COUNT - 1);
        else if (!strcmp(key, "second"))
            arc_variant_selected[1] = arc_clamp(value, 0, ARC_VARIANT_COUNT - 1);
        else if (!strcmp(key, "known"))
            arc_mayo_known = value ? 1 : 0;
        else if (!strcmp(key, "streak"))
            arc_streak = arc_clamp(value, 0, 9999);
        else if (!strcmp(key, "best_streak"))
            arc_best_streak = arc_clamp(value, 0, 9999);
    }
    fclose(file);

    for (int player = 0; player < 2; player++)
        if (!arc_mayo_known && arc_variant_selected[player] >= ARC_VISIBLE_COUNT) arc_variant_selected[player] = player;
    if (arc_variant_selected[0] == arc_variant_selected[1])
        arc_variant_selected[1] = (arc_variant_selected[0] + 1) % ARC_VISIBLE_COUNT;
    if (arc_best_streak < arc_streak) arc_best_streak = arc_streak;
}

static const int arc_setup_main[] = {
    arc_setup_start, arc_setup_players, arc_setup_player_one, arc_setup_player_two,
    arc_setup_world, arc_setup_length,  arc_setup_quit,
};

static int arc_setup_rows(void) {
    return (int) A_SIZE(arc_setup_main);
}

static int arc_setup_item(void) {
    return arc_setup_main[arc_clamp(arc_menu_selected, 0, arc_setup_rows() - 1)];
}

static const arc_planet *arc_planet_current(void) {
    return &arc_planets[arc_planet_selected];
}

static void arc_set_hidden(lv_obj_t *object, const int hidden) {
    if (!object) return;
    if ((hidden ? 1 : 0) == (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) ? 1 : 0)) return;
    if (hidden)
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static void arc_place(lv_obj_t *object, const int x, const int y) {
    if (lv_obj_get_style_x(object, LV_PART_MAIN) == x && lv_obj_get_style_y(object, LV_PART_MAIN) == y) return;
    lv_obj_set_pos(object, x, y);
}

static void arc_set_opa(lv_obj_t *object, const int opa) {
    const lv_opa_t value = (lv_opa_t) arc_clamp(opa, 0, 255);
    if (lv_obj_get_style_opa(object, LV_PART_MAIN) == value) return;
    lv_obj_set_style_opa(object, value, MU_OBJ_MAIN_DEFAULT);
}

static void arc_set_fill(lv_obj_t *object, const int opa) {
    const lv_opa_t value = (lv_opa_t) arc_clamp(opa, 0, 255);
    if (lv_obj_get_style_bg_opa(object, LV_PART_MAIN) == value) return;
    lv_obj_set_style_bg_opa(object, value, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(object, value, MU_OBJ_MAIN_DEFAULT);
}

static void arc_set_text(lv_obj_t *label, const char *text) {
    const char *current = lv_label_get_text(label);
    if (current && strcmp(current, text) == 0) return;
    lv_label_set_text(label, text);
}

static void arc_style_clear(lv_obj_t *object) {
    lv_obj_remove_style_all(object);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *arc_dot(lv_obj_t *parent, const uint32_t colour) {
    lv_obj_t *dot = lv_obj_create(parent);
    arc_style_clear(dot);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(dot, lv_color_hex(colour), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    arc_set_hidden(dot, 1);
    return dot;
}

static lv_obj_t *arc_label(lv_obj_t *parent, const char *text, const uint32_t colour) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_color(label, lv_color_hex(colour), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    return label;
}

static int arc_guard_active(const int player) {
    return arc_shield[player] > 0 || arc_regen[player] > 0;
}

static int arc_extras_locked(const int player) {
    return arc_turns_taken[player] <= 1;
}

static int arc_is_instant(const arc_weapon *weapon) {
    return weapon->kind == arc_kind_shield || weapon->kind == arc_kind_heal || weapon->kind == arc_kind_hop
           || weapon->kind == arc_kind_trench;
}

static void arc_weapon_text(char *out, const size_t size) {
    const int variant = arc_variant_selected[arc_turn];
    if (arc_armed[arc_turn] == 1) {
        snprintf(out, size, "%s (%d)", TRS((char *) arc_specials[variant].name), arc_ammo[arc_turn]);
        return;
    }
    if (arc_armed[arc_turn] == 2) {
        snprintf(out, size, "%s (%d)", TRS((char *) arc_guards[variant].name), arc_guard_ammo[arc_turn]);
        return;
    }
    snprintf(out, size, "%s", TRS((char *) arc_main_weapon.name));
}

static void arc_refresh_footer(void) {
    nav_hide_all();

    if (arc_screen == arc_screen_play) {
        char weapon[128];
        arc_weapon_text(weapon, sizeof(weapon));
        setup_nav((struct nav_bar[]) {
            {ui_lbl_nav_lr_glyph, "", 0},
            {ui_lbl_nav_lr, TRS("Aim / Power"), 0},
            {ui_lbl_nav_a_glyph, "", 0},
            {ui_lbl_nav_a,
             arc_armed[arc_turn] == 2 && arc_is_instant(&arc_guards[arc_variant_selected[arc_turn]]) ? TRS("Use")
                                                                                                     : TRS("Fire"),
             0},
            {ui_lbl_nav_x_glyph, "", 0},
            {ui_lbl_nav_x, weapon, 0},
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

static int arc_text_width(lv_obj_t *label, const char *text) {
    lv_point_t size;
    lv_txt_get_size(
        &size, text, lv_obj_get_style_text_font(label, LV_PART_MAIN),
        lv_obj_get_style_text_letter_space(label, LV_PART_MAIN), 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE
    );
    return size.x;
}

static void arc_start_line(char *out, const size_t size, const char *first, const char *second, const char *planet) {
    snprintf(
        out, size, "%s %s %s %s %s, %s %d", first, TRS("against"), second, TRS("on"), planet, TRS("best of"),
        arc_best_of
    );
}

static int arc_record_line(char *out, const size_t size, const int variant) {
    const arc_record *record = &arc_records[variant];
    if (!record->won && !record->lost) return 0;
    snprintf(
        out, size, "%s %d %s, %d %s, %d%% %s, %s %d m", TRS("Record:"), record->won, TRS("won"), record->lost,
        TRS("lost"), record->shots ? record->hits * 100 / record->shots : 0, TRS("hits"), TRS("longest"),
        record->longest
    );
    return 1;
}

static void arc_setup_line(char *out, const size_t size) {
    const char *text = " ";

    switch (arc_setup_item()) {
        case arc_setup_start:
            arc_start_line(
                out, size, TRS((char *) arc_variants[arc_variant_selected[0]].name),
                arc_players == 1 ? TRS("the computer") : TRS((char *) arc_variants[arc_variant_selected[1]].name),
                TRS((char *) arc_planet_current()->name)
            );
            return;
        case arc_setup_players:
            text = arc_players == 1 ? "One player against the computer" : "Two players taking turns on one device";
            break;
        case arc_setup_length:
            snprintf(out, size, "%s %d %s", TRS("First to win"), arc_best_of / 2 + 1, TRS("rounds takes the match"));
            return;
        case arc_setup_world: {
            const arc_planet *planet = arc_planet_current();
            text = arc_setup_flip == 1 ? planet->rules : arc_setup_flip == 2 ? planet->watch : planet->trait;
            break;
        }
        case arc_setup_player_one:
            if (arc_setup_flip == 3 && arc_record_line(out, size, arc_variant_selected[0])) return;
            text = arc_setup_flip == 1   ? arc_specials[arc_variant_selected[0]].about
                   : arc_setup_flip == 2 ? arc_guards[arc_variant_selected[0]].about
                                         : arc_variants[arc_variant_selected[0]].trait;
            break;
        case arc_setup_player_two:
            if (arc_players == 1) {
                if (arc_setup_flip >= 2 && arc_best_streak > 0) {
                    snprintf(
                        out, size, "%s %d, %s %d", TRS("Winning streak"), arc_streak, TRS("best"), arc_best_streak
                    );
                    return;
                }
                text = arc_difficulty_traits[arc_difficulty];
            } else {
                if (arc_setup_flip == 3 && arc_record_line(out, size, arc_variant_selected[1])) return;
                text = arc_setup_flip == 1   ? arc_specials[arc_variant_selected[1]].about
                       : arc_setup_flip == 2 ? arc_guards[arc_variant_selected[1]].about
                                             : arc_variants[arc_variant_selected[1]].trait;
            }
            break;
        case arc_setup_quit:
            text = "Return to Applications";
            break;
        default:
            break;
    }

    snprintf(out, size, "%s", TRS((char *) text));
}

static int arc_trait_width(void) {
    int widest = 0;

    const char *fixed[] = {
        "One player against the computer", "Two players taking turns on one device", "Return to Applications"
    };
    for (size_t index = 0; index < A_SIZE(fixed); index++) {
        const int width = arc_text_width(arc_menu_subtitle, TRS((char *) fixed[index]));
        if (width > widest) widest = width;
    }

    const char *long_name = TRS("the computer");
    const char *long_planet = "";
    for (int index = 0; index < ARC_VARIANT_COUNT; index++) {
        const int width = arc_text_width(arc_menu_subtitle, TRS((char *) arc_specials[index].about));
        if (width > widest) widest = width;
        const int guard_width = arc_text_width(arc_menu_subtitle, TRS((char *) arc_guards[index].about));
        if (guard_width > widest) widest = guard_width;
        if (strlen(TRS((char *) arc_variants[index].name)) > strlen(long_name))
            long_name = TRS((char *) arc_variants[index].name);
    }
    for (int index = 0; index < ARC_PLANET_COUNT; index++) {
        if (strlen(TRS((char *) arc_planets[index].name)) > strlen(long_planet))
            long_planet = TRS((char *) arc_planets[index].name);
        const char *lines[] = {arc_planets[index].rules, arc_planets[index].watch};
        for (size_t line = 0; line < A_SIZE(lines); line++) {
            const int width = arc_text_width(arc_menu_subtitle, TRS((char *) lines[line]));
            if (width > widest) widest = width;
        }
    }

    char sample[256];
    arc_start_line(sample, sizeof(sample), long_name, long_name, long_planet);
    int width = arc_text_width(arc_menu_subtitle, sample);
    if (width > widest) widest = width;
    snprintf(sample, sizeof(sample), "%s %d %s", TRS("First to win"), 4, TRS("rounds takes the match"));
    width = arc_text_width(arc_menu_subtitle, sample);
    if (width > widest) widest = width;
    snprintf(
        sample, sizeof(sample), "%s %d %s, %d %s, %d%% %s, %s %d m", TRS("Record:"), 888, TRS("won"), 888, TRS("lost"),
        88, TRS("hits"), TRS("longest"), 888
    );
    width = arc_text_width(arc_menu_subtitle, sample);
    if (width > widest) widest = width;

    for (int index = 0; index < ARC_VARIANT_COUNT + arc_difficulty_count + ARC_PLANET_COUNT; index++) {
        const char *text = index < ARC_VARIANT_COUNT ? arc_variants[index].trait
                           : index < ARC_VARIANT_COUNT + arc_difficulty_count
                               ? arc_difficulty_traits[index - ARC_VARIANT_COUNT]
                               : arc_planets[index - ARC_VARIANT_COUNT - arc_difficulty_count].trait;
        const int width = arc_text_width(arc_menu_subtitle, TRS((char *) text));
        if (width > widest) widest = width;
    }

    return widest;
}

static void arc_hide_aim(void) {
    for (int index = 0; index < ARC_AIM_DOTS; index++)
        arc_set_hidden(arc_aim[index], 1);
}

static void arc_hide_path(void) {
    for (int index = 0; index < ARC_PATH_DOTS; index++)
        arc_set_hidden(arc_path[index], 1);
}

static void arc_show_menu(
    const enum arc_screen screen, const char *title, const char *subtitle, const char *const *options, const int count
) {
    arc_screen = screen;
    arc_menu_count = arc_clamp(count, 0, ARC_MENU_ROWS);
    arc_menu_selected = 0;

    lv_label_set_text(arc_menu_title, title ? title : "");
    lv_label_set_text(arc_menu_subtitle, subtitle ? subtitle : "");

    const int line_h = lv_font_get_line_height(lv_obj_get_style_text_font(arc_menu_subtitle, LV_PART_MAIN));
    const int gap = arc_clamp(arc_scale(6), 4, 10);
    const int heads = (title && title[0] ? 1 : 0) + (subtitle && subtitle[0] ? 1 : 0);
    const int rule_gap = heads ? arc_clamp(arc_scale(6), 4, 10) : 0;
    const int room = arc_height - 16 - 32 - rule_gap * 2 - (arc_menu_count - 1) * gap;
    int row_h = arc_clamp(arc_scale(42), 34, 58);
    if (arc_menu_count + heads > 0 && row_h * (arc_menu_count + heads) > room) row_h = room / (arc_menu_count + heads);
    if (row_h < line_h + 4) row_h = line_h + 4;

    const int title_h = title && title[0] ? row_h : 0;
    const int subtitle_h = subtitle && subtitle[0] ? row_h : 0;
    const int panel_h = 32 + title_h + subtitle_h + rule_gap * 2 + arc_menu_count * row_h + (arc_menu_count - 1) * gap;
    int panel_w = arc_clamp(arc_width * 3 / 5, 280, 660);
    if (screen == arc_screen_setup) {
        const int trait_w = arc_trait_width() + 48;
        if (trait_w > panel_w) panel_w = arc_clamp(trait_w, 280, arc_width - 16);
    } else if (subtitle && subtitle[0]) {
        const int subtitle_w = arc_text_width(arc_menu_subtitle, subtitle) + 48;
        if (subtitle_w > panel_w) panel_w = arc_clamp(subtitle_w, 280, arc_width - 16);
    }

    lv_obj_set_size(arc_menu, panel_w, arc_clamp(panel_h, 160, arc_height - 16));
    lv_obj_center(arc_menu);
    lv_obj_set_y(arc_menu_title, 10 + (title_h - line_h) / 2);
    lv_obj_set_y(arc_menu_subtitle, 10 + title_h + (subtitle_h - line_h) / 2);
    lv_obj_set_height(arc_menu_subtitle, line_h);

    lv_obj_set_size(arc_menu_rule, panel_w - 32, 1);
    lv_obj_set_pos(arc_menu_rule, 16, 10 + title_h + subtitle_h + rule_gap);
    arc_set_hidden(arc_menu_rule, !heads);

    int row_y = 16 + title_h + subtitle_h + rule_gap * 2;
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
    arc_set_hidden(arc_hud, !keep_arena);
    for (int index = 0; index < 2; index++)
        arc_set_hidden(arc_actor[index], !keep_arena);
    arc_hide_aim();
    arc_hide_path();
    arc_set_hidden(arc_menu, 0);
    arc_style_menu();
    arc_refresh_footer();
}

static float arc_hash_noise(const int x, const int y, const uint32_t seed) {
    uint32_t value = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u + seed * 2246822519u;
    value = (value ^ (value >> 13)) * 1274126177u;
    return (float) ((value ^ (value >> 16)) & 0xFFFF) / 65535.0f;
}

static float arc_value_noise(const float x, const float y, const uint32_t seed) {
    const int cell_x = (int) floorf(x);
    const int cell_y = (int) floorf(y);
    float fx = x - (float) cell_x;
    float fy = y - (float) cell_y;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    const float a = arc_hash_noise(cell_x, cell_y, seed);
    const float b = arc_hash_noise(cell_x + 1, cell_y, seed);
    const float c = arc_hash_noise(cell_x, cell_y + 1, seed);
    const float d = arc_hash_noise(cell_x + 1, cell_y + 1, seed);
    return (a + (b - a) * fx) * (1.0f - fy) + (c + (d - c) * fx) * fy;
}

static uint32_t arc_surface_colour(const int x, const int y, const uint32_t base) {
    const uint32_t noise = ((uint32_t) (x >> 1) * 73856093u) ^ ((uint32_t) (y >> 1) * 19349663u);
    const int grain = (int) ((noise >> 7) & 15) - 8;

    switch (arc_planet_current()->surface) {
        case arc_surface_craters:
            if (arc_hash_noise(x, y, 0xC4A7u) < 0.012f) return arc_shade(base, -34);
            return arc_shade(base, grain + (int) ((noise >> 12) & 7));
        case arc_surface_volcanic: {
            const float vein = arc_value_noise((float) x / 16.0f, (float) y / 16.0f, 0x5EED1u);
            const float deep = arc_clampf((float) (y - arc_ground_top) / (float) arc_scale(60), 0.0f, 1.0f);
            const float glow = (1.0f - arc_clampf(fabsf(vein - 0.5f) / 0.018f, 0.0f, 1.0f)) * deep;
            const uint32_t rock = arc_shade(base, grain - 10);
            return glow > 0.0f ? arc_mix(0xFF7A1F, rock, (int) (220.0f * glow)) : rock;
        }
        case arc_surface_mesas: {
            const int stride = arc_clamp(arc_scale(5), 3, 8);
            const int layer = (y / stride) % 3;
            return arc_shade(base, grain / 2 + (layer == 0 ? 12 : layer == 1 ? -4 : -14));
        }
        case arc_surface_bands: {
            const float band = sinf((float) y * 0.14f + sinf((float) x * 0.021f) * 1.6f);
            return arc_shade(base, (int) (band * 18.0f) + grain / 3);
        }
        case arc_surface_ice:
            if (arc_hash_noise(x, y, 0x1CEu) < 0.004f) return 0xFFFFFF;
            return arc_shade(arc_mix(base, 0xE6F6FF, 210), grain / 2);
        default:
            return arc_shade(base, grain);
    }
}

static void arc_paint_ground(const int from, const int to) {
    if (!arc_terrain_buf || !arc_ground) return;

    const int start = from <= 0 ? 0 : arc_clamp(from, 0, arc_width - 1) + arc_pad_left;
    const int end = to >= arc_width - 1 ? arc_canvas_w - 1 : arc_clamp(to, 0, arc_width - 1) + arc_pad_left;
    const int lip = arc_clamp(arc_scale(3), 2, 5);
    const int band = lip + arc_clamp(arc_scale(5), 3, 8);
    const uint32_t shadow = arc_shade(arc_lip_colour, -70);

    int highest = arc_canvas_h;
    for (int column = start; column <= end; column++) {
        const int x = arc_clamp(column - arc_pad_left, 0, arc_width - 1);
        if (arc_ground[x] < highest) highest = arc_ground[x];
    }

    for (int y = 0; y < arc_canvas_h; y++) {
        lv_color32_t *row = arc_terrain_buf + (size_t) y * (size_t) arc_canvas_w;
        if (y < highest) {
            for (int column = start; column <= end; column++)
                row[column].full = 0;
            continue;
        }

        for (int column = start; column <= end; column++) {
            const int x = arc_clamp(column - arc_pad_left, 0, arc_width - 1);
            const int depth = y - arc_ground[x];
            if (depth < 0) {
                row[column].full = 0;
                continue;
            }

            uint32_t colour;
            if (depth < lip) {
                colour = arc_lip_colour;
            } else if (depth < band) {
                colour = arc_mix(shadow, arc_row_colour[y], 255 - (depth - lip) * 255 / (band - lip));
                colour = arc_mix(colour, arc_row_colour[y], 150);
            } else {
                colour = arc_surface_colour(column, y, arc_row_colour[y]);
            }

            if (arc_stain[x] && depth < band + 2) colour = arc_mix(arc_stain_colour[x], colour, arc_stain[x]);
            if (depth < arc_scorch[x]) colour = arc_mix(0x000000, colour, 110 - depth * 110 / (arc_scorch[x] + 1));

            row[column].full = 0xFF000000u | colour;
        }
    }

    canvas_invalidate_gpu_texture(arc_terrain);
    lv_obj_invalidate(arc_terrain);
}

static void arc_build_palette(void) {
    const arc_planet *planet = arc_planet_current();
    const uint32_t theme_ground = arc_mix(arc_focus_colour(), arc_panel_colour(), 77);
    const uint32_t surface = arc_shade(arc_mix(planet->ground, theme_ground, 150), -24);
    const uint32_t deep = arc_mix(0x000000, surface, 120);
    arc_lip_colour = arc_mix(arc_focus_colour(), planet->ground, 150);
    if (planet->surface == arc_surface_ice) arc_lip_colour = arc_mix(0xF4FBFF, arc_lip_colour, 200);
    if (planet->surface == arc_surface_volcanic) arc_lip_colour = arc_mix(0x2A1E1A, arc_lip_colour, 120);

    const int span = arc_height - arc_ground_top > 1 ? arc_height - arc_ground_top : 1;
    for (int y = 0; y < arc_canvas_h; y++) {
        const int depth = arc_clamp((y - arc_ground_top) * 255 / span, 0, 255);
        arc_row_colour[y] = arc_mix(deep, surface, depth);
    }
}

static float arc_smooth(const float edge0, const float edge1, const float value) {
    const float t = arc_clampf((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static uint32_t arc_seed_next(uint32_t *seed) {
    *seed = *seed * 1664525u + 1013904223u;
    return *seed >> 8;
}

static float arc_seed_float(uint32_t *seed) {
    return (float) (arc_seed_next(seed) & 0xFFFF) / 65535.0f;
}

static uint32_t arc_pixel(const uint32_t colour, const float alpha) {
    const int value = arc_clamp((int) lroundf(alpha * 255.0f), 0, 255);
    return value ? ((uint32_t) value << 24) | colour : 0;
}

static void arc_paint_sun(const arc_planet *planet, const int sun) {
    if (!arc_sun_buf) return;

    const float haze = (float) planet->cloud_opa / 255.0f;
    const float centre = (float) arc_sun_box * 0.5f;
    const float radius = (float) sun * 0.5f;
    const float reach = arc_clampf(radius * (1.7f + 1.6f * haze) + (float) arc_scale(8), radius + 2.0f, centre - 1.0f);
    const float edge = 1.0f + haze * radius * 0.3f;
    const float disc_opa = 1.0f - haze * 0.3f;
    const float glow_opa = 0.22f + 0.38f * haze;
    const uint32_t core = arc_mix(planet->cloud, 0xFFFFFF, (int) (70.0f * haze));
    const uint32_t limb = arc_mix(planet->sky_bottom, 0xFFD27A, (int) (90.0f * haze));
    const uint32_t halo = arc_mix(planet->sky_bottom, planet->halo, (int) (120.0f * haze));
    const float spike = planet->distance >= 500 ? 0.55f : 0.0f;

    for (int y = 0; y < arc_sun_box; y++) {
        lv_color32_t *row = arc_sun_buf + (size_t) y * (size_t) arc_sun_box;
        const float dy = (float) y + 0.5f - centre;
        for (int x = 0; x < arc_sun_box; x++) {
            const float dx = (float) x + 0.5f - centre;
            const float distance = sqrtf(dx * dx + dy * dy);
            if (distance >= reach) {
                row[x].full = 0;
                continue;
            }

            const float disc = arc_clampf((radius - distance) / edge + 0.5f, 0.0f, 1.0f) * disc_opa;
            const float ratio = arc_clampf(distance / radius, 0.0f, 1.0f);
            const float limb_light = sqrtf(1.0f - ratio * ratio);
            const uint32_t face =
                arc_mix(core, limb, (int) (255.0f * arc_clampf(0.15f + 1.2f * limb_light, 0.0f, 1.0f)));

            const float fall = 1.0f - arc_clampf((distance - radius * 0.7f) / (reach - radius * 0.7f), 0.0f, 1.0f);
            float glow = fall * fall * fall * glow_opa;
            if (spike > 0.0f) {
                const float along = 1.0f - distance / reach;
                const float beam_x = arc_clampf(1.0f - fabsf(dy) / 1.2f, 0.0f, 1.0f);
                const float beam_y = arc_clampf(1.0f - fabsf(dx) / 1.2f, 0.0f, 1.0f);
                glow = arc_clampf(glow + (beam_x > beam_y ? beam_x : beam_y) * along * along * spike, 0.0f, 1.0f);
            }

            const float alpha = disc + glow * (1.0f - disc);
            const uint32_t colour = alpha > 0.0f ? arc_mix(face, halo, (int) (255.0f * disc / alpha)) : halo;
            row[x].full = arc_pixel(colour, alpha);
        }
    }

    canvas_invalidate_gpu_texture(arc_sun);
    lv_obj_invalidate(arc_sun);
}

static uint32_t arc_body_surface(const int body, const float longitude, const float latitude) {
    if (body == arc_body_jupiter) {
        const float swirl = arc_value_noise(longitude * 4.0f, latitude * 9.0f, 0x10Bu);
        const float band = sinf(latitude * 15.0f + swirl * 2.2f);
        uint32_t colour = arc_mix(0xE8D9BC, 0xB07A4F, (int) (127.0f + 127.0f * band));
        const float spot_x = (longitude - 0.35f) / 0.32f;
        const float spot_y = (latitude - 0.38f) / 0.11f;
        const float spot = spot_x * spot_x + spot_y * spot_y;
        if (spot < 1.0f) colour = arc_mix(0xC8623E, colour, (int) (255.0f * arc_smooth(1.0f, 0.4f, spot)));
        return colour;
    }

    const float land = arc_value_noise(longitude * 2.6f + 3.0f, latitude * 2.6f, 0xEA27u) * 0.7f
                       + arc_value_noise(longitude * 6.0f, latitude * 6.0f, 0xEA28u) * 0.3f;
    const float cloud = arc_value_noise(longitude * 5.0f, latitude * 8.0f, 0xC10Du);
    uint32_t colour = land > 0.56f ? arc_mix(0x5E7F3A, 0xA08550, (int) (255.0f * arc_smooth(0.56f, 0.7f, land)))
                                   : arc_mix(0x1E4FA0, 0x2E6CC0, (int) (255.0f * land));
    if (fabsf(latitude) > 1.25f) colour = 0xF2F6FA;
    if (cloud > 0.58f) colour = arc_mix(0xFFFFFF, colour, (int) (255.0f * arc_smooth(0.58f, 0.75f, cloud)));
    return colour;
}

static void arc_paint_galaxy(const int size, const int opa) {
    const float centre = (float) arc_body_box * 0.5f;
    const float length = (float) size;
    const float tilt_cos = cosf(0.62f);
    const float tilt_sin = sinf(0.62f);
    const float strength = (float) opa / 255.0f;

    for (int y = 0; y < arc_body_box; y++) {
        lv_color32_t *row = arc_body_buf + (size_t) y * (size_t) arc_body_box;
        const float dy = (float) y + 0.5f - centre;
        for (int x = 0; x < arc_body_box; x++) {
            const float dx = (float) x + 0.5f - centre;
            const float along = (dx * tilt_cos + dy * tilt_sin) / length;
            const float across = (-dx * tilt_sin + dy * tilt_cos) / (length * 0.3f);
            const float reach = sqrtf(along * along + across * across);
            if (reach >= 1.0f) {
                row[x].full = 0;
                continue;
            }

            const float core = expf(-reach * reach * 40.0f);
            const float arm = 0.55f + 0.45f * sinf(2.0f * atan2f(across, along) - reach * 11.0f);
            const float dust = arc_value_noise((float) x / 3.0f, (float) y / 3.0f, 0xA31u);
            const float disc = (1.0f - arc_smooth(0.2f, 1.0f, reach)) * (0.45f + 0.35f * arm) * (0.7f + 0.3f * dust);

            const float sx = dx / length + 0.28f;
            const float sy = dy / length + 0.05f;
            const float satellite = expf(-(sx * sx + sy * sy) * 900.0f) * 0.6f;

            const float alpha = arc_clampf((core * 0.9f + disc * 0.38f + satellite) * strength, 0.0f, 0.85f);
            const uint32_t colour = arc_mix(0xFFF0D6, 0xC8D4FF, (int) (255.0f * arc_clampf(core * 2.0f, 0.0f, 1.0f)));
            row[x].full = arc_pixel(colour, alpha);
        }
    }
}

static void arc_paint_body(const arc_planet *planet, const int size) {
    if (!arc_body_buf) return;

    if (planet->body == arc_body_galaxy) {
        arc_paint_galaxy(size, planet->star_opa);
    } else {
        const float centre = (float) arc_body_box * 0.5f;
        const float radius = (float) size;
        const float light_x = 0.84f;
        const float light_y = -0.28f;
        const float light_z = 0.46f;

        for (int y = 0; y < arc_body_box; y++) {
            lv_color32_t *row = arc_body_buf + (size_t) y * (size_t) arc_body_box;
            const float dy = (float) y + 0.5f - centre;
            for (int x = 0; x < arc_body_box; x++) {
                const float dx = (float) x + 0.5f - centre;
                const float distance = sqrtf(dx * dx + dy * dy);
                const float edge = arc_clampf(radius - distance + 0.5f, 0.0f, 1.0f);
                if (edge <= 0.0f) {
                    row[x].full = 0;
                    continue;
                }

                const float nx = arc_clampf(dx / radius, -1.0f, 1.0f);
                const float ny = arc_clampf(dy / radius, -1.0f, 1.0f);
                const float nz = sqrtf(fmaxf(1.0f - nx * nx - ny * ny, 0.0f));
                const float lit = nx * light_x + ny * light_y + nz * light_z;
                const float day = arc_smooth(-0.08f, 0.3f, lit) * (0.55f + 0.45f * arc_clampf(lit, 0.0f, 1.0f));
                const float shine = 0.06f + 0.94f * day;
                const float limb = 0.75f + 0.25f * nz;

                const uint32_t surface = arc_body_surface(planet->body, atan2f(nx, nz), asinf(ny));
                const uint32_t colour =
                    arc_mix(surface, 0x000000, (int) (255.0f * arc_clampf(shine * limb, 0.0f, 1.0f)));
                row[x].full = arc_pixel(colour, edge);
            }
        }
    }

    canvas_invalidate_gpu_texture(arc_body);
    lv_obj_invalidate(arc_body);
}

static void arc_paint_cloud(arc_cloud *cloud, const arc_planet *planet, const int opa) {
    if (!cloud->buf) return;

    float puff_x[12];
    float puff_y[12];
    float puff_r[12];
    uint32_t seed = cloud->seed;
    const float width = (float) cloud->width;
    const float height = (float) cloud->height;
    const float base = height * 0.8f;

    const int style = planet->cloud_style;
    const int wispy = style == arc_cloud_cirrus || style == arc_cloud_streak;
    const float stretch_x = style == arc_cloud_cumulus ? 1.0f : style == arc_cloud_stratus ? 0.7f : 0.42f;
    const float stretch_y = style == arc_cloud_cumulus ? 1.0f : style == arc_cloud_stratus ? 1.6f : 2.4f;
    const float shading = style == arc_cloud_cumulus ? 0.75f : style == arc_cloud_stratus ? 0.45f : 0.2f;
    const float tilt = style == arc_cloud_streak ? 0.0f : (arc_seed_float(&seed) - 0.5f) * 0.4f;

    const int row = (wispy ? 4 : 5) + (int) (arc_seed_next(&seed) % 3);
    for (int index = 0; index < row; index++) {
        const float place = ((float) index + 0.5f) / (float) row;
        const float hump = sinf(place * (float) M_PI);
        puff_r[index] = height * (0.16f + 0.16f * hump + 0.06f * arc_seed_float(&seed));
        puff_x[index] = width * (0.1f + 0.8f * place) + (arc_seed_float(&seed) - 0.5f) * width * 0.06f;
        puff_y[index] = base - puff_r[index] * (0.55f + 0.2f * arc_seed_float(&seed));
        if (wispy) {
            puff_r[index] = height * (0.14f + 0.08f * hump + 0.05f * arc_seed_float(&seed));
            puff_y[index] = height * (0.5f + tilt * (place - 0.5f)) + sinf(place * 5.0f + tilt * 9.0f) * height * 0.08f;
        }
    }

    const int tops = style == arc_cloud_cumulus ? 2 + (int) (arc_seed_next(&seed) % 3) : 0;
    for (int index = 0; index < tops; index++) {
        const int puff = row + index;
        puff_x[puff] = width * (0.28f + 0.44f * ((float) index + arc_seed_float(&seed)) / (float) tops);
        puff_r[puff] = height * (0.2f + 0.12f * arc_seed_float(&seed));
        puff_y[puff] = base - height * (0.3f + 0.2f * arc_seed_float(&seed));
    }

    const int count = row + tops;
    for (int index = 0; index < count; index++)
        if (puff_y[index] - puff_r[index] < 1.0f) puff_y[index] = puff_r[index] + 1.0f;

    const float strength = (float) opa / 255.0f * (wispy ? 0.75f : 1.0f);
    const float grain = height * 0.16f;
    const uint32_t top = arc_shade(planet->cloud, 12);
    const uint32_t belly = arc_mix(arc_shade(planet->cloud, -70), planet->sky_bottom, 140);

    for (int y = 0; y < cloud->height; y++) {
        lv_color32_t *line = cloud->buf + (size_t) y * (size_t) cloud->width;
        const float py = (float) y + 0.5f;
        const float floor = wispy ? 1.0f : arc_clampf((base + height * 0.06f - py) / (height * 0.2f), 0.0f, 1.0f);
        const float depth = arc_clampf((py - height * 0.15f) / (base - height * 0.15f), 0.0f, 1.0f);

        for (int x = 0; x < cloud->width; x++) {
            const float px = (float) x + 0.5f;
            float density = 0.0f;
            float lit_weight = 0.0f;
            float lit_sum = 0.0f;
            for (int index = 0; index < count; index++) {
                const float dx = px - puff_x[index];
                const float dy = py - puff_y[index];
                const float value =
                    1.0f - sqrtf(dx * dx * stretch_x * stretch_x + dy * dy * stretch_y * stretch_y) / puff_r[index];
                if (value <= 0.0f) continue;
                if (value > density) density = value;
                const float weight = value * value;
                lit_weight += weight;
                lit_sum += weight * (0.5f - (dy * 0.8f + dx * 0.3f) / (puff_r[index] * 2.0f));
            }

            if (density <= 0.0f || floor <= 0.0f) {
                line[x].full = 0;
                continue;
            }

            const float grain_x = wispy ? grain * 3.0f : grain;
            const float noise = arc_value_noise(px / grain_x, py / grain, cloud->seed) * 0.65f
                                + arc_value_noise(px / grain_x * 2.3f, py / grain * 2.3f, cloud->seed + 7u) * 0.35f;
            density += (noise - 0.5f) * (wispy ? 0.45f : 0.3f);

            const float alpha = arc_smooth(0.0f, 0.35f, density) * floor * strength;
            if (alpha <= 0.0f) {
                line[x].full = 0;
                continue;
            }

            const float lit = lit_sum / lit_weight;
            const float light =
                arc_clampf(1.0f - depth * shading + (lit - 0.5f) * 0.6f + (noise - 0.5f) * 0.2f, 0.0f, 1.0f);
            line[x].full = arc_pixel(arc_mix(top, belly, (int) (255.0f * light)), alpha);
        }
    }

    canvas_invalidate_gpu_texture(cloud->obj);
    lv_obj_invalidate(cloud->obj);
}

static void arc_layout_sky(void) {
    const arc_planet *planet = arc_planet_current();

    lv_obj_set_style_bg_color(arc_sky, lv_color_hex(planet->sky_top), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_color(arc_sky, lv_color_hex(planet->sky_bottom), MU_OBJ_MAIN_DEFAULT);

    const float distance = (float) planet->distance / 100.0f;
    const int sun = arc_clamp((int) lroundf((float) arc_scale(40) / sqrtf(distance)), 4, arc_scale(64));
    const int sun_x = arc_width * 72 / 100;
    const int sun_y = arc_hud_bottom + arc_scale(34) + sun / 2;
    arc_paint_sun(planet, sun);
    lv_obj_set_pos(arc_sun, sun_x - arc_sun_box / 2, sun_y - arc_sun_box / 2);

    if (arc_body) {
        const int size = arc_clamp(arc_scale(planet->body_size), 4, arc_body_box / 2 - 2);
        if (planet->body != arc_body_none) {
            arc_paint_body(planet, size);
            const int body_x = arc_width * 24 / 100;
            const int body_y = planet->body == arc_body_galaxy ? arc_hud_bottom + arc_scale(44)
                                                               : arc_hud_bottom + arc_scale(26) + size;
            lv_obj_set_pos(arc_body, body_x - arc_body_box / 2, body_y - arc_body_box / 2);
        }
        arc_set_hidden(arc_body, planet->body == arc_body_none);
    }

    for (int index = 0; index < ARC_STAR_COUNT; index++) {
        arc_star *star = &arc_stars[index];
        star->base_opa = planet->star_opa * (120 + arc_rand(136)) / 255;
        arc_set_hidden(star->obj, star->base_opa < 8);
    }

    for (int index = 0; index < ARC_CLOUD_COUNT; index++) {
        arc_cloud *cloud = &arc_clouds[index];
        if (planet->cloud_opa) arc_paint_cloud(cloud, planet, planet->cloud_opa * (150 + index * 25) / 255);
        arc_set_hidden(cloud->obj, planet->cloud_opa == 0);
    }
}

static void arc_apply_planet(void) {
    arc_layout_sky();
    arc_build_palette();
    arc_paint_ground(0, arc_width - 1);
}

static void arc_roll_wind(const int fresh) {
    const int limit = arc_planet_current()->wind;
    if (!limit) {
        arc_wind = 0;
        return;
    }
    const int shift = arc_clamp(arc_planet_current()->gusts, 1, 6);
    if (fresh)
        arc_wind = arc_rand(limit * 2 + 1) - limit;
    else
        arc_wind = arc_clamp(arc_wind + arc_rand(shift * 2 + 1) - shift, -limit, limit);
}

static float arc_wind_accel(void) {
    return (float) arc_wind * (float) arc_width * 0.012f;
}

static float arc_gravity(const int player) {
    return (float) arc_height * 1.2f * (float) arc_variants[arc_variant_selected[player]].weight / 100.0f
           * (float) arc_planet_current()->pull / 100.0f;
}

static const arc_weapon *arc_weapon_for(const int player) {
    if (arc_armed[player] == 2) return &arc_guards[arc_variant_selected[player]];
    if (arc_armed[player] == 1) return &arc_specials[arc_variant_selected[player]];
    return &arc_main_weapon;
}

static void arc_launch(const int player, const int angle, const int power, float *x, float *y, float *vx, float *vy) {
    const float radians = (float) angle * (float) M_PI / 180.0f;
    const float direction = player ? -1.0f : 1.0f;
    const float velocity = (float) arc_width * (0.25f + 0.008f * (float) power)
                           * (float) arc_variants[arc_variant_selected[player]].launch / 100.0f
                           * (float) arc_weapon_for(player)->launch / 100.0f;
    *x = arc_actor_x[player] + arc_actor_w * 0.5f;
    *y = arc_actor_y[player] - arc_scale(3);
    *vx = direction * cosf(radians) * velocity;
    *vy = -sinf(radians) * velocity;
}

static void
arc_physics(float *x, float *y, float *vx, float *vy, const float gravity, const float wind, const float dt) {
    const float drag = 1.0f - 0.045f * (float) arc_planet_current()->air / 100.0f * dt;
    *vx = *vx * drag + wind * dt;
    *vy = *vy * drag + gravity * dt;
    *x += *vx * dt;
    *y += *vy * dt;
}

static int arc_inside_actor(const int player, const float x, const float y) {
    const float inset_x = arc_actor_w * 0.20f;
    const float inset_y = arc_actor_h * 0.10f;
    return x >= arc_actor_x[player] + inset_x && x <= arc_actor_x[player] + arc_actor_w - inset_x
           && y >= arc_actor_y[player] + inset_y && y <= arc_actor_y[player] + arc_actor_h - inset_y;
}

static float arc_shell_gravity(const int player) {
    return arc_gravity(player) * (float) arc_weapon_for(player)->gravity / 100.0f;
}

static void arc_shell_start(arc_shell *shell, const int player, const int angle, const int power) {
    memset(shell, 0, sizeof(*shell));
    arc_launch(player, angle, power, &shell->x, &shell->y, &shell->vx, &shell->vy);
    shell->peak = shell->y;
    shell->hit = -1;
    shell->active = 1;
}

static int arc_shell_wall(arc_shell *shell) {
    const float inset = arc_walls == arc_walls_rubber || arc_walls == arc_walls_concrete ? (float) arc_wall_w : 0.0f;
    const float left = inset;
    const float right = (float) arc_width - inset;
    if (shell->x >= left && shell->x < right) return arc_event_fly;

    const int side = shell->x < left ? 0 : 1;
    if (arc_wall_live && arc_walls != arc_walls_open) {
        arc_wall_pending[side] = 1;
        arc_wall_pending_y[side] = (int) lroundf(shell->y);
    }

    switch (arc_walls) {
        case arc_walls_rubber:
            if (!side) {
                shell->x = left * 2.0f - shell->x;
                shell->vx = fabsf(shell->vx) * 0.8f;
            } else {
                shell->x = right * 2.0f - shell->x - 1.0f;
                shell->vx = -fabsf(shell->vx) * 0.8f;
            }
            shell->x = arc_clampf(shell->x, left, right - 1.0f);
            return arc_event_fly;
        case arc_walls_wrap:
            shell->x += side ? -right : right;
            shell->x = arc_clampf(shell->x, 0.0f, right - 1.0f);
            return arc_event_fly;
        case arc_walls_concrete:
            shell->x = arc_clampf(shell->x, left, right - 1.0f);
            return arc_event_ground;
        default:
            return arc_event_lost;
    }
}

static int arc_shell_step(
    arc_shell *shell, const arc_weapon *weapon, const int shooter, const float gravity, const float wind, const float dt
) {
    shell->age += dt;
    if (shell->age >= 20.0f) return arc_event_lost;

    if (shell->rolling) {
        const int column = arc_clamp((int) shell->x, 1, arc_width - 2);
        const float slope = (float) (arc_ground[column + 1] - arc_ground[column - 1]) * 0.5f;
        shell->vx += gravity * 0.7f * slope / sqrtf(1.0f + slope * slope) * dt;
        shell->vx *= 1.0f - 0.6f * dt;
        shell->x += shell->vx * dt;
        shell->roll += dt;
        const int wall = arc_shell_wall(shell);
        if (wall != arc_event_fly) return wall;
        shell->y = (float) arc_ground[arc_clamp((int) shell->x, 0, arc_width - 1)] - 1.0f;

        for (int player = 0; player < 2; player++) {
            if (player == shooter && shell->roll < 0.6f) continue;
            if (fabsf(shell->x - (arc_actor_x[player] + arc_actor_w * 0.5f)) < arc_actor_w * 0.45f) {
                shell->hit = player;
                return arc_event_actor;
            }
        }

        if (shell->roll >= 3.4f || fabsf(shell->vx) < (float) arc_scale(8)) return arc_event_ground;
        return arc_event_fly;
    }

    arc_physics(&shell->x, &shell->y, &shell->vx, &shell->vy, gravity, wind, dt);
    if (shell->y < shell->peak) shell->peak = shell->y;

    if (shell->age >= 0.2f) {
        for (int player = 0; player < 2; player++) {
            if (arc_inside_actor(player, shell->x, shell->y)) {
                shell->hit = player;
                return arc_event_actor;
            }
        }
    }

    if (shell->y >= (float) arc_height) return arc_event_lost;
    const int wall = arc_shell_wall(shell);
    if (wall != arc_event_fly) return wall;

    if (shell->y >= 0.0f) {
        const int column = arc_clamp((int) shell->x, 0, arc_width - 1);
        if (shell->y >= (float) arc_ground[column]) {
            if (weapon->kind == arc_kind_roller) {
                const float speed = fabsf(shell->vx) * 0.45f + fabsf(shell->vy) * 0.15f;
                shell->vx = (shell->vx < 0.0f ? -1.0f : 1.0f) * fmaxf(speed, (float) arc_scale(45));
                shell->vy = 0.0f;
                shell->y = (float) arc_ground[column] - 1.0f;
                shell->rolling = 1;
                return arc_event_fly;
            }
            if (weapon->kind == arc_kind_bounce && shell->bounces < 2 && fabsf(shell->vy) > (float) arc_scale(40)) {
                shell->y = (float) arc_ground[column] - 1.0f;
                shell->vy = -fabsf(shell->vy) * 0.5f;
                shell->vx *= 0.75f;
                shell->bounces++;
                return arc_event_fly;
            }
            return arc_event_ground;
        }
    }

    if (weapon->kind == arc_kind_cluster && !shell->split && shell->vy >= 0.0f) {
        shell->split = 1;
        return arc_event_split;
    }

    return arc_event_fly;
}

static float arc_simulate(const int player, const int angle, const int power) {
    arc_shell shell;
    arc_shell_start(&shell, player, angle, power);
    const arc_weapon *weapon = arc_weapon_for(player);
    const float gravity = arc_shell_gravity(player);
    const float wind = arc_wind_accel();
    const int target = player ^ 1;
    const float aim_x = arc_actor_x[target] + arc_actor_w * 0.5f;
    const float dt = ARC_STEP;
    const float far = (float) arc_width * 4.0f;

    for (int step = 1; step < 2200; step++) {
        switch (arc_shell_step(&shell, weapon, player, gravity, wind, dt)) {
            case arc_event_ground:
                return shell.x - aim_x;
            case arc_event_actor:
                return shell.hit == target ? 0.0f : far;
            case arc_event_lost:
                return far;
            default:
                break;
        }
    }

    return far;
}

static void arc_plan_cpu(int *angle_out, int *power_out) {
    int good_angle[16];
    int good_power[16];
    int good = 0;
    float best = FLT_MAX;
    int best_angle = 45;
    int best_power = 60;

    for (int angle = 15; angle <= 80; angle += 5) {
        float angle_best = FLT_MAX;
        int angle_power = 60;
        for (int power = 20; power <= 100; power += 2) {
            const float miss = fabsf(arc_simulate(1, angle, power));
            if (miss < angle_best) {
                angle_best = miss;
                angle_power = power;
            }
        }
        if (angle_best < best) {
            best = angle_best;
            best_angle = angle;
            best_power = angle_power;
        }
        if (angle_best < arc_actor_w * 0.45f && good < (int) A_SIZE(good_angle)) {
            good_angle[good] = angle;
            good_power[good] = angle_power;
            good++;
        }
    }

    if (good) {
        const int pick = arc_rand(good);
        best_angle = good_angle[pick];
        best_power = good_power[pick];
        best = fabsf(arc_simulate(1, best_angle, best_power));
    }

    for (int power = best_power - 1; power <= best_power + 1; power += 2) {
        if (power < 20 || power > 100) continue;
        const float miss = fabsf(arc_simulate(1, best_angle, power));
        if (miss < best) {
            best = miss;
            best_power = power;
        }
    }

    *angle_out = best_angle;
    *power_out = best_power;
}

static void arc_wind_text(char *out, const size_t size) {
    if (!arc_wind) {
        snprintf(out, size, "%s %s", TRS("Wind"), TRS("Calm"));
        return;
    }

    const int strength = abs(arc_wind);
    const char *arrows[] = {">", ">>", ">>>"};
    const char *backs[] = {"<", "<<", "<<<"};
    const int level = arc_clamp((strength - 1) / 3, 0, 2);
    if (arc_wind > 0)
        snprintf(out, size, "%s %d %s", TRS("Wind"), strength, arrows[level]);
    else
        snprintf(out, size, "%s %s %d", TRS("Wind"), backs[level], strength);
}

static void arc_refresh_setup(void) {
    static const char *difficulty_names[arc_difficulty_count] = {
        "Pushover", "Easy", "Medium", "Hard", "Insane",
    };
    const char *labels[arc_setup_count] = {
        TRS("Start"), TRS("Players"),  TRS("Match Length"),
        TRS("World"), TRS("Player 1"), arc_players == 1 ? TRS("Difficulty") : TRS("Player 2"),
        TRS("Quit"),
    };
    const char *values[arc_setup_count] = {
        "",
        arc_players == 1 ? TRS("1 Player") : TRS("2 Players"),
        arc_best_of == 3   ? TRS("Best of 3")
        : arc_best_of == 5 ? TRS("Best of 5")
                           : TRS("Best of 7"),
        TRS((char *) arc_planet_current()->name),
        TRS((char *) arc_variants[arc_variant_selected[0]].name),
        arc_players == 1 ? TRS((char *) difficulty_names[arc_difficulty])
                         : TRS((char *) arc_variants[arc_variant_selected[1]].name),
        "",
    };

    char line[256];
    arc_setup_line(line, sizeof(line));
    arc_set_text(arc_menu_subtitle, line);

    for (int index = 0; index < arc_setup_rows(); index++) {
        const int item = arc_setup_main[index];
        arc_set_text(arc_menu_label[index], labels[item]);
        if (item == arc_setup_start || item == arc_setup_quit) {
            lv_obj_set_width(arc_menu_label[index], LV_PCT(100));
            lv_obj_set_style_text_align(arc_menu_label[index], LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
            lv_obj_center(arc_menu_label[index]);
            arc_set_hidden(arc_menu_value[index], 1);
            continue;
        }

        lv_obj_set_width(arc_menu_label[index], LV_PCT(48));
        lv_obj_set_style_text_align(arc_menu_label[index], LV_TEXT_ALIGN_LEFT, MU_OBJ_MAIN_DEFAULT);
        lv_obj_align(arc_menu_label[index], LV_ALIGN_LEFT_MID, arc_scale(10), 0);
        arc_set_text(arc_menu_value[index], values[item]);
        lv_obj_set_width(arc_menu_value[index], LV_PCT(48));
        lv_obj_set_style_text_align(arc_menu_value[index], LV_TEXT_ALIGN_RIGHT, MU_OBJ_MAIN_DEFAULT);
        lv_obj_align(arc_menu_value[index], LV_ALIGN_RIGHT_MID, -arc_scale(10), 0);
        arc_set_hidden(arc_menu_value[index], 0);
    }

    arc_style_menu();
}

static void arc_show_setup(void) {
    static const char *options[ARC_MENU_ROWS] = {"", "", "", "", "", "", ""};
    arc_set_hidden(arc_bubble, 1);
    arc_show_menu(arc_screen_setup, NULL, " ", options, arc_setup_rows());
    arc_refresh_setup();
}

static int arc_aim_size(void) {
    return arc_clamp(arc_scale(7), 5, 11);
}

static int arc_is_aiming(void) {
    return arc_screen == arc_screen_play && !arc_shot_active && !arc_explosion_active && !arc_hazard_kind
           && arc_round_winner == -2;
}

static void arc_update_aim(const uint32_t now) {
    if (!arc_is_aiming() || arc_is_instant(arc_weapon_for(arc_turn))) {
        arc_hide_aim();
        return;
    }

    const float radians = (float) arc_angle[arc_turn] * (float) M_PI / 180.0f;
    const float direction = arc_turn ? -1.0f : 1.0f;
    const float start_x = arc_actor_x[arc_turn] + arc_actor_w * 0.5f;
    const float start_y = arc_actor_y[arc_turn] - arc_scale(2);
    const float spacing = (float) arc_scale(5) + (float) arc_scale(12) * (float) arc_power[arc_turn] / 100.0f;
    int guide = arc_variants[arc_variant_selected[arc_turn]].guide;
    if (arc_condition == arc_hazard_flare) guide = 0;
    if (arc_condition == arc_hazard_haze) guide = arc_clamp(guide / 2, 2, guide);
    const float wave = (float) ((now / 70) % (uint32_t) (guide + 4));
    const int base = arc_aim_size();

    for (int index = 0; index < ARC_AIM_DOTS; index++) {
        if (index >= guide) {
            arc_set_hidden(arc_aim[index], 1);
            continue;
        }
        const float distance = spacing * (float) (index + 1);
        const int x = (int) lroundf(start_x + direction * cosf(radians) * distance);
        const int y = (int) lroundf(start_y - sinf(radians) * distance);
        const float pulse = arc_clampf(1.0f - fabsf((float) index - wave) / 1.5f, 0.0f, 1.0f);
        const int size = base + (int) lroundf(pulse * (float) arc_clamp(arc_scale(3), 2, 4));
        if (lv_obj_get_style_width(arc_aim[index], LV_PART_MAIN) != size) lv_obj_set_size(arc_aim[index], size, size);
        arc_place(arc_aim[index], x - size / 2, y - size / 2);
        arc_set_fill(arc_aim[index], 205 + (int) (50.0f * pulse) - index * 3);
        arc_set_hidden(arc_aim[index], 0);
    }
}

static void arc_show_path(void) {
    arc_hide_path();
    if (!arc_is_aiming()) return;
    if (arc_players == 1 && arc_turn == 1) return;
    if (arc_condition == arc_hazard_haze) return;

    const int count = arc_path_count[arc_turn];
    for (int index = 0; index < count; index++) {
        const int size = arc_clamp(arc_scale(4), 3, 6);
        arc_place(
            arc_path[index], (int) lroundf(arc_path_points[arc_turn][index].x) - size / 2,
            (int) lroundf(arc_path_points[arc_turn][index].y) - size / 2
        );
        arc_set_hidden(arc_path[index], 0);
    }
}

static void arc_update_hud(void) {
    if (arc_variant_selected[0] < 0 || arc_variant_selected[1] < 0) return;

    char score[256];
    snprintf(
        score, sizeof(score), "%s  %d  -  %d  %s", TRS((char *) arc_variants[arc_variant_selected[0]].name),
        arc_score[0], arc_score[1], TRS((char *) arc_variants[arc_variant_selected[1]].name)
    );
    arc_set_text(arc_hud_score, score);

    char wind[64];
    arc_wind_text(wind, sizeof(wind));
    const char *condition = arc_condition == arc_hazard_gust    ? TRS("Gusting")
                            : arc_condition == arc_hazard_flare ? TRS("Aim guide offline")
                            : arc_condition == arc_hazard_haze  ? TRS("Aim guide hazy")
                                                                : "";
    char values[256];
    const int armed = arc_armed[arc_turn];
    snprintf(
        values, sizeof(values), "%s %d   %s %d%%   %s%s%s", TRS("Angle"), arc_angle[arc_turn], TRS("Power"),
        arc_power[arc_turn], wind, condition[0] ? "   " : "", condition
    );
    const int target = arc_turn ^ 1;
    if (arc_shield[target] > 0) {
        const size_t used = strlen(values);
        snprintf(values + used, sizeof(values) - used, "   %s %d%%", TRS("Guarded"), arc_shield[target]);
    }
    arc_set_text(arc_hud_values, values);

    if (arc_screen == arc_screen_play) {
        char weapon[128];
        arc_weapon_text(weapon, sizeof(weapon));
        if (strcmp(lv_label_get_text(ui_lbl_nav_x), weapon) != 0) arc_refresh_footer();
    }

    const uint32_t tint = armed ? arc_variants[arc_variant_selected[arc_turn]].colour : arc_focus_colour();
    if (tint != arc_aim_tint) {
        arc_aim_tint = tint;
        for (int index = 0; index < ARC_AIM_DOTS; index++) {
            lv_obj_set_style_bg_color(arc_aim[index], lv_color_hex(tint), MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_border_color(
                arc_aim[index], lv_color_hex(arc_mix(tint, 0x000000, 70)), MU_OBJ_MAIN_DEFAULT
            );
        }
    }

    char round_text[64];
    snprintf(round_text, sizeof(round_text), "%s %d", TRS("Round"), arc_round_number);
    arc_set_text(arc_hud_round, round_text);

    for (int player = 0; player < 2; player++) {
        const int full = arc_variants[arc_variant_selected[player]].health;
        const int lag = (int) lroundf(arc_health_lag[player]);
        if (player == 0) {
            lv_bar_set_value(arc_health_bar[0], arc_health[0], LV_ANIM_OFF);
            lv_bar_set_value(arc_health_lag_bar[0], lag, LV_ANIM_OFF);
        } else {
            lv_bar_set_value(arc_health_bar[1], full, LV_ANIM_OFF);
            lv_bar_set_start_value(arc_health_bar[1], full - arc_health[1], LV_ANIM_OFF);
            lv_bar_set_value(arc_health_lag_bar[1], full, LV_ANIM_OFF);
            lv_bar_set_start_value(arc_health_lag_bar[1], full - lag, LV_ANIM_OFF);
        }
    }

    arc_update_aim(lv_tick_get());
}

static void arc_position_actor(const int player) {
    const int centre = arc_clamp((int) lroundf(arc_actor_x[player] + arc_actor_w * 0.5f), 0, arc_width - 1);
    arc_actor_y[player] = arc_ground[centre] - arc_actor_h;
    lv_obj_set_pos(arc_actor[player], (int) lroundf(arc_actor_x[player]), (int) lroundf(arc_actor_y[player]));
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

static void arc_spawn_drop(arc_drop *drop, const int anywhere) {
    const int weather = arc_planet_current()->weather;
    const int sky_top = arc_hud_bottom - arc_scale(10);
    drop->phase = arc_randf() * 6.28f;
    drop->x = (float) arc_rand(arc_width);

    if (weather == arc_weather_dust) {
        drop->y = (float) (arc_ground_top - arc_scale(30) + arc_rand(arc_height - arc_ground_top + arc_scale(20)));
        if (!anywhere)
            drop->x = arc_wind < 0 ? (float) (arc_width + arc_rand(arc_scale(20))) : -(float) arc_rand(arc_scale(20));
        drop->speed = 0.6f + arc_randf() * 0.8f;
        return;
    }

    drop->y =
        anywhere ? (float) (sky_top + arc_rand(arc_height - sky_top)) : (float) (sky_top - arc_rand(arc_scale(40)));
    drop->speed = 0.75f + arc_randf() * 0.5f;
}

static void arc_roll_weather(void) {
    const arc_planet *planet = arc_planet_current();
    int level = 0;

    switch (planet->weather) {
        case arc_weather_rain:
            level = arc_rand(100) < 45 ? 0 : ARC_DROP_COUNT * (40 + arc_rand(61)) / 100;
            break;
        case arc_weather_virga:
            level = ARC_DROP_COUNT * (45 + arc_rand(40)) / 100;
            break;
        case arc_weather_dust:
            level = ARC_DROP_COUNT * (30 + arc_rand(50)) / 100;
            break;
        case arc_weather_snow:
            level = ARC_DROP_COUNT * (planet->wind ? 35 + arc_rand(50) : 10 + arc_rand(20)) / 100;
            break;
        case arc_weather_diamond:
            level = ARC_DROP_COUNT * (25 + arc_rand(30)) / 100;
            break;
        default:
            level = 0;
            break;
    }

    arc_drop_level = level;
    arc_meteor_at = lv_tick_get() + 1500 + (uint32_t) arc_rand(3000);
    arc_meteor_vx = 0.0f;

    const int thin = arc_clamp(arc_scale(1), 1, 2);
    for (int index = 0; index < ARC_DROP_COUNT; index++) {
        arc_drop *drop = &arc_drops[index];
        int width = thin;
        int height = thin;
        int radius = LV_RADIUS_CIRCLE;
        uint32_t colour = planet->precip;

        switch (planet->weather) {
            case arc_weather_rain:
                height = arc_clamp(arc_scale(9), 5, 14);
                break;
            case arc_weather_virga:
                height = arc_clamp(arc_scale(7), 4, 11);
                break;
            case arc_weather_dust:
                width = height = arc_clamp(arc_scale(1 + arc_rand(2)), 1, 3);
                break;
            case arc_weather_snow:
                width = height = arc_clamp(arc_scale(2 + arc_rand(2)), 2, 5);
                break;
            case arc_weather_diamond:
                width = height = arc_clamp(arc_scale(2), 2, 3);
                radius = 0;
                break;
            default:
                width = height = index < ARC_METEOR_DOTS ? arc_clamp(arc_scale(3) - index / 2, 1, 4) : thin;
                break;
        }

        drop->length = height;
        lv_obj_set_size(drop->obj, width, height);
        lv_obj_set_style_radius(drop->obj, radius, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_color(drop->obj, lv_color_hex(colour), MU_OBJ_MAIN_DEFAULT);
        arc_set_hidden(drop->obj, 1);
        if (index < level) arc_spawn_drop(drop, 1);
    }
}

static void arc_update_meteor(const uint32_t now, const float dt) {
    if (!arc_meteor_vx) {
        if ((int32_t) (now - arc_meteor_at) < 0) return;
        const float pace = (float) arc_width * (0.9f + arc_randf() * 0.6f);
        const float slope = 0.25f + arc_randf() * 0.35f;
        const int leftward = arc_rand(2);
        arc_meteor_x =
            leftward ? (float) arc_width * (0.55f + arc_randf() * 0.45f) : (float) arc_width * arc_randf() * 0.45f;
        arc_meteor_y = (float) (arc_hud_bottom + arc_rand(arc_height / 6 + 1));
        arc_meteor_vx = leftward ? -pace : pace;
        arc_meteor_vy = pace * slope;
    }

    arc_meteor_x += arc_meteor_vx * dt;
    arc_meteor_y += arc_meteor_vy * dt;
    const int gone = arc_meteor_x < -(float) arc_width * 0.1f || arc_meteor_x > (float) arc_width * 1.1f
                     || arc_meteor_y > (float) arc_ground_top;

    for (int index = 0; index < ARC_METEOR_DOTS; index++) {
        arc_drop *drop = &arc_drops[index];
        if (gone) {
            arc_set_hidden(drop->obj, 1);
            continue;
        }
        const float back = (float) index * 0.012f;
        const int size = lv_obj_get_style_width(drop->obj, LV_PART_MAIN);
        arc_place(
            drop->obj, (int) (arc_meteor_x - arc_meteor_vx * back) - size / 2,
            (int) (arc_meteor_y - arc_meteor_vy * back) - size / 2
        );
        arc_set_fill(drop->obj, 255 - index * 38);
        arc_set_hidden(drop->obj, 0);
    }

    if (gone) {
        arc_meteor_vx = 0.0f;
        arc_meteor_at = now + 2500 + (uint32_t) arc_rand(4500);
    }
}

static void arc_update_weather(const uint32_t now, const float dt) {
    const int weather = arc_planet_current()->weather;
    if (weather == arc_weather_meteor) {
        arc_update_meteor(now, dt);
        return;
    }

    const float drift = (float) arc_wind * (float) arc_scale(14);
    const float fall = (float) arc_height;
    const float evaporate = (float) arc_ground_top - (float) (arc_ground_top - arc_hud_bottom) * 0.3f;

    for (int index = 0; index < arc_drop_level; index++) {
        arc_drop *drop = &arc_drops[index];
        int opa = 160;

        switch (weather) {
            case arc_weather_rain:
                drop->y += fall * 1.3f * drop->speed * dt;
                drop->x += drift * 1.5f * dt;
                break;
            case arc_weather_virga:
                drop->y += fall * 0.45f * drop->speed * dt;
                drop->x += drift * dt;
                opa = (int) (150.0f * arc_clampf((evaporate - drop->y) / (float) arc_scale(60), 0.0f, 1.0f));
                if (opa <= 0) arc_spawn_drop(drop, 0);
                break;
            case arc_weather_dust:
                drop->x += ((arc_wind < 0 ? -1.0f : 1.0f) * (float) arc_scale(40) + drift * 3.0f) * drop->speed * dt;
                drop->y += sinf((float) now * 0.003f + drop->phase) * (float) arc_scale(12) * dt;
                opa = 110 + (int) (60.0f * sinf((float) now * 0.002f + drop->phase));
                if (drop->x < -(float) arc_scale(24) || drop->x > (float) (arc_width + arc_scale(24)))
                    arc_spawn_drop(drop, 0);
                break;
            case arc_weather_snow:
                drop->y += fall * 0.1f * drop->speed * dt;
                drop->x += (drift + sinf((float) now * 0.0018f + drop->phase) * (float) arc_scale(16)) * dt;
                opa = 200;
                break;
            case arc_weather_diamond:
                drop->y += fall * 0.32f * drop->speed * dt;
                drop->x += drift * 0.5f * dt;
                opa = 90 + (int) (165.0f * fabsf(sinf((float) now * 0.006f + drop->phase * 3.0f)));
                break;
            default:
                break;
        }

        if (drop->x < -(float) arc_scale(30)) drop->x += (float) (arc_width + arc_scale(30));
        if (drop->x > (float) (arc_width + arc_scale(30))) drop->x -= (float) (arc_width + arc_scale(30));

        const int column = arc_clamp((int) drop->x, 0, arc_width - 1);
        if (weather != arc_weather_dust && drop->y + (float) drop->length >= (float) arc_ground[column])
            arc_spawn_drop(drop, 0);

        arc_place(drop->obj, (int) drop->x, (int) drop->y);
        arc_set_fill(drop->obj, opa);
        arc_set_hidden(drop->obj, drop->y + (float) drop->length < (float) arc_hud_bottom - (float) arc_scale(10));
    }
}

static void arc_add_crater(const int centre, const int radius) {
    const float depth = (float) radius * 0.5f;
    const float rim = (float) radius * 0.2f;
    for (int x = centre - radius * 2; x <= centre + radius * 2; x++) {
        if (x < 0 || x >= arc_width) continue;
        const float distance = (float) abs(x - centre) / (float) radius;
        float change = -rim * expf(-((distance - 1.0f) * (distance - 1.0f)) / 0.09f);
        if (distance < 1.0f) change += depth * (1.0f - distance * distance);
        arc_ground[x] = (int16_t) lroundf((float) arc_ground[x] + change);
    }
}

static void arc_add_volcano(const int centre, const int radius, const float height) {
    for (int x = centre - radius * 3; x <= centre + radius * 3; x++) {
        if (x < 0 || x >= arc_width) continue;
        const float distance = (float) abs(x - centre) / (float) radius;
        float change = -height * expf(-distance * distance * 1.5f);
        if (distance < 0.2f) change += height * 0.3f * (1.0f - distance * distance / 0.04f);
        arc_ground[x] = (int16_t) lroundf((float) arc_ground[x] + change);
    }
}

static int arc_feature_spot(void) {
    return arc_width * (28 + arc_rand(45)) / 100;
}

static void arc_shape_surface(const int surface, const int top, const int bottom) {
    const int range = bottom - top;

    switch (surface) {
        case arc_surface_craters:
            for (int count = 3 + arc_rand(2); count > 0; count--)
                arc_add_crater(arc_feature_spot(), arc_width * (3 + arc_rand(5)) / 100);
            break;
        case arc_surface_volcanic:
            for (int count = 1 + arc_rand(2); count > 0; count--)
                arc_add_volcano(
                    arc_feature_spot(), arc_width * (6 + arc_rand(4)) / 100,
                    (float) range * (0.2f + (float) arc_rand(12) / 100.0f)
                );
            break;
        case arc_surface_mesas: {
            const float step = (float) range / 5.0f;
            for (int x = 0; x < arc_width; x++) {
                const float level = (float) (arc_ground[x] - top) / step;
                const float whole = floorf(level);
                const float shaped = whole + arc_smooth(0.3f, 0.7f, level - whole);
                arc_ground[x] = (int16_t) lroundf((float) top + shaped * step);
            }
            if (arc_rand(2)) arc_add_crater(arc_feature_spot(), arc_width * (4 + arc_rand(3)) / 100);
            break;
        }
        default:
            break;
    }

    for (int x = 0; x < arc_width; x++)
        arc_ground[x] = (int16_t) arc_clamp(arc_ground[x], top, bottom);
}

static void arc_generate_ground(void) {
    const int top = arc_ground_top;
    const int bottom = arc_height - arc_scale(34);
    const int range = bottom - top;
    const float phase = (float) arc_rand(628) / 100.0f;
    const float cycles = 1.6f + (float) arc_rand(140) / 100.0f;
    const float feature_x = (float) (arc_width / 2 + arc_rand(arc_width / 10 + 1) - arc_width / 20);
    const float feature_spread = (float) arc_width / 9.0f;
    const float feature_size = (float) range * (0.24f + (float) arc_rand(15) / 100.0f) * (arc_rand(4) ? -1.0f : 1.0f);
    const int surface = arc_planet_current()->surface;
    const float roll = surface == arc_surface_bands ? 0.55f : 1.0f;
    const float rough = surface == arc_surface_ice ? 1.6f : surface == arc_surface_bands ? 0.3f : 1.0f;
    int drift = 0;

    for (int x = 0; x < arc_width; x++) {
        drift = arc_clamp(drift + arc_rand(3) - 1, -range / 12, range / 12);
        const float progress = (float) x / (float) arc_width;
        const float primary = sinf(progress * cycles * 2.0f * (float) M_PI + phase) * (float) range * 0.27f * roll;
        float detail =
            sinf(progress * cycles * 2.3f * 2.0f * (float) M_PI + phase * 0.63f) * (float) range * 0.08f * rough;
        if (surface == arc_surface_ice) {
            const float saw = progress * cycles * 3.0f + phase;
            const float peak = 1.0f - fabsf(2.0f * (saw - floorf(saw)) - 1.0f);
            detail -= peak * peak * (float) range * 0.2f;
        }
        const float offset = (float) x - feature_x;
        const float feature = feature_size * expf(-(offset * offset) / (2.0f * feature_spread * feature_spread));
        const float half = (float) range * 0.5f;
        const float shape = tanhf((primary + detail + feature + (float) drift) / half * 0.9f) / tanhf(0.9f);
        const int height = top + range / 2 + (int) lroundf(shape * half);
        arc_ground[x] = (int16_t) arc_clamp(height, top, bottom);
    }

    int16_t *smooth = malloc((size_t) arc_width * sizeof(*smooth));
    if (smooth) {
        const int passes = surface == arc_surface_ice ? 1 : surface == arc_surface_bands ? 4 : 3;
        const int reach = arc_clamp(arc_width / (surface == arc_surface_ice ? 120 : 48), 2, 20);
        for (int pass = 0; pass < passes; pass++) {
            int total = 0;
            for (int sample = 0; sample <= reach && sample < arc_width; sample++)
                total += arc_ground[sample];
            for (int x = 0; x < arc_width; x++) {
                const int start = x - reach;
                const int end = x + reach;
                const int count = arc_clamp(end, 0, arc_width - 1) - arc_clamp(start, 0, arc_width - 1) + 1;
                smooth[x] = (int16_t) (total / count);
                if (end + 1 < arc_width) total += arc_ground[end + 1];
                if (start >= 0) total -= arc_ground[start];
            }
            memcpy(arc_ground, smooth, (size_t) arc_width * sizeof(*smooth));
        }
        free(smooth);
    }

    arc_shape_surface(surface, top, bottom);

    const int left = arc_width * 14 / 100;
    const int right = arc_width * 86 / 100;
    arc_flatten_pad(left);
    arc_flatten_pad(right);

    memset(arc_scorch, 0, (size_t) arc_width);
    memset(arc_stain, 0, (size_t) arc_width);

    arc_actor_x[0] = left - arc_actor_w * 0.5f;
    arc_actor_x[1] = right - arc_actor_w * 0.5f;
    arc_position_actor(0);
    arc_position_actor(1);
    arc_apply_planet();
    arc_roll_weather();
}

static void arc_apply_variant(const int player) {
    const arc_variant *variant = &arc_variants[arc_variant_selected[player]];
    lv_obj_set_style_bg_color(arc_actor[player], lv_color_hex(variant->colour), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(arc_actor_cap[player], lv_color_hex(variant->cap), MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(arc_actor_glyph[player], variant->glyph);
    lv_obj_set_style_text_color(arc_actor_glyph[player], lv_color_hex(variant->ink), MU_OBJ_MAIN_DEFAULT);
    lv_bar_set_range(arc_health_bar[player], 0, variant->health);
    lv_bar_set_range(arc_health_lag_bar[player], 0, variant->health);
    lv_obj_set_style_bg_color(
        arc_health_bar[player], lv_color_hex(variant->colour), LV_PART_INDICATOR | MU_OBJ_MAIN_DEFAULT
    );
    arc_actor_flash[player] = -1;
}

static void arc_clear_shot(void) {
    arc_shot_active = 0;
    for (int index = 0; index < ARC_SHELL_COUNT; index++) {
        arc_shells[index].active = 0;
        arc_set_hidden(arc_shell_obj[index], 1);
    }
    for (int index = 0; index < ARC_TRAIL_COUNT; index++) {
        arc_trail_points[index].visible = 0;
        arc_set_hidden(arc_trail[index], 1);
    }
}

static void arc_show_toast(const char *text) {
    lv_label_set_text(arc_toast_label, TRS((char *) text));
    lv_obj_align(arc_toast, LV_ALIGN_TOP_MID, 0, arc_height * 30 / 100);
    arc_set_opa(arc_toast, 0);
    arc_set_hidden(arc_toast, 0);
    arc_toast_started = lv_tick_get();
    if (!arc_toast_started) arc_toast_started = 1;
}

static void arc_say(const int player, const char *text) {
    const char *line = TRS((char *) text);
    const int pad = arc_clamp(arc_scale(8), 6, 12);
    const int widest = arc_width * 42 / 100;
    const int width = arc_clamp(arc_text_width(arc_bubble_label, line) + 2, 24, widest);

    lv_point_t size;
    lv_txt_get_size(
        &size, line, lv_obj_get_style_text_font(arc_bubble_label, LV_PART_MAIN),
        lv_obj_get_style_text_letter_space(arc_bubble_label, LV_PART_MAIN),
        lv_obj_get_style_text_line_space(arc_bubble_label, LV_PART_MAIN), width, LV_TEXT_FLAG_NONE
    );

    lv_label_set_text(arc_bubble_label, line);
    lv_obj_set_width(arc_bubble_label, width);
    const int bubble_w = width + pad * 2;
    const int bubble_h = size.y + pad * 2;
    lv_obj_set_size(arc_bubble, bubble_w, bubble_h);
    lv_obj_set_style_border_color(
        arc_bubble, lv_color_hex(arc_variants[arc_variant_selected[player]].colour), MU_OBJ_MAIN_DEFAULT
    );

    const int centre = (int) lroundf(arc_actor_x[player] + arc_actor_w * 0.5f);
    const int x = arc_clamp(centre - bubble_w / 2, 8, arc_width - bubble_w - 8);
    const int top = lv_obj_has_flag(arc_hud, LV_OBJ_FLAG_HIDDEN) ? 8 : arc_hud_bottom + 6;
    const int y = arc_clamp((int) lroundf(arc_actor_y[player]) - bubble_h - arc_scale(16), top, arc_height - bubble_h);
    lv_obj_set_pos(arc_bubble, x, y);
    arc_set_opa(arc_bubble, 0);
    arc_set_hidden(arc_bubble, 0);
    arc_bubble_started = lv_tick_get();
    if (!arc_bubble_started) arc_bubble_started = 1;
}

static void arc_cpu_say(const char *text, const int chance) {
    if (arc_players != 1 || arc_rand(100) >= chance) return;
    arc_say(1, text);
}

static void arc_show_number(const int player, const int amount, const int heal) {
    if (amount <= 0) return;
    char text[32];
    snprintf(text, sizeof(text), heal ? "+%d" : "-%d", amount);
    lv_obj_set_style_text_color(
        arc_damage_label[player], lv_color_hex(heal ? 0x7CE07C : arc_focus_colour()), MU_OBJ_MAIN_DEFAULT
    );
    lv_label_set_text(arc_damage_label[player], text);
    arc_damage_x[player] = (int) lroundf(arc_actor_x[player] + arc_actor_w * 0.5f);
    arc_damage_y[player] = (int) lroundf(arc_actor_y[player]) - arc_scale(12);
    arc_damage_started[player] = lv_tick_get();
    if (!arc_damage_started[player]) arc_damage_started[player] = 1;
    arc_set_opa(arc_damage_label[player], 255);
    arc_set_hidden(arc_damage_label[player], 0);
}

static arc_particle *arc_next_particle(void) {
    arc_particle *oldest = &arc_particles[0];
    for (int index = 0; index < ARC_PARTICLE_COUNT; index++) {
        if (!arc_particles[index].active) return &arc_particles[index];
        if (arc_particles[index].born < oldest->born) oldest = &arc_particles[index];
    }
    return oldest;
}

static void arc_spawn_particle(
    const int kind, const float x, const float y, const float vx, const float vy, const uint32_t colour, const int life
) {
    arc_particle *particle = arc_next_particle();
    const int size =
        kind == arc_particle_confetti ? arc_clamp(arc_scale(5), 3, 8) : arc_clamp(arc_scale(3 + arc_rand(4)), 2, 9);
    particle->x = x;
    particle->y = y;
    particle->vx = vx;
    particle->vy = vy;
    particle->sway = arc_randf() * 6.28f;
    particle->born = lv_tick_get();
    particle->life = life;
    particle->kind = kind;
    particle->active = 1;
    lv_obj_set_size(particle->obj, size, kind == arc_particle_confetti ? size / 2 + 1 : size);
    lv_obj_set_style_radius(particle->obj, kind == arc_particle_confetti ? 1 : LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(particle->obj, lv_color_hex(colour), MU_OBJ_MAIN_DEFAULT);
    arc_set_fill(particle->obj, 255);
    arc_place(particle->obj, (int) x, (int) y);
    arc_set_hidden(particle->obj, 0);
}

static void arc_spawn_confetti(const int winner) {
    for (int index = 0; index < 26; index++) {
        const int side = winner >= 0 ? winner : index & 1;
        const uint32_t colour = index % 3 == 2 ? arc_focus_colour() : arc_variants[arc_variant_selected[side]].colour;
        arc_spawn_particle(
            arc_particle_confetti, (float) arc_rand(arc_width), (float) -arc_rand(arc_height / 3),
            (arc_randf() - 0.5f) * (float) arc_scale(40), (float) arc_scale(30 + arc_rand(50)), colour,
            2600 + arc_rand(900)
        );
    }
}

static void arc_shake_for(const int strength, const uint32_t duration) {
    arc_shake_strength = arc_clamp(strength, 0, arc_scale(9));
    arc_shake_ms = duration;
    arc_shake_started = lv_tick_get();
    if (!arc_shake_started) arc_shake_started = 1;
}

static void arc_shake(const int strength) {
    arc_shake_for(strength, ARC_SHAKE_MS);
}

static int arc_plan_wall(int *angle_out, int *power_out) {
    const float own = arc_actor_x[1] + arc_actor_w * 0.5f;
    const float aim_x = arc_actor_x[0] + arc_actor_w * 0.5f;
    const float goal = own + (aim_x - own) * 0.3f;
    float best = FLT_MAX;

    for (int angle = 45; angle <= 80; angle += 5) {
        for (int power = 20; power <= 100; power += 2) {
            const float miss = arc_simulate(1, angle, power);
            if (fabsf(miss) >= (float) arc_width * 2.0f) continue;
            const float gap = fabsf(miss + aim_x - goal);
            if (gap < best) {
                best = gap;
                *angle_out = angle;
                *power_out = power;
            }
        }
    }

    return best < (float) arc_width * 0.08f;
}

static int arc_cpu_guard(void) {
    static const int guard_chance[arc_difficulty_count] = {10, 20, 30, 40, 50};
    if (arc_extras_locked(1) || arc_guard_ammo[1] <= 0 || arc_guard_active(1)
        || arc_rand(100) >= guard_chance[arc_difficulty])
        return 0;

    const arc_weapon *guard = &arc_guards[arc_variant_selected[1]];
    const int full = arc_variants[arc_variant_selected[1]].health;
    int wanted = 0;
    switch (guard->kind) {
        case arc_kind_heal:
            wanted = arc_health[1] * 100 < full * 60;
            break;
        case arc_kind_shield:
            wanted = arc_health[1] * 100 < full * 75;
            break;
        case arc_kind_trench:
            wanted = arc_health[1] * 100 < full * 85;
            break;
        case arc_kind_hop:
        case arc_kind_wall:
            wanted = arc_rand(2);
            break;
        default:
            break;
    }
    if (!wanted) return 0;

    int angle = arc_angle[1];
    int power = arc_power[1];
    arc_armed[1] = 2;
    if (guard->kind == arc_kind_wall && !arc_plan_wall(&angle, &power)) {
        arc_armed[1] = 0;
        return 0;
    }

    arc_cpu_angle = angle;
    arc_cpu_power = power;
    arc_cpu_from_angle = arc_angle[1];
    arc_cpu_from_power = arc_power[1];
    arc_cpu_started = lv_tick_get();
    arc_cpu_deadline = arc_cpu_started + 900;
    if (!arc_cpu_deadline) arc_cpu_deadline = 1;
    arc_cpu_say(ARC_PICK(arc_say_special), 50);
    arc_update_hud();
    return 1;
}

static void arc_prepare_cpu(void) {
    static const int angle_error[arc_difficulty_count] = {14, 8, 4, 2, 0};
    static const int power_error[arc_difficulty_count] = {24, 14, 8, 4, 1};
    static const int delay[arc_difficulty_count] = {1300, 1150, 1000, 850, 700};
    arc_cpu_deadline = 0;
    if (arc_players != 1 || arc_turn != 1 || arc_screen != arc_screen_play) return;
    if (arc_shot_active || arc_explosion_active || arc_hazard_kind || arc_round_winner != -2) return;

    static const int special_chance[arc_difficulty_count] = {10, 20, 35, 45, 55};
    int angle;
    int power;
    if (arc_cpu_guard()) return;

    arc_armed[1] = !arc_extras_locked(1) && arc_ammo[1] > 0 && arc_rand(100) < special_chance[arc_difficulty];
    arc_plan_cpu(&angle, &power);
    if (arc_armed[1] && fabsf(arc_simulate(1, angle, power)) > arc_actor_w * 0.6f) {
        arc_armed[1] = 0;
        arc_plan_cpu(&angle, &power);
    }
    if (arc_armed[1]) arc_cpu_say(ARC_PICK(arc_say_special), 70);

    const float learning =
        arc_difficulty == arc_difficulty_pushover ? 1.0f : 1.0f / (1.0f + 0.6f * (float) arc_cpu_misses);
    const int blind = arc_condition == arc_hazard_flare ? 2 : arc_condition == arc_hazard_haze ? 1 : 0;
    const int angle_spread = (int) lroundf((float) angle_error[arc_difficulty] * learning) + blind * 2;
    const int power_spread = (int) lroundf((float) power_error[arc_difficulty] * learning) + blind * 4;
    arc_cpu_angle = arc_clamp(angle + arc_rand(angle_spread * 2 + 1) - angle_spread, 15, 80);
    arc_cpu_power = arc_clamp(power + arc_rand(power_spread * 2 + 1) - power_spread, 20, 100);
    arc_cpu_from_angle = arc_angle[1];
    arc_cpu_from_power = arc_power[1];
    arc_cpu_started = lv_tick_get();
    arc_cpu_deadline = arc_cpu_started + (uint32_t) delay[arc_difficulty];
    if (!arc_cpu_deadline) arc_cpu_deadline = 1;
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
    arc_show_path();
    arc_prepare_cpu();
}

static void arc_round_toast(void) {
    const int left_point = arc_score[0] == arc_wins_needed - 1;
    const int right_point = arc_score[1] == arc_wins_needed - 1;
    char text[96];

    if (arc_best_of > 1 && left_point && right_point)
        snprintf(text, sizeof(text), "%s", TRS("Final Round"));
    else if (arc_best_of > 1 && (left_point || right_point))
        snprintf(text, sizeof(text), "%s %d, %s", TRS("Round"), arc_round_number, TRS("Match Point"));
    else
        snprintf(text, sizeof(text), "%s %d", TRS("Round"), arc_round_number);

    if (arc_planet_current()->walls == arc_walls_random) {
        const size_t used = strlen(text);
        snprintf(text + used, sizeof(text) - used, ", %s", TRS((char *) arc_wall_names[arc_walls]));
    }

    lv_label_set_text(arc_toast_label, text);
    lv_obj_align(arc_toast, LV_ALIGN_TOP_MID, 0, arc_height * 30 / 100);
    arc_set_opa(arc_toast, 0);
    arc_set_hidden(arc_toast, 0);
    arc_toast_started = lv_tick_get();
    if (!arc_toast_started) arc_toast_started = 1;
}

static uint32_t arc_wall_pixel(const int side, const int x, const int y) {
    const int inner = side ? x : arc_wall_w - 1 - x;
    const float depth = (float) inner / (float) (arc_wall_w > 1 ? arc_wall_w - 1 : 1);
    const float across = ((float) x + 0.5f) / (float) arc_wall_w;

    switch (arc_walls) {
        case arc_walls_concrete: {
            const int block = arc_clamp(arc_scale(18), 10, 30);
            const int row = y / block;
            const int along = y % block;
            const int joint = (row & 1) && abs(x - arc_wall_w / 2) < 1;
            if (along == 0 || joint) return 0xFF000000u | 0x4E4C48;
            const int grain = (int) (arc_hash_noise(x, y, 0xC0DEu) * 10.0f) - 5;
            const int tone = (int) (arc_hash_noise(row, side, 0xB10Cu) * 26.0f) - 13;
            uint32_t colour = arc_shade(0x8E8C86, tone + grain);
            if (along == 1) colour = arc_shade(colour, 18);
            if (inner == 0) colour = arc_shade(colour, 34);
            return 0xFF000000u | arc_mix(colour, 0x000000, 255 - (int) (depth * 90.0f));
        }
        case arc_walls_rubber: {
            const int band = arc_clamp(arc_scale(26), 14, 40);
            const float round = 0.45f + 0.55f * sinf((float) M_PI * across);
            const float spec = expf(-((across - 0.35f) * (across - 0.35f)) / 0.012f) * 0.55f;
            uint32_t colour = arc_mix(0x3FAE5C, 0x000000, (int) (255.0f * round));
            if (y % band < 2) colour = arc_shade(colour, -40);
            colour = arc_mix(0xFFFFFF, colour, (int) (255.0f * spec));
            return 0xFF000000u | colour;
        }
        case arc_walls_wrap: {
            const float shimmer = 0.75f + 0.25f * sinf((float) y * 0.09f + (float) side * 3.1f);
            const float glow = depth * depth * 0.85f * shimmer + (inner >= arc_wall_w - 2 ? 0.35f : 0.0f);
            const uint32_t colour = arc_mix(0xD8F0FF, 0x4FA8E8, (int) (255.0f * depth * depth));
            return arc_pixel(colour, arc_clampf(glow, 0.0f, 0.95f));
        }
        default:
            return 0;
    }
}

static void arc_paint_wall(const int side) {
    lv_color32_t *buf = arc_wall_buf[side];
    if (!buf) return;
    for (int y = 0; y < arc_wall_h; y++) {
        lv_color32_t *row = buf + (size_t) y * (size_t) arc_wall_w;
        for (int x = 0; x < arc_wall_w; x++)
            row[x].full = arc_wall_pixel(side, x, y);
    }
    canvas_invalidate_gpu_texture(arc_wall_obj[side]);
    lv_obj_invalidate(arc_wall_obj[side]);
}

static void arc_show_walls(void) {
    for (int side = 0; side < 2; side++) {
        lv_obj_t *wall = arc_wall_obj[side];
        if (!wall) continue;
        arc_wall_pending[side] = 0;
        arc_wall_hit_at[side] = 0;
        for (int index = 0; index < ARC_WALL_SPARKS; index++) {
            arc_wall_spark_y[side][index] = (float) (arc_rand(arc_wall_h) - arc_pad_top);
            arc_set_hidden(arc_wall_spark[side][index], arc_walls != arc_walls_wrap);
        }
        if (arc_walls == arc_walls_open) {
            arc_set_hidden(wall, 1);
            continue;
        }
        arc_paint_wall(side);
        arc_place(wall, side ? arc_width - arc_wall_w : 0, -arc_pad_top);
        arc_set_hidden(wall, 0);
    }
}

static void arc_wall_feedback(void) {
    static const uint32_t colours[] = {0x000000, 0x6FD884, 0xBFE6FF, 0xA8A6A0};
    const uint32_t now = lv_tick_get();
    for (int side = 0; side < 2; side++) {
        if (!arc_wall_pending[side]) continue;
        arc_wall_pending[side] = 0;
        arc_wall_hit_at[side] = now ? now : 1;

        const int sides = arc_walls == arc_walls_wrap ? 2 : 1;
        for (int pass = 0; pass < sides; pass++) {
            const int at = pass ? side ^ 1 : side;
            const float x = at ? (float) (arc_width - arc_wall_w) : (float) arc_wall_w;
            const float away = at ? -1.0f : 1.0f;
            if (pass) arc_wall_hit_at[at] = arc_wall_hit_at[side];
            for (int index = 0; index < 7; index++) {
                const float speed = (float) arc_scale(110) * (0.4f + arc_randf());
                const float angle = (arc_randf() - 0.5f) * 1.6f;
                arc_spawn_particle(
                    arc_walls == arc_walls_concrete ? arc_particle_debris : arc_particle_splat, x,
                    (float) arc_wall_pending_y[side], away * cosf(angle) * speed, sinf(angle) * speed - speed * 0.3f,
                    colours[arc_walls], 450 + arc_rand(300)
                );
            }
        }
        if (arc_walls == arc_walls_rubber) play_sound(snd_navigate);
    }
}

static void arc_update_walls(const uint32_t now, const float dt) {
    if (arc_walls == arc_walls_open) return;

    for (int side = 0; side < 2; side++) {
        if (!arc_wall_obj[side]) continue;
        const uint32_t age = arc_wall_hit_at[side] ? now - arc_wall_hit_at[side] : 100000;

        int shift = 0;
        if (arc_walls == arc_walls_rubber && age < 420) {
            const float left = 1.0f - (float) age / 420.0f;
            shift = (int) lroundf((side ? 1.0f : -1.0f) * sinf((float) age * 0.045f) * (float) arc_scale(5) * left);
        }
        if (shift != arc_wall_shift[side]) {
            lv_obj_set_style_translate_x(arc_wall_obj[side], shift, MU_OBJ_MAIN_DEFAULT);
            arc_wall_shift[side] = shift;
        }

        if (arc_walls != arc_walls_wrap) continue;
        const float rush = age < 500 ? 4.0f : 1.0f;
        const float travel = (side ? 1.0f : -1.0f) * (float) arc_scale(70) * rush * dt;
        const int size = arc_clamp(arc_scale(3), 2, 5);
        const int x = side ? arc_width - size - 1 : 1;
        for (int index = 0; index < ARC_WALL_SPARKS; index++) {
            float *y = &arc_wall_spark_y[side][index];
            *y += travel;
            if (*y < -(float) arc_pad_top) *y += (float) arc_wall_h;
            if (*y >= (float) (arc_wall_h - arc_pad_top)) *y -= (float) arc_wall_h;
            arc_place(arc_wall_spark[side][index], x, (int) lroundf(*y));
            const float flicker = 0.6f + 0.4f * sinf((float) now * 0.01f + (float) index * 1.7f);
            arc_set_fill(arc_wall_spark[side][index], (int) ((age < 500 ? 255.0f : 200.0f) * flicker));
        }
    }
}

static void arc_preview_world(void) {
    const int walls = arc_planet_current()->walls;
    arc_walls = walls == arc_walls_random ? arc_walls_open : walls;
    arc_roll_wind(1);
    arc_generate_ground();
    arc_show_walls();
}

static void arc_begin_round(void) {
    arc_round_number++;
    for (int player = 0; player < 2; player++) {
        arc_health[player] = arc_variants[arc_variant_selected[player]].health;
        arc_health_lag[player] = (float) arc_health[player];
        arc_angle[player] = 45;
        arc_power[player] = 60;
        arc_ammo[player] = arc_specials[arc_variant_selected[player]].ammo;
        arc_guard_ammo[player] = arc_guards[arc_variant_selected[player]].ammo;
        arc_armed[player] = 0;
        arc_shield[player] = 0;
        arc_shield_fade[player] = 0;
        arc_regen[player] = 0;
        arc_path_count[player] = 0;
        arc_actor_ko_at[player] = 0;
        arc_actor_hit_at[player] = 0;
        arc_actor_recoil_at[player] = 0;
        arc_damage_started[player] = 0;
        arc_set_hidden(arc_damage_label[player], 1);
        lv_obj_set_size(arc_actor[player], arc_actor_w, arc_actor_h);
        arc_actor_size_w[player] = arc_actor_w;
        arc_actor_fall_at[player] = 0;
    }
    arc_turn = (arc_round_number - 1) & 1;
    arc_turns_taken[0] = 0;
    arc_turns_taken[1] = 0;
    arc_turns_taken[arc_turn] = 1;
    arc_walls =
        arc_planet_current()->walls == arc_walls_random ? arc_rand(arc_walls_random) : arc_planet_current()->walls;
    arc_show_walls();
    arc_hazard_kind = arc_hazard_none;
    arc_hazard_rolled = 0;
    arc_condition = arc_hazard_none;
    arc_condition_next = arc_hazard_none;
    for (int index = 0; index < ARC_BOLT_DOTS; index++)
        arc_set_hidden(arc_bolt[index], 1);
    arc_round_winner = -2;
    arc_explosion_active = 0;
    arc_cpu_misses = 0;
    arc_set_hidden(arc_explosion, 1);
    arc_set_hidden(arc_blast_ring, 1);
    arc_set_hidden(arc_bubble, 1);
    arc_clear_shot();
    arc_roll_wind(1);
    arc_apply_variant(0);
    arc_apply_variant(1);
    arc_generate_ground();
    arc_show_play();
    arc_round_toast();
}

static void arc_begin_match(void) {
    arc_score[0] = 0;
    arc_score[1] = 0;
    arc_shots[0] = 0;
    arc_shots[1] = 0;
    arc_hits[0] = 0;
    arc_hits[1] = 0;
    arc_match_best[0] = 0;
    arc_match_best[1] = 0;
    arc_match_long[0] = 0;
    arc_match_long[1] = 0;
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

static void arc_record_match(const int winner, char *note, const size_t size) {
    note[0] = '\0';
    int new_long = 0;
    int new_damage = 0;

    for (int player = 0; player < 2; player++) {
        if (arc_players == 1 && player == 1) continue;
        arc_record *record = &arc_records[arc_variant_selected[player]];
        if (player == winner)
            record->won++;
        else
            record->lost++;
        record->rounds += arc_score[player];
        record->shots += arc_shots[player];
        record->hits += arc_hits[player];
        if (arc_match_long[player] > record->longest) {
            record->longest = arc_match_long[player];
            new_long = arc_match_long[player];
        }
        if (arc_match_best[player] > record->best_damage) {
            record->best_damage = arc_match_best[player];
            new_damage = arc_match_best[player];
        }
    }

    int new_streak = 0;
    if (arc_players == 1) {
        arc_streak = winner == 0 ? arc_streak + 1 : 0;
        if (arc_streak > arc_best_streak) {
            arc_best_streak = arc_streak;
            new_streak = arc_streak > 1;
        }
    }

    if (new_streak)
        snprintf(note, size, "   %s %d", TRS("New best streak:"), arc_best_streak);
    else if (new_long)
        snprintf(note, size, "   %s %d m", TRS("New longest hit:"), new_long);
    else if (new_damage)
        snprintf(note, size, "   %s %d", TRS("New biggest hit:"), new_damage);
    else if (arc_players == 1 && arc_streak > 1)
        snprintf(note, size, "   %s %d", TRS("Winning streak"), arc_streak);

    arc_save();
}

static void arc_finish_round(void) {
    static const char *next_options[] = {"Next Round", "Setup"};
    static const char *match_options[] = {"Rematch", "Setup", "Exit"};
    static char subtitle[256];
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
    arc_set_hidden(arc_bubble, 1);

    if (arc_score[0] >= arc_wins_needed || arc_score[1] >= arc_wins_needed) {
        const int winner = arc_score[1] > arc_score[0] ? 1 : 0;
        snprintf(
            title, sizeof(title), "%s %s", TRS((char *) arc_variants[arc_variant_selected[winner]].name), TRS("wins!")
        );
        char note[96];
        arc_record_match(winner, note, sizeof(note));
        snprintf(
            subtitle, sizeof(subtitle), "%s %d/%d   %s %d/%d%s",
            TRS((char *) arc_variants[arc_variant_selected[0]].name), arc_hits[0], arc_shots[0],
            TRS((char *) arc_variants[arc_variant_selected[1]].name), arc_hits[1], arc_shots[1], note
        );
        for (size_t index = 0; index < A_SIZE(match_options); index++)
            match_options[index] = TRS((char *) match_options[index]);
        arc_show_menu(arc_screen_match, title, subtitle, match_options, A_SIZE(match_options));
        arc_spawn_confetti(winner);
        arc_spawn_confetti(winner);
    } else {
        for (size_t index = 0; index < A_SIZE(next_options); index++)
            next_options[index] = TRS((char *) next_options[index]);
        arc_show_menu(arc_screen_round, title, NULL, next_options, A_SIZE(next_options));
        arc_spawn_confetti(arc_round_winner);
    }
}

static void arc_drop_actor(const int player, const float drop, const uint32_t now) {
    if (drop <= 0.5f) return;
    float left = 0.0f;
    if (arc_actor_fall_at[player]) {
        const float t = (float) (now - arc_actor_fall_at[player]) / 1000.0f;
        left = fmaxf(arc_actor_fall[player] - 0.5f * (float) arc_scale(1400) * t * t, 0.0f);
    }
    arc_actor_fall[player] = left + drop;
    arc_actor_fall_at[player] = now ? now : 1;
}

static void arc_damage_ground(const int centre, const int radius, const uint32_t colour, float drop[2]) {
    const float before[2] = {arc_actor_y[0], arc_actor_y[1]};
    const float crater = (float) arc_planet_current()->crater / 100.0f;
    const int reach = radius + radius / 3;
    const int start = arc_clamp(centre - reach, 0, arc_width - 1);
    const int end = arc_clamp(centre + reach, 0, arc_width - 1);
    for (int x = start; x <= end; x++) {
        const int dx = x - centre;
        if (abs(dx) <= radius) {
            const float depth = sqrtf((float) (radius * radius - dx * dx)) * 0.55f * crater;
            arc_ground[x] = (int16_t) arc_clamp(arc_ground[x] + (int) lroundf(depth), 0, arc_height - 1);
            const int scorch = arc_clamp(arc_scale(4) + (radius - abs(dx)) / 4, 0, 40);
            if (scorch > arc_scorch[x]) arc_scorch[x] = (uint8_t) scorch;
        }
        const int strength = arc_clamp(230 - abs(dx) * 230 / (reach + 1) + arc_rand(40) - 20, 0, 230);
        if (strength > arc_stain[x] / 2) {
            arc_stain_colour[x] = colour;
            arc_stain[x] = (uint8_t) strength;
        }
    }
    arc_position_actor(0);
    arc_position_actor(1);
    arc_paint_ground(start, end);

    const uint32_t now = lv_tick_get();
    for (int player = 0; player < 2; player++) {
        drop[player] = arc_actor_y[player] - before[player];
        arc_drop_actor(player, drop[player], now);
    }
}

static int arc_absorb(const int player, const int amount, const uint32_t now) {
    if (arc_shield[player] <= 0 || amount <= 0) return amount;
    const int blocked = amount * arc_shield[player] / 100;
    arc_shield[player] = arc_clamp(arc_shield[player] - (blocked + 1) / 2, 0, 100);
    arc_shield_hit_at[player] = now ? now : 1;
    arc_volley_absorbed[player] += blocked;
    return amount - blocked;
}

static void arc_fall_damage(const float drop[2], int damage[2]) {
    if (!arc_planet_current()->falls) return;
    const int free_fall = arc_scale(10);
    for (int player = 0; player < 2; player++) {
        if (drop[player] <= (float) free_fall || arc_health[player] <= 0) continue;
        const int amount = arc_absorb(
            player, arc_clamp((int) lroundf((drop[player] - (float) free_fall) * 25.0f / (float) arc_scale(40)), 0, 35),
            lv_tick_get()
        );
        const int before = arc_health[player];
        arc_health[player] = arc_clamp(before - amount, 0, arc_variants[arc_variant_selected[player]].health);
        damage[player] += before - arc_health[player];
    }
}

static void arc_cap_damage(int damage[2], const int already[2], const int percent) {
    for (int player = 0; player < 2; player++) {
        const int full = arc_variants[arc_variant_selected[player]].health;
        const int cap = arc_clamp(full * percent / 100 - already[player], 0, full);
        if (damage[player] <= cap) continue;
        arc_health[player] = arc_clamp(arc_health[player] + damage[player] - cap, 0, full);
        damage[player] = cap;
    }
}

static void arc_resolve_messages(
    const int shooter, const int direct_player, const int damage[2], const float x, const int radius, const int out
) {
    const int target = shooter ^ 1;
    const int cpu_shot = arc_players == 1 && shooter == 1;

    if (!out && damage[target] > 0) arc_hits[shooter]++;

    if (arc_health[0] <= 0 && arc_health[1] <= 0) {
        arc_show_toast("Double knockout!");
        return;
    }
    if (arc_health[target] <= 0) {
        arc_show_toast(arc_rand(2) ? "Knockout!" : "Splattered!");
        if (cpu_shot) arc_cpu_say(ARC_PICK(arc_say_gloat), 100);
        return;
    }
    if (arc_health[shooter] <= 0) {
        arc_show_toast("Self destruct!");
        return;
    }

    int landed = 0;
    if (out == 2) {
        arc_show_toast(ARC_PICK(arc_toast_orbit));
    } else if (out) {
        arc_show_toast(ARC_PICK(arc_toast_out));
    } else if (arc_volley_absorbed[target] > 0) {
        char text[96];
        snprintf(text, sizeof(text), "%s %d", TRS("Defence absorbed"), arc_volley_absorbed[target]);
        arc_show_toast(text);
        landed = damage[target] > 0;
    } else if (direct_player == target) {
        arc_show_toast(damage[target] >= 40 ? ARC_PICK(arc_toast_heavy) : ARC_PICK(arc_toast_direct));
        landed = 1;
    } else if (damage[target] > 0 && damage[shooter] > 0) {
        arc_show_toast(ARC_PICK(arc_toast_both));
        landed = 1;
    } else if (damage[target] > 0) {
        arc_show_toast(damage[target] >= 40 ? ARC_PICK(arc_toast_heavy) : ARC_PICK(arc_toast_glance));
        landed = 1;
    } else if (damage[shooter] > 0) {
        arc_show_toast(ARC_PICK(arc_toast_self));
    } else {
        const float gap = fabsf(x - (arc_actor_x[target] + arc_actor_w * 0.5f));
        arc_show_toast(gap < (float) radius * 2.2f ? ARC_PICK(arc_toast_close) : ARC_PICK(arc_toast_miss));
    }

    if (cpu_shot) {
        if (landed) {
            arc_cpu_misses = 0;
            arc_cpu_say(ARC_PICK(arc_say_gloat), 65);
        } else {
            arc_cpu_misses++;
            arc_cpu_say(ARC_PICK(arc_say_excuse), 50);
        }
    } else if (arc_players == 1) {
        if (landed)
            arc_cpu_say(ARC_PICK(arc_say_whine), 60);
        else
            arc_cpu_say(ARC_PICK(arc_say_mock), 40);
    }
}

static void arc_detonate(const float x, const float y, const int direct_player) {
    const int shooter = arc_turn;
    const arc_variant *variant = &arc_variants[arc_variant_selected[shooter]];
    const int base = arc_clamp(arc_width / 13, arc_scale(28), arc_scale(70));
    const int blast = variant->blast * arc_volley_weapon->blast / 100;
    const int radius = arc_clamp(base * blast / 100, arc_scale(12), arc_scale(150));
    const uint32_t now = lv_tick_get();
    int damage[2] = {0, 0};

    arc_explosion_active = 1;
    arc_explosion_started = now;
    arc_explosion_radius = radius;
    arc_explosion_x = (int) lroundf(x);
    arc_explosion_y = (int) lroundf(y);
    lv_obj_set_style_bg_color(arc_explosion, lv_color_hex(arc_mix(0xFFFFFF, variant->colour, 90)), MU_OBJ_MAIN_DEFAULT);
    arc_set_hidden(arc_explosion, 0);
    arc_set_hidden(arc_blast_ring, 0);

    for (int player = 0; player < 2; player++) {
        const float px = arc_actor_x[player] + arc_actor_w * 0.5f;
        const float py = arc_actor_y[player] + arc_actor_h * 0.5f;
        const float dx = px - x;
        const float dy = py - y;
        const float distance = sqrtf(dx * dx + dy * dy);
        const int scale =
            variant->damage * arc_variants[arc_variant_selected[player]].defence / 100 * arc_volley_weapon->damage;
        int amount = distance < radius ? (int) lroundf((1.0f - distance / radius) * 60.0f) : 0;
        if (player == direct_player && amount < 65) amount = 65;
        amount = arc_absorb(player, amount * scale / 10000, now);
        const int before = arc_health[player];
        arc_health[player] = arc_clamp(before - amount, 0, arc_variants[arc_variant_selected[player]].health);
        damage[player] = before - arc_health[player];
    }

    const int column = arc_clamp((int) lroundf(x), 0, arc_width - 1);
    if (y >= (float) (arc_ground[column] - radius - arc_actor_h)) {
        float drop[2];
        arc_damage_ground(column, radius, variant->colour, drop);
        arc_fall_damage(drop, damage);
    }
    arc_cap_damage(damage, arc_volley_damage, ARC_VOLLEY_CAP);

    for (int player = 0; player < 2; player++) {
        arc_volley_damage[player] += damage[player];
        if (damage[player] > 0) {
            arc_health_hit_at[player] = now;
            arc_actor_hit_at[player] = now;
            arc_show_number(player, damage[player], 0);
            if (arc_health[player] <= 0) arc_actor_ko_at[player] = now;
        }
    }

    for (int index = 0; index < 14; index++) {
        const float angle = (float) M_PI * (1.05f + arc_randf() * 0.9f);
        const float speed = (float) arc_scale(120) * (0.5f + arc_randf());
        arc_spawn_particle(
            arc_particle_splat, x, y, cosf(angle) * speed, sinf(angle) * speed, variant->colour, 700 + arc_rand(500)
        );
    }
    for (int index = 0; index < 8; index++) {
        const float angle = (float) M_PI * (1.1f + arc_randf() * 0.8f);
        const float speed = (float) arc_scale(150) * (0.4f + arc_randf());
        arc_spawn_particle(
            arc_particle_debris, x, y, cosf(angle) * speed, sinf(angle) * speed, arc_lip_colour, 800 + arc_rand(400)
        );
    }

    arc_shake(arc_scale(2) + (damage[0] + damage[1]) / 8);
    arc_update_hud();

    arc_volley_bursts++;
    arc_volley_x = x;
    arc_volley_radius = radius;
    if (direct_player >= 0 && (arc_volley_direct < 0 || direct_player == (shooter ^ 1)))
        arc_volley_direct = direct_player;
    play_sound(damage[0] + damage[1] > 0 ? snd_error : snd_navigate);
}

static void arc_push_trail(void) {
    const float step = (float) arc_clamp(arc_scale(5), 3, 9);
    const float dx = arc_shot_x - arc_trail_last_x;
    const float dy = arc_shot_y - arc_trail_last_y;
    if (dx * dx + dy * dy < step * step) return;
    arc_trail_last_x = arc_shot_x;
    arc_trail_last_y = arc_shot_y;

    arc_trail_head = (arc_trail_head + 1) % ARC_TRAIL_COUNT;
    arc_trail_points[arc_trail_head].x = arc_shot_x;
    arc_trail_points[arc_trail_head].y = arc_shot_y;
    arc_trail_points[arc_trail_head].visible = 1;

    if (++arc_path_step % 3 == 0 && arc_path_count[arc_turn] < ARC_PATH_DOTS && arc_shot_y >= 0.0f) {
        arc_path_points[arc_turn][arc_path_count[arc_turn]].x = arc_shot_x;
        arc_path_points[arc_turn][arc_path_count[arc_turn]].y = arc_shot_y;
        arc_path_count[arc_turn]++;
    }

    for (int offset = 0; offset < ARC_TRAIL_COUNT; offset++) {
        const int index = (arc_trail_head - offset + ARC_TRAIL_COUNT) % ARC_TRAIL_COUNT;
        if (!arc_trail_points[index].visible) {
            arc_set_hidden(arc_trail[index], 1);
            continue;
        }
        const int size = arc_clamp(arc_scale(7) - offset / 3, 2, arc_scale(7));
        lv_obj_set_size(arc_trail[index], size, size);
        arc_place(
            arc_trail[index], (int) lroundf(arc_trail_points[index].x) - size / 2,
            (int) lroundf(arc_trail_points[index].y) - size / 2
        );
        arc_set_fill(arc_trail[index], arc_clamp(200 - offset * 15, 30, 200));
        arc_set_hidden(arc_trail[index], 0);
    }
}

static void arc_guard_flash(const int player, const uint32_t colour) {
    const uint32_t now = lv_tick_get();
    arc_explosion_active = 1;
    arc_explosion_started = now;
    arc_explosion_radius = arc_actor_w;
    arc_explosion_x = (int) lroundf(arc_actor_x[player] + arc_actor_w * 0.5f);
    arc_explosion_y = (int) lroundf(arc_actor_y[player] + arc_actor_h * 0.5f);
    lv_obj_set_style_bg_color(arc_explosion, lv_color_hex(arc_mix(0xFFFFFF, colour, 120)), MU_OBJ_MAIN_DEFAULT);
    arc_set_hidden(arc_explosion, 0);
    arc_set_hidden(arc_blast_ring, 0);
}

static void arc_reshape_ground(const int from, const int to) {
    for (int x = arc_clamp(from, 0, arc_width - 1); x <= arc_clamp(to, 0, arc_width - 1); x++)
        arc_ground[x] = (int16_t) arc_clamp(arc_ground[x], arc_hud_bottom + arc_scale(12), arc_height - arc_scale(10));
    arc_position_actor(0);
    arc_position_actor(1);
    arc_paint_ground(from, to);
}

static void arc_raise_ground(const int centre, const int half, const float height) {
    for (int column = centre - half; column <= centre + half; column++) {
        if (column < 0 || column >= arc_width) continue;
        const float shape = 0.5f + 0.5f * cosf((float) M_PI * (float) (column - centre) / (float) half);
        arc_ground[column] = (int16_t) lroundf((float) arc_ground[column] - height * shape);
    }
    arc_reshape_ground(centre - half, centre + half);
}

static void arc_build_wall(const float x, const arc_weapon *weapon) {
    const int centre = arc_clamp((int) lroundf(x), 0, arc_width - 1);
    const int half = arc_clamp(arc_scale(36), 16, 60);
    arc_raise_ground(centre, half, (float) arc_scale(62) * (float) weapon->strength / 100.0f);

    for (int index = 0; index < 10; index++) {
        const float angle = (float) M_PI * (1.15f + arc_randf() * 0.7f);
        const float speed = (float) arc_scale(110) * (0.4f + arc_randf());
        arc_spawn_particle(
            arc_particle_debris, x, (float) arc_ground[centre], cosf(angle) * speed, sinf(angle) * speed,
            arc_lip_colour, 700 + arc_rand(400)
        );
    }

    arc_explosion_active = 1;
    arc_explosion_started = lv_tick_get();
    arc_explosion_radius = half;
    arc_explosion_x = centre;
    arc_explosion_y = arc_ground[centre];
    lv_obj_set_style_bg_color(arc_explosion, lv_color_hex(arc_lip_colour), MU_OBJ_MAIN_DEFAULT);
    arc_set_hidden(arc_explosion, 0);
    arc_set_hidden(arc_blast_ring, 0);
    arc_shake(arc_scale(2));
    arc_volley_built = 1;
    play_sound(snd_navigate);
}

static void arc_dig_trench(const int player, const arc_weapon *weapon) {
    const int centre = arc_clamp((int) lroundf(arc_actor_x[player] + arc_actor_w * 0.5f), 0, arc_width - 1);
    const int pit = arc_actor_w * 7 / 10;
    const int reach = arc_actor_w * 17 / 10;
    const float depth = (float) arc_actor_h * 0.45f * (float) weapon->strength / 100.0f;
    const float bank = (float) arc_actor_h * 0.55f * (float) weapon->strength / 100.0f;
    for (int column = centre - reach; column <= centre + reach; column++) {
        if (column < 0 || column >= arc_width) continue;
        const int distance = abs(column - centre);
        float change;
        if (distance <= pit)
            change = depth * (0.5f + 0.5f * cosf((float) M_PI * (float) distance / (float) pit));
        else
            change = -bank * sinf((float) M_PI * (float) (distance - pit) / (float) (reach - pit));
        arc_ground[column] = (int16_t) lroundf((float) arc_ground[column] + change);
    }
    arc_reshape_ground(centre - reach, centre + reach);
}

static void arc_hop_back(const int player, const arc_weapon *weapon) {
    const float away = player ? 1.0f : -1.0f;
    const float from = arc_actor_x[player] + arc_actor_w * 0.5f;
    const float distance = (float) arc_width * (float) weapon->strength / 100.0f;
    const float lowest = (float) arc_width * 0.04f + arc_actor_w * 0.5f;
    const float highest = (float) arc_width * 0.96f - arc_actor_w * 0.5f;
    const float target = arc_clampf(from + away * distance, lowest, highest);
    const arc_variant *variant = &arc_variants[arc_variant_selected[player]];

    for (int index = 0; index < 8; index++) {
        const float angle = (float) M_PI * (1.1f + arc_randf() * 0.8f);
        const float speed = (float) arc_scale(90) * (0.4f + arc_randf());
        arc_spawn_particle(
            arc_particle_splat, from, arc_actor_y[player] + arc_actor_h, cosf(angle) * speed, sinf(angle) * speed,
            variant->colour, 600 + arc_rand(300)
        );
    }

    arc_actor_x[player] = target - arc_actor_w * 0.5f;
    arc_flatten_pad((int) lroundf(target));
    arc_reshape_ground(0, arc_width - 1);
}

static void arc_use_guard(const int player, const arc_weapon *weapon) {
    const arc_variant *variant = &arc_variants[arc_variant_selected[player]];

    switch (weapon->kind) {
        case arc_kind_shield:
            break;
        case arc_kind_heal: {
            const int before = arc_health[player];
            arc_health[player] = arc_clamp(before + weapon->strength, 0, variant->health);
            arc_show_number(player, arc_health[player] - before, 1);
            arc_regen[player] = ARC_REGEN_TURNS;
            arc_regen_amount[player] = weapon->regen;
            break;
        }
        case arc_kind_hop:
            arc_hop_back(player, weapon);
            break;
        case arc_kind_trench:
            arc_dig_trench(player, weapon);
            break;
        default:
            break;
    }

    if (weapon->guard > 0) {
        arc_shield[player] = weapon->guard;
        arc_shield_fade[player] = weapon->fade;
        arc_shield_name[player] = weapon->name;
        arc_shield_hit_at[player] = 0;
    }
    arc_guard_ammo[player]--;
    arc_armed[player] = 0;
    arc_cpu_deadline = 0;
    arc_set_hidden(arc_hud, 1);
    arc_set_hidden(arc_bubble, 1);
    arc_hide_aim();
    arc_hide_path();
    arc_guard_flash(player, variant->cap);

    char text[96];
    snprintf(text, sizeof(text), "%s!", TRS((char *) weapon->name));
    arc_show_toast(text);
    arc_update_hud();
    play_sound(snd_option);
}

static void arc_shell_look(const int index, const int size) {
    lv_obj_t *dot = arc_shell_obj[index];
    lv_obj_set_size(dot, size, size);
    arc_place(dot, (int) lroundf(arc_shells[index].x) - size / 2, (int) lroundf(arc_shells[index].y) - size / 2);
    arc_set_hidden(dot, 0);
}

static int arc_shell_size(void) {
    const int base = arc_clamp(arc_scale(9), 6, 14);
    return arc_clamp(base * arc_volley_weapon->size / 100, 4, base * 2);
}

static void arc_fire(void) {
    if (!arc_is_aiming()) return;
    if (arc_players == 1 && arc_turn == 1 && !arc_cpu_deadline) return;

    const arc_weapon *weapon = arc_weapon_for(arc_turn);
    if (arc_is_instant(weapon)) {
        arc_use_guard(arc_turn, weapon);
        return;
    }

    arc_volley_weapon = weapon;
    arc_volley_built = 0;
    arc_volley_damage[0] = 0;
    arc_volley_damage[1] = 0;
    arc_volley_absorbed[0] = 0;
    arc_volley_absorbed[1] = 0;
    arc_volley_direct = -1;
    arc_volley_bursts = 0;
    arc_volley_orbit = 0;
    arc_volley_radius = 0;
    arc_lead_shell = 0;

    for (int index = 0; index < ARC_SHELL_COUNT; index++)
        arc_shells[index].active = 0;

    const int spread = weapon->kind == arc_kind_spread;
    for (int index = 0; index < (spread ? 3 : 1); index++) {
        const int angle = arc_clamp(arc_angle[arc_turn] + (spread ? (index - 1) * 6 : 0), 5, 85);
        arc_shell_start(&arc_shells[index], arc_turn, angle, arc_power[arc_turn]);
    }

    arc_shot_x = arc_shells[0].x;
    arc_shot_y = arc_shells[0].y;
    arc_shot_gravity = arc_shell_gravity(arc_turn);
    arc_shot_wind = arc_wind_accel();
    arc_shot_tick = lv_tick_get();
    arc_shot_accum = 0.0f;
    arc_shot_active = 1;
    arc_cpu_deadline = 0;
    if (weapon->kind != arc_kind_wall) arc_shots[arc_turn]++;
    arc_actor_recoil_at[arc_turn] = arc_shot_tick;
    arc_set_hidden(arc_hud, 1);
    arc_set_hidden(arc_bubble, 1);
    arc_hide_path();
    arc_path_count[arc_turn] = 0;
    arc_path_step = 0;
    arc_trail_head = 0;
    arc_trail_last_x = arc_shot_x;
    arc_trail_last_y = arc_shot_y;
    memset(arc_trail_points, 0, sizeof(arc_trail_points));

    const arc_variant *variant = &arc_variants[arc_variant_selected[arc_turn]];
    const uint32_t border = weapon->kind == arc_kind_main ? theme.header.text : variant->cap;
    for (int index = 0; index < ARC_SHELL_COUNT; index++) {
        lv_obj_set_style_bg_color(arc_shell_obj[index], lv_color_hex(variant->colour), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_color(arc_shell_obj[index], lv_color_hex(border), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_width(arc_shell_obj[index], weapon->kind == arc_kind_main ? 1 : 2, MU_OBJ_MAIN_DEFAULT);
        if (arc_shells[index].active) arc_shell_look(index, arc_shell_size());
    }
    for (int index = 0; index < ARC_TRAIL_COUNT; index++)
        lv_obj_set_style_bg_color(arc_trail[index], lv_color_hex(variant->colour), MU_OBJ_MAIN_DEFAULT);

    if (weapon->kind != arc_kind_main) {
        if (arc_armed[arc_turn] == 2)
            arc_guard_ammo[arc_turn]--;
        else
            arc_ammo[arc_turn]--;
        arc_armed[arc_turn] = 0;
        char text[96];
        snprintf(text, sizeof(text), "%s!", TRS((char *) weapon->name));
        arc_show_toast(text);
    }

    arc_hide_aim();
    play_sound(snd_confirm);
}

static void arc_tick_guard(const int player) {
    if (arc_regen[player] > 0) {
        const int full = arc_variants[arc_variant_selected[player]].health;
        const int before = arc_health[player];
        arc_health[player] = arc_clamp(before + arc_regen_amount[player], 0, full);
        arc_show_number(player, arc_health[player] - before, 1);
        arc_regen[player]--;
    }

    if (arc_shield[player] <= 0) return;
    arc_shield[player] = arc_clamp(arc_shield[player] - arc_shield_fade[player], 0, 100);
    if (arc_shield[player] > 0 || !arc_shield_name[player]) return;

    char text[96];
    snprintf(text, sizeof(text), "%s %s", TRS((char *) arc_shield_name[player]), TRS("wore off"));
    arc_show_toast(text);
}

static void arc_apply_condition(void) {
    if (arc_condition == arc_hazard_calm) {
        arc_wind = 0;
    } else if (arc_condition == arc_hazard_gust) {
        const int surge = arc_clamp(arc_planet_current()->wind * 2 + 2, 5, 16);
        arc_wind = arc_wind < 0 || (!arc_wind && arc_rand(2)) ? -surge : surge;
    }
}

static void arc_next_turn(void) {
    arc_turn ^= 1;
    arc_turns_taken[arc_turn]++;
    arc_tick_guard(arc_turn);
    arc_condition = arc_condition_next;
    arc_condition_next = arc_hazard_none;
    arc_hazard_rolled = 0;
    arc_roll_wind(0);
    arc_apply_condition();
    arc_set_hidden(arc_hud, 0);
    arc_update_hud();
    arc_show_path();
    arc_prepare_cpu();

    if (arc_players == 1 && arc_turn == 1 && lv_obj_has_flag(arc_bubble, LV_OBJ_FLAG_HIDDEN)) {
        if (arc_rand(3) == 0)
            arc_cpu_say(arc_say_variant[arc_variant_selected[1]][arc_rand(2)], 100);
        else
            arc_cpu_say(ARC_PICK(arc_say_turn), 30);
    }
}

static int arc_is_condition(const int kind) {
    return kind == arc_hazard_gust || kind == arc_hazard_calm || kind == arc_hazard_flare || kind == arc_hazard_haze;
}

static int arc_roll_hazard(void) {
    const arc_planet *planet = arc_planet_current();
    arc_hazard_rolled = 1;

    int count = 0;
    while (count < ARC_HAZARD_SLOTS && planet->hazards[count].kind != arc_hazard_none)
        count++;
    if (!count || arc_rand(100) >= planet->chance) return 0;

    const arc_hazard *hazard = &planet->hazards[arc_rand(count)];
    arc_hazard_kind = hazard->kind;
    arc_hazard_name = hazard->name;
    arc_hazard_stage = 0;
    arc_hazard_struck = 0;
    arc_hazard_at = lv_tick_get() + ARC_HAZARD_WAIT_MS;

    if (arc_hazard_kind == arc_hazard_eruption || arc_hazard_kind == arc_hazard_geyser) {
        arc_hazard_x = (float) arc_feature_spot();
    } else if (arc_rand(2)) {
        const int player = arc_rand(2);
        const float spread = arc_actor_w * 2.0f;
        arc_hazard_x = arc_actor_x[player] + arc_actor_w * 0.5f + (arc_randf() - 0.5f) * spread * 2.0f;
    } else {
        arc_hazard_x = (float) arc_width * (0.08f + arc_randf() * 0.84f);
    }
    arc_hazard_x = arc_clampf(arc_hazard_x, 2.0f, (float) arc_width - 3.0f);
    return 1;
}

static void arc_after_volley(void) {
    arc_set_hidden(arc_hud, 0);
    if (arc_round_winner != -2) {
        arc_finish_round();
        return;
    }
    if (!arc_hazard_rolled && arc_roll_hazard()) return;
    arc_next_turn();
}

static void arc_hazard_hurt(int damage[2], const uint32_t now) {
    static const int none[2] = {0, 0};
    arc_cap_damage(damage, none, ARC_HAZARD_CAP);
    if (damage[0] <= 0 && damage[1] <= 0) return;

    for (int player = 0; player < 2; player++) {
        if (damage[player] <= 0) continue;
        arc_health_hit_at[player] = now;
        arc_actor_hit_at[player] = now;
        arc_show_number(player, damage[player], 0);
        if (arc_health[player] <= 0) arc_actor_ko_at[player] = now;
    }

    if (arc_health[0] <= 0 && arc_health[1] <= 0)
        arc_round_winner = -1;
    else if (arc_health[0] <= 0)
        arc_round_winner = 1;
    else if (arc_health[1] <= 0)
        arc_round_winner = 0;

    if (arc_round_winner == -1)
        arc_show_toast("Double knockout!");
    else if (arc_round_winner >= 0)
        arc_show_toast("Knockout!");
    else if (damage[0] > damage[1])
        arc_cpu_say(ARC_PICK(arc_say_hazard), 60);

    arc_update_hud();
    play_sound(snd_error);
}

static void arc_hazard_strike(const float x, const int radius, const int peak, const int dig, const uint32_t colour) {
    const uint32_t now = lv_tick_get();
    const int column = arc_clamp((int) lroundf(x), 0, arc_width - 1);
    const float y = (float) arc_ground[column];
    int damage[2] = {0, 0};

    for (int player = 0; player < 2 && peak > 0; player++) {
        if (arc_health[player] <= 0) continue;
        const float dx = arc_actor_x[player] + arc_actor_w * 0.5f - x;
        const float dy = arc_actor_y[player] + arc_actor_h * 0.5f - y;
        const float distance = sqrtf(dx * dx + dy * dy);
        if (distance >= (float) radius) continue;

        int amount = (int) lroundf((1.0f - distance / (float) radius) * (float) peak);
        amount = arc_absorb(player, amount * arc_variants[arc_variant_selected[player]].defence / 100, now);
        const int before = arc_health[player];
        arc_health[player] = arc_clamp(before - amount, 0, arc_variants[arc_variant_selected[player]].health);
        damage[player] = before - arc_health[player];
    }

    if (dig) {
        float drop[2];
        arc_damage_ground(column, radius, colour, drop);
        arc_fall_damage(drop, damage);
    }

    arc_explosion_active = 1;
    arc_explosion_started = now;
    arc_explosion_radius = radius;
    arc_explosion_x = column;
    arc_explosion_y = (int) lroundf(y);
    lv_obj_set_style_bg_color(arc_explosion, lv_color_hex(arc_mix(0xFFFFFF, colour, 110)), MU_OBJ_MAIN_DEFAULT);
    arc_set_hidden(arc_explosion, 0);
    arc_set_hidden(arc_blast_ring, 0);

    for (int index = 0; index < 12; index++) {
        const float angle = (float) M_PI * (1.1f + arc_randf() * 0.8f);
        const float speed = (float) arc_scale(140) * (0.4f + arc_randf());
        arc_spawn_particle(
            index & 1 ? arc_particle_debris : arc_particle_splat, x, y, cosf(angle) * speed, sinf(angle) * speed,
            index & 1 ? arc_lip_colour : colour, 700 + arc_rand(500)
        );
    }

    arc_shake(arc_scale(3) + (damage[0] + damage[1]) / 8);
    arc_hazard_hurt(damage, now);
    if (damage[0] <= 0 && damage[1] <= 0) play_sound(snd_navigate);
}

static void arc_hazard_fountain(const float x, const uint32_t colour, const int count) {
    const float y = (float) arc_ground[arc_clamp((int) lroundf(x), 0, arc_width - 1)];
    for (int index = 0; index < count; index++) {
        const float angle = (float) M_PI * (1.35f + arc_randf() * 0.3f);
        const float speed = (float) arc_scale(220) * (0.5f + arc_randf() * 0.7f);
        arc_spawn_particle(
            arc_particle_splat, x, y, cosf(angle) * speed, sinf(angle) * speed,
            index % 3 ? colour : arc_shade(colour, 50), 900 + arc_rand(600)
        );
    }
}

static void arc_quake_ground(const uint32_t now) {
    const float before[2] = {arc_actor_y[0], arc_actor_y[1]};
    const float phase = arc_randf() * 6.28f;
    const float cycles = 2.0f + arc_randf() * 2.0f;
    const float sink = (float) arc_scale(18);
    const int reach = arc_clamp(arc_width / 60, 2, 12);

    int16_t *copy = malloc((size_t) arc_width * sizeof(*copy));
    if (copy) {
        memcpy(copy, arc_ground, (size_t) arc_width * sizeof(*copy));
        for (int x = 0; x < arc_width; x++) {
            int total = 0;
            int samples = 0;
            for (int offset = -reach; offset <= reach; offset++) {
                const int column = x + offset;
                if (column < 0 || column >= arc_width) continue;
                total += copy[column];
                samples++;
            }
            const float wave = sinf((float) x / (float) arc_width * cycles * 2.0f * (float) M_PI + phase);
            arc_ground[x] = (int16_t) lroundf((float) total / (float) samples + sink * (0.5f + 0.5f * wave));
        }
        free(copy);
    }
    arc_reshape_ground(0, arc_width - 1);

    for (int index = 0; index < 10; index++) {
        const float x = (float) arc_rand(arc_width);
        const float angle = (float) M_PI * (1.2f + arc_randf() * 0.6f);
        const float speed = (float) arc_scale(90) * (0.4f + arc_randf());
        arc_spawn_particle(
            arc_particle_debris, x, (float) arc_ground[arc_clamp((int) x, 0, arc_width - 1)], cosf(angle) * speed,
            sinf(angle) * speed, arc_lip_colour, 600 + arc_rand(400)
        );
    }

    float drop[2];
    int damage[2] = {0, 0};
    for (int player = 0; player < 2; player++) {
        drop[player] = arc_actor_y[player] - before[player];
        arc_drop_actor(player, drop[player], now);
    }
    arc_fall_damage(drop, damage);
    arc_hazard_hurt(damage, now);
}

static void arc_hide_bolt(void) {
    for (int index = 0; index < ARC_BOLT_DOTS; index++)
        arc_set_hidden(arc_bolt[index], 1);
}

static void arc_place_bolt(void) {
    float point_x[7];
    float point_y[7];
    const int column = arc_clamp((int) lroundf(arc_hazard_x), 0, arc_width - 1);
    const float top = -(float) arc_pad_top;
    const float bottom = (float) arc_ground[column];
    const float jag = (float) arc_scale(16);

    for (int index = 0; index < 7; index++) {
        const float place = (float) index / 6.0f;
        point_y[index] = top + (bottom - top) * place;
        point_x[index] = arc_hazard_x + (index == 6 ? 0.0f : (arc_randf() - 0.5f) * jag * 2.0f * (1.0f - place * 0.5f));
    }

    float length = 0.0f;
    for (int index = 1; index < 7; index++)
        length += hypotf(point_x[index] - point_x[index - 1], point_y[index] - point_y[index - 1]);

    const int size = arc_clamp(arc_scale(5), 3, 8);
    int segment = 1;
    float walked = 0.0f;
    float segment_start = 0.0f;
    for (int index = 0; index < ARC_BOLT_DOTS; index++) {
        const float want = length * (float) index / (float) (ARC_BOLT_DOTS - 1);
        while (segment < 6) {
            const float piece =
                hypotf(point_x[segment] - point_x[segment - 1], point_y[segment] - point_y[segment - 1]);
            if (walked + piece >= want) break;
            walked += piece;
            segment_start = walked;
            segment++;
        }
        const float piece = hypotf(point_x[segment] - point_x[segment - 1], point_y[segment] - point_y[segment - 1]);
        const float t = piece > 0.0f ? arc_clampf((want - segment_start) / piece, 0.0f, 1.0f) : 0.0f;
        const float x = point_x[segment - 1] + (point_x[segment] - point_x[segment - 1]) * t;
        const float y = point_y[segment - 1] + (point_y[segment] - point_y[segment - 1]) * t;
        lv_obj_set_size(arc_bolt[index], size, size);
        arc_place(arc_bolt[index], (int) lroundf(x) - size / 2, (int) lroundf(y) - size / 2);
        arc_set_fill(arc_bolt[index], 255);
        arc_set_hidden(arc_bolt[index], 0);
    }
}

static void arc_place_meteor(const float progress) {
    const int column = arc_clamp((int) lroundf(arc_hazard_x), 0, arc_width - 1);
    const float to_y = (float) arc_ground[column];
    const float ease = progress * progress;
    const float x = arc_hazard_from_x + (arc_hazard_x - arc_hazard_from_x) * ease;
    const float y = arc_hazard_from_y + (to_y - arc_hazard_from_y) * ease;
    const float dx = arc_hazard_x - arc_hazard_from_x;
    const float dy = to_y - arc_hazard_from_y;
    const float span = fmaxf(hypotf(dx, dy), 1.0f);
    const float spacing = (float) arc_clamp(arc_scale(6), 4, 10);
    const int size = arc_clamp(arc_scale(11), 6, 18);

    lv_obj_t *rock = arc_shell_obj[0];
    lv_obj_set_size(rock, size, size);
    arc_place(rock, (int) lroundf(x) - size / 2, (int) lroundf(y) - size / 2);
    arc_set_hidden(rock, 0);

    for (int index = 0; index < ARC_TRAIL_COUNT; index++) {
        const float back = spacing * (float) (index + 1) * (0.4f + progress);
        const int dot = arc_clamp(size - index, 2, size);
        lv_obj_set_size(arc_trail[index], dot, dot);
        arc_place(
            arc_trail[index], (int) lroundf(x - dx / span * back) - dot / 2,
            (int) lroundf(y - dy / span * back) - dot / 2
        );
        arc_set_fill(arc_trail[index], arc_clamp(230 - index * 18, 20, 230));
        arc_set_hidden(arc_trail[index], 0);
    }
}

static void arc_end_hazard(void) {
    arc_hazard_kind = arc_hazard_none;
    arc_clear_shot();
    arc_hide_bolt();
    if (!arc_explosion_active) arc_after_volley();
}

static void arc_begin_hazard(const uint32_t now) {
    const arc_planet *planet = arc_planet_current();
    arc_hazard_stage = 1;
    arc_hazard_at = now;
    arc_show_toast(arc_hazard_name);

    if (arc_is_condition(arc_hazard_kind)) {
        arc_condition_next = arc_hazard_kind;
        play_sound(snd_option);
        arc_end_hazard();
        return;
    }

    switch (arc_hazard_kind) {
        case arc_hazard_meteor: {
            const float side = arc_rand(2) ? 1.0f : -1.0f;
            arc_hazard_from_x = arc_hazard_x - side * (float) arc_width * (0.2f + arc_randf() * 0.2f);
            arc_hazard_from_y = -(float) arc_pad_top - (float) arc_scale(20);
            lv_obj_set_style_bg_color(
                arc_shell_obj[0], lv_color_hex(arc_shade(planet->ground, -60)), MU_OBJ_MAIN_DEFAULT
            );
            lv_obj_set_style_border_color(arc_shell_obj[0], lv_color_hex(planet->halo), MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_border_width(arc_shell_obj[0], 2, MU_OBJ_MAIN_DEFAULT);
            for (int index = 0; index < ARC_TRAIL_COUNT; index++)
                lv_obj_set_style_bg_color(
                    arc_trail[index], lv_color_hex(arc_mix(0xFFE08A, 0xFF6A1F, 255 - index * 20)), MU_OBJ_MAIN_DEFAULT
                );
            arc_place_meteor(0.0f);
            break;
        }
        case arc_hazard_lightning:
            arc_place_bolt();
            break;
        case arc_hazard_quake:
            arc_shake_for(arc_scale(7), 1000);
            play_sound(snd_error);
            break;
        default:
            arc_shake_for(arc_scale(3), 500);
            break;
    }
}

static void arc_update_hazard(const uint32_t now) {
    if (arc_hazard_stage == 0) {
        if ((int32_t) (now - arc_hazard_at) >= 0) arc_begin_hazard(now);
        return;
    }

    const arc_planet *planet = arc_planet_current();
    const uint32_t age = now - arc_hazard_at;

    switch (arc_hazard_kind) {
        case arc_hazard_meteor: {
            const float progress = arc_clampf((float) age / 700.0f, 0.0f, 1.0f);
            if (progress < 1.0f) {
                arc_place_meteor(progress);
                return;
            }
            arc_clear_shot();
            arc_hazard_strike(
                arc_hazard_x, arc_clamp(arc_width / 15, arc_scale(22), arc_scale(64)), 30, 1,
                arc_shade(planet->ground, -40)
            );
            arc_end_hazard();
            return;
        }
        case arc_hazard_lightning:
            if (!arc_hazard_struck && age >= 90) {
                arc_hazard_struck = 1;
                arc_hazard_strike(
                    arc_hazard_x, arc_clamp(arc_width / 20, arc_scale(18), arc_scale(48)), 28, 1, 0xFFF4B0
                );
            }
            if (age < 300) {
                for (int index = 0; index < ARC_BOLT_DOTS; index++)
                    arc_set_fill(arc_bolt[index], (age / 45) % 2 ? 130 : 255);
                return;
            }
            arc_end_hazard();
            return;
        case arc_hazard_eruption:
            if (age < 450) return;
            arc_raise_ground((int) lroundf(arc_hazard_x), arc_clamp(arc_scale(40), 18, 64), (float) arc_scale(56));
            arc_hazard_fountain(arc_hazard_x, 0xFF7A1F, 16);
            arc_hazard_strike(arc_hazard_x, arc_clamp(arc_width / 9, arc_scale(30), arc_scale(90)), 22, 0, 0xFF7A1F);
            arc_end_hazard();
            return;
        case arc_hazard_geyser:
            if (age < 350) return;
            arc_raise_ground((int) lroundf(arc_hazard_x), arc_clamp(arc_scale(26), 12, 44), (float) arc_scale(70));
            arc_hazard_fountain(arc_hazard_x, planet->precip, 20);
            arc_hazard_strike(arc_hazard_x, arc_clamp(arc_scale(30), 14, 48), 0, 0, planet->precip);
            arc_end_hazard();
            return;
        case arc_hazard_quake:
            if (!arc_hazard_struck && age >= 300) {
                arc_hazard_struck = 1;
                arc_quake_ground(now);
            }
            if (age >= 1000) arc_end_hazard();
            return;
        default:
            arc_end_hazard();
            return;
    }
}

static void arc_end_volley(void) {
    const int shooter = arc_turn;
    arc_clear_shot();

    if (arc_volley_built) {
        arc_show_toast("Wall up!");
    } else if (!arc_volley_bursts) {
        const int none[2] = {0, 0};
        arc_resolve_messages(shooter, -1, none, arc_shot_x, 0, arc_volley_orbit ? 2 : 1);
    } else {
        if (arc_health[0] <= 0 && arc_health[1] <= 0)
            arc_round_winner = -1;
        else if (arc_health[0] <= 0)
            arc_round_winner = 1;
        else if (arc_health[1] <= 0)
            arc_round_winner = 0;
        else
            arc_round_winner = -2;
        arc_resolve_messages(shooter, arc_volley_direct, arc_volley_damage, arc_volley_x, arc_volley_radius, 0);

        const int target = shooter ^ 1;
        if (arc_volley_damage[target] > arc_match_best[shooter]) arc_match_best[shooter] = arc_volley_damage[target];
        if (arc_volley_direct == target) {
            const int metres =
                (int) lroundf(fabsf(arc_actor_x[target] - arc_actor_x[shooter]) * 1000.0f / (float) arc_width);
            if (metres > arc_match_long[shooter]) arc_match_long[shooter] = metres;
        }
    }

    if (!arc_explosion_active) arc_after_volley();
}

static void arc_split_shell(const int parent) {
    static const float spread[4] = {0.55f, 0.85f, 1.15f, 1.45f};
    const arc_shell origin = arc_shells[parent];
    int slot = parent;

    for (int piece = 0; piece < 4; piece++) {
        if (piece > 0) {
            slot = -1;
            for (int index = 0; index < ARC_SHELL_COUNT; index++) {
                if (!arc_shells[index].active) {
                    slot = index;
                    break;
                }
            }
            if (slot < 0) return;
        }
        arc_shell *shell = &arc_shells[slot];
        *shell = origin;
        shell->vx = origin.vx * spread[piece];
        shell->vy = origin.vy - (float) arc_scale(30) * (float) (piece & 1);
        shell->split = 1;
        shell->active = 1;
        arc_shell_look(slot, arc_clamp(arc_shell_size() * 3 / 4, 4, 14));
    }
}

static int arc_step_shells(void) {
    int alive = 0;
    arc_wall_live = 1;
    for (int index = 0; index < ARC_SHELL_COUNT; index++) {
        arc_shell *shell = &arc_shells[index];
        if (!shell->active) continue;

        const int event = arc_shell_step(shell, arc_volley_weapon, arc_turn, arc_shot_gravity, arc_shot_wind, ARC_STEP);
        switch (event) {
            case arc_event_split:
                arc_split_shell(index);
                break;
            case arc_event_ground:
            case arc_event_actor:
                shell->active = 0;
                arc_set_hidden(arc_shell_obj[index], 1);
                if (arc_volley_weapon->kind == arc_kind_wall)
                    arc_build_wall(shell->x, arc_volley_weapon);
                else
                    arc_detonate(shell->x, shell->y, event == arc_event_actor ? shell->hit : -1);
                break;
            case arc_event_lost:
                shell->active = 0;
                arc_set_hidden(arc_shell_obj[index], 1);
                if (shell->peak < -(float) arc_height * 0.6f) arc_volley_orbit = 1;
                break;
            default:
                break;
        }
    }

    arc_wall_live = 0;
    arc_wall_feedback();

    for (int index = 0; index < ARC_SHELL_COUNT; index++)
        if (arc_shells[index].active) alive++;
    return alive;
}

static void arc_step_shot(const uint32_t now) {
    float dt = (float) (now - arc_shot_tick) / 1000.0f;
    arc_shot_tick = now;
    if (dt > 0.1f) dt = 0.1f;
    if (dt <= 0.0f) return;

    arc_shot_accum += dt;
    int alive = 1;
    while (arc_shot_accum >= ARC_STEP && alive) {
        arc_shot_accum -= ARC_STEP;
        alive = arc_step_shells();
    }

    for (int index = 0; index < ARC_SHELL_COUNT; index++) {
        if (!arc_shells[index].active) continue;
        const int size = lv_obj_get_style_width(arc_shell_obj[index], LV_PART_MAIN);
        arc_place(
            arc_shell_obj[index], (int) lroundf(arc_shells[index].x) - size / 2,
            (int) lroundf(arc_shells[index].y) - size / 2
        );
    }

    if (!alive) {
        arc_end_volley();
        return;
    }

    if (!arc_shells[arc_lead_shell].active) {
        for (int index = 0; index < ARC_SHELL_COUNT; index++) {
            if (arc_shells[index].active) {
                arc_lead_shell = index;
                break;
            }
        }
        arc_trail_last_x = arc_shells[arc_lead_shell].x;
        arc_trail_last_y = arc_shells[arc_lead_shell].y;
    }
    arc_shot_x = arc_shells[arc_lead_shell].x;
    arc_shot_y = arc_shells[arc_lead_shell].y;
    arc_push_trail();
}

static void arc_draw_explosion(const uint32_t age) {
    const float progress = arc_clampf((float) age / (float) ARC_EXPLOSION_MS, 0.0f, 1.0f);
    const float grow = arc_clampf((float) age / 180.0f, 0.0f, 1.0f);
    const int radius = (int) lroundf((float) arc_explosion_radius * (0.45f + 0.55f * grow));
    lv_obj_set_size(arc_explosion, radius * 2, radius * 2);
    arc_place(arc_explosion, arc_explosion_x - radius, arc_explosion_y - radius);
    arc_set_fill(arc_explosion, (int) (255.0f * (1.0f - progress)));

    const float ring_progress = arc_clampf((float) age / 480.0f, 0.0f, 1.0f);
    const int ring = (int) lroundf((float) arc_explosion_radius * (0.6f + 0.9f * ring_progress));
    lv_obj_set_size(arc_blast_ring, ring * 2, ring * 2);
    arc_place(arc_blast_ring, arc_explosion_x - ring, arc_explosion_y - ring);
    lv_obj_set_style_border_opa(arc_blast_ring, (lv_opa_t) (230.0f * (1.0f - ring_progress)), MU_OBJ_MAIN_DEFAULT);
    arc_set_hidden(arc_blast_ring, ring_progress >= 1.0f);
}

static void arc_update_explosion(const uint32_t now) {
    const uint32_t age = now - arc_explosion_started;
    arc_draw_explosion(age);

    if (age < ARC_EXPLOSION_MS) return;
    arc_explosion_active = 0;
    arc_set_hidden(arc_explosion, 1);
    arc_set_hidden(arc_blast_ring, 1);
    if (arc_shot_active || arc_hazard_kind) return;
    arc_after_volley();
}

static void arc_update_cpu(const uint32_t now) {
    if (!arc_cpu_deadline || !arc_is_aiming()) return;
    if ((int32_t) (now - arc_cpu_deadline) >= 0) {
        arc_angle[1] = arc_cpu_angle;
        arc_power[1] = arc_cpu_power;
        arc_update_hud();
        arc_fire();
        return;
    }

    const uint32_t span = (arc_cpu_deadline - arc_cpu_started) * 3 / 4;
    const float progress = span ? arc_clampf((float) (now - arc_cpu_started) / (float) span, 0.0f, 1.0f) : 1.0f;
    const float ease = progress * progress * (3.0f - 2.0f * progress);
    const int angle = arc_cpu_from_angle + (int) lroundf((float) (arc_cpu_angle - arc_cpu_from_angle) * ease);
    const int power = arc_cpu_from_power + (int) lroundf((float) (arc_cpu_power - arc_cpu_from_power) * ease);
    if (angle != arc_angle[1] || power != arc_power[1]) {
        arc_angle[1] = angle;
        arc_power[1] = power;
        arc_update_hud();
    }
}

static void arc_update_actors(const uint32_t now) {
    for (int player = 0; player < 2; player++) {
        int shift_x = 0;
        int shift_y = 0;
        int width = arc_actor_w;
        int height = arc_actor_h;
        int flash = 0;

        if (arc_actor_ko_at[player]) {
            const float squash =
                arc_clampf((float) (now - arc_actor_ko_at[player]) / (float) ARC_SQUASH_MS, 0.0f, 1.0f);
            width = (int) lroundf((float) arc_actor_w * (1.0f + 0.35f * squash));
            height = (int) lroundf((float) arc_actor_h * (1.0f - 0.5f * squash));
            shift_x = -(width - arc_actor_w) / 2;
            shift_y = arc_actor_h - height;
        } else if (arc_screen == arc_screen_play && player == arc_turn && arc_is_aiming()) {
            shift_y = -(int) lroundf(fabsf(sinf((float) now * 0.006f)) * (float) arc_scale(3));
        }

        if (arc_actor_recoil_at[player] && now - arc_actor_recoil_at[player] < ARC_RECOIL_MS) {
            const float left = 1.0f - (float) (now - arc_actor_recoil_at[player]) / (float) ARC_RECOIL_MS;
            shift_x += (int) lroundf((player ? 1.0f : -1.0f) * (float) arc_scale(4) * left);
        }

        if (arc_actor_hit_at[player] && now - arc_actor_hit_at[player] < ARC_HIT_MS) {
            const float left = 1.0f - (float) (now - arc_actor_hit_at[player]) / (float) ARC_HIT_MS;
            shift_x += (int) lroundf(sinf((float) now * 0.09f) * (float) arc_scale(4) * left);
            flash = (int) (left * 170.0f);
        }

        if (arc_actor_fall_at[player]) {
            const float t = (float) (now - arc_actor_fall_at[player]) / 1000.0f;
            const float left = arc_actor_fall[player] - 0.5f * (float) arc_scale(1400) * t * t;
            if (left <= 0.0f)
                arc_actor_fall_at[player] = 0;
            else
                shift_y -= (int) lroundf(left);
        }

        if (width != arc_actor_size_w[player] || lv_obj_get_style_height(arc_actor[player], LV_PART_MAIN) != height) {
            lv_obj_set_size(arc_actor[player], width, height);
            arc_actor_size_w[player] = width;
        }
        if (shift_x != arc_actor_shift_x[player]) {
            lv_obj_set_style_translate_x(arc_actor[player], shift_x, MU_OBJ_MAIN_DEFAULT);
            arc_actor_shift_x[player] = shift_x;
        }
        if (shift_y != arc_actor_shift_y[player]) {
            lv_obj_set_style_translate_y(arc_actor[player], shift_y, MU_OBJ_MAIN_DEFAULT);
            arc_actor_shift_y[player] = shift_y;
        }
        if (flash != arc_actor_flash[player] && (flash == 0 || abs(flash - arc_actor_flash[player]) >= 16)) {
            const uint32_t colour = arc_variants[arc_variant_selected[player]].colour;
            lv_obj_set_style_bg_color(
                arc_actor[player], lv_color_hex(arc_mix(0xFFFFFF, colour, flash)), MU_OBJ_MAIN_DEFAULT
            );
            arc_actor_flash[player] = flash;
        }
    }

    for (int player = 0; player < 2; player++) {
        if (arc_shield[player] <= 0 || arc_screen != arc_screen_play) {
            arc_set_hidden(arc_shield_obj[player], 1);
            continue;
        }
        const arc_variant *variant = &arc_variants[arc_variant_selected[player]];
        if (lv_obj_get_style_bg_color(arc_shield_obj[player], LV_PART_MAIN).full != lv_color_hex(variant->cap).full) {
            lv_obj_set_style_bg_color(arc_shield_obj[player], lv_color_hex(variant->cap), MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_border_color(arc_shield_obj[player], lv_color_hex(variant->colour), MU_OBJ_MAIN_DEFAULT);
        }
        const int size = arc_actor_w * 17 / 10;
        arc_place(
            arc_shield_obj[player],
            (int) lroundf(arc_actor_x[player] + arc_actor_w * 0.5f) - size / 2 + arc_actor_shift_x[player],
            (int) lroundf(arc_actor_y[player] + arc_actor_h * 0.5f) - size / 2 + arc_actor_shift_y[player]
        );
        int glow = (20 + (int) (30.0f * sinf((float) now * 0.005f))) * arc_shield[player] / 60 + 12;
        if (arc_shield_hit_at[player] && now - arc_shield_hit_at[player] < 400)
            glow = 160 - (int) ((now - arc_shield_hit_at[player]) * 120 / 400);
        glow = arc_clamp(glow, 0, 200);
        if (lv_obj_get_style_bg_opa(arc_shield_obj[player], LV_PART_MAIN) != (lv_opa_t) glow) {
            lv_obj_set_style_bg_opa(arc_shield_obj[player], (lv_opa_t) glow, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_border_opa(
                arc_shield_obj[player], (lv_opa_t) arc_clamp(glow + 120, 0, 255), MU_OBJ_MAIN_DEFAULT
            );
        }
        arc_set_hidden(arc_shield_obj[player], 0);
    }
}

static void arc_update_health(const uint32_t now, const float dt) {
    int changed = 0;
    for (int player = 0; player < 2; player++) {
        if (arc_health_lag[player] <= (float) arc_health[player]) {
            if (arc_health_lag[player] != (float) arc_health[player]) {
                arc_health_lag[player] = (float) arc_health[player];
                changed = 1;
            }
        } else if (now - arc_health_hit_at[player] > 420) {
            const int before = (int) lroundf(arc_health_lag[player]);
            const float rate = (float) arc_variants[arc_variant_selected[player]].health * 0.9f;
            arc_health_lag[player] -= rate * dt;
            if (arc_health_lag[player] < (float) arc_health[player])
                arc_health_lag[player] = (float) arc_health[player];
            if ((int) lroundf(arc_health_lag[player]) != before) changed = 1;
        }

        const int full = arc_variants[arc_variant_selected[player]].health;
        int opa = 255;
        if (arc_health[player] > 0 && arc_health[player] * 4 <= full)
            opa = 150 + (int) (105.0f * fabsf(sinf((float) now * 0.008f)));
        if ((lv_obj_get_style_bg_opa(arc_health_bar[player], LV_PART_INDICATOR) >> 4) != (opa >> 4))
            lv_obj_set_style_bg_opa(arc_health_bar[player], (lv_opa_t) opa, LV_PART_INDICATOR | MU_OBJ_MAIN_DEFAULT);
    }
    if (changed) arc_update_hud();
}

static void arc_update_overlays(const uint32_t now) {
    if (arc_toast_started) {
        const uint32_t age = now - arc_toast_started;
        if (age >= ARC_TOAST_MS) {
            arc_toast_started = 0;
            arc_set_hidden(arc_toast, 1);
        } else {
            const int fade_in = arc_clamp((int) age * 255 / 140, 0, 255);
            const int fade_out = arc_clamp((int) (ARC_TOAST_MS - age) * 255 / 300, 0, 255);
            arc_set_opa(arc_toast, fade_in < fade_out ? fade_in : fade_out);
            const int rise = arc_clamp(arc_scale(10) - (int) age * arc_scale(10) / 160, 0, arc_scale(10));
            if (lv_obj_get_style_translate_y(arc_toast, LV_PART_MAIN) != rise)
                lv_obj_set_style_translate_y(arc_toast, rise, MU_OBJ_MAIN_DEFAULT);
        }
    }

    if (arc_bubble_started) {
        const uint32_t age = now - arc_bubble_started;
        if (age >= ARC_BUBBLE_MS) {
            arc_bubble_started = 0;
            arc_set_hidden(arc_bubble, 1);
        } else {
            const int fade_in = arc_clamp((int) age * 255 / 120, 0, 255);
            const int fade_out = arc_clamp((int) (ARC_BUBBLE_MS - age) * 255 / 300, 0, 255);
            arc_set_opa(arc_bubble, fade_in < fade_out ? fade_in : fade_out);
        }
    }

    for (int player = 0; player < 2; player++) {
        if (!arc_damage_started[player]) continue;
        const uint32_t age = now - arc_damage_started[player];
        if (age >= ARC_DAMAGE_MS) {
            arc_damage_started[player] = 0;
            arc_set_hidden(arc_damage_label[player], 1);
            continue;
        }
        const int rise = (int) age * arc_scale(34) / ARC_DAMAGE_MS;
        const int width = lv_obj_get_width(arc_damage_label[player]);
        arc_place(
            arc_damage_label[player], arc_damage_x[player] - width / 2,
            arc_damage_y[player] - rise - lv_obj_get_height(arc_damage_label[player])
        );
        arc_set_opa(arc_damage_label[player], arc_clamp((int) (ARC_DAMAGE_MS - age) * 255 / 400, 0, 255));
    }
}

static void arc_update_particles(const uint32_t now, const float dt) {
    const float gravity = (float) arc_height * 1.4f;
    for (int index = 0; index < ARC_PARTICLE_COUNT; index++) {
        arc_particle *particle = &arc_particles[index];
        if (!particle->active) continue;

        const uint32_t age = now - particle->born;
        if ((int) age >= particle->life || particle->y > (float) arc_height) {
            particle->active = 0;
            arc_set_hidden(particle->obj, 1);
            continue;
        }

        if (particle->kind == arc_particle_confetti) {
            particle->sway += dt * 5.0f;
            particle->x += (particle->vx + sinf(particle->sway) * (float) arc_scale(30)) * dt;
            particle->y += particle->vy * dt;
        } else {
            particle->vy += gravity * dt;
            particle->x += particle->vx * dt;
            particle->y += particle->vy * dt;
            const int column = arc_clamp((int) particle->x, 0, arc_width - 1);
            if (particle->vy > 0.0f && particle->y >= arc_ground[column]) {
                particle->y = arc_ground[column];
                particle->vy *= -0.25f;
                particle->vx *= 0.5f;
            }
        }

        arc_place(particle->obj, (int) particle->x, (int) particle->y);
        const int remaining = particle->life - (int) age;
        arc_set_fill(particle->obj, arc_clamp(remaining * 255 / 350, 0, 255));
    }
}

static void arc_update_shake(const uint32_t now) {
    int x = 0;
    int y = 0;
    if (arc_shake_started) {
        const uint32_t age = now - arc_shake_started;
        if (age >= arc_shake_ms) {
            arc_shake_started = 0;
        } else {
            const float left = 1.0f - (float) age / (float) arc_shake_ms;
            x = (int) lroundf(sinf((float) age * 0.11f) * (float) arc_shake_strength * left);
            y = (int) lroundf(cosf((float) age * 0.13f) * (float) arc_shake_strength * left * 0.6f);
        }
    }
    if (x != arc_shake_x) {
        lv_obj_set_style_translate_x(arc_root, x, MU_OBJ_MAIN_DEFAULT);
        arc_shake_x = x;
    }
    if (y != arc_shake_y) {
        lv_obj_set_style_translate_y(arc_root, y, MU_OBJ_MAIN_DEFAULT);
        arc_shake_y = y;
    }
}

static void arc_update_sky(const uint32_t now, const float dt) {
    if (arc_frame % 3 == 0) {
        for (int index = 0; index < ARC_STAR_COUNT; index++) {
            arc_star *star = &arc_stars[index];
            if (star->base_opa < 8) continue;
            const float twinkle = 0.55f + 0.45f * sinf((float) now * star->speed + star->phase);
            arc_set_fill(star->obj, (int) ((float) star->base_opa * twinkle));
        }
    }

    const float breeze = (float) arc_scale(5) + (float) abs(arc_wind) * (float) arc_scale(5);
    const float direction = arc_wind < 0 ? -1.0f : 1.0f;
    for (int index = 0; index < ARC_CLOUD_COUNT; index++) {
        arc_cloud *cloud = &arc_clouds[index];
        if (lv_obj_has_flag(cloud->obj, LV_OBJ_FLAG_HIDDEN)) continue;
        cloud->x += direction * breeze * cloud->drift * dt;
        if (cloud->x > (float) arc_width) cloud->x = -(float) cloud->width;
        if (cloud->x < -(float) cloud->width) cloud->x = (float) arc_width;
        arc_place(cloud->obj, (int) cloud->x, cloud->y);
    }
}

static void arc_timer_tick(lv_timer_t *timer __attribute__((unused))) {
    const uint32_t now = lv_tick_get();
    float dt = arc_last_tick ? (float) (now - arc_last_tick) / 1000.0f : 0.0f;
    arc_last_tick = now;
    if (dt > 0.05f) dt = 0.05f;
    arc_frame++;

    arc_update_sky(now, dt);
    arc_update_walls(now, dt);
    arc_update_weather(now, dt);
    arc_update_particles(now, dt);
    arc_update_overlays(now);
    arc_update_shake(now);

    if (arc_screen == arc_screen_setup) {
        if (now - arc_setup_flip_at >= 2600) {
            arc_setup_flip_at = now;
            arc_setup_flip = (arc_setup_flip + 1) % 4;
            if (arc_setup_item() == arc_setup_player_one || arc_setup_item() == arc_setup_player_two
                || arc_setup_item() == arc_setup_world)
                arc_refresh_setup();
        }
        return;
    }
    arc_update_actors(now);
    arc_update_health(now, dt);
    if (arc_screen != arc_screen_play) return;

    const int busy = arc_shot_active || arc_explosion_active || arc_hazard_kind;
    if (arc_shot_active) arc_step_shot(now);
    if (arc_hazard_kind) arc_update_hazard(now);
    if (arc_explosion_active) arc_update_explosion(now);
    if (busy) return;

    arc_update_cpu(now);
    if (arc_is_aiming()) arc_update_aim(now);
}

static void arc_move_menu(const int direction) {
    if (arc_menu_count < 1) return;
    arc_setup_flip = 0;
    arc_setup_flip_at = lv_tick_get();
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
    const int variant_count = arc_mayo_known ? ARC_VARIANT_COUNT : ARC_VISIBLE_COUNT;

    switch (arc_setup_item()) {
        case arc_setup_players:
            arc_players = arc_players == 1 ? 2 : 1;
            if (arc_players == 2 && arc_variant_selected[1] == arc_variant_selected[0])
                arc_variant_selected[1] = arc_cycle(arc_variant_selected[1], 1, variant_count);
            break;
        case arc_setup_length:
            arc_best_of = 3 + arc_cycle((arc_best_of - 3) / 2, direction, 3) * 2;
            break;
        case arc_setup_world:
            arc_planet_selected = arc_cycle(arc_planet_selected, direction, ARC_PLANET_COUNT);
            arc_preview_world();
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
    arc_save();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "apps");
    mux_input_stop();
}

static void arc_select_menu(void) {
    switch (arc_screen) {
        case arc_screen_setup:
            if (arc_setup_item() == arc_setup_start) {
                if (arc_players == 1) arc_pick_cpu();
                play_sound(snd_confirm);
                arc_save();
                arc_begin_match();
            } else if (arc_setup_item() == arc_setup_quit) {
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

static void arc_handle_x(void) {
    if (arc_screen != arc_screen_play || !arc_is_aiming()) return;
    if (arc_players == 1 && arc_turn == 1) return;
    if (arc_extras_locked(arc_turn)) {
        arc_show_toast("Specials and defences unlock next turn");
        play_sound(snd_error);
        return;
    }

    const int current = arc_armed[arc_turn];
    int next = current;
    for (int step = 0; step < 3; step++) {
        next = (next + 1) % 3;
        if (next == 0 || (next == 1 && arc_ammo[arc_turn] > 0)
            || (next == 2 && arc_guard_ammo[arc_turn] > 0 && !arc_guard_active(arc_turn)))
            break;
    }

    if (next == current) {
        arc_show_toast("No specials left");
        play_sound(snd_error);
        return;
    }

    arc_armed[arc_turn] = next;
    arc_update_hud();
    play_sound(snd_option);
}

static int arc_repeat_step(void) {
    const uint32_t held = mux_input_tick() - arc_repeat_since;
    if (held > 1600) return 4;
    if (held > 700) return 2;
    return 1;
}

static void arc_adjust_angle(const int direction) {
    if (!arc_is_aiming()) return;
    if (arc_players == 1 && arc_turn == 1) return;
    const int angle = arc_clamp(arc_angle[arc_turn] + direction, 15, 80);
    if (angle == arc_angle[arc_turn]) return;
    arc_angle[arc_turn] = angle;
    arc_update_hud();
    play_sound(snd_navigate);
}

static void arc_adjust_power(const int direction) {
    if (!arc_is_aiming()) return;
    if (arc_players == 1 && arc_turn == 1) return;
    const int power = arc_clamp(arc_power[arc_turn] + direction, 20, 100);
    if (power == arc_power[arc_turn]) return;
    arc_power[arc_turn] = power;
    arc_update_hud();
    play_sound(snd_navigate);
}

static void arc_up(const int step) {
    if (arc_screen == arc_screen_play)
        arc_adjust_power(step);
    else
        arc_move_menu(-1);
}

static void arc_down(const int step) {
    if (arc_screen == arc_screen_play)
        arc_adjust_power(-step);
    else
        arc_move_menu(1);
}

static void arc_left(const int step) {
    if (arc_screen == arc_screen_play)
        arc_adjust_angle(arc_turn ? -step : step);
    else if (arc_screen == arc_screen_setup)
        arc_change_setup(-1);
}

static void arc_right(const int step) {
    if (arc_screen == arc_screen_play)
        arc_adjust_angle(arc_turn ? step : -step);
    else if (arc_screen == arc_screen_setup)
        arc_change_setup(1);
}

static void arc_press_up(void) {
    arc_repeat_since = mux_input_tick();
    arc_up(1);
}

static void arc_press_down(void) {
    arc_repeat_since = mux_input_tick();
    arc_down(1);
}

static void arc_press_left(void) {
    arc_repeat_since = mux_input_tick();
    arc_left(1);
}

static void arc_press_right(void) {
    arc_repeat_since = mux_input_tick();
    arc_right(1);
}

static void arc_hold_up(void) {
    arc_up(arc_repeat_step());
}

static void arc_hold_down(void) {
    arc_down(arc_repeat_step());
}

static void arc_hold_left(void) {
    arc_left(arc_repeat_step());
}

static void arc_hold_right(void) {
    arc_right(arc_repeat_step());
}

static void arc_handle_menu(void) {
    if (arc_screen == arc_screen_play) arc_show_pause();
}

static void arc_mayo_press(void) {
    arc_mayo_player = -1;
    if (arc_screen == arc_screen_setup && arc_setup_item() == arc_setup_player_one)
        arc_mayo_player = 0;
    else if (arc_screen == arc_screen_setup && arc_players == 2 && arc_setup_item() == arc_setup_player_two)
        arc_mayo_player = 1;
    arc_mayo_since = arc_mayo_player >= 0 ? mux_input_tick() : 0;
}

static void arc_mayo_hold(void) {
    if (!arc_mayo_since || arc_mayo_player < 0) return;
    if (mux_input_tick() - arc_mayo_since < ARC_SECRET_MS) return;
    arc_variant_selected[arc_mayo_player] = ARC_VARIANT_COUNT - 1;
    arc_mayo_known = 1;
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
    lv_obj_set_style_bg_opa(arc_menu, theme.dialogue.background_alpha * 78 / 100, MU_OBJ_MAIN_DEFAULT);
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

    arc_menu_rule = lv_obj_create(arc_menu);
    arc_style_clear(arc_menu_rule);
    lv_obj_set_style_bg_color(arc_menu_rule, lv_color_hex(theme.dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(arc_menu_rule, theme.dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);

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
    arc_actor_size_w[player] = arc_actor_w;
    lv_obj_set_style_radius(arc_actor[player], arc_actor_w / 4, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(arc_actor[player], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(arc_actor[player], lv_color_hex(theme.header.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(arc_actor[player], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(arc_actor[player], arc_clamp(arc_scale(2), 1, 4), MU_OBJ_MAIN_DEFAULT);
    arc_set_hidden(arc_actor[player], 1);

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

static lv_obj_t *arc_canvas(lv_color32_t *buf, const int width, const int height) {
    lv_obj_t *canvas = lv_canvas_create(arc_root);
    lv_canvas_set_buffer(canvas, buf, width, height, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    mu_img_no_shadow(canvas);
    return canvas;
}

static int arc_create_sky(void) {
    arc_sky = lv_obj_create(arc_root);
    arc_style_clear(arc_sky);
    lv_obj_set_size(arc_sky, arc_pad_left + arc_width + arc_pad_right, arc_pad_top + arc_height + arc_pad_bottom);
    lv_obj_set_pos(arc_sky, -arc_pad_left, -arc_pad_top);
    lv_obj_set_style_bg_opa(arc_sky, 170, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_grad_dir(arc_sky, LV_GRAD_DIR_VER, MU_OBJ_MAIN_DEFAULT);

    for (int index = 0; index < ARC_STAR_COUNT; index++) {
        arc_star *star = &arc_stars[index];
        star->size = arc_clamp(arc_scale(1 + arc_rand(3)), 1, 4);
        star->x = arc_rand(arc_width);
        star->y = arc_rand(arc_pad_top + arc_height * 45 / 100) - arc_pad_top;
        star->phase = arc_randf() * 6.28f;
        star->speed = 0.0015f + arc_randf() * 0.004f;
        star->obj = arc_dot(arc_root, 0xFFFFFF);
        lv_obj_set_size(star->obj, star->size, star->size);
        lv_obj_set_pos(star->obj, star->x, star->y);
    }

    arc_body_box = arc_scale(50) * 3;
    arc_body_buf = calloc((size_t) arc_body_box * (size_t) arc_body_box, sizeof(*arc_body_buf));
    if (!arc_body_buf) return 0;
    arc_body = arc_canvas(arc_body_buf, arc_body_box, arc_body_box);
    arc_set_hidden(arc_body, 1);

    arc_sun_box = arc_scale(64) * 3;
    arc_sun_buf = calloc((size_t) arc_sun_box * (size_t) arc_sun_box, sizeof(*arc_sun_buf));
    if (!arc_sun_buf) return 0;
    arc_sun = arc_canvas(arc_sun_buf, arc_sun_box, arc_sun_box);

    for (int index = 0; index < ARC_CLOUD_COUNT; index++) {
        arc_cloud *cloud = &arc_clouds[index];
        cloud->width = arc_width / 5 + arc_rand(arc_width / 7 + 1);
        cloud->height = arc_clamp(cloud->width * 55 / 100, 12, arc_scale(90));
        cloud->x = (float) (index * arc_width / ARC_CLOUD_COUNT + arc_rand(arc_width / 8 + 1));
        cloud->y = arc_hud_bottom + arc_rand(arc_height / 4 + 1);
        cloud->drift = 0.6f + arc_randf() * 0.8f;
        cloud->seed = (uint32_t) random();
        cloud->buf = calloc((size_t) cloud->width * (size_t) cloud->height, sizeof(*cloud->buf));
        if (!cloud->buf) return 0;
        cloud->obj = arc_canvas(cloud->buf, cloud->width, cloud->height);
        lv_obj_set_pos(cloud->obj, (int) cloud->x, cloud->y);
    }

    for (int index = 0; index < ARC_DROP_COUNT; index++)
        arc_drops[index].obj = arc_dot(arc_root, 0xFFFFFF);

    return 1;
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

    lv_area_t area;
    lv_obj_get_coords(arc_root, &area);
    arc_pad_left = arc_clamp(area.x1, 0, device.mux.width);
    arc_pad_top = arc_clamp(area.y1, 0, device.mux.height);
    arc_pad_right = arc_clamp(device.mux.width - arc_pad_left - arc_width, 0, device.mux.width);
    arc_pad_bottom = arc_clamp(device.mux.height - arc_pad_top - arc_height, 0, device.mux.height);
    arc_canvas_w = arc_pad_left + arc_width + arc_pad_right;
    arc_canvas_h = arc_height + arc_pad_bottom;
    lv_obj_add_flag(arc_root, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_bg_opa(ui_pnl_header, theme.header.background_alpha * 45 / 100, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_footer, theme.footer.background_alpha * 45 / 100, MU_OBJ_MAIN_DEFAULT);

    arc_ground = calloc((size_t) arc_width, sizeof(*arc_ground));
    arc_scorch = calloc((size_t) arc_width, sizeof(*arc_scorch));
    arc_stain = calloc((size_t) arc_width, sizeof(*arc_stain));
    arc_stain_colour = calloc((size_t) arc_width, sizeof(*arc_stain_colour));
    arc_row_colour = calloc((size_t) arc_canvas_h, sizeof(*arc_row_colour));
    arc_terrain_buf = calloc((size_t) arc_canvas_w * (size_t) arc_canvas_h, sizeof(*arc_terrain_buf));
    if (!arc_ground || !arc_scorch || !arc_stain || !arc_stain_colour || !arc_row_colour || !arc_terrain_buf) return 0;

    arc_ground_top = arc_clamp(arc_height * 38 / 100, arc_scale(90), arc_height - arc_scale(120));
    arc_actor_w = arc_clamp(arc_width / 18, 30, 58);
    arc_actor_h = arc_clamp(arc_height / 12, 28, 52);

    const int hud_padding = arc_clamp(arc_scale(10), 8, 14);
    const int bar_height = arc_clamp(arc_scale(12), 8, 16);
    const int bar_gap = arc_clamp(arc_scale(6), 4, 8);
    const int hud_width = arc_width - 16;

    arc_hud = lv_obj_create(arc_root);
    arc_style_clear(arc_hud);
    arc_hud_round = arc_label(arc_hud, "", theme.header.text);
    char round_sample[64];
    snprintf(round_sample, sizeof(round_sample), "%s 99", TRS("Round"));
    const int round_width = arc_text_width(arc_hud_round, round_sample) + hud_padding * 2;
    const int line_height = lv_font_get_line_height(lv_obj_get_style_text_font(arc_hud_round, LV_PART_MAIN));
    const int top_row = line_height > bar_height ? line_height : bar_height;
    const int hud_height = hud_padding * 2 + top_row + (bar_gap + line_height) * 2;
    arc_hud_bottom = 6 + hud_height;

    if (!arc_create_sky()) return 0;

    arc_terrain = lv_canvas_create(arc_root);
    lv_canvas_set_buffer(arc_terrain, arc_terrain_buf, arc_canvas_w, arc_canvas_h, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_obj_clear_flag(arc_terrain, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(arc_terrain, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(arc_terrain, -arc_pad_left, 0);
    mu_img_no_shadow(arc_terrain);

    for (int index = 0; index < ARC_PATH_DOTS; index++) {
        arc_path[index] = arc_dot(arc_root, theme.header.text);
        const int size = arc_clamp(arc_scale(4), 3, 6);
        lv_obj_set_size(arc_path[index], size, size);
        lv_obj_set_style_bg_opa(arc_path[index], 70, MU_OBJ_MAIN_DEFAULT);
    }

    for (int index = 0; index < ARC_AIM_DOTS; index++) {
        arc_aim[index] = arc_dot(arc_root, arc_focus_colour());
        lv_obj_set_size(arc_aim[index], arc_aim_size(), arc_aim_size());
        lv_obj_set_style_border_color(
            arc_aim[index], lv_color_hex(arc_mix(arc_focus_colour(), 0x000000, 70)), MU_OBJ_MAIN_DEFAULT
        );
        lv_obj_set_style_border_opa(arc_aim[index], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_width(arc_aim[index], arc_aim_size() >= 9 ? 2 : 1, MU_OBJ_MAIN_DEFAULT);
    }

    arc_create_actor(0);
    arc_create_actor(1);

    for (int player = 0; player < 2; player++) {
        arc_shield_obj[player] = arc_dot(arc_root, 0xFFFFFF);
        lv_obj_set_style_border_width(arc_shield_obj[player], arc_clamp(arc_scale(2), 2, 3), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_size(arc_shield_obj[player], arc_actor_w * 17 / 10, arc_actor_w * 17 / 10);
    }

    for (int index = 0; index < ARC_TRAIL_COUNT; index++)
        arc_trail[index] = arc_dot(arc_root, arc_focus_colour());

    for (int index = 0; index < ARC_SHELL_COUNT; index++) {
        arc_shell_obj[index] = arc_dot(arc_root, arc_focus_colour());
        lv_obj_set_style_border_color(arc_shell_obj[index], lv_color_hex(theme.header.text), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_opa(arc_shell_obj[index], LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_width(arc_shell_obj[index], 1, MU_OBJ_MAIN_DEFAULT);
    }

    arc_wall_w = arc_clamp(arc_scale(12), 8, 20);
    arc_wall_h = arc_pad_top + arc_height + arc_pad_bottom;
    for (int side = 0; side < 2; side++) {
        arc_wall_buf[side] = calloc((size_t) arc_wall_w * (size_t) arc_wall_h, sizeof(*arc_wall_buf[side]));
        if (!arc_wall_buf[side]) return 0;
        arc_wall_obj[side] = arc_canvas(arc_wall_buf[side], arc_wall_w, arc_wall_h);
        arc_set_hidden(arc_wall_obj[side], 1);
        for (int index = 0; index < ARC_WALL_SPARKS; index++) {
            arc_wall_spark[side][index] = arc_dot(arc_root, 0xE6F6FF);
            const int size = arc_clamp(arc_scale(3), 2, 5);
            lv_obj_set_size(arc_wall_spark[side][index], size, size);
        }
    }

    arc_explosion = arc_dot(arc_root, arc_focus_colour());

    arc_blast_ring = lv_obj_create(arc_root);
    arc_style_clear(arc_blast_ring);
    lv_obj_set_style_radius(arc_blast_ring, LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(arc_blast_ring, lv_color_hex(arc_focus_colour()), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(arc_blast_ring, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(arc_blast_ring, arc_clamp(arc_scale(3), 2, 5), MU_OBJ_MAIN_DEFAULT);
    arc_set_hidden(arc_blast_ring, 1);

    for (int index = 0; index < ARC_PARTICLE_COUNT; index++)
        arc_particles[index].obj = arc_dot(arc_root, arc_focus_colour());

    for (int player = 0; player < 2; player++) {
        arc_damage_label[player] = arc_label(arc_root, "", arc_focus_colour());
        lv_shadow_zone_register(
            arc_damage_label[player], lv_color_black(), LV_OPA_70, 1, 1, lv_color_black(), LV_OPA_70, 1, 1
        );
        arc_set_hidden(arc_damage_label[player], 1);
    }

    lv_obj_move_foreground(arc_hud);
    lv_obj_set_width(arc_hud, hud_width);
    lv_obj_set_height(arc_hud, hud_height);
    lv_obj_set_pos(arc_hud, 8, 6);
    lv_obj_set_style_bg_color(arc_hud, lv_color_hex(theme.header.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(arc_hud, arc_clamp(theme.header.background_alpha * 3 / 4, 96, 220), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(arc_hud, theme.dialogue.radius.main, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_width(arc_hud_round, round_width);
    lv_obj_align(arc_hud_round, LV_ALIGN_TOP_MID, 0, hud_padding + (top_row - line_height) / 2);
    arc_set_hidden(arc_hud, 1);

    const int bar_width = (hud_width - hud_padding * 2 - round_width) / 2;
    for (int player = 0; player < 2; player++) {
        for (int layer = 0; layer < 2; layer++) {
            lv_obj_t *bar = lv_bar_create(arc_hud);
            lv_obj_set_size(bar, bar_width, bar_height);
            lv_obj_set_pos(
                bar, player ? hud_width - hud_padding - bar_width : hud_padding,
                hud_padding + (top_row - bar_height) / 2
            );
            lv_bar_set_mode(bar, player ? LV_BAR_MODE_RANGE : LV_BAR_MODE_NORMAL);
            lv_bar_set_range(bar, 0, 100);
            lv_obj_set_style_radius(bar, bar_height / 3, MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_radius(bar, bar_height / 3, LV_PART_INDICATOR | MU_OBJ_MAIN_DEFAULT);
            lv_obj_set_style_anim_time(bar, 0, MU_OBJ_MAIN_DEFAULT);
            if (layer == 0) {
                lv_obj_set_style_bg_color(bar, lv_color_hex(theme.footer.background), MU_OBJ_MAIN_DEFAULT);
                lv_obj_set_style_bg_opa(bar, LV_OPA_70, MU_OBJ_MAIN_DEFAULT);
                lv_obj_set_style_bg_color(
                    bar, lv_color_hex(theme.header.text), LV_PART_INDICATOR | MU_OBJ_MAIN_DEFAULT
                );
                lv_obj_set_style_bg_opa(bar, LV_OPA_80, LV_PART_INDICATOR | MU_OBJ_MAIN_DEFAULT);
                arc_health_lag_bar[player] = bar;
            } else {
                lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
                lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR | MU_OBJ_MAIN_DEFAULT);
                arc_health_bar[player] = bar;
            }
        }
    }

    arc_hud_score = arc_label(arc_hud, "", theme.header.text);
    lv_obj_set_width(arc_hud_score, LV_PCT(100));
    lv_obj_align(arc_hud_score, LV_ALIGN_TOP_MID, 0, hud_padding + top_row + bar_gap);
    arc_hud_values = arc_label(arc_hud, "", arc_focus_colour());
    lv_obj_set_width(arc_hud_values, LV_PCT(100));
    lv_obj_align(arc_hud_values, LV_ALIGN_TOP_MID, 0, hud_padding + top_row + bar_gap * 2 + line_height);

    arc_toast = lv_obj_create(arc_root);
    arc_style_clear(arc_toast);
    lv_obj_set_size(arc_toast, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(arc_toast, arc_clamp(arc_scale(16), 10, 24), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_ver(arc_toast, arc_clamp(arc_scale(8), 5, 12), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(arc_toast, lv_color_hex(theme.header.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(arc_toast, arc_clamp(theme.header.background_alpha, 160, 235), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(arc_toast, lv_color_hex(arc_focus_colour()), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(arc_toast, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(arc_toast, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(arc_toast, LV_RADIUS_CIRCLE, MU_OBJ_MAIN_DEFAULT);
    arc_toast_label = arc_label(arc_toast, "", arc_focus_colour());
    arc_set_hidden(arc_toast, 1);

    arc_bubble = lv_obj_create(arc_root);
    arc_style_clear(arc_bubble);
    lv_obj_set_style_bg_color(arc_bubble, lv_color_hex(theme.dialogue.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(arc_bubble, arc_clamp(theme.dialogue.background_alpha, 200, 255), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(arc_bubble, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(arc_bubble, 2, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(arc_bubble, arc_clamp(arc_scale(12), 6, 16), MU_OBJ_MAIN_DEFAULT);
    arc_bubble_label = arc_label(arc_bubble, "", theme.dialogue.content);
    lv_label_set_long_mode(arc_bubble_label, LV_LABEL_LONG_WRAP);
    lv_obj_center(arc_bubble_label);
    arc_set_hidden(arc_bubble, 1);

    for (int index = 0; index < ARC_BOLT_DOTS; index++) {
        arc_bolt[index] = arc_dot(arc_root, 0xF6F2FF);
        lv_obj_set_style_border_color(arc_bolt[index], lv_color_hex(0x9FB8FF), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_width(arc_bolt[index], 1, MU_OBJ_MAIN_DEFAULT);
    }

    arc_create_menu();
    lv_obj_move_foreground(arc_menu);

    arc_preview_world();
    return 1;
}

static void arc_cleanup(void) {
    if (arc_timer) {
        lv_timer_del(arc_timer);
        arc_timer = NULL;
    }
    if (arc_terrain) canvas_invalidate_gpu_texture(arc_terrain);
    if (arc_sun) canvas_invalidate_gpu_texture(arc_sun);
    if (arc_body) canvas_invalidate_gpu_texture(arc_body);
    for (int side = 0; side < 2; side++)
        if (arc_wall_obj[side]) canvas_invalidate_gpu_texture(arc_wall_obj[side]);
    for (int index = 0; index < ARC_CLOUD_COUNT; index++) {
        if (arc_clouds[index].obj) canvas_invalidate_gpu_texture(arc_clouds[index].obj);
    }
    if (arc_root && lv_obj_is_valid(arc_root)) lv_obj_del(arc_root);
    arc_root = NULL;
    arc_terrain = NULL;
    free(arc_terrain_buf);
    arc_terrain_buf = NULL;
    arc_sun = NULL;
    free(arc_sun_buf);
    arc_sun_buf = NULL;
    arc_body = NULL;
    free(arc_body_buf);
    arc_body_buf = NULL;
    memset(arc_bolt, 0, sizeof(arc_bolt));
    arc_hazard_kind = arc_hazard_none;
    arc_shake_ms = ARC_SHAKE_MS;
    for (int index = 0; index < ARC_CLOUD_COUNT; index++)
        free(arc_clouds[index].buf);
    memset(arc_clouds, 0, sizeof(arc_clouds));
    memset(arc_drops, 0, sizeof(arc_drops));
    arc_drop_level = 0;
    arc_meteor_vx = 0.0f;
    free(arc_ground);
    arc_ground = NULL;
    free(arc_scorch);
    arc_scorch = NULL;
    free(arc_stain);
    arc_stain = NULL;
    free(arc_stain_colour);
    arc_stain_colour = NULL;
    free(arc_row_colour);
    arc_row_colour = NULL;
    arc_last_tick = 0;
    arc_toast_started = 0;
    arc_bubble_started = 0;
    arc_shake_started = 0;
    arc_shake_x = 0;
    arc_shake_y = 0;
    for (int player = 0; player < 2; player++) {
        arc_actor_shift_x[player] = 0;
        arc_actor_shift_y[player] = 0;
        arc_actor_flash[player] = -1;
        arc_damage_started[player] = 0;
    }
    memset(arc_particles, 0, sizeof(arc_particles));
    for (int side = 0; side < 2; side++) {
        arc_wall_obj[side] = NULL;
        free(arc_wall_buf[side]);
        arc_wall_buf[side] = NULL;
        arc_wall_shift[side] = 0;
        arc_wall_hit_at[side] = 0;
        arc_wall_pending[side] = 0;
    }
    memset(arc_wall_spark, 0, sizeof(arc_wall_spark));
    arc_actor_fall_at[0] = 0;
    arc_actor_fall_at[1] = 0;
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
    arc_load();

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
                [mux_input_x] = arc_handle_x,
                [mux_input_y] = arc_mayo_press,
                [mux_input_dpad_up] = arc_press_up,
                [mux_input_dpad_down] = arc_press_down,
                [mux_input_dpad_left] = arc_press_left,
                [mux_input_dpad_right] = arc_press_right,
            },
        .hold_handler =
            {
                [mux_input_y] = arc_mayo_hold,
                [mux_input_dpad_up] = arc_hold_up,
                [mux_input_dpad_down] = arc_hold_down,
                [mux_input_dpad_left] = arc_hold_left,
                [mux_input_dpad_right] = arc_hold_right,
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
