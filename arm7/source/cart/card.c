#include <nds.h>
#include "card.h"
#include "interrupts.h"

#define BASE_DELAY (100)

static void twlEnableSlot1(void)
{
    int oldIME = enterCriticalSection();

    while ((REG_SCFG_MC & SCFG_MC_PWR_MASK) == SCFG_MC_PWR_REQUEST_OFF)
        swiDelay(1 * BASE_DELAY);

    if ((REG_SCFG_MC & SCFG_MC_PWR_MASK) == SCFG_MC_PWR_OFF)
    {
        REG_SCFG_MC = (REG_SCFG_MC & ~SCFG_MC_PWR_MASK) | SCFG_MC_PWR_RESET;
        swiDelay(10 * BASE_DELAY);
        REG_SCFG_MC = (REG_SCFG_MC & ~SCFG_MC_PWR_MASK) | SCFG_MC_PWR_ON;
        swiDelay(10 * BASE_DELAY);
    }

    leaveCriticalSection(oldIME);
}

static void twlDisableSlot1(void)
{
    int oldIME = enterCriticalSection();

    while ((REG_SCFG_MC & SCFG_MC_PWR_MASK) == SCFG_MC_PWR_REQUEST_OFF)
        swiDelay(1 * BASE_DELAY);

    if ((REG_SCFG_MC & SCFG_MC_PWR_MASK) == SCFG_MC_PWR_ON)
    {
        REG_SCFG_MC = (REG_SCFG_MC & ~SCFG_MC_PWR_MASK) | SCFG_MC_PWR_REQUEST_OFF;
        while ((REG_SCFG_MC & SCFG_MC_PWR_MASK) != SCFG_MC_PWR_OFF)
            swiDelay(1 * BASE_DELAY);
    }

    leaveCriticalSection(oldIME);
}

static inline void waitFrames(int frames) {
    swiDelay(1 * frames * 60 * 0x20BA);
}

void cardWriteCommand(const u8 *command)
{
    REG_AUXSPICNTH = CARD_SPICNTH_ENABLE | CARD_SPICNTH_IRQ;

    for (int index = 0; index < 8; index++)
        REG_CARD_COMMAND[7 - index] = command[index];
}

void cardPolledTransfer(u32 flags, u32 *destination, u32 length, const u8 *command)
{
    u32 data;
    cardWriteCommand(command);

    REG_ROMCTRL = flags;
    u32 *target = destination + length;

    do
    {
        // Read data if available
        if (REG_ROMCTRL & CARD_DATA_READY)
        {
            data = REG_CARD_DATA_RD;
            if (destination != NULL && destination < target)
                *destination++ = data;
        }
    } while (REG_ROMCTRL & CARD_BUSY);
}

void cardParamCommand(u8 command, u32 parameter, u32 flags, u32 *destination, u32 length)
{
    u8 cmdData[8];

    cmdData[7] = (u8)command;
    cmdData[6] = (u8)(parameter >> 24);
    cmdData[5] = (u8)(parameter >> 16);
    cmdData[4] = (u8)(parameter >> 8);
    cmdData[3] = (u8)(parameter >> 0);
    cmdData[2] = 0;
    cmdData[1] = 0;
    cmdData[0] = 0;

    cardPolledTransfer(flags, destination, length, cmdData);
}

void cardReset(void)
{
    const u8 cmdData[8] = { 0, 0, 0, 0, 0, 0, 0, CARD_CMD_DUMMY };

    cardWriteCommand(cmdData);
    REG_ROMCTRL = CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW | CARD_BLK_SIZE(5)
                  | CARD_DELAY2(0x18);
    u32 read = 0;

    do
    {
        if (REG_ROMCTRL & CARD_DATA_READY)
        {
            if (read < 0x2000)
            {
                u32 data = REG_CARD_DATA_RD;
                (void)data;
                read += 4;
            }
        }
    } while (REG_ROMCTRL & CARD_BUSY);
}

u32 cardWriteAndRead(const u8 *command, u32 flags)
{
    cardWriteCommand(command);

    REG_ROMCTRL = flags | CARD_ACTIVATE | CARD_nRESET | CARD_BLK_SIZE(7);

    while (!(REG_ROMCTRL & CARD_DATA_READY));

    return REG_CARD_DATA_RD;
}

u32 cardReadID(u32 flags)
{
    const u8 command[8] = {0, 0, 0, 0, 0, 0, 0, CARD_CMD_HEADER_CHIPID};

    return cardWriteAndRead(command, flags);
}

static void mycardReadHeader(u32 *header)
{
    REG_ROMCTRL = 0;
    REG_AUXSPICNTH = 0;

    swiDelay(167550);

    REG_AUXSPICNTH = CARD_SPICNTH_ENABLE | CARD_SPICNTH_IRQ;
    REG_ROMCTRL = CARD_nRESET | CARD_SEC_SEED;

    while (REG_ROMCTRL & CARD_BUSY);

    cardReset();

    while (REG_ROMCTRL & CARD_BUSY);

    u32 id = cardReadID(CARD_CLK_SLOW);
    bool normalChip = (id & 0x80000000) != 0;        // ROM chip ID MSB, 1T-ROM
    while (REG_ROMCTRL & CARD_BUSY);

    int numblocks = normalChip ? 8 : 1;
    int log_blocksize = normalChip ? 1 : 3;

    for(int i = 0; i < numblocks; ++i) {
        uint32_t flags = CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW
                         | CARD_BLK_SIZE(log_blocksize) | CARD_DELAY1(0x1FFF) | CARD_DELAY2(0x3F);

        int blocksize = (0x100 << log_blocksize);
        cardParamCommand(CARD_CMD_HEADER_READ, i * blocksize, flags, header, blocksize / 4);
        header += blocksize / 4;
    }
}


static void cart_reset(int frames) {
    twlDisableSlot1();
    waitFrames(frames);
    twlEnableSlot1();

    while(REG_ROMCTRL & CARD_BUSY)
        waitFrames(1);

    waitFrames(2);
    REG_ROMCTRL = CARD_nRESET;
    waitFrames(15);
}

static tDSiHeader header;

uint64_t readTid() {
    cart_reset(20);
    mycardReadHeader((u32*)&header);
    if (header.ndshdr.unitCode == 0) {
        header.tid_high = 0;
        header.tid_low = __builtin_bswap32(*(u32*)header.ndshdr.gameCode);
    }
    return header.tid_low | (((uint64_t)header.tid_high) << 32);
}
