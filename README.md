# Tic-Tac-Toe OS

A small, freestanding 32-bit x86 kernel that boots directly through GRUB and runs a game of Tic-Tac-Toe. It uses VGA text mode for its interface, PS/2 keyboard polling for input, and a minimax-based computer opponent.

## Features

- Boots as a standalone kernel through GRUB
- Clean VGA text-mode Tic-Tac-Toe board
- Arrow-key cursor navigation
- Three computer difficulty levels
- Minimax AI, including unbeatable hard mode
- Replay and halt controls

## Requirements

Install the following tools before building:

- `gcc` with 32-bit compilation support
- GNU Binutils (`as` and `ld`)
- `make`
- QEMU (`qemu-system-i386`) to run the kernel in a virtual machine

To build a bootable ISO, also install:

- `grub-mkrescue`
- `xorriso`


## Build and run

From the project directory:

```bash
make
```

This produces `kernel.elf`.

Run it directly in QEMU:

```bash
make run
```

## Build a bootable ISO

Create a GRUB bootable ISO image:

```bash
make iso
```

This produces `os.iso`. Run that image with:

```bash
make run-iso
```

## Controls

### Difficulty selection

| Key | Difficulty | Behavior |
| --- | --- | --- |
| `1` | Easy | Mostly random moves |
| `2` | Medium | Searches several moves ahead |
| `3` | Hard | Perfect-play minimax opponent |

### During a game

| Key | Action |
| --- | --- |
| Arrow keys | Move the selection cursor |
| `Enter` or `Space` | Place an X in the selected cell |
| `R` | Start a new game after a result |
| `Esc` | Halt the kernel after a result |

## Project layout

| File | Purpose |
| --- | --- |
| `boot.S` | Multiboot header and assembly entry point |
| `kernel.c` | VGA rendering, PS/2 input, game rules, and AI |
| `linker.ld` | Kernel memory layout for the linker |
| `grub.cfg` | GRUB boot menu configuration |
| `Makefile` | Build, run, ISO, and cleanup targets |

## Clean generated files

```bash
make clean
```

This removes object files, the ELF kernel, and ISO build output.

## Notes

This project runs without a standard library or host operating system. Hardware access is performed directly through x86 I/O ports and the VGA text buffer, so it is intended for QEMU or another compatible x86 emulator.
