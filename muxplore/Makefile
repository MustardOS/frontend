ifeq ($(DEVICE), RG35XX)
	ARCH = -march=armv7-a \
		-mtune=cortex-a9 \
		-mfpu=neon-vfpv3 \
		-mfloat-abi=softfp 
else ifeq ($(DEVICE), RG35XXPLUS)
	ARCH = -march=armv8-a+simd \
		-mtune=cortex-a53
else ifeq ($(DEVICE), RG28XX)
	ARCH = -march=armv8-a+simd \
		-mtune=cortex-a53
else
	$(error Unsupported Device: $(DEVICE))
endif

TARGET = ${shell basename $$(pwd)}

CC = ccache $(CROSS_COMPILE)gcc -static -O3

CFLAGS  = $(ARCH) -flto=auto -ffunction-sections -fdata-sections \
	-flto -finline-functions -Wall -Wno-format-zero-length \
	-Wno-implicit-function-declaration

LDFLAGS = $(CFLAGS) -lpthread -Wl,--gc-sections -s

EXTRA = $(LDFLAGS) -fno-exceptions -fno-stack-protector -fomit-frame-pointer \
	-fno-unroll-loops -fmerge-all-constants -fno-ident \
	-ffast-math -funroll-loops -falign-functions

LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= ${shell pwd}/..

SSRCS := ${shell cat ui/filelist.txt}

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

all: message compile

%.o: %.c
	@$(CC) -D$(DEVICE) $(CFLAGS) -c $< -o $@ $(EXTRA)

message:
	@printf "\nDevice:\t$(DEVICE)\n"
	@printf "Name:\t$(TARGET)\n"

compile: $(MAINOBJ) $(AOBJS) $(COBJS) $(SOBJS)
	@$(CC) -D$(DEVICE) $(MAINOBJ) $(AOBJS) $(COBJS) $(SOBJS) -o $(TARGET) \
		../common/json/json.c \
		../common/mini/mini.c \
		../common/content_items.c \
		../common/config.c \
		../common/device.c \
		../common/glyph.c \
		../common/common.c \
		../common/theme.c \
		theme.c \
		$(EXTRA)

clean:
	@rm -f $(MAINOBJ) $(AOBJS) $(COBJS) $(SOBJS) $(TARGET)

