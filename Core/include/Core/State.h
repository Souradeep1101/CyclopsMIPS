#pragma once
#include <array>
#include <cstdint>


namespace MIPS {

// The raw data layout of a MIPS R2000 CPU
struct CpuState {
  // 32 General Purpose Registers (GPR)
  // $0 is always 0, but we store it anyway for simplicity in indexing
  std::array<uint32_t, 32> regs = {0};

  // Program Counter
  uint32_t pc = 0;
  uint32_t next_pc = 0; // Latch for synchronous updating at cycle end

  // HI/LO registers (used for Multiply/Divide results)
  uint32_t hi = 0;
  uint32_t lo = 0;
};

} // namespace MIPS