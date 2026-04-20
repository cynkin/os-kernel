# --- Makefile for OS Kernel ---

# Cross-compiler and tools
CROSS_COMPILE ?=
CC = gcc -m32
AS = nasm
LD = ld -m elf_i386
OBJCOPY = $(CROSS_COMPILE)objcopy
ISO = $(BUILD_DIR)/kernel.iso
GRUB_CFG = iso/boot/grub/grub.cfg

# Compiler and Assembler flags
CFLAGS = -ffreestanding -O2 -g -Wall -Wextra -std=gnu99 -I.
CFLAGS += -m32
ASFLAGS = -f elf32
LDFLAGS = -T linker.ld

# Directories
BUILD_DIR = build
SRC_DIR = .
ARCH_X86_DIR = arch/x86
CPU_DIR = cpu
DRIVERS_VGA_DIR = drivers/vga
GDT_IDT_DIR = gdt_and_idt
KERNEL_DIR = kernel
LIB_DIR = lib
LOG_DIR = log
PIC_DIR = pic
PIT_DIR = pit
TIME_DIR = time

# Source files
C_SOURCES = \
    $(SRC_DIR)/kmalloc.c \
    $(SRC_DIR)/pcb.c \
    $(SRC_DIR)/scheduler.c \
    $(DRIVERS_VGA_DIR)/vga.c \
    $(CPU_DIR)/isr.c \
    $(GDT_IDT_DIR)/gdt.c \
    $(GDT_IDT_DIR)/idt.c \
    $(KERNEL_DIR)/kernel_main.c \
    $(KERNEL_DIR)/panic.c \
    $(LIB_DIR)/memory.c \
    $(LIB_DIR)/printf.c \
    $(LOG_DIR)/log.c \
    $(PIC_DIR)/pic.c \
    $(PIT_DIR)/pit.c \
    $(TIME_DIR)/time.c \
    $(KERNEL_DIR)/stubs.c \
    $(KERNEL_DIR)/tests.c

ASM_SOURCES = \
    boot/boot.asm \
    $(GDT_IDT_DIR)/gdt_flush.asm \
    $(GDT_IDT_DIR)/idt_flush.asm \
    $(GDT_IDT_DIR)/isr_stubs.asm \
    $(SRC_DIR)/switch.asm

# Object files
C_OBJECTS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst %.asm, $(BUILD_DIR)/%.o, $(ASM_SOURCES))

OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

# Target executable
KERNEL_BIN = $(BUILD_DIR)/kernel.bin

.PHONY: all clean run

all: $(KERNEL_BIN)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/boot
	mkdir -p $(BUILD_DIR)/cpu
	mkdir -p $(BUILD_DIR)/drivers/vga
	mkdir -p $(BUILD_DIR)/gdt_and_idt
	mkdir -p $(BUILD_DIR)/kernel
	mkdir -p $(BUILD_DIR)/lib
	mkdir -p $(BUILD_DIR)/log
	mkdir -p $(BUILD_DIR)/pic
	mkdir -p $(BUILD_DIR)/pit
	mkdir -p $(BUILD_DIR)/time

# Compile C sources
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ASM (boot)
$(BUILD_DIR)/boot/%.o: boot/%.asm | $(BUILD_DIR)/boot
	$(AS) $(ASFLAGS) $< -o $@

# ASM (arch)
# $(BUILD_DIR)/arch/x86/%.o: arch/x86/%.asm | $(BUILD_DIR)/arch/x86
# 	$(AS) $(ASFLAGS) $< -o $@

# ASM (gdt/idt)
$(BUILD_DIR)/gdt_and_idt/%.o: gdt_and_idt/%.asm | $(BUILD_DIR)/gdt_and_idt
	$(AS) $(ASFLAGS) $< -o $@

# Link all object files
$(KERNEL_BIN): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

$(BUILD_DIR)/%.o: %.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(GRUB_CFG):
	mkdir -p iso/boot/grub
	echo 'set timeout=0'                          > $(GRUB_CFG)
	echo 'set default=0'                         >> $(GRUB_CFG)
	echo 'menuentry "MyOS" {'                    >> $(GRUB_CFG)
	echo '    multiboot /boot/kernel.bin'        >> $(GRUB_CFG)
	echo '}'                                     >> $(GRUB_CFG)

$(ISO): $(KERNEL_BIN) $(GRUB_CFG)
	mkdir -p iso/boot
	cp $(KERNEL_BIN) iso/boot/kernel.bin
	grub-mkrescue -o $(ISO) iso

run: $(ISO)
	qemu-system-i386 -m 32M -serial stdio -cdrom $(ISO) -d int,cpu_reset -no-reboot

clean:
	rm -rf $(BUILD_DIR)
