// SPDX-License-Identifier: MPL-2.0

    .syntax unified
    .section ".crt0","ax"
    .global  _start

    .balign 16
    .arm

// -----------------------------------------------------------------------------
// Startup code
// -----------------------------------------------------------------------------

_start:

    mov     r0, #0x04000000 // IME = 0;
    str     r0, [r0, #0x208]

    mov     r0, #0x12       // Switch to IRQ Mode
    msr     cpsr, r0
    ldr     sp, =__sp_irq   // Set IRQ stack

    mov     r0, #0x13       // Switch to SVC Mode
    msr     cpsr, r0
    ldr     sp, =__sp_svc   // Set SVC stack

    mov     r0, #0x1F       // Switch to System Mode
    msr     cpsr, r0
    ldr     sp, =__sp_usr   // Set system and user stack

    // Copy arm7 binary from LMA to VMA (EWRAM to IWRAM)

    adr     r0, arm7lma         // Calculate ARM7 LMA
    ldr     r1, [r0]
    add     r1, r1, r0
    ldr     r2, =__arm7_start__
    ldr     r3, =__arm7_size__
    bl      CopyMem

    ldr     r0, =__bss_start__  // Clear BSS section to 0x00
    ldr     r1, =__bss_size__
    bl      ClearMem


    ldr     r3, =__libc_init_array  // global constructors
    bl      _blx_r3_stub


    ldr     r3, =main

    // Fall through to _blx_r3_stub to call main()

_blx_r3_stub:
    bx      r3

arm7lma:
    .word   __arm7_lma__ - .
    .pool

// -----------------------------------------------------------------------------
// Clear memory to 0x00
//  r0 = Start Address
//  r1 = Length (if zero, it returns right away)
// -----------------------------------------------------------------------------

ClearMem:

    mov     r2, #3      // Round down to nearest word boundary
    add     r1, r1, r2  // Shouldn't be needed
    bics    r1, r1, r2  // Clear 2 LSB (and set Z)
    bxeq    lr          // Quit if copy size is 0

    mov     r2, #0
ClrLoop:
    stmia   r0!, {r2}
    subs    r1, r1, #4
    bne     ClrLoop
    bx      lr

// -----------------------------------------------------------------------------
// Copy memory
//  r1 = Source Address
//  r2 = Dest Address
//  r3 = Length
// -----------------------------------------------------------------------------

CopyMem:

    mov     r0, #3          // These commands are used in cases where
    add     r3, r3, r0      // the length is not a multiple of 4,
    bics    r3, r3, r0      // even though it should be.
    bxeq    lr              // Length is zero, so exit
CIDLoop:
    ldmia   r1!, {r0}
    stmia   r2!, {r0}
    subs    r3, r3, #4
    bne     CIDLoop
    bx      lr

    .balign 4
    .pool
    .end
