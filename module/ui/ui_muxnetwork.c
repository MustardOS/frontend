#include "ui_muxnetwork.h"
#include "../../common/device.h"

lv_obj_t *ui_scrNetwork;

lv_obj_t *ui_pnlEnable;
lv_obj_t *ui_pnlIdentifier;
lv_obj_t *ui_pnlPassword;
lv_obj_t *ui_pnlType;
lv_obj_t *ui_pnlAddress;
lv_obj_t *ui_pnlSubnet;
lv_obj_t *ui_pnlGateway;
lv_obj_t *ui_pnlDNS;
lv_obj_t *ui_pnlConnect;

lv_obj_t *ui_lblEnable;
lv_obj_t *ui_lblIdentifier;
lv_obj_t *ui_lblPassword;
lv_obj_t *ui_lblType;
lv_obj_t *ui_lblAddress;
lv_obj_t *ui_lblSubnet;
lv_obj_t *ui_lblGateway;
lv_obj_t *ui_lblDNS;
lv_obj_t *ui_lblConnect;

lv_obj_t *ui_icoEnable;
lv_obj_t *ui_icoIdentifier;
lv_obj_t *ui_icoPassword;
lv_obj_t *ui_icoType;
lv_obj_t *ui_icoAddress;
lv_obj_t *ui_icoSubnet;
lv_obj_t *ui_icoGateway;
lv_obj_t *ui_icoDNS;
lv_obj_t *ui_icoConnect;

lv_obj_t *ui_lblEnableValue;
lv_obj_t *ui_lblIdentifierValue;
lv_obj_t *ui_lblPasswordValue;
lv_obj_t *ui_lblTypeValue;
lv_obj_t *ui_lblAddressValue;
lv_obj_t *ui_lblSubnetValue;
lv_obj_t *ui_lblGatewayValue;
lv_obj_t *ui_lblDNSValue;
lv_obj_t *ui_lblConnectValue;

lv_obj_t *ui_pnlEntry;
lv_obj_t *ui_txtEntry;

void ui_init(lv_obj_t *ui_screen, lv_obj_t *ui_pnlContent, struct theme_config *theme) {
    ui_pnlEnable = lv_obj_create(ui_pnlContent);
    ui_pnlIdentifier = lv_obj_create(ui_pnlContent);
    ui_pnlPassword = lv_obj_create(ui_pnlContent);
    ui_pnlType = lv_obj_create(ui_pnlContent);
    ui_pnlAddress = lv_obj_create(ui_pnlContent);
    ui_pnlSubnet = lv_obj_create(ui_pnlContent);
    ui_pnlGateway = lv_obj_create(ui_pnlContent);
    ui_pnlDNS = lv_obj_create(ui_pnlContent);
    ui_pnlConnect = lv_obj_create(ui_pnlContent);

    ui_lblEnable = lv_label_create(ui_pnlEnable);
    ui_lblIdentifier = lv_label_create(ui_pnlIdentifier);
    ui_lblPassword = lv_label_create(ui_pnlPassword);
    ui_lblType = lv_label_create(ui_pnlType);
    ui_lblAddress = lv_label_create(ui_pnlAddress);
    ui_lblSubnet = lv_label_create(ui_pnlSubnet);
    ui_lblGateway = lv_label_create(ui_pnlGateway);
    ui_lblDNS = lv_label_create(ui_pnlDNS);
    ui_lblConnect = lv_label_create(ui_pnlConnect);

    ui_icoEnable = lv_img_create(ui_pnlEnable);
    ui_icoIdentifier = lv_img_create(ui_pnlIdentifier);
    ui_icoPassword = lv_img_create(ui_pnlPassword);
    ui_icoType = lv_img_create(ui_pnlType);
    ui_icoAddress = lv_img_create(ui_pnlAddress);
    ui_icoSubnet = lv_img_create(ui_pnlSubnet);
    ui_icoGateway = lv_img_create(ui_pnlGateway);
    ui_icoDNS = lv_img_create(ui_pnlDNS);
    ui_icoConnect = lv_img_create(ui_pnlConnect);

    ui_lblEnableValue = lv_label_create(ui_pnlEnable);
    ui_lblIdentifierValue = lv_label_create(ui_pnlIdentifier);
    ui_lblPasswordValue = lv_label_create(ui_pnlPassword);
    ui_lblTypeValue = lv_label_create(ui_pnlType);
    ui_lblAddressValue = lv_label_create(ui_pnlAddress);
    ui_lblSubnetValue = lv_label_create(ui_pnlSubnet);
    ui_lblGatewayValue = lv_label_create(ui_pnlGateway);
    ui_lblDNSValue = lv_label_create(ui_pnlDNS);
    ui_lblConnectValue = lv_label_create(ui_pnlConnect);

    ui_pnlEntry = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnlEntry, device.MUX.WIDTH);
    lv_obj_set_height(ui_pnlEntry, device.MUX.HEIGHT);
    lv_obj_set_align(ui_pnlEntry, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnlEntry, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnlEntry, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnlEntry, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnlEntry, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnlEntry, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnlEntry, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_pnlEntry, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnlEntry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_txtEntry = lv_textarea_create(ui_pnlEntry);
    lv_obj_set_width(ui_txtEntry, device.MUX.WIDTH * 5 / 6);
    lv_obj_set_height(ui_txtEntry, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txtEntry, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txtEntry, 1024);
    lv_textarea_set_one_line(ui_txtEntry, true);
    lv_obj_set_style_radius(ui_txtEntry, theme->OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_txtEntry, lv_color_hex(theme->OSK.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_txtEntry, theme->OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_txtEntry, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
}
