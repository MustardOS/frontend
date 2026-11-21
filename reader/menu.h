#pragma once

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

void menu_refresh_values(void);
