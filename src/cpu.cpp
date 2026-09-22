#include "cpu.h"
#include <cstdint>
#include <format>
#include <stdexcept>

CPU::CPU(Bus* bus_arg) {
    bus = bus_arg;
    reset();
}

CPU::~CPU() {
}

void CPU::reset() {
    for (auto i = 0; i < 32; i++) {
        regs[i] = 0;
        regsCOP0[i] = 0;
    }
    hi = 0;
    lo = 0;
    pc = 0xBFC00000;
    nextPC = pc + 4;
    regLoad = 0;
    regValue = 0;
    delLoad = 0;
    delValue = 0;
    instrCount = 0;
    inDelaySlot = false;
    nextDelaySlot = false;

    // Status Register (SR) reset value: BEV = 1 (Boot Exception Vectors in ROM)
    regsCOP0[12] = 0x10900000;
}

void CPU::triggerException(Exception cause, uint32_t currentPC) {
    // 1. Shift COP0 Status (Reg 12) mode bits (KUc/IEc -> KUp/IEp -> KUo/IEo)
    uint32_t sr = regsCOP0[12];
    uint32_t mode = sr & 0x3F;
    regsCOP0[12] = (sr & ~0x3F) | ((mode << 2) & 0x3F);

    // 2. Set COP0 Cause (Reg 13)
    // Clear ExcCode (bits 6:2) and BD (bit 31), then set new ExcCode
    regsCOP0[13] &= ~0x8000007C;
    regsCOP0[13] |= (static_cast<uint32_t>(cause) << 2);

    // 3. Set COP0 EPC (Reg 14)
    if (inDelaySlot) {
        regsCOP0[13] |= (1u << 31); // Set BD (Branch Delay) bit in Cause
        regsCOP0[14] = currentPC - 4; // Point EPC back to the branch instruction
    } else {
        regsCOP0[14] = currentPC;
    }

    // 4. Jump to Exception Vector
    // If BEV bit (bit 22) is set -> 0xBFC00180 (ROM), else 0x80000080 (RAM)
    if (regsCOP0[12] & (1 << 22)) {
        pc = 0xBFC00180;
    } else {
        pc = 0x80000080;
    }
    nextPC = pc + 4;
}

void CPU::step() {
    auto currentPC = pc;
    auto instr = bus->read32(pc);

    // Advance delay slot tracker
    inDelaySlot = nextDelaySlot;
    nextDelaySlot = false;

    // Advance PC pipeline
    pc = nextPC;
    nextPC = pc + 4;

    // Decode fields
    auto opcode = (instr >> 26) & 0b111111;
    auto rs     = (instr >> 21) & 0b11111;
    auto rt     = (instr >> 16) & 0b11111;
    auto rd     = (instr >> 11) & 0b11111;
    auto sa     = (instr >> 6)  & 0b11111;

    uint32_t imm    = instr & 0xFFFF;
    int32_t  imm_se = static_cast<int16_t>(instr & 0xFFFF);

    switch (opcode) {
        // =====================================================================
        // SPECIAL INSTRUCTIONS (Opcode 0x00)
        // =====================================================================
        case 0x00: {
            auto funct = instr & 0b111111;
            switch (funct) {
                case 0x00: regs[rd] = regs[rt] << sa; break; // SLL
                case 0x02: regs[rd] = regs[rt] >> sa; break; // SRL
                case 0x03: regs[rd] = static_cast<int32_t>(regs[rt]) >> sa; break; // SRA
                case 0x04: regs[rd] = regs[rt] << (regs[rs] & 0x1F); break; // SLLV
                case 0x06: regs[rd] = regs[rt] >> (regs[rs] & 0x1F); break; // SRLV
                case 0x07: regs[rd] = static_cast<int32_t>(regs[rt]) >> (regs[rs] & 0x1F); break; // SRAV

                case 0x08: // JR
                    nextPC = regs[rs];
                    nextDelaySlot = true;
                    break;

                case 0x09: // JALR
                    nextPC = regs[rs];
                    regs[rd] = pc + 4;
                    nextDelaySlot = true;
                    break;

                case 0x0C: // SYSCALL
                    triggerException(Exception::Syscall, currentPC);
                    break;

                case 0x0D: // BREAK
                    triggerException(Exception::Break, currentPC);
                    break;

                case 0x10: regs[rd] = hi; break; // MFHI
                case 0x11: hi = regs[rs]; break; // MTHI
                case 0x12: regs[rd] = lo; break; // MFLO
                case 0x13: lo = regs[rs]; break; // MTLO

                case 0x18: { // MULT
                    int64_t lhs = static_cast<int32_t>(regs[rs]);
                    int64_t rhs = static_cast<int32_t>(regs[rt]);
                    uint64_t res = static_cast<uint64_t>(lhs * rhs);
                    lo = static_cast<uint32_t>(res);
                    hi = static_cast<uint32_t>(res >> 32);
                    break;
                }
                case 0x19: { // MULTU
                    uint64_t lhs = regs[rs];
                    uint64_t rhs = regs[rt];
                    uint64_t res = lhs * rhs;
                    lo = static_cast<uint32_t>(res);
                    hi = static_cast<uint32_t>(res >> 32);
                    break;
                }
                case 0x1A: { // DIV
                    int32_t n = static_cast<int32_t>(regs[rs]);
                    int32_t d = static_cast<int32_t>(regs[rt]);
                    if (d == 0) {
                        hi = static_cast<uint32_t>(n);
                        lo = (n >= 0) ? 0xFFFFFFFF : 1;
                    } else if (static_cast<uint32_t>(n) == 0x80000000 && d == -1) {
                        hi = 0;
                        lo = 0x80000000;
                    } else {
                        lo = static_cast<uint32_t>(n / d);
                        hi = static_cast<uint32_t>(n % d);
                    }
                    break;
                }
                case 0x1B: { // DIVU
                    uint32_t n = regs[rs];
                    uint32_t d = regs[rt];
                    if (d == 0) {
                        hi = n;
                        lo = 0xFFFFFFFF;
                    } else {
                        lo = n / d;
                        hi = n % d;
                    }
                    break;
                }
                case 0x20: // ADD
                case 0x21: // ADDU
                    regs[rd] = regs[rs] + regs[rt];
                    break;

                case 0x22: // SUB
                case 0x23: // SUBU
                    regs[rd] = regs[rs] - regs[rt];
                    break;

                case 0x24: regs[rd] = regs[rs] & regs[rt]; break;   // AND
                case 0x25: regs[rd] = regs[rs] | regs[rt]; break;   // OR
                case 0x26: regs[rd] = regs[rs] ^ regs[rt]; break;   // XOR
                case 0x27: regs[rd] = ~(regs[rs] | regs[rt]); break; // NOR

                case 0x2A: // SLT
                    regs[rd] = (static_cast<int32_t>(regs[rs]) < static_cast<int32_t>(regs[rt])) ? 1 : 0;
                    break;
                case 0x2B: // SLTU
                    regs[rd] = (regs[rs] < regs[rt]) ? 1 : 0;
                    break;

                default:
                    triggerException(Exception::ReservedInstruction, currentPC);
                    break;
            }
            break;
        }

        // =====================================================================
        // REGIMM INSTRUCTIONS (Opcode 0x01)
        // =====================================================================
        case 0x01: {
            auto targetAddr = pc + (imm_se * 4);
            auto signedVal = static_cast<int32_t>(regs[rs]);
            nextDelaySlot = true;

            switch (rt) {
                case 0x00: { // BLTZ
                    if (signedVal < 0) nextPC = targetAddr;
                    break;
                }
                case 0x01: { // BGEZ
                    if (signedVal >= 0) nextPC = targetAddr;
                    break;
                }
                case 0x10: { // BLTZAL
                    bool shouldBranch = (signedVal < 0);
                    regs[31] = pc + 4;
                    if (shouldBranch) nextPC = targetAddr;
                    break;
                }
                case 0x11: { // BGEZAL
                    bool shouldBranch = (signedVal >= 0);
                    regs[31] = pc + 4;
                    if (shouldBranch) nextPC = targetAddr;
                    break;
                }
                default:
                    triggerException(Exception::ReservedInstruction, currentPC);
                    break;
            }
            break;
        }

        // =====================================================================
        // JUMPS & BRANCHES
        // =====================================================================
        case 0x02: { // J
            auto addr = (instr & 0x3FFFFFF) << 2;
            nextPC = (pc & 0xF0000000) | addr;
            nextDelaySlot = true;
            break;
        }
        case 0x03: { // JAL
            auto addr = (instr & 0x3FFFFFF) << 2;
            regs[31] = pc + 4;
            nextPC = (pc & 0xF0000000) | addr;
            nextDelaySlot = true;
            break;
        }
        case 0x04: { // BEQ
            if (regs[rs] == regs[rt]) nextPC = pc + (imm_se * 4);
            nextDelaySlot = true;
            break;
        }
        case 0x05: { // BNE
            if (regs[rs] != regs[rt]) nextPC = pc + (imm_se * 4);
            nextDelaySlot = true;
            break;
        }
        case 0x06: { // BLEZ
            if (static_cast<int32_t>(regs[rs]) <= 0) nextPC = pc + (imm_se * 4);
            nextDelaySlot = true;
            break;
        }
        case 0x07: { // BGTZ
            if (static_cast<int32_t>(regs[rs]) > 0) nextPC = pc + (imm_se * 4);
            nextDelaySlot = true;
            break;
        }

        // =====================================================================
        // ARITHMETIC / LOGIC IMMEDIATE
        // =====================================================================
        case 0x08: // ADDI
        case 0x09: // ADDIU
            regs[rt] = regs[rs] + imm_se;
            break;
        case 0x0A: // SLTI
            regs[rt] = (static_cast<int32_t>(regs[rs]) < imm_se) ? 1 : 0;
            break;
        case 0x0B: // SLTIU
            regs[rt] = (regs[rs] < static_cast<uint32_t>(imm_se)) ? 1 : 0;
            break;
        case 0x0C: // ANDI
            regs[rt] = regs[rs] & imm;
            break;
        case 0x0D: // ORI
            regs[rt] = regs[rs] | imm;
            break;
        case 0x0E: // XORI
            regs[rt] = regs[rs] ^ imm;
            break;
        case 0x0F: // LUI
            regs[rt] = imm << 16;
            break;

        // =====================================================================
        // SYSTEM CONTROL COPROCESSOR (COP0)
        // =====================================================================
        case 0x10: {
            switch (rs) {
                case 0x00: // MFC0 (With load delay)
                    regLoad = rt;
                    regValue = regsCOP0[rd];
                    break;
                case 0x04: // MTC0
                    regsCOP0[rd] = regs[rt];
                    break;
                case 0x10: { // RFE (Return From Exception)
                    if ((instr & 0x3F) == 0x10) {
                        uint32_t mode = regsCOP0[12] & 0x3F;
                        regsCOP0[12] = (regsCOP0[12] & ~0x3F) | (mode >> 2);
                    } else {
                        triggerException(Exception::ReservedInstruction, currentPC);
                    }
                    break;
                }
                default:
                    triggerException(Exception::ReservedInstruction, currentPC);
                    break;
            }
            break;
        }

        // Coprocessor unusable exceptions
        case 0x11: triggerException(Exception::CopUnusable, currentPC); break; // COP1
        case 0x12: break; // COP2 (GTE on PS1) - Stubbed
        case 0x13: triggerException(Exception::CopUnusable, currentPC); break; // COP3

        // =====================================================================
        // LOADS (Routed through load delay pipeline)
        // =====================================================================
        case 0x20: { // LB
            auto addr = regs[rs] + imm_se;
            regLoad = rt;
            regValue = static_cast<int32_t>(static_cast<int8_t>(bus->read8(addr)));
            break;
        }
        case 0x21: { // LH
            auto addr = regs[rs] + imm_se;
            regLoad = rt;
            regValue = static_cast<int32_t>(static_cast<int16_t>(bus->read16(addr)));
            break;
        }
        case 0x22: { // LWL (Load Word Left)
            auto addr = regs[rs] + imm_se;
            uint32_t aligned = addr & ~3;
            uint32_t shift = (addr & 3) * 8;
            uint32_t mem = bus->read32(aligned);
            
            uint32_t cur = (regLoad == rt) ? regValue : regs[rt];
            regLoad = rt;
            regValue = (cur & (0x00FFFFFF >> shift)) | (mem << (24 - shift));
            break;
        }
        case 0x23: { // LW
            auto addr = regs[rs] + imm_se;
            regLoad = rt;
            regValue = bus->read32(addr);
            break;
        }
        case 0x24: { // LBU
            auto addr = regs[rs] + imm_se;
            regLoad = rt;
            regValue = bus->read8(addr);
            break;
        }
        case 0x25: { // LHU
            auto addr = regs[rs] + imm_se;
            regLoad = rt;
            regValue = bus->read16(addr);
            break;
        }
        case 0x26: { // LWR (Load Word Right)
            auto addr = regs[rs] + imm_se;
            uint32_t aligned = addr & ~3;
            uint32_t shift = (addr & 3) * 8;
            uint32_t mem = bus->read32(aligned);

            uint32_t cur = (regLoad == rt) ? regValue : regs[rt];
            regLoad = rt;
            regValue = (cur & (0xFFFFFF00 << (24 - shift))) | (mem >> shift);
            break;
        }

        // =====================================================================
        // STORES
        // =====================================================================
        case 0x28: { // SB
            if (regsCOP0[12] & 0x10000) break; // Isolate cache
            auto addr = regs[rs] + imm_se;
            bus->write8(addr, regs[rt] & 0xFF);
            break;
        }
        case 0x29: { // SH
            if (regsCOP0[12] & 0x10000) break;
            auto addr = regs[rs] + imm_se;
            bus->write16(addr, regs[rt] & 0xFFFF);
            break;
        }
        case 0x2A: { // SWL (Store Word Left)
            if (regsCOP0[12] & 0x10000) break;
            auto addr = regs[rs] + imm_se;
            uint32_t aligned = addr & ~3;
            uint32_t shift = (addr & 3) * 8;
            uint32_t mem = bus->read32(aligned);
            uint32_t val = regs[rt];

            uint32_t result = (mem & (0xFFFFFF00 << shift)) | (val >> (24 - shift));
            bus->write32(aligned, result);
            break;
        }
        case 0x2B: { // SW
            if (regsCOP0[12] & 0x10000) break;
            auto addr = regs[rs] + imm_se;
            bus->write32(addr, regs[rt]);
            break;
        }
        case 0x2E: { // SWR (Store Word Right)
            if (regsCOP0[12] & 0x10000) break;
            auto addr = regs[rs] + imm_se;
            uint32_t aligned = addr & ~3;
            uint32_t shift = (addr & 3) * 8;
            uint32_t mem = bus->read32(aligned);
            uint32_t val = regs[rt];

            uint32_t result = (mem & (0x00FFFFFF >> (24 - shift))) | (val << shift);
            bus->write32(aligned, result);
            break;
        }

        // Coprocessor Loads/Stores
        case 0x30: triggerException(Exception::CopUnusable, currentPC); break; // LWC0
        case 0x31: triggerException(Exception::CopUnusable, currentPC); break; // LWC1
        case 0x32: break; // LWC2 (GTE) - Stubbed
        case 0x33: triggerException(Exception::CopUnusable, currentPC); break; // LWC3
        case 0x38: triggerException(Exception::CopUnusable, currentPC); break; // SWC0
        case 0x39: triggerException(Exception::CopUnusable, currentPC); break; // SWC1
        case 0x3A: break; // SWC2 (GTE) - Stubbed
        case 0x3B: triggerException(Exception::CopUnusable, currentPC); break; // SWC3

        default:
            triggerException(Exception::ReservedInstruction, currentPC);
            break;
    }

    // =========================================================================
    // LOAD DELAY PIPELINE STEPPING
    // =========================================================================
    if (delLoad != 0) {
        regs[delLoad] = delValue;
    }
    delLoad = regLoad;
    delValue = regValue;
    regLoad = 0;
    regValue = 0;

    // Register $0 is hardwired to 0
    regs[0] = 0;
    instrCount++;
}