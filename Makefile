CC      ?= cc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Wpedantic -ffreestanding -fno-stack-protector \
           -fno-pic -no-pie
LDFLAGS := -static -nostdlib

all: z80dec

z80dec: src/z80.c src/z80.h src/z80_opcodes.h
	$(CC) $(CFLAGS) $(LDFLAGS) -DZ80_DECODE_DEMO -o $@ src/z80.c

run: z80dec
	./z80dec $(ROM)

clean:
	rm -f z80dec sakhr-emu

.PHONY: all run clean
