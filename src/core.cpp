#include <core.h>

Core::Core(): cpu(&bus){
	
}

Core::~Core(){
	
}

void Core::stepFrame(){
	for(auto i=0; i<cpuClockPerFrame;i++){
		cpu.step();
	}
}

uint16_t* Core::getFramebuffer(){
	return nullptr;
}

std::span<const int16_t> Core::getAudioSamples() {
	return {};
}

void Core::clearAudioSamples() {
	
}

void Core::setButton(int port, Button button, bool pressed){
	
}

void Core::setAxis(int port, Axis axis, float value) {
	
}

DisplayRegion Core::getDisplayRegion() const {
	return {0, 0, 320, 240};
}

bool Core::loadBIOS(unsigned char* fileData, int fileSize){
	return bus.loadBIOS(fileData, fileSize);
}