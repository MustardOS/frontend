include ../common.mk

CFLAGS = $(BASE_CFLAGS) -I.
LDFLAGS = $(COMMON_LIBS) $(SHARED_PIC)

SRC_DIRS = .
SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))

OBJS = $(SRCS:.c=.o)

LIB_DIR = ${shell pwd}/../bin/lib
LIB_FILE = liblookup.so

all: $(LIB_DIR)/$(LIB_FILE)

$(LIB_DIR)/$(LIB_FILE): $(OBJS)
	@printf "Linking objects into %s\n" "$@"
	@mkdir -p $(LIB_DIR)
	@$(CC) $(OBJS) $(LDFLAGS) -o $@
	@find ${shell pwd} -name "*.o" -exec rm -f {} +

%.o: %.c
	@printf "Compiling %s into %s\n" "$<" "$@"
	@$(CC) -c $< -o $@ $(CFLAGS)
