CC_native = gcc
CC_mips = mipsel-linux-gnu-gcc

CFLAGS = -static -Os -s

SRC = src/microhttpi.c

all: native

native:
	$(CC_native) $(SRC) -o microhttpi_native

mips:
	$(CC_mips) $(CFLAGS) $(SRC) -o microhttpi_mips

clean:
	rm -f tinyhttpi_native microhttpi_mips
