include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(BIN_DIR)/muxretro

CFLAGS  := $(BASE_CFLAGS) $(STRICT_CFLAGS) -DRC_CLIENT_SUPPORTS_HASH -fPIE
LDFLAGS := $(COMMON_LIBS) $(RETRO_EXTERNAL_LDFLAGS) $(BIN_LDFLAGS) -ldl

LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg

INCLUDES := -I../module -I../module/ui -I../common \
            -I../common/input -I../common/mini -Icore

SRC := $(wildcard coredef/*.c) $(wildcard coreinfo/*.c) \
       core/main.c core/startup.c core/core.c core/environment.c core/runahead.c core/perf.c \
       core/governor_boost.c core/power.c core/power_protocol.c \
       video/video.c video/gl_dispatch.c video/colour.c video/overlay_bridge.c video/frame_pacer.c \
       video/ui_display.c video/ui_colfilter.c video/ui_shader.c video/ui_shaderadjust.c video/ui_viewport.c \
       video/ui_viewportadjust.c video/ui_viewportcrop.c video/ui_overlay.c video/ui_vignette.c \
       video/image_writer.c video/ui_videosettings.c video/hw_render.c \
       video/filters/filters.c video/filters/scale2x.c video/filters/super_eagle.c \
       audio/audio.c audio/ui_soundsettings.c \
       input/input_bridge.c input/rumble.c input/hotkeys.c input/ui_hotkeys.c \
       input/ui_inputsettings.c input/ui_controlleroptions.c input/ui_inputport.c \
       input/ui_buttonmapping.c input/nav_repeat.c input/core_input_meta.c \
       input/deck.c \
       macro/macro.c macro/relish.c macro/runtime.c macro/ui_macros.c \
       core/subsystem.c \
       state/core_state.c state/state_writer.c state/state.c state/gamestate.c state/ui_gamestate.c state/vfs.c \
       state/content_hash.c state/bios_check.c state/persistent.c \
       state/patch.c state/ui_storagesettings.c state/manual.c state/history.c \
       cheevo/account.c cheevo/cache.c cheevo/cache_key.c cheevo/spool.c cheevo/cheevo.c cheevo/ui_cheevo.c \
       netplay/address.c netplay/wire.c netplay/netplay.c netplay/ui_netplay.c \
       link/link.c link/ui_link.c \
       settings/settings.c settings/submenu.c settings/ui_settings.c \
       settings/ui_performancesettings.c settings/ui_hudsettings.c \
       ui/ui_pause.c ui/ui_advisory.c ui/ui_loading.c ui/options.c ui/ui_options.c ui/ui_information.c \
       ui/ui_diskcontrol.c ui/cheats.c ui/ui_cheats.c ui/ui_patch.c ui/ui_manual.c

OBJ := $(SRC:.c=.o)

state/content_hash.o state/patch.o state/persistent.o state/vfs.o: CFLAGS += -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDLIBS) $(LDFLAGS)

DEPDIR := $(DEP_ROOT)/retro
DEPS   := $(foreach O,$(OBJ),$(call DEP_NAME,retro,$(basename $(O))))
$(shell mkdir -p $(DEPDIR))

-include $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@ -MF $(call DEP_NAME,retro,$*)

clean:
	rm -f $(OBJ)
	rm -rf $(DEPDIR)
