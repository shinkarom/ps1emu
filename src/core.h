#pragma once
#include <cstdint>
#include <span>

#include "bus.h"

class Core {
	public:
		Core();
		~Core();
		void stepFrame();
		uint16_t* getFramebuffer();
		std::span<const int16_t> getAudioSamples();
		void clearAudioSamples();
		
	private:
		Bus bus;
};