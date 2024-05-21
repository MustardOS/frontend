ifeq ($(DEVICE), RG35XX)
    ARCH = -marm -mtune=cortex-a9 -mfpu=neon-vfpv3 -mfloat-abi=softfp -march=armv7-a
else ifeq ($(DEVICE), RG35XXPLUS)
	ARCH = -march=armv8-a+simd -mtune=cortex-a53
else
	$(error Unsupported Device: $(DEVICE))
endif

OUT = ./bin

CC = $(CROSS_COMPILE)gcc -static
ST = $(CROSS_COMPILE)strip -s -x

CFLAGS  = -O3 $(ARCH) -flto=auto -ffunction-sections -fdata-sections -flto -finline-functions -Wall -Wno-implicit-function-declaration
LDFLAGS = $(CFLAGS) -lpthread -Wl,--gc-sections -s

EXTRA = $(LDFLAGS) -fno-exceptions -fno-stack-protector -fomit-frame-pointer -fno-unroll-loops -fmerge-all-constants -fno-ident -ffast-math -funroll-loops -falign-functions

all: muaudio mubright musleep mushotog mushotplus mushuffle muwatch

$(info DEVICE is $(DEVICE))

muaudio: muaudio.c common.c
	@mkdir -p $(OUT)
	$(CC) -D$(DEVICE) -o $(OUT)/muaudio muaudio.c common.c $(EXTRA)
	$(ST) $(OUT)/muaudio

mubright: mubright.c common.c
	@mkdir -p $(OUT)
	$(CC) -D$(DEVICE) -o $(OUT)/mubright mubright.c common.c $(EXTRA)
	$(ST) $(OUT)/mubright

musleep: musleep.c common.c
	@mkdir -p $(OUT)
	$(CC) -D$(DEVICE) -o $(OUT)/musleep musleep.c common.c $(EXTRA)
	$(ST) $(OUT)/musleep

mushotog: mushotog.c common.c
	@mkdir -p $(OUT)
	$(CC) -D$(DEVICE) -o $(OUT)/mushotog mushotog.c common.c $(EXTRA)
	$(ST) $(OUT)/mushotog

mushotplus: mushotplus.c common.c
	@mkdir -p $(OUT)
	$(CC) -D$(DEVICE) -o $(OUT)/mushotplus mushotplus.c common.c $(EXTRA)
	$(ST) $(OUT)/mushotplus

mushuffle: mushuffle.c common.c
	@mkdir -p $(OUT)
	$(CC) -D$(DEVICE) -o $(OUT)/mushuffle mushuffle.c common.c $(EXTRA)
	$(ST) $(OUT)/mushuffle

mushutdown: mushutdown.c common.c
	@mkdir -p $(OUT)
	$(CC) -D$(DEVICE) -o $(OUT)/mushutdown mushutdown.c common.c $(EXTRA)
	$(ST) $(OUT)/mushutdown

muwatch: muwatch.c common.c
	@mkdir -p $(OUT)
	$(CC) -D$(DEVICE) -o $(OUT)/muwatch muwatch.c common.c $(EXTRA)
	$(ST) $(OUT)/muwatch

clean:
	rm -rf $(OUT)
