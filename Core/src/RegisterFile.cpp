#include "Core/RegisterFile.h"
#include <iostream>
#include <format>

namespace MIPS {

    void RegisterFile::dump() const {
        std::cout << std::format("--- Register File Dump ---\n");
        std::cout << std::format("PC: {:#010x}\n", state.pc);

        for (int i = 0; i < 32; ++i) {
            // Only print registers that have data (reduce noise)
            if (state.regs[i] != 0) {
                std::cout << std::format("R{:02d} ($...): {:#010x} ({})\n", i, state.regs[i], static_cast<int32_t>(state.regs[i]));
            }
        }
        std::cout << std::format("--------------------------\n");
    }

}