#include "muxshare.h"
#include "muxcredits.h"
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

static void timeout_task() {
    safe_quit(0);
    mux_input_stop();
}

int muxcredits_main(int argc, char *argv[]) {
    (void) argc;

    snprintf(mux_module, sizeof(mux_module), "muxcredits");
    
        
    init_theme(0, 0);
    init_muxcredits();

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
                            .press_handler = timeout_task,
                    },
            }
    };
    init_input(&input_opts, false);
    mux_input_task(&input_opts);

    return 0;
}
