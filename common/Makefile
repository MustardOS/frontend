include ../common.mk

CFLAGS   = $(BASE_CFLAGS) $(STRICT_CFLAGS) -I. -Iinput -Ijson -Imini -Iminiz -Icore -Ixxhash -Icollection -Imojibake -isystem miniz \
           -I../plutosvg/source -I../plutosvg/plutovg/include
LDFLAGS  = $(COMMON_LIBS) $(LIB_LDFLAGS) $(SHARED_PIC) -L../bin/lib -lplutosvg

SRC_DIRS = . saver input json mini miniz core xxhash ui collection mojibake

ifeq ($(BUILD),release)
    SRCS = $(filter-out %crash_stub.c, $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c)))
else
    SRCS = $(filter-out %crash.c, $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c)))
endif
OBJS     = $(SRCS:.c=.o)

miniz/miniz.o: CFLAGS += -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES

LIB_DIR  = ${shell pwd}/../bin/lib
LIB_FILE = libmuxcom.so

all: $(LIB_DIR)/$(LIB_FILE)

$(LIB_DIR)/$(LIB_FILE): $(OBJS)
	@printf "Linking objects into %s\n" "$@"
	@mkdir -p $(LIB_DIR)
	@$(CC) $(OBJS) $(LDFLAGS) -o $@
	@find ${shell pwd} -name "*.o" -exec rm -f {} +

%.o: %.c
	@printf "Compiling %s into %s\n" "$<" "$@"
	@$(CC) -c $< -o $@ $(CFLAGS)
