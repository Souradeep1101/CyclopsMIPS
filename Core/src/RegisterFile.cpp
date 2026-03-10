#include "Core/RegisterFile.h"
#include <print> // C++23

namespace MIPS {

    void RegisterFile::dump() const {
        std::println("--- Register File Dump ---");
        std::println("PC: {:#010x}", state.pc);

        for (int i = 0; i < 32; ++i) {
            // Only print registers that have data (reduce noise)
            if (state.regs[i] != 0) {
                std::println("R{:02d} ($...): {:#010x} ({})", i, state.regs[i], static_cast<int32_t>(state.regs[i]));
            }
        }
        std::println("--------------------------");
    }

}