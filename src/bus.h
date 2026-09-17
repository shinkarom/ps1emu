#pragma once
#include <cstdint>
#include <vector>

constexpr auto RAMSize = 2*1024*1024;
constexpr auto BIOSSize = 512*1024;
constexpr auto ScratchpadSize = 1024;

class Bus {
	public:
		Bus();
		~Bus();
		bool loadBIOS(unsigned char* fileData, int fileSize);
		uint8_t read8(uint32_t address);
		uint16_t read16(uint32_t address);
		uint32_t read32(uint32_t address);
		void write8(uint32_t address, uint8_t value);
		void write16(uint32_t address, uint16_t value);
		void write32(uint32_t address, uint32_t value);
	private:
		std::vector<uint8_t> RAM;
		std::vector<uint8_t> BIOS;
		std::vector<uint8_t> Scratchpad;
		uint32_t resolveAddress(uint32_t address);
};