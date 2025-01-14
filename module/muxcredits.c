#include "../lvgl/lvgl.h"
#include "ui/ui_muxcredits.h"
#include <string.h>
#include <unistd.h>
#include "../common/common.h"
#include "../common/config.h"
#include "../common/language.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/theme.h"
#include "../common/input.h"
#include "../common/ui_common.h"

char *mux_module;
static int js_fd;
static int js_fd_sys;

int turbo_mode = 0;
int msgbox_active = 0;
int SD2_found = 0;
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

int main() {
    load_device(&device);
    load_config(&config);

    init_display();
    init_mux();

    animFade_Animation(ui_conStart, -1000);
    animScroll_Animation(ui_conScroll, 12000);
    animFade_Animation(ui_conSpecial, 65000);
    animFade_Animation(ui_conKofi, 75000);

    if (!config.BOOT.FACTORY_RESET) write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "credit");

    lv_timer_create(timeout_task, 90000, NULL);

    init_input(&js_fd, &js_fd_sys);
    init_timer(NULL, NULL, NULL);

    mux_input_options input_opts = {
            .gamepad_fd = js_fd,
            .system_fd = js_fd_sys,
            .max_idle_ms = IDLE_MS,
            .combo = {
                    {
                            .type_mask = BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_START),
                            .press_handler = handle_quit,
                    },
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);

    close(js_fd);
    close(js_fd_sys);

    return 0;
}
