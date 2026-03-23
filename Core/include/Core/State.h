#pragma once
#include <array>
#include <cstdint>


namespace MIPS {

// ---------------------------------------------------------------------------
// Performance Counters — lightweight, zero-overhead profiling
// ---------------------------------------------------------------------------
struct PerfCounters {
    uint64_t cycles        = 0; // Total clock cycles executed
    uint64_t instructions  = 0; // Instructions retired (not counting stall slots)
    uint64_t stallCycles   = 0; // Cycles spent stalling (load-use hazards)
    uint64_t cacheMisses   = 0; // L1I + L1D combined misses
    uint64_t branchMisses  = 0; // BTB mispredictions

    // Derived metric: Instructions Per Cycle
    [[nodiscard]] double ipc() const {
        return (cycles > 0) ? static_cast<double>(instructions) / static_cast<double>(cycles) : 0.0;
    }

    void reset() { *this = {}; }
};

// ---------------------------------------------------------------------------
// The raw data layout of a MIPS R2000 CPU
// ---------------------------------------------------------------------------
struct CpuState {
    // 32 General Purpose Registers (GPR)
    // $0 is always 0, but we store it anyway for simplicity in indexing
    std::array<uint32_t, 32> regs = {0};

    // Program Counter
    uint32_t pc      = 0;
    uint32_t next_pc = 0; // Latch for synchronous updating at cycle end

    // HI/LO registers (used for Multiply/Divide results)
    uint32_t hi = 0;
    uint32_t lo = 0;

    // Performance counters (updated by CPU each cycle)
    PerfCounters perf;
};

} // namespace MIPS