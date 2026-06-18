#pragma once

#include "../../lvgl/lvgl.h"

#define TSN_FADE_IN      0

#define TSN_SLIDE_RIGHT  1
#define TSN_SLIDE_LEFT   2
#define TSN_SLIDE_UP     3
#define TSN_SLIDE_DOWN   4

#define TSN_BOUNCE_RIGHT 5
#define TSN_BOUNCE_LEFT  6
#define TSN_BOUNCE_UP    7
#define TSN_BOUNCE_DOWN  8

#define TSN_SHOOT_RIGHT  9
#define TSN_SHOOT_LEFT   10
#define TSN_SHOOT_UP     11
#define TSN_SHOOT_DOWN   12

#define TSN_DISABLED     13

void transition_box_art_init(void (*on_scroll_stop)(void));

void transition_box_art_nav_activity(void);

void transition_box_art_key_released(void);

void transition_box_art_apply_in(int type);

void transition_box_art_destroy(void);
