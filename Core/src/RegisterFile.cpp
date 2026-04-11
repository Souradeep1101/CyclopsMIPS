#include "Core/RegisterFile.h"
#include "Core/CyclopsPrint.h"

namespace MIPS {

    void RegisterFile::dump() const {
        Cyclops::println("--- Register File Dump ---");
        Cyclops::println("PC: {:#010x}", state.pc);

        for (int i = 0; i < 32; ++i) {
            // Only print registers that have data (reduce noise)
            if (state.regs[i] != 0) {
                Cyclops::println("R{:02d} ($...): {:#010x} ({})", i, state.regs[i], static_cast<int32_t>(state.regs[i]));
            }
        }
        Cyclops::println("--------------------------");
    }

}