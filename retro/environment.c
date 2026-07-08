#include <stdarg.h>
#include <stdio.h>
#include "../common/ui/common.h"
#include "../common/init.h"
#include "../common/log.h"
#include "muxretro.h"
#include "options.h"
#include "paths.h"
#include "rumble.h"
#include "vfs.h"

static enum retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_0RGB1555;

static const struct retro_disk_control_callback *disk_control_cb = NULL;
static const struct retro_disk_control_ext_callback *disk_control_ext_cb = NULL;

enum retro_pixel_format mux_retro_get_pixel_format(void) {
    return pixel_format;
}

int mux_retro_disk_get_num_images(void) {
    if (disk_control_ext_cb && disk_control_ext_cb->get_num_images) return (int) disk_control_ext_cb->get_num_images();
    if (disk_control_cb && disk_control_cb->get_num_images) return (int) disk_control_cb->get_num_images();
    return 0;
}

unsigned mux_retro_disk_get_image_index(void) {
    if (disk_control_ext_cb && disk_control_ext_cb->get_image_index) return disk_control_ext_cb->get_image_index();
    if (disk_control_cb && disk_control_cb->get_image_index) return disk_control_cb->get_image_index();
    return 0;
}

bool mux_retro_disk_set_image_index(const unsigned index) {
    if (disk_control_ext_cb && disk_control_ext_cb->set_image_index) return disk_control_ext_cb->set_image_index(index);
    if (disk_control_cb && disk_control_cb->set_image_index) return disk_control_cb->set_image_index(index);
    return false;
}

bool mux_retro_disk_set_eject_state(const bool ejected) {
    if (disk_control_ext_cb && disk_control_ext_cb->set_eject_state) {
        return disk_control_ext_cb->set_eject_state(ejected);
    }
    if (disk_control_cb && disk_control_cb->set_eject_state) return disk_control_cb->set_eject_state(ejected);
    return false;
}

bool mux_retro_disk_get_eject_state(void) {
    if (disk_control_ext_cb && disk_control_ext_cb->get_eject_state) return disk_control_ext_cb->get_eject_state();
    if (disk_control_cb && disk_control_cb->get_eject_state) return disk_control_cb->get_eject_state();
    return false;
}

bool mux_retro_disk_get_image_label(const unsigned index, char *label, const size_t len) {
    if (disk_control_ext_cb && disk_control_ext_cb->get_image_label) {
        return disk_control_ext_cb->get_image_label(index, label, len);
    }
    return false;
}

static void mux_retro_log_printf(const enum retro_log_level level, const char *fmt, ...) {
    char msg[512];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    switch (level) {
        case RETRO_LOG_ERROR:
            LOG_ERROR(mux_module, "core: %s", msg);
            break;
        case RETRO_LOG_WARN:
            LOG_WARN(mux_module, "core: %s", msg);
            break;
        default:
            LOG_INFO(mux_module, "core: %s", msg);
            break;
    }
}

bool mux_retro_environment_cb(const unsigned cmd, void *data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_CAN_DUPE: {
            *(bool *) data = true;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            pixel_format = *(const enum retro_pixel_format *) data;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
            static const char *dir = STORAGE_BIOS;
            *(const char **) data = dir;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
            static const char *dir = RETRO_SRM_PATH;
            *(const char **) data = dir;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
            static struct retro_log_callback log_cb;
            log_cb.log = mux_retro_log_printf;
            *(struct retro_log_callback *) data = log_cb;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_VARIABLES: {
            if (options_count == 0) options_store_legacy(data);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            struct retro_variable *v = data;
            v->value = options_get_value(v->key);
            return v->value != NULL;
        }

        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
            *(bool *) data = options_dirty;
            options_dirty = false;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION: {
            *(unsigned *) data = 2;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS: {
            options_store_v1(data);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
            const struct retro_core_options_intl *intl = data;
            if (intl && intl->us) options_store_v1(intl->us);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: {
            options_store_v2(data);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL: {
            const struct retro_core_options_v2_intl *intl = data;
            if (intl && intl->us) options_store_v2(intl->us);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
            return true;

        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE: {
            disk_control_cb = (const struct retro_disk_control_callback *) data;
            return true;
        }

        case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION: {
            *(unsigned *) data = 1;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE: {
            disk_control_ext_cb = (const struct retro_disk_control_ext_callback *) data;
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE: {
            const struct retro_message *msg = data;
            if (msg && msg->msg && ui_pnl_message) toast_message(msg->msg, msg->frames > 0 ? msg->frames * 16 : 3000);
            return true;
        }

        case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: {
            const struct retro_message_ext *msg = data;
            if (msg && msg->msg && ui_pnl_message) toast_message(msg->msg, msg->duration > 0 ? msg->duration : 3000);
            return true;
        }

        case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
            return vfs_bridge_get_interface(data);
        }

        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
            return rumble_bridge_get_interface(data);
        }

        default:
            return false;
    }
}
