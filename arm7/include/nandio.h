#pragma once

#ifdef __cplusplus
extern "C" {
#endif

bool nandReadSectors(unsigned int sect, unsigned char *buf, unsigned int count);
bool nandInit(void);

#ifdef __cplusplus
}
#endif
