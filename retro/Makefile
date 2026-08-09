include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(BIN_DIR)/muxretro

CFLAGS  := $(BASE_CFLAGS) $(STRICT_CFLAGS) -DRC_CLIENT_SUPPORTS_HASH -fPIE
LDFLAGS := $(COMMON_LIBS) $(BIN_LDFLAGS) -lssl -lcrypto -ldl

ifeq ($(DEVICE), NATIVE)
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg -lz -llzma -lbz2
else
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg \
           -Wl,-Bstatic -lz -llzma -Wl,-Bdynamic -lbz2
endif

INCLUDES := -I../module -I../module/ui -I../common \
            -I../common/input -I../common/mini -Icore \
            -Icheevo/vendor/rcheevos/include -Icheevo/vendor/rcheevos/src/rcheevos

SRC := $(wildcard coredef/*.c) core/main.c core/startup.c core/core.c core/environment.c core/runahead.c core/perf.c \
       core/governor_boost.c core/power.c core/power_protocol.c \
       video/video.c video/gl_dispatch.c video/colour.c video/overlay_bridge.c video/frame_pacer.c \
       video/ui_display.c video/ui_colfilter.c video/ui_shader.c video/ui_viewport.c \
       video/ui_viewportadjust.c video/ui_viewportcrop.c video/ui_overlay.c \
       video/image_writer.c video/ui_videosettings.c video/hw_render.c \
       video/filters/filters.c video/filters/scale2x.c video/filters/super_eagle.c \
       audio/audio.c audio/ui_soundsettings.c \
       input/input_bridge.c input/rumble.c input/hotkeys.c input/ui_hotkeys.c \
       input/ui_inputsettings.c input/ui_controlleroptions.c input/ui_inputport.c \
       input/ui_buttonmapping.c input/nav_repeat.c input/core_input_meta.c \
       macro/macro.c macro/relish.c macro/runtime.c macro/ui_macros.c \
       state/state.c state/gamestate.c state/ui_gamestate.c state/vfs.c \
       state/content_hash.c state/bios_check.c state/sram.c \
       state/patch.c state/ui_storagesettings.c state/manual.c \
       cheevo/account.c cheevo/cache.c cheevo/cache_key.c cheevo/cheevo.c cheevo/ui_cheevo.c \
       netplay/address.c netplay/wire.c netplay/netplay.c netplay/ui_netplay.c \
       settings/settings.c settings/submenu.c settings/ui_settings.c \
       settings/ui_performancesettings.c settings/ui_hudsettings.c \
       ui/ui_pause.c ui/ui_loading.c ui/options.c ui/ui_options.c ui/ui_information.c \
       ui/ui_diskcontrol.c ui/cheats.c ui/ui_cheats.c ui/ui_patch.c ui/ui_manual.c

OBJ := $(SRC:.c=.o)

state/content_hash.o state/patch.o state/sram.o state/vfs.o: CFLAGS += -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES

RC_DIR := cheevo/vendor/rcheevos
RC_SRC := $(filter-out $(RC_DIR)/src/rc_client_external.c $(RC_DIR)/src/rc_client_raintegration.c, \
          $(wildcard $(RC_DIR)/src/*.c)) \
          $(wildcard $(RC_DIR)/src/rcheevos/*.c) \
          $(wildcard $(RC_DIR)/src/rapi/*.c) \
          $(wildcard $(RC_DIR)/src/rhash/*.c)
RC_OBJ := $(RC_SRC:.c=.o)
RC_CFLAGS := $(BASE_CFLAGS) -DRC_CLIENT_SUPPORTS_HASH -I$(RC_DIR)/include \
             -I$(RC_DIR)/src/rcheevos -Icore

LA_DIR     := ../common/libarchive
LA_SRC     := $(wildcard $(LA_DIR)/*.c)
LA_OBJ     := $(LA_SRC:.c=.o)
LA_CFLAGS  := $(BASE_CFLAGS) -DHAVE_CONFIG_H -DLIBARCHIVE_STATIC -I$(LA_DIR)

.PHONY: all clean

all: $(TARGET) clean

$(TARGET): $(OBJ) $(RC_OBJ) $(LA_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJ) $(RC_OBJ) $(LA_OBJ) -o $@ $(LDLIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LA_DIR)/%.o: $(LA_DIR)/%.c
	$(CC) $(LA_CFLAGS) -c $< -o $@

$(RC_DIR)/%.o: $(RC_DIR)/%.c
	$(CC) $(RC_CFLAGS) -c $< -o $@

clean: | $(TARGET)
	rm -f $(OBJ) $(RC_OBJ) $(LA_OBJ)
