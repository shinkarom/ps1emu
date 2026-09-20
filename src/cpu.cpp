#include "cpu.h"

CPU::CPU(Bus* bus_arg) {
	bus = bus_arg;
	reset();
}

CPU::~CPU(){
	
}

void CPU::reset() {
	for(auto i=0;i<32;i++){
		regs[i]=0;
	}
	hi=0;
	lo=0;
	pc=0xBFC00000;
	nextPC=pc+4;
}

void CPU::step() {
	auto instr = bus->read32(pc);
	pc+=4;
	nextPC=pc+4;
	auto opcode=(instr>>26)&0b111111;
	switch(opcode){
		case 0x0F:{ //LUI
		auto imm = instr & 0xFFFF;
		auto rt =(instr>>16)&0b11111;
		regs[rt] = imm<<16;
		break;}
		default:
			throw std::runtime_error(std::format("Unknown opcode {:02X} at {:08X}", opcode, pc-4 	));
	}
	regs[0]=0;
}