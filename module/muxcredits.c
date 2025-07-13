#include "muxshare.h"
#include "ui/ui_muxcredits.h"
#include "../lvgl/src/drivers/display/sdl.h"

typedef struct {
    lv_obj_t *element;
    int delay;
} CreditElement;

static void timeout_task() {
    close_input();
    safe_quit(0);
    mux_input_stop();
}

static void trigger_timeout() {
    if (TEST_IMAGE) timeout_task();
}

static void handle_b() {
    if (!config.BOOT.FACTORY_RESET) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "credit");
        timeout_task();
    }
}

static int anim_sequence() {
    CreditElement anim_elements[] = {
            {ui_conStart,     -1250},
            {ui_conOfficial,  12250},
            {ui_conWizard,    23250},
            {ui_conHeroOne,   34250},
            {ui_conHeroTwo,   45250},
            {ui_conKnightOne, 56250},
            {ui_conKnightTwo, 67250},
            {ui_conContrib,   78250},
            {ui_conSpecial,   89250},
            {ui_conKofi,      101250},
            {ui_conMusic,     112250}
    };

    size_t count = sizeof(anim_elements) / sizeof(anim_elements[0]);

    for (size_t i = 0; i < count; i++) {
        animFade_Animation(anim_elements[i].element, anim_elements[i].delay);
    }

    return anim_elements[count - 1].delay + 22250;
}

int main(void) {
    init_module("muxcredits");

    load_device(&device);
    load_config(&config);

    init_theme(0, 0);
    init_display(0);

    const lv_font_t *header_font = (strcasecmp(device.DEVICE.NAME, "tui-brick") == 0)
                                   ? &ui_font_NotoSansBigHD
                                   : &ui_font_NotoSansBig;

    init_muxcredits(header_font);
    load_font_text(ui_scrCredits);

    int anim_duration = anim_sequence();
    if (config.BOOT.FACTORY_RESET) {
        lv_timer_create(timeout_task, anim_duration, NULL);
    } else {
        lv_timer_create(handle_b, anim_duration, NULL);
    }

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
