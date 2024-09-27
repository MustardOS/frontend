CFLAGS = -fPIC -O3 -Wall -Wno-unused-function
SRCS = $(wildcard *.c)

all: $(SRCS:.c=.so)

%.so: %.c
	@printf "Compiling %s into %s\n" "$<" "$@"
	@$(CC) -shared -o lib$@ $< $(CFLAGS)
	@printf "\tMoving $@ to lib folder...\n"
	@mkdir -p ../../bin/lib
	@mv *.so ../../bin/lib

clean:
	@printf "Cleaning up...\n"
	@rm -rf ../../bin/lib
