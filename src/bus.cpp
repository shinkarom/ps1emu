#include "bus.h"
#include <cstring>
#include <cstdio>

Bus::Bus(): RAM(RAMSize), BIOS(BIOSSize), Scratchpad(ScratchpadSize){
	
}

Bus::~Bus(){
	
}

bool Bus::loadBIOS(unsigned char* fileData, int fileSize) {
	auto loadedSize = fileSize > BIOSSize ? BIOSSize : fileSize;
	std::memcpy(BIOS.data(), fileData, loadedSize);
	return true;
}

uint32_t Bus::resolveAddress(uint32_t address){
	return address & 0x1FFFFFFF;
}

uint8_t Bus::read8(uint32_t address) {
	auto resolved = resolveAddress(address);
	if(resolved>=0&&resolved<RAMSize){
		return RAM[resolved];
	} else if (resolved>=0x1FC00000&&resolved<0x1FC00000+BIOSSize){
		return BIOS[resolved-0x1FC00000];
	} else if (resolved>=0x1F800000&&resolved<0x1F800000+ScratchpadSize) {
		return Scratchpad[resolved-0x1F800000];
	}
	return 0;
}

uint16_t Bus::read16(uint32_t address){
	auto lo=read8(address);
	auto hi=read8(address+1);
	return (hi<<8)|lo;
}

uint32_t Bus::read32(uint32_t address){
	auto lo=read16(address);
	auto hi=read16(address+2);
	return (static_cast<uint32_t>(hi)<<16)|lo;
}

void Bus::write8(uint32_t address, uint8_t value){
	auto resolved = resolveAddress(address);
	if(resolved>=0&&resolved<RAMSize){
		RAM[resolved] = value;
	} else if (resolved>=0x1F800000&&resolved<0x1F800000+ScratchpadSize) {
		Scratchpad[resolved-0x1F800000]= value;
	}
}

void Bus::write16(uint32_t address, uint16_t value){
	auto lo=value&0xFF;
	auto hi=(value>>8)&0xFF;
	write8(address, lo);
	write8(address+1, hi);
}

void Bus::write32(uint32_t address, uint32_t value){
	auto lo=value&0xFFFF;
	auto hi=(value>>16)&0xFFFF;
	write16(address, lo);
	write16(address+2, hi);
}