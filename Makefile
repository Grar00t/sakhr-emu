CC      ?= cc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -ffreestanding -nostdlib \
           -fno-stack-protector -fno-pic -no-pie
LDFLAGS := -static -nostdlib

all: z80dec sakhr-m1

z80dec: src/z80.c src/z80.h src/z80_opcodes.h
	$(CC) $(CFLAGS) $(LDFLAGS) -DZ80_DECODE_DEMO -o $@ src/z80.c

sakhr-m1: tests/z80_m1_test.c tests/m1_ok.trace tests/m1_ok.rom src/z80_exec.c src/z80.h
	$(CC) $(CFLAGS) $(LDFLAGS) -Isrc -o $@ tests/z80_m1_test.c src/z80_exec.c

test-m1: sakhr-m1
	./sakhr-m1

test: test-m1

run-m1: sakhr-m1
	./sakhr-m1

run: z80dec
	./z80dec $(ROM)

clean:
	rm -f z80dec sakhr-m1

.PHONY: all test test-m1 run run-m1 clean
