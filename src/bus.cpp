#include "bus.h"
#include <cstring>
#include <cstdio>

Bus::Bus(): RAM(RAMSize), BIOS(BIOSSize){
	
}

Bus::~Bus(){
	
}

bool Bus::loadBIOS(unsigned char* fileData, int fileSize) {
	auto loadedSize = fileSize > BIOSSize ? BIOSSize : fileSize;
	std::memcpy(BIOS.data(), fileData, loadedSize);
	return true;
}