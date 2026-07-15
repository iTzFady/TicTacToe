/*
 * boot.S — Multiboot entry point
 * ------------------------------------------------------------
 * This is the very first code that runs. GRUB (or QEMU's built-in
 * multiboot loader) scans the first 8KB of the kernel binary for the
 * Multiboot magic header below; when found, it loads us at 1MB, sets
 * the CPU to 32-bit protected mode with flat segments, and jumps to
 * _start. No bootloader of our own is needed.
 *
 * Spec: https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 */

.set MB_MAGIC,    0x1BADB002      /* Multiboot magic GRUB looks for      */
.set MB_ALIGN,    1 << 0          /* align loaded modules on page boundaries */
.set MB_MEMINFO,  1 << 1          /* ask for a memory map                */
.set MB_FLAGS,    (MB_ALIGN | MB_MEMINFO)
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a"
.align 4
    .long MB_MAGIC
    .long MB_FLAGS
    .long MB_CHECKSUM

/* 16 KB stack — plenty for the minimax recursion (max depth 9) plus
 * VGA/keyboard helper calls. Kept in .bss so it costs no disk space. */
.section .bss
.align 16
stack_bottom:
    .skip 131072
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp   /* set up our stack; GRUB gives us none    */
    push %ebx              /* multiboot info pointer (unused for now) */
    push %eax              /* multiboot magic (unused for now)        */

    call kmain              /* hand off to the C kernel                */

    /* kmain should never return, but if it does, halt forever
     * rather than executing garbage. */
    cli
halt_loop:
    hlt
    jmp halt_loop

.size _start, . - _start

.section .note.GNU-stack,"",@progbits