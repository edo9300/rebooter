#include <nds.h>
#include <array>
#include <string_view>
#include <span>
#include <algorithm>
#include <ranges>

#include "card.h"
#include "my_i2c.h"

#include "fatfs/ff.h"

static char tmdPath[100];
std::array<char, 8> uintToHex(unsigned int hex) {
	std::array<char, 8> res;
	for(auto& elem : res | std::views::reverse) {
		if(auto val = (hex & 0xF); val > 9) {
			elem = 'a' + (val - 10);
		} else {
			elem = '0' + val;
		}
		hex >>= 4;
	}
	return res;
}

auto constructLauncherPath(unsigned int launcher_tid) {
	auto [_, out] = std::ranges::copy("nand:/title/00030017/", std::begin(tmdPath));
	auto [_, out1] = std::ranges::copy(uintToHex(launcher_tid), out - 1);
	auto [_, out2] = std::ranges::copy("/content/", out1);
	return out2 - 1;
}

void parseLauncherInfo(unsigned int launcher_tid) {
	auto launcherPathEndIt = constructLauncherPath(launcher_tid);

	DIR dirp;
	if(f_opendir(&dirp, tmdPath) != FR_OK) {
		*tmdPath = 0;
		return;
	}

	FILINFO fno;
	
	while(true) {
		if(f_readdir(&dirp, &fno) != FR_OK)
			break;
		
		if (fno.fattrib & AM_DIR) 
			continue;

		std::string_view filename{fno.fname};
		
		if (filename.empty())
			break;

		if(filename.size() != 12 || !filename.ends_with(".APP"))
			continue;

		std::ranges::copy(filename, launcherPathEndIt);
		FIL file;
		f_open(&file, tmdPath, FA_READ);
		UINT bytes_read = 0;
		uint8_t buff[0x20];
		f_read(&file, buff, 0x20, &bytes_read);
		f_close(&file);

		static constexpr std::array<uint8_t, 0xF> hna
			{'L','A','U','N','C','H','E','R','\0','\0','\0','\0','H','N','A'};

		if(bytes_read != 0x20 || !std::ranges::equal(hna, std::span{buff, buff+0xF})) {
			*tmdPath = 0;
			continue;
		}
	}
}

void retrieveInstalledLauncherInfo() {
	// HNAA in case of failure, best we can do
	uint32_t launcherTid = 0x484e4141;
	{
		FIL file;
		if(f_open(&file, "nand:/sys/HWINFO_S.dat", FA_READ) == FR_OK) {
			f_lseek(&file, 0xA0);
			UINT bytes_read = 0;
			f_read(&file, &launcherTid, sizeof(uint32_t), &bytes_read);
			if(bytes_read != 4)
				launcherTid = 0x484e4141;
			f_close(&file);
		}
	}
	parseLauncherInfo(launcherTid);
}

static void unlaunchSetFilename() {
	if(*tmdPath == 0)
		return;
	std::ranges::fill(std::span{(volatile uint8_t*)0x02000800, 0x400}, 0);
	std::ranges::copy("AutoLoadInfo", (volatile uint8_t*)0x02000800);
	*(volatile uint16_t*)(0x0200080C) = 0x3F0;	// Unlaunch Length for CRC16 (fixed, must be 3F0h)
	*(volatile uint32_t*)(0x02000810) = 3;		// Bit 0 and 1 set, Load the title at 2000838h and use colors 2000814h
	*(volatile uint16_t*)(0x02000814) = 0x7FFF;	// Unlaunch Upper screen BG color (0..7FFFh)
	*(volatile uint16_t*)(0x02000816) = 0x7FFF;	// Unlaunch Lower screen BG color (0..7FFFh)

	// Unlaunch Device:/Path/Filename.ext (16bit Unicode,end by 0000h)
	std::ranges::copy(tmdPath, (volatile uint16_t*)0x02000838);

	*(volatile uint16_t*)(0x0200080E) = swiCRC16(0xFFFF, (void*)0x02000810, 0x3F0);		// Unlaunch CRC16
}

static void setNintendoAutoboot(uint64_t tid) {
	std::ranges::fill(std::span{(volatile uint8_t*)0x02000300, 0x100}, 0);

	// AutoLoad ID ("TLNC")
	std::ranges::copy("TLNC", (volatile uint8_t*)0x02000300);
	
	// AutoLoad Unknown/unused (usually 01h)
	*(volatile uint8_t*)(0x02000304) = 0x01;
	
	// AutoLoad Length of data at 2000308h (01h..18h,for CRC,18h=norm)
	*(volatile uint8_t*)(0x02000305) = 0x18;

	// AutoLoad Old Title ID (former title) (can be 0=anonymous)
	*(volatile uint64_t*)(0x02000308) = tid;

	// AutoLoad New Title ID (new title to be started,0=none/launcher)
	*(volatile uint64_t*)(0x02000310) = tid;

	// AutoLoad Flags (bit0, 1-3, 4, 5,6,7) 
	// BIT 0 "is valid"
	// BIT 1 "cart boot"
	// BIT 4 "skip logo" (mandatory on 3ds as it's not implemented there)
	*(volatile uint32_t*)(0x02000318) = BIT(0) | BIT(1) | BIT(4);

	// AutoLoad CRC16 of data at 2000308h (with initial value FFFFh)
	*(volatile uint16_t*)(0x02000306) = swiCRC16(0xFFFF, (void*)0x02000308, 0x18);
}

void fail() {
	writePowerManagement(PM_CONTROL_REG, PM_SYSTEM_PWR);
	while(1);
}

[[gnu::noinline,gnu::used]] void TWL_FUNC(dummy)(){while(1);}

int main() {
	if((REG_SCFG_EXT & BIT(31)) == 0)
		fail();
	REG_SCFG_EXT |= SCFG_EXT_I2C | SCFG_EXT_SDMMC | SCFG_EXT_INTERRUPT | SCFG_EXT_AES;
	if(!is3ds()) {
		FATFS fs_info;
		irqInit();

		if (f_mount(&fs_info, "nand:/", 0) == FR_OK) {
			retrieveInstalledLauncherInfo();
		}
		unlaunchSetFilename();
	}
	setNintendoAutoboot(readTid());
	myi2cReboot();
	dummy();
	return 0;
}
