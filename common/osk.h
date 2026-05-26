#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <linux/input.h>
#include "../lvgl/lvgl.h"

extern int key_show;
extern int key_curr;
extern int key_map;

extern lv_obj_t *key_entry;
extern lv_obj_t *num_entry;
extern lv_obj_t *hex_entry;

extern const char *key_lower_map[];
extern const char *key_upper_map[];
extern const char *key_special_map[];
extern const char *key_number_map[];
extern const char *key_hex_map[];

// include_numpad: 0 = alpha only, 1 = w/numpad, 2 = w/hexpad
void init_osk(lv_obj_t *ui_pnlEntry, lv_obj_t *ui_txtEntry, int include_numpad, bool is_password, uint16_t max_len);

void osk_handler(lv_event_t *e);

void reset_osk(lv_obj_t *osk);

void close_osk(lv_obj_t *osk, lv_group_t *ui, lv_obj_t *entry, lv_obj_t *panel);

void key_up(void);

void key_down(void);

void key_left(void);

void key_right(void);

void key_swap(void);

void key_swap_back(void);

void key_backspace(lv_obj_t *entry);

void key_space(lv_obj_t *entry);

void osk_show(lv_obj_t *panel);

void osk_hide(lv_obj_t *panel);

void osk_refresh_labels(void);

void process_key_event(struct input_event *ev, lv_obj_t *entry);

char get_shifted_char(uint16_t key);
