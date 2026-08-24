// SPDX-License-Identifier: Zlib
// SPDX-FileNotice: Modified from the original version by the BlocksDS project.
//
// Copyright (C) 2011 Dave Murphy (WinterMute)

// I2C control for the ARM7

#include <nds/bios.h>
#include "my_i2c.h"

#include <nds/ndstypes.h>

#define REG_I2CDATA     (*(vu8 *)0x4004500)
#define REG_I2CCNT      (*(vu8 *)0x4004501)

#define I2CCNT_STOP       BIT(0)
#define I2CCNT_START      BIT(1)
#define I2CCNT_ERROR      BIT(2)
#define I2CCNT_ACK        BIT(4)
#define I2CCNT_WRITE      (0)
#define I2CCNT_READ       BIT(5)
#define I2CCNT_ENABLE_IRQ BIT(6)
#define I2CCNT_ENABLE     BIT(7)
#define I2CCNT_BUSY       BIT(7)


// Registers for Power Management (I2C_PM)
#define I2CREGPM_BATUNK         0x00
#define I2CREGPM_PWRIF          0x10
#define I2CREGPM_PWRCNT         0x11
#define I2CREGPM_MMCPWR         0x12
#define I2CREGPM_BATTERY        0x20
#define I2CREGPM_WIFILED        0x30
#define I2CREGPM_CAMLED         0x31
#define I2CREGPM_VOL            0x40
#define I2CREGPM_BACKLIGHT      0x41
#define I2CREGPM_RESETFLAG      0x70


static inline void myi2cWaitBusy(void)
{
    while (REG_I2CCNT & I2CCNT_BUSY);
}

enum i2cDevices
{
    I2C_CAM0    = 0x7A,
    I2C_CAM1    = 0x78,
    I2C_UNK1    = 0xA0,
    I2C_UNK2    = 0xE0,
    I2C_PM      = 0x4A,
    I2C_UNK3    = 0x40,
    I2C_GPIO    = 0x90
};
static const u32 i2cCurrentDelay = 0x180;

static void i2cDelay(void)
{
    myi2cWaitBusy();
    swiDelay(i2cCurrentDelay);
}

static void i2cStop(u8 arg0)
{
    if (i2cCurrentDelay)
    {
        REG_I2CCNT = arg0 | I2CCNT_ENABLE | I2CCNT_ENABLE_IRQ;
        i2cDelay();
        REG_I2CCNT = I2CCNT_ENABLE | I2CCNT_ENABLE_IRQ | I2CCNT_ERROR | I2CCNT_STOP;
    }
    else
    {
        REG_I2CCNT = arg0 | I2CCNT_ENABLE | I2CCNT_ENABLE_IRQ | I2CCNT_STOP;
    }
}

static u8 i2cGetResult(void)
{
    myi2cWaitBusy();
    return (REG_I2CCNT >> 4) & 0x01;
}

static u8 i2cGetData(void)
{
    myi2cWaitBusy();
    return REG_I2CDATA;
}

static u8 i2cSelectDevice(u8 device)
{
    myi2cWaitBusy();
    REG_I2CDATA = device;
    REG_I2CCNT = I2CCNT_ENABLE | I2CCNT_ENABLE_IRQ | I2CCNT_START;
    return i2cGetResult();
}

static u8 i2cSelectRegister(u8 reg)
{
    i2cDelay();
    REG_I2CDATA = reg;
    REG_I2CCNT = I2CCNT_ENABLE | I2CCNT_ENABLE_IRQ;
    return i2cGetResult();
}

static u8 myi2cWritePM(u8 reg, u8 data)
{
	u8 device = I2C_PM;

    for (int i = 0; i < 8; i++)
    {
        if ((i2cSelectDevice(device) != 0) && (i2cSelectRegister(reg) != 0))
        {
            i2cDelay();
            REG_I2CDATA = data;
            i2cStop(I2CCNT_WRITE);
            if (i2cGetResult() != 0)
                return 1;
        }
        REG_I2CCNT = I2CCNT_ENABLE | I2CCNT_ENABLE_IRQ | I2CCNT_STOP | I2CCNT_ERROR;
    }

    return 0;
}

u8 myi2cReadPM(u8 reg)
{
	u8 device = I2C_PM;

    for (int i = 0; i < 8; i++)
    {
        if ((i2cSelectDevice(device) != 0) && (i2cSelectRegister(reg) != 0))
        {
            i2cDelay();
            if (i2cSelectDevice(device | 1))
            {
                i2cDelay();
                i2cStop(I2CCNT_READ);
                return i2cGetData();
            }
        }

        REG_I2CCNT = I2CCNT_ENABLE | I2CCNT_ENABLE_IRQ | I2CCNT_STOP | I2CCNT_ERROR;
    }

    return 0xff;
}


bool is3ds(void) {
	bool is_3ds = false;

	uint8_t read_val = myi2cReadPM(0x80);
	uint8_t to_write_val = 0x10;
	if(read_val == to_write_val)
		to_write_val = 0x11;

	// Check if writing this register works
	myi2cWritePM(0x80, to_write_val);
	if(myi2cReadPM(0x80) != to_write_val)
		is_3ds = true;

	// Restore initial register state
	myi2cWritePM(0x80, read_val);
	return is_3ds;
}

void myi2cReboot(void) {
	// "isTwl" bit, required for 3ds to reboot reliably
	myi2cWritePM(I2CREGPM_MMCPWR, myi2cReadPM(I2CREGPM_MMCPWR) | BIT(0));
	myi2cWritePM(I2CREGPM_RESETFLAG, 1);
	myi2cWritePM(I2CREGPM_PWRCNT, 1);
	swiDelay(20 * 0x20BA);
}
