# The cross-compiler tools we used
CC = i686-elf-gcc
AS = i686-elf-as
LD = i686-elf-gcc

# Compiler flags: 
# -ffreestanding tells the compiler we don't have a standard C library (like printf or malloc)
# -Iinclude tells it to look in /include folder for header files
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS = -T linker.ld -ffreestanding -O2 -nostdlib

# To automatically find all C and Assembly files in folders
C_SOURCES = $(shell find . -type f -name '*.c')
S_SOURCES = $(shell find . -type f -name '*.s')

# To convert the source lists into a list of .o (object) target files
OBJ = $(C_SOURCES:.c=.o) $(S_SOURCES:.s=.o)

# The final output binary name
TARGET = os-kernel.bin

# Default rule which runs when we just type "make"
all: $(TARGET)

# To link all the .o files into the final binary
$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

# To compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# To compile .s (assembly) files into .o files
%.o: %.s
	$(AS) $< -o $@

# Cleanup rule which runs when we type "make clean" to delete compiled files
clean:
	rm -f $(OBJ) $(TARGET)