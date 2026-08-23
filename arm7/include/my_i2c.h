// SPDX-License-Identifier: Zlib
// SPDX-FileNotice: Modified from the original version by the BlocksDS project.
//
// Copyright (C) 2011 Dave Murphy (WinterMute)

// I2C control for the ARM7

#ifndef I2C_H
#define I2C_H

#ifdef __cplusplus
extern "C" {
#endif

bool is3ds(void);

void myi2cReboot(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_H
