include ../common.mk

CFLAGS = $(BASE_CFLAGS) -I. -Iimg -Iinput -Ijson -Imini -Iminiz -Icore -Ixxhash -isystem miniz
LDFLAGS = $(COMMON_LIBS) $(SHARED_PIC)

SRC_DIRS = . img input json mini miniz core xxhash
SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))

OBJS = $(SRCS:.c=.o)

LIB_DIR = ${shell pwd}/../bin/lib
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
