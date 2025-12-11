#include "muxshare.h"
#include "ui/ui_muxcredits.h"
#include "../lvgl/src/drivers/display/sdl.h"

#define CREDIT_ITEM_COUNT 11
#define CREDIT_DELAY 4675
#define CREDIT_FADE 3250

static int credit_index = 0;

static lv_obj_t *credit_elements[CREDIT_ITEM_COUNT];

static void fade_anim_cb(void *obj, int32_t v);

static void fade_in_finish(lv_anim_t *a);

static void fade_in(lv_obj_t *obj);

static void fade_out(lv_obj_t *obj);

static void timeout_task(void) {
    close_input();
    safe_quit(0);
    mux_input_stop();
}

static void trigger_timeout(void) {
    if (TEST_IMAGE) timeout_task();
}

static void create_credit_elements_array(void) {
    credit_elements[0] = ui_conStart;
    credit_elements[1] = ui_conOfficial;
    credit_elements[2] = ui_conWizard;
    credit_elements[3] = ui_conHeroOne;
    credit_elements[4] = ui_conHeroTwo;
    credit_elements[5] = ui_conKnightOne;
    credit_elements[6] = ui_conKnightTwo;
    credit_elements[7] = ui_conContrib;
    credit_elements[8] = ui_conSpecial;
    credit_elements[9] = ui_conKofi;
    credit_elements[10] = ui_conMusic;
}

static void fade_anim_cb(void *obj, int32_t v) {
    lv_obj_set_style_opa(obj, v, 0);
}

static void fade_in_finish(lv_anim_t *a) {
    lv_obj_remove_local_style_prop(a->var, LV_STYLE_OPA, 0);
    fade_out(a->var);
}

static void fade_out_finish(lv_anim_t *a) {
    credit_index++;

    if (credit_index >= CREDIT_ITEM_COUNT) {
        if (config.BOOT.FACTORY_RESET) {
            timeout_task();
            return;
        }

        credit_index = 0;
    }

    fade_in(credit_elements[credit_index]);
}

static void fade_in(lv_obj_t *obj) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 0, LV_OPA_COVER);
    lv_anim_set_exec_cb(&a, fade_anim_cb);
    lv_anim_set_ready_cb(&a, fade_in_finish);
    lv_anim_set_time(&a, CREDIT_FADE);
    lv_anim_set_delay(&a, 0);
    lv_anim_start(&a);
}

static void fade_out(lv_obj_t *obj) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, lv_obj_get_style_opa(obj, 0), LV_OPA_TRANSP);
    lv_anim_set_exec_cb(&a, fade_anim_cb);
    lv_anim_set_ready_cb(&a, fade_out_finish);
    lv_anim_set_time(&a, CREDIT_FADE);
    lv_anim_set_delay(&a, CREDIT_DELAY);
    lv_anim_start(&a);
}

static void handle_b(void) {
    if (!config.BOOT.FACTORY_RESET) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "credit");
        timeout_task();
    }
}

int main(void) {
    const char *m = "muxcredits";
    set_process_name(m);
    init_module(m);

    load_device(&device);
    load_config(&config);

    init_theme(0, 0);
    init_display(0);

    const lv_font_t *header_font = (strcasecmp(device.BOARD.NAME, "tui-brick") == 0)
                                   ? &ui_font_NotoSansBigHD
                                   : &ui_font_NotoSansBig;

    init_muxcredits(header_font);
    load_font_text(ui_scrCredits);
    create_credit_elements_array();
    fade_in(ui_conStart);

    mux_input_options input_opts = {
            .press_handler = {
                    [MUX_INPUT_B] = handle_b
            },
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_START) | BIT(MUX_INPUT_MENU_SHORT),
                            .press_handler = trigger_timeout,
                    },
                    {
                            .type_mask = BIT(MUX_INPUT_START) | BIT(MUX_INPUT_MENU_LONG),
                            .press_handler = trigger_timeout,
                    },
            }
    };
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    sdl_cleanup();
    return 0;
}
