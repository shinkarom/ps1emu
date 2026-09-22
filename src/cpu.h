#pragma once
#include <cstdint>
#include <format>
#include <iostream>
#include "bus.h"

enum class Exception : uint32_t {
    Interrupt           = 0x00,
    LoadAddressError    = 0x04,
    StoreAddressError   = 0x05,
    BusErrorFetch       = 0x06,
    BusErrorData        = 0x07,
    Syscall             = 0x08,
    Break               = 0x09,
    ReservedInstruction = 0x0A,
    CopUnusable         = 0x0B,
    Overflow            = 0x0C
};

class CPU {
    public:
        CPU(Bus* bus_arg);
        ~CPU();
        void reset();
        void step();

    private:
        void triggerException(Exception cause, uint32_t currentPC);

        Bus* bus;
        uint32_t regs[32];
        uint32_t regsCOP0[32];
        uint32_t hi, lo;
        uint32_t pc, nextPC;
        uint32_t regLoad, regValue;
        uint32_t delLoad, delValue;
        uint64_t instrCount;

        bool inDelaySlot;
        bool nextDelaySlot;
};