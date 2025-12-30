include ../common.mk

CFLAGS = $(BASE_CFLAGS)
LDFLAGS = $(COMMON_LIBS) $(SHARED_PIC)

SRC_DIRS = .
SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))

LIB_DIR = ${shell pwd}/../bin/lib
LIB_FILES = $(patsubst %.c,$(LIB_DIR)/lib%.so,$(notdir $(SRCS)))

all: $(LIB_FILES)

$(LIB_DIR)/lib%.so: $(SRC_DIRS)/%.c
	@printf "Compiling %s into %s...\n" "$<" "$@"
	@mkdir -p $(LIB_DIR)
	@$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@
	@find ${shell pwd} -name "*.o" -exec rm -f {} +
