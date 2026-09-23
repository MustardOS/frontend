include ../common.mk

BIN_DIR := ../bin
LIB_DIR := $(BIN_DIR)/lib
TARGET  := $(BIN_DIR)/muxterm

CFLAGS  := $(BASE_CFLAGS) $(STRICT_CFLAGS) -I.. -I../module -I../module/ui -I../vendor
LDFLAGS := $(COMMON_LIBS) $(BIN_LDFLAGS) -lutil
LDLIBS  := -L$(LIB_DIR) -lui -lmuxcom -lmuxmod -lplutosvg

SRC := main.c config.c input.c menu.c osk.c render.c sixel.c vt.c
OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDLIBS) $(LDFLAGS)

DEPDIR := $(DEP_ROOT)/terminal
DEPS   := $(foreach O,$(OBJ),$(call DEP_NAME,terminal,$(basename $(O))))
$(shell mkdir -p $(DEPDIR))

-include $(DEPS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -MF $(call DEP_NAME,terminal,$*)

clean:
	rm -f $(OBJ)
	rm -rf $(DEPDIR)
