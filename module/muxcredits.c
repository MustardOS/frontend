#include "../lvgl/lvgl.h"
#include "ui/ui_muxcredits.h"
#include <string.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/config.h"
#include "../common/language.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/theme.h"

char *mux_module;

int msgbox_active = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;

lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

void timeout_task() {
    mux_input_stop();
}

void handle_quit(void) {
    mux_input_stop();
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);

    init_theme(0, 0);
    init_display();
    init_mux();

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

    if (!config.BOOT.FACTORY_RESET) write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "credit");

    lv_timer_create(timeout_task, 100000, NULL);

    mux_input_options input_opts = {
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_START),
                            .press_handler = handle_quit,
                    },
            }
    };
    init_input(&input_opts);
    mux_input_task(&input_opts);

    safe_quit(0);
    return 0;
}
