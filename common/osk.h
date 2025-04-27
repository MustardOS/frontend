#pragma once

#include <stdio.h>
#include <linux/input.h>

extern int key_show;
extern int key_curr;

extern lv_obj_t *key_entry;
extern lv_obj_t *num_entry;

extern const char *key_lower_map[];
extern const char *key_upper_map[];
extern const char *key_special_map[];
extern const char *key_number_map[];

void osk_handler(lv_event_t *e);

void init_osk(lv_obj_t *ui_pnlEntry, lv_obj_t *ui_txtEntry, bool include_numkey);

void reset_osk(lv_obj_t *osk);

void close_osk(lv_obj_t *osk, lv_group_t *ui, lv_obj_t *entry, lv_obj_t *panel);

void key_up();

void key_down();

void key_left();

void key_right();

void key_swap();

void key_backspace(lv_obj_t *entry);

void process_key_event(struct input_event *ev, lv_obj_t *entry);
