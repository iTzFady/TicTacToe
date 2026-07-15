# Tic-Tac-Toe OS — build targets
#
#   make            -> build kernel.elf
#   make run         -> boot it directly in QEMU (fastest, no ISO needed)
#   make iso         -> build os.iso (bootable via GRUB, for real VMs/USB)
#   make run-iso     -> boot os.iso in QEMU (closer to how a real VM sees it)
#   make clean        -> remove build artifacts
#
# Requires: gcc, gnu binutils (as/ld) — almost certainly already on any
# Linux box. `make iso` additionally needs grub-mkrescue + xorriso
# (package "grub-pc-bin" + "xorriso" on Debian/Ubuntu). `make run*`
# needs qemu-system-i386.

CC      = gcc
AS      = as
LD      = ld
CFLAGS = -m32 \
          -ffreestanding \
          -fno-pic \
          -fno-stack-protector \
          -nostdlib \
          -Wall -Wextra \
          -O2 \
          -mno-sse \
          -mno-sse2 \
          -mno-mmx \
          -msoft-float
ASFLAGS = --32
LDFLAGS = -m elf_i386 -T linker.ld

.PHONY: all run iso run-iso clean

all: kernel.elf

boot.o: boot.S
	$(AS) $(ASFLAGS) boot.S -o boot.o

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

kernel.elf: boot.o kernel.o linker.ld
	$(LD) $(LDFLAGS) -o kernel.elf boot.o kernel.o

run: kernel.elf
	qemu-system-i386 -kernel kernel.elf

iso: kernel.elf grub.cfg
	rm -rf isodir
	mkdir -p isodir/boot/grub
	cp kernel.elf isodir/boot/kernel.elf
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o os.iso isodir
	rm -rf isodir

run-iso: iso
	qemu-system-i386 -cdrom os.iso

clean:
	rm -f boot.o kernel.o kernel.elf os.iso
	rm -rf isodir