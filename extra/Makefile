CFLAGS = -O3
LDFLAGS = -lSDL2 -lSDL2_ttf -lSDL2_image

SRC = sdl2_term.c
OUT = sdl2_term

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)
