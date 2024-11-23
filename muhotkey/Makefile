START_TIME := $(shell date +%s)

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
	-flto -finline-functions -Wall -Wno-format-zero-length

LDFLAGS = $(CFLAGS) -Wl,--gc-sections -s

EXTRA = $(LDFLAGS) -fno-exceptions -fno-stack-protector -fomit-frame-pointer \
	-fno-unroll-loops -fmerge-all-constants -fno-ident \
	-ffast-math -funroll-loops -falign-functions

MAINSRC = ./main.c

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
	@START=$(START_TIME); \
	$(CC) -D$(DEVICE) $(MAINOBJ) $(AOBJS) $(COBJS) $(SOBJS) -o $(TARGET) \
		../common/json/json.c \
		../common/mini/mini.c \
		../common/config.c \
		../common/device.c \
		../common/common.c \
		../common/input.c \
		../common/controller_profile.c \
		$(EXTRA); \
	END=$$(date +%s); \
	ELAPSED_TIME=$$((END - START)); \
	printf "DONE (%ds)\n" "$${ELAPSED_TIME}"

move: compile
	@mkdir -p ../bin
	@mv $(TARGET) ../bin/
	@rm -f $(MAINOBJ) $(AOBJS) $(COBJS) $(SOBJS) $(TARGET)
