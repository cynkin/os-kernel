CC = gcc -m32
LD = gcc -m32
NASM = nasm

INCLUDE_DIRS = $(patsubst %,-I%,$(shell find . -type d -not -path './.git*'))
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie $(INCLUDE_DIRS)
LDFLAGS = -m32 -T linker.ld -ffreestanding -O2 -nostdlib -static -no-pie

C_SOURCES = $(shell find . -type f -name '*.c' -not -path './.git*')
S_SOURCES = $(shell find . -type f -name '*.s' -not -path './.git*')
ASM_SOURCES = $(shell find . -type f -name '*.asm' -not -path './.git*')

OBJ = $(C_SOURCES:.c=.o) $(S_SOURCES:.s=.o) $(ASM_SOURCES:.asm=.o)

TARGET = os-kernel.bin

all: $(TARGET)

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	as --32 $< -o $@

%.o: %.asm
	$(NASM) -f elf32 $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)