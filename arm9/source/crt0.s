// SPDX-License-Identifier: MPL-2.0

    .syntax unified
    .arch   armv5te
    .cpu    arm946e-s

    .balign 16
    .arm

.section ".crt0","ax"
.org 0x800

    .section ".crt0","ax"
    .global  _start

// -----------------------------------------------------------------------------
// Startup code
// -----------------------------------------------------------------------------

_start:

    mov     r0, #0x04000000 // IME = 0;
    str     r0, [r0, #0x208]

    // Set sensible stacks to allow BIOS calls

    mov     r0, #0x13       // Switch to SVC Mode
    msr     cpsr, r0
    mov     r1, #0x03000000
    sub     r1, r1, #0x1000
    mov     sp, r1
    mov     r0, #0x1F       // Switch to System Mode
    msr     cpsr, r0
    sub     r1, r1, #0x100
    mov     sp, r1

    ldr     r3, =__libnds_mpu_setup
    blx     r3

    mov     r0, #0x12       // Switch to IRQ Mode
    msr     cpsr, r0
    ldr     sp, =__sp_irq   // Set IRQ stack

    mov     r0, #0x13       // Switch to SVC Mode
    msr     cpsr, r0
    ldr     sp, =__sp_svc   // Set SVC stack

    mov     r0, #0x1F       // Switch to System Mode
    msr     cpsr, r0
    ldr     sp, =__sp_usr   // Set user stack

    ldr     r3, =main
    bx      r3

    .end
