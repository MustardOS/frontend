EXTERNAL_ROOT   := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
EXTERNAL_PREFIX := $(EXTERNAL_ROOT)/prefix/$(DEVICE)
EXTERNAL_BUILD  := $(EXTERNAL_ROOT)/build.sh
EXTERNAL_LIB    := $(EXTERNAL_PREFIX)/lib

EXTERNAL_CFLAGS := -isystem $(EXTERNAL_PREFIX)/include

-include $(EXTERNAL_PREFIX)/libarchive.mk

FFMPEG_LIBS := -Wl,--start-group \
               $(EXTERNAL_LIB)/libavformat.a \
               $(EXTERNAL_LIB)/libavcodec.a \
               $(EXTERNAL_LIB)/libswscale.a \
               $(EXTERNAL_LIB)/libswresample.a \
               $(EXTERNAL_LIB)/libavutil.a \
               -Wl,--end-group -lm -lpthread -latomic

MOJIBAKE_LIBS := $(EXTERNAL_LIB)/libmojibake.a
LIBARCHIVE_LIBS := $(EXTERNAL_LIB)/libarchive.a $(LIBARCHIVE_SYSLIBS)
RCHEEVOS_LIBS := $(EXTERNAL_LIB)/librcheevos.a
OPENSSL_LIBS := $(EXTERNAL_LIB)/libssl.a $(EXTERNAL_LIB)/libcrypto.a -ldl -lpthread

EXTERNAL_HIDE := -Wl,--exclude-libs,ALL

COMMON_EXTERNAL_LDFLAGS := $(FFMPEG_LIBS) $(MOJIBAKE_LIBS) $(EXTERNAL_HIDE)
RETRO_EXTERNAL_LDFLAGS  := $(RCHEEVOS_LIBS) $(LIBARCHIVE_LIBS) $(OPENSSL_LIBS) $(EXTERNAL_HIDE)
