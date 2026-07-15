/*
 * linker.ld — lays the kernel out at 1MB, the conventional Multiboot
 * load address (low enough to avoid needing paging, high enough to
 * stay clear of BIOS/real-mode memory).
 */
ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* Multiboot header must be within the first 8KB, so keep it first. */
    .multiboot ALIGN(4K) : {
        *(.multiboot)
    }

    .text ALIGN(4K) : {
        *(.text)
    }

    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .data ALIGN(4K) : {
        *(.data)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
    }
}