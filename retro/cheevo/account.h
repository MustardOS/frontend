#pragma once

#include "cheevo.h"

typedef struct {
    int enabled;
    int hardcore;
    int unofficial;
    cheevo_notification_mode notifications;
    cheevo_achievement_sort achievement_sort;
    cheevo_achievement_view achievement_view;
    char username[128];
    char token[256];
} cheevo_account;

void cheevo_account_defaults(cheevo_account *account);
int cheevo_account_load(cheevo_account *account);
int cheevo_account_save(const cheevo_account *account);
void cheevo_account_delete(void);
