include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(BIN_DIR)/muxmedia

CFLAGS  := $(BASE_CFLAGS) $(STRICT_CFLAGS) $(VIDEO_FFMPEG_CFLAGS) -I. -I.. -I../module -I../module/ui -I../vendor \
           -Icore -Icontent -Isettings -Istate -Iui -Ivideo
LDFLAGS := $(COMMON_LIBS) $(VIDEO_FFMPEG_LIBS) $(EXTERNAL_HIDE) $(BIN_LDFLAGS)
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg

SRC := core/audio.c core/audio_source.c core/audio_transition.c core/main.c core/player.c content/library.c settings/assets.c settings/catalogue.c \
       settings/session.c settings/settings.c state/state.c ui/ui_audio.c ui/ui_loading.c ui/ui_pause.c ui/ui_progress.c \
       video/effects.c video/video.c visualiser/visualiser.c visualiser/spectrum.c visualiser/waveform.c \
       visualiser/pulse.c visualiser/orbit.c visualiser/meter.c visualiser/phase.c visualiser/radial.c \
       visualiser/starfield.c
OBJ := $(SRC:.c=.o)
OBJ += video/scale2x.o video/super_eagle.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ) $(VIDEO_FFMPEG_ARCHIVES)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDLIBS) $(LDFLAGS)

DEPDIR := $(DEP_ROOT)/video
DEPS   := $(foreach O,$(OBJ),$(call DEP_NAME,video,$(basename $(O))))
$(shell mkdir -p $(DEPDIR))

-include $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -MF $(call DEP_NAME,video,$*)

video/scale2x.o: ../retro/video/filters/scale2x.c
	$(CC) $(CFLAGS) -c $< -o $@ -MF $(call DEP_NAME,video,video/scale2x)

video/super_eagle.o: ../retro/video/filters/super_eagle.c
	$(CC) $(CFLAGS) -c $< -o $@ -MF $(call DEP_NAME,video,video/super_eagle)

clean:
	rm -f $(OBJ) *.o
	rm -rf $(DEPDIR)
