#include "ui_muxshare.h"
#include "ui_muxnetadv.h"

#define NETADV(NAME, UDATA)                                                                                            \
    lv_obj_t *ui_pnl_##NAME##_netadv;                                                                                  \
    lv_obj_t *ui_lbl_##NAME##_netadv;                                                                                  \
    lv_obj_t *ui_ico_##NAME##_netadv;                                                                                  \
    lv_obj_t *ui_dro_##NAME##_netadv;

NETADV_ELEMENTS
#undef NETADV

void init_muxnetadv(lv_obj_t *ui_pnl_content) {
#define NETADV(NAME, UDATA) CREATE_OPTION_ITEM(netadv, NAME);
    NETADV_ELEMENTS
#undef NETADV
}

int wait_retry_int[] = {1, 3, 5, 7, 10, 15, 20, 25, 30};

char *wait_retry_str[] = {"1", "3", "5", "7", "10", "15", "20", "25", "30"};
