#pragma once

#include <stdint.h>

void menu_init(void);

int menu_active(void);

void menu_open(void);

void menu_open_contents(void);

void menu_close(void);

void menu_move(int direction);

void menu_shoulder(int direction);

void menu_change(int direction);

void menu_accept(void);

void menu_back(void);

int menu_action_x(void);

int menu_action_y(void);

void menu_refresh_values(void);

void menu_hotkey_feedback(int hotkey);

void menu_tick(void);

void menu_peek(int peeking);

int menu_dialogue_input(uint64_t edge);
