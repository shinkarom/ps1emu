#pragma once
#include <cstdint>
#include "bus.h"

class CPU {
	public:
		CPU(Bus* bus_arg);
		~CPU();
	private:
		Bus* bus;
};