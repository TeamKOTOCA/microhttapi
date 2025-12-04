CC_native = gcc
CC_mips = mipsel-linux-gnu-gcc

CFLAGS = -static -Os -s

SRC = src/microhttp.c

all: native

native:
	$(CC_native) $(SRC) -o microhttp_native

mips:
	$(CC_mips) $(CFLAGS) $(SRC) -o microhttp_mips

clean:
	rm -f tinyhttp_native microhttp_mips
