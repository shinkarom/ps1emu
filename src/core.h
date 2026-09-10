#pragma once
#include <cstdint>

#include "bus.h"

class Core {
	public:
		Core();
		~Core();
		void stepFrame();
		uint16_t* getFramebuffer();
		
	private:
		Bus bus;
};