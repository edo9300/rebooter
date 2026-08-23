// SPDX-License-Identifier: Zlib
// SPDX-FileNotice: Modified from the original version by the BlocksDS project.
//
// Copyright (C) 2005 Dave Murphy (WinterMute)
// Copyright (C) 2025 Antonio Niño Díaz

#ifdef ARM7
#include <nds/arm7/i2c.h>
#include <nds/arm7/tmio.h>
#endif
#include <nds/interrupts.h>
#include <nds/ipc.h>
#include <nds/system.h>

void IntrMain(void); // Prototype for assembly interrupt dispatcher

VoidFn irqTable[MAX_INTERRUPTS];
VoidFn irqTableAUX[MAX_INTERRUPTS_AUX];

void irqInitAUX(void)
{
    // Set all interrupts to dummy functions.
    for (int i = 0; i < MAX_INTERRUPTS_AUX; i++)
        irqTableAUX[i] = NULL;
}

static void __irqSet(u32 mask, VoidFn handler, VoidFn irqTable_[], u32 max)
{
    // Set every bit in order. Skip bits which are out of bounds.
    while (mask)
    {
        u32 i = __builtin_clz(mask) ^ 0x1F;
        if (i < max)
            irqTable_[i] = handler;
        mask &= ~(1 << i);
    }
}

void irqSet(u32 mask, VoidFn handler)
{
    int oldIME = enterCriticalSection();

    __irqSet(mask, handler, irqTable, MAX_INTERRUPTS);

    if (mask & IRQ_VBLANK)
        REG_DISPSTAT |= DISP_VBLANK_IRQ;
    if (mask & IRQ_HBLANK)
        REG_DISPSTAT |= DISP_HBLANK_IRQ;
    if (mask & IRQ_IPC_SYNC)
        REG_IPC_SYNC |= IPC_SYNC_IRQ_ENABLE;

    leaveCriticalSection(oldIME);
}

void irqInitHandler(VoidFn handler)
{
    REG_IME = 0;
    REG_IE = 0;
    REG_IF = ~0;

#ifdef ARM7
    if (true)
    {
        REG_AUXIE = 0;
        REG_AUXIF = ~0;
    }
#endif
    IRQ_HANDLER = handler;
}

void irqInit(void)
{
    irqInitHandler(IntrMain);

    // Set all interrupts to dummy functions.
    for (int i = 0; i < MAX_INTERRUPTS; i++)
        irqTable[i] = NULL;

    irqInitAUX();
	TMIO_init();

    REG_IME = 1; // Enable interrupts
}

void irqEnable(uint32_t irq)
{
    int oldIME = enterCriticalSection();

    if (irq & IRQ_VBLANK)
        REG_DISPSTAT |= DISP_VBLANK_IRQ;
    if (irq & IRQ_HBLANK)
        REG_DISPSTAT |= DISP_HBLANK_IRQ;
    if (irq & IRQ_VCOUNT)
        REG_DISPSTAT |= DISP_YTRIGGER_IRQ;
    if (irq & IRQ_IPC_SYNC)
        REG_IPC_SYNC |= IPC_SYNC_IRQ_ENABLE;

    REG_IE |= irq;

    leaveCriticalSection(oldIME);
}

void irqDisable(uint32_t irq)
{
    int oldIME = enterCriticalSection();

    if (irq & IRQ_VBLANK)
        REG_DISPSTAT &= ~DISP_VBLANK_IRQ;
    if (irq & IRQ_HBLANK)
        REG_DISPSTAT &= ~DISP_HBLANK_IRQ;
    if (irq & IRQ_VCOUNT)
        REG_DISPSTAT &= ~DISP_YTRIGGER_IRQ;
    if (irq & IRQ_IPC_SYNC)
        REG_IPC_SYNC &= ~IPC_SYNC_IRQ_ENABLE;

    REG_IE &= ~irq;

    leaveCriticalSection(oldIME);
}

void irqClear(u32 mask)
{
    int oldIME = enterCriticalSection();

    __irqSet(mask, NULL, irqTable, MAX_INTERRUPTS);
    irqDisable(mask);

    leaveCriticalSection(oldIME);
}

#ifdef ARM7

void (irqSetAUX)(u32 mask, VoidFn handler)
{
    int oldIME = enterCriticalSection();

    __irqSet(mask, handler, irqTableAUX, MAX_INTERRUPTS_AUX);

    leaveCriticalSection(oldIME);
}

void (irqEnableAUX)(uint32_t irq)
{
    int oldIME = enterCriticalSection();

    REG_AUXIE |= irq;

    leaveCriticalSection(oldIME);
}

#endif
