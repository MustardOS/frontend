include ../common.mk

# PlutoSVG + PlutoVG source layout (vendor into this directory):
#   source/plutosvg.h
#   source/plutosvg.c
#   plutovg/include/plutovg.h
#   plutovg/source/*.c
#
# Clone PlutoSVG with its submodule:
#   git clone --recurse-submodules https://github.com/sammycage/plutosvg

PLUTOSVG_SRC = source/plutosvg.c
PLUTOVG_SRCS = $(wildcard plutovg/source/*.c)

CFLAGS = $(BASE_CFLAGS) -fPIC \
         -Isource -Iplutovg/include

ALL_OBJS = $(PLUTOSVG_SRC:.c=.o) $(PLUTOVG_SRCS:.c=.o)

LIB_DIR  = ${shell pwd}/../bin/lib
LIB_FILE = libplutosvg.so

all: $(LIB_DIR)/$(LIB_FILE)

$(LIB_DIR)/$(LIB_FILE): $(ALL_OBJS)
	@printf "Linking objects into %s\n" "$@"
	@mkdir -p $(LIB_DIR)
	@$(CC) $(ALL_OBJS) $(SHARED_PIC) $(CLOSED_LIB_LDFLAGS) -lm -o $@

DEPDIR := $(DEP_ROOT)/plutosvg
DEPS   := $(foreach O,$(ALL_OBJS),$(call DEP_NAME,plutosvg,$(basename $(O))))
$(shell mkdir -p $(DEPDIR))

-include $(DEPS)

%.o: %.c
	@printf "Compiling %s into %s\n" "$<" "$@"
	@$(CC) -c $< -o $@ $(CFLAGS) -MF $(call DEP_NAME,plutosvg,$*)
