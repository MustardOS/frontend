ifeq ($(DEVICE), RG35XX)
	ARCH = -march=armv7-a \
		-mtune=cortex-a9 \
		-mfpu=neon-vfpv3 \
		-mfloat-abi=softfp 
else ifeq ($(DEVICE), RG35XXPLUS)
	ARCH = -march=armv8-a+simd \
		-mtune=cortex-a53
else
	$(error Unsupported Device: $(DEVICE))
endif

TARGET = ${shell basename $$(pwd)}

CC = ccache $(CROSS_COMPILE)gcc -O3

CFLAGS  = $(ARCH) -flto=auto -ffunction-sections -fdata-sections \
	-flto -finline-functions -Wall -Wno-format-zero-length \
	-Wno-implicit-function-declaration

LDFLAGS = $(CFLAGS) -lSDL2 -lSDL2_mixer -lpthread -Wl,--gc-sections -s

EXTRA = $(LDFLAGS) -fno-exceptions -fno-stack-protector -fomit-frame-pointer \
	-fno-unroll-loops -fmerge-all-constants -fno-ident \
	-ffast-math -funroll-loops -falign-functions

LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= ${shell pwd}/..

SSRCS := $(wildcard ./ui/*.c)
MAINSRC = ./main.c

include $(LVGL_DIR)/$(LVGL_DIR_NAME)/lvgl.mk
include $(LVGL_DIR)/$(LVGL_DIR_NAME)/drivers/lv_drivers.mk

OBJEXT ?= .o

AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))
SOBJS = $(SSRCS:.c=$(OBJEXT))

MAINOBJ = $(MAINSRC:.c=$(OBJEXT))

SRCS = $(ASRCS) $(CSRCS) $(SSRCS) $(MAINSRC)
OBJS = $(AOBJS) $(COBJS) $(SOBJS)

all: clean compile move

%.o: %.c
	@$(CC) -D$(DEVICE) $(CFLAGS) -c $< -o $@ $(EXTRA)

clean:
	@rm -f $(MAINOBJ) $(AOBJS) $(COBJS) $(SOBJS) $(TARGET)

compile: $(MAINOBJ) $(AOBJS) $(COBJS) $(SOBJS)
	@printf "Building: %s... " "$(TARGET)"
	@START=$$(date +%s); \
	$(CC) -D$(DEVICE) $(MAINOBJ) $(AOBJS) $(COBJS) $(SOBJS) -o $(TARGET) \
		../common/font/awesome_small.c \
		../common/font/gamepad_nav.c \
		../common/font/notosans.c \
		../common/img/nothing.c \
		../common/json/json.c \
		../common/mini/mini.c \
		../common/miniz/miniz.c \
		../common/config.c \
		../common/device.c \
		../common/common.c \
		../common/ui_common.c \
		../common/theme.c \
		$(EXTRA); \
	END=$$(date +%s); \
	ELAPSED_TIME=$$((END - START)); \
	printf "DONE (%ds)\n" "$${ELAPSED_TIME}"

move:
	@mkdir -p ../bin
	@mv $(TARGET) ../bin/
	@rm -f $(MAINOBJ) $(AOBJS) $(COBJS) $(SOBJS) $(TARGET)
