#include "ui_muxshare.h"
#include "ui_muxnetadv.h"

#define NETADV(NAME, ENUM, UDATA)    \
    lv_obj_t *ui_pnl##NAME##_netadv; \
    lv_obj_t *ui_lbl##NAME##_netadv; \
    lv_obj_t *ui_ico##NAME##_netadv; \
    lv_obj_t *ui_dro##NAME##_netadv;

NETADV_ELEMENTS
#undef NETADV

void init_muxnetadv(lv_obj_t *ui_pnlContent) {
#define NETADV(NAME, ENUM, UDATA) CREATE_OPTION_ITEM(netadv, NAME);
    NETADV_ELEMENTS
#undef NETADV
}

int wait_retry_int[] = {
        1, 3, 5, 7, 10, 15, 20, 25, 30
};

char *wait_retry_str[] = {
        "1", "3", "5", "7", "10", "15", "20", "25", "30"
};
