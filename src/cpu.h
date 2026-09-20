#pragma once
#include <cstdint>
#include <format>
#include "bus.h"

class CPU {
	public:
		CPU(Bus* bus_arg);
		~CPU();
		void reset();
		void step();
	private:
		Bus* bus;
		uint32_t regs[32];
		uint32_t regsCOP0[32];
		uint32_t hi, lo;
		uint32_t pc, nextPC;
		int instrCount = 0;
};