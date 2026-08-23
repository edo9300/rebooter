#include <nds.h>

#include <nds/arm7/nand_crypto.h>
#include <nds/arm7/sdmmc.h>

#define IS_WORD_ALIGNED(buff) (!(((uintptr_t) (buff)) & 0x3))

#define SECTOR_CAP 2047
static sec_t remainingSectors;
static sec_t startingSector;

#define SECTOR_SIZE             0x200
#define AES_BLOCK_SIZE          16


static sec_t setupAesRegs(u32 sectorNum, sec_t totalSectors)
{
    REG_AES_CNT = AES_CNT_MODE(2) |
                  AES_WRFIFO_FLUSH |
                  AES_RDFIFO_FLUSH |
                  // Apply keyslot 3 containing the nand normal key
                  AES_CNT_KEY_APPLY | AES_CNT_KEYSLOT(3) |
                  // Set both input and output expected dma size to 16 words
                  AES_CNT_DMA_WRITE_SIZE(0) | AES_CNT_DMA_READ_SIZE(3);

    // The blkcnt register holds the number of total blocks (16 bytes) to be parsed
    // by the current aes operation
    sec_t toReadSectors = totalSectors;
    if (toReadSectors > SECTOR_CAP)
    {
        toReadSectors = SECTOR_CAP;
    }
    u32 aesBlockCount = toReadSectors * (SECTOR_SIZE / AES_BLOCK_SIZE);
    REG_AES_BLKCNT = aesBlockCount << 16;

    u32 offset = sectorNum * (SECTOR_SIZE / AES_BLOCK_SIZE);
    // The ctr is the base ctr calculated by the sha of the CID + (address / 16)
    // the aes engine will take care of incrementing it automatically
    nandCrypt_SetIV(offset);

    REG_AES_CNT |= AES_CNT_ENABLE;

    return totalSectors - toReadSectors;
}

static void cryptSectorsRead(u32 fifo, void *buffer, u32 numBytes)
{
    const bool word_aligned = IS_WORD_ALIGNED(buffer);
    vu32 *inSdmcFifo32 = (vu32*)fifo;
    if (word_aligned)
    {
        vu32 *out32 = (vu32*)buffer;
        for (unsigned int i = 0; i < numBytes / (4 * 16); ++i)
        {
            for (int j = 0; j < 16; ++j)
            {
                REG_AES_WRFIFO = *inSdmcFifo32;
            }

            while (((REG_AES_CNT >> 0x5) & 0x1F) < 16);

            for (int j = 0; j < 16; ++j)
            {
                *out32++ = REG_AES_RDFIFO;
            }
        }
    }
    else
    {
        vu8 *out8 = (vu8*)buffer;
        for (unsigned int i = 0; i < numBytes / (4 * 16); ++i)
        {
            for (int j = 0; j < 16; ++j)
            {
                REG_AES_WRFIFO = *inSdmcFifo32;
            }

            while (((REG_AES_CNT >> 0x5) & 0x1F) < 16);

            for (int j = 0; j < 16; ++j)
            {
                const u32 tmp = REG_AES_RDFIFO;
                *out8++ = tmp;
                *out8++ = tmp >> 8;
                *out8++ = tmp >> 16;
                *out8++ = tmp >> 24;
            }
        }
    }
}

static void sector_crypt_callback(u32 fifo, void *buffer, u32 numBytes, bool read)
{
	cryptSectorsRead(fifo, buffer, numBytes);

    if (remainingSectors != 0)
    {
        u32 cnt = REG_AES_CNT;
        if ((cnt & AES_CNT_ENABLE) == 0)
        {
            startingSector += SECTOR_CAP;
            remainingSectors = setupAesRegs(startingSector, remainingSectors);
        }
    }
}

bool nandReadSectors(unsigned int sect, unsigned char *buf, unsigned int count)
{
    u32 result;

    if (!nandCrypt_Initialized())
		return SDMMC_ERR_LOCKED;

	startingSector = sect;
	remainingSectors = setupAesRegs(sect, count);
	result = SDMMC_readSectorsCrypt(SDMMC_DEV_eMMC, sect, buf, count, sector_crypt_callback);

    return result == SDMMC_ERR_NONE;
}

bool nandInit(void)
{
	if(SDMMC_init(SDMMC_DEV_eMMC) != SDMMC_ERR_NONE)
		return false;
	nandCrypt_Init();
	return true;
}


