CC      ?= cc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Wpedantic -ffreestanding -fno-stack-protector \
           -fno-pic -no-pie
LDFLAGS := -static -nostdlib

all: z80dec

z80dec: src/z80.c src/z80.h src/z80_opcodes.h
	$(CC) $(CFLAGS) $(LDFLAGS) -DZ80_DECODE_DEMO -o $@ src/z80.c

tests/z80_decode_test: tests/z80_decode_test.c src/z80.c src/z80.h src/z80_opcodes.h\n\t$(CC) -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Isrc -o $@ tests/z80_decode_test.c src/z80.c\n\ntest: tests/z80_decode_test\n\t./tests/z80_decode_test\n\nrun: z80dec
	./z80dec $(ROM)

clean:
	rm -f z80dec sakhr-emu tests/z80_decode_test

.PHONY: all test run clean
