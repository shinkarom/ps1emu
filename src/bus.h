#pragma once
#include <cstdint>
#include <vector>

constexpr auto RAMSize = 2*1024*1024;
constexpr auto BIOSSize = 512*1024;

class Bus {
	public:
		Bus();
		~Bus();
		bool loadBIOS(unsigned char* fileData, int fileSize);
	private:
		std::vector<uint8_t> RAM;
		std::vector<uint8_t> BIOS;
};