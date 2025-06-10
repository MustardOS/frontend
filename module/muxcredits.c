#include "muxshare.h"
#include "ui/ui_muxcredits.h"
#include "../lvgl/src/drivers/display/sdl.h"

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

int main(void) {
    init_module("muxcredits");

    load_device(&device);
    load_config(&config);

    init_theme(0, 0);
    init_display(0);

    const lv_font_t *header_font;
    if (strcasecmp(device.DEVICE.NAME, "tui-brick") == 0) {
        header_font = &ui_font_NotoSansBigHD;
    } else {
        header_font = &ui_font_NotoSansBig;
    }

    init_muxcredits(header_font);
    load_font_text(ui_scrCredits);

    animFade_Animation(ui_conStart, -1000);
    animFade_Animation(ui_conOfficial, 8000);
    animFade_Animation(ui_conWizard, 17000);
    animFade_Animation(ui_conHeroOne, 26000);
    animFade_Animation(ui_conHeroTwo, 35000);
    animFade_Animation(ui_conKnightOne, 44000);
    animFade_Animation(ui_conKnightTwo, 53000);
    animFade_Animation(ui_conSpecial, 62000);
    animFade_Animation(ui_conKofi, 71000);
    animFade_Animation(ui_conMusic, 80000);

    if (config.BOOT.FACTORY_RESET) lv_timer_create(timeout_task, 100000, NULL);

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
