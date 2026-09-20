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
    auto rs     = (instr >> 21) & 0b11111;
    auto rt     = (instr >> 16) & 0b11111;
    auto rd     = (instr >> 11) & 0b11111;
	uint32_t imm    = instr & 0xFFFF;
    int32_t  imm_se = (int16_t)(instr & 0xFFFF);
	switch(opcode){
		case 0x00:{ //SPECIAL
			auto opcode2 = instr & 0b111111;
			switch(opcode2){
				case 0x00:{ // SLL
					auto sa = (instr>>6)&0b11111;
					regs[rd] = regs[rt]<<sa; 
					break;
				}
				default:
					throw std::runtime_error(std::format("Unknown special opcode {:02X} at {:08X}", opcode2, pc-4));
			}
			break;
		}
		case 0x09:{ // ADDIU
			regs[rt] = regs[rs] + imm_se;
			break;
		}
		case 0x0D:{ //ORI
			regs[rt] = regs[rs] | imm;
			break;
		}
		case 0x0F:{ //LUI
			regs[rt] = imm<<16;
			break;}
		case 0x2B:{ //SW
			auto addr = regs[rs] + imm_se;
			bus->write32(addr, regs[rt]);
			break;
		}
		default:
			throw std::runtime_error(std::format("Unknown opcode {:02X} at {:08X}", opcode, pc-4));
	}
	regs[0]=0;
}