#pragma once
#include "State.h"

namespace MIPS {

    class RegisterFile {
    public:
        explicit RegisterFile(CpuState& state) : state(state) {}

        // Read Register (combinational read)
        [[nodiscard]]
        uint32_t read(uint8_t regIndex) const {
            // Hardware Rule: Register 0 is hardwired to ground (0)
            if (regIndex == 0) return 0;
            return state.regs[regIndex];
        }

        // Write Register (synchronous write)
        void write(uint8_t regIndex, uint32_t value) {
            // Hardware Rule: Writes to $zero are ignored
            if (regIndex == 0) return;
            state.regs[regIndex] = value;
        }

        // Debug helper: Print all non-zero registers
        void dump() const;

    private:
        CpuState& state; // Reference to the master state
    };

}