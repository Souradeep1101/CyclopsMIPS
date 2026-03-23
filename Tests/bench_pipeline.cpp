// =============================================================================
// bench_pipeline.cpp — Google Benchmark suite for CyclopsMIPS
// Measures: assembler throughput, instruction throughput, cache-heavy and
//            branch-heavy workloads.
// =============================================================================
#include "Assembler/Assembler.h"
#include "Core/CPU.h"
#include "Core/MemoryBus.h"
#include <benchmark/benchmark.h>
#include <string>

using namespace MIPS;

// ---------------------------------------------------------------------------
// Helper: assemble once outside the loop, time only the CPU execution
// ---------------------------------------------------------------------------
static void runCPU(CPU& cpu, const std::vector<uint32_t>& program, int cycles) {
    cpu.loadProgram(program);
    for (int i = 0; i < cycles; ++i)
        (void)cpu.step();
}

// ---------------------------------------------------------------------------
// BM_AssemblerThroughput
// Measures how fast the assembler can convert source text → machine code.
// Represents student "compile time" for moderately complex programs.
// ---------------------------------------------------------------------------
static void BM_AssemblerThroughput(benchmark::State& state) {
    // Build a 100-instruction test program
    std::string src;
    for (int i = 0; i < 25; ++i) {
        src += "ADDI $t0, $zero, 1\n";
        src += "ADD  $t1, $t0, $t0\n";
        src += "SUB  $t2, $t1, $t0\n";
        src += "AND  $t3, $t0, $t1\n";
    }

    for (auto _ : state) {
        auto result = Assembler::assemble(src);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(BM_AssemblerThroughput);

// ---------------------------------------------------------------------------
// BM_InstructionThroughput
// Measures raw instruction execution speed: instructions retired per second.
// Linear program (no branches/memory) — best-case pipeline throughput.
// ---------------------------------------------------------------------------
static void BM_InstructionThroughput(benchmark::State& state) {
    // 20 independent ADDI instructions — minimal hazards
    std::string src;
    for (int i = 0; i < 20; ++i)
        src += "ADDI $t0, $zero, " + std::to_string(i) + "\n";

    auto result = Assembler::assemble(src);
    if (!result) return;
    const auto& program = result->machineCode;

    const int cycles = static_cast<int>(state.range(0));
    MemoryBus bus;
    CPU cpu(bus);

    for (auto _ : state) {
        runCPU(cpu, program, cycles);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * cycles);
}
BENCHMARK(BM_InstructionThroughput)->Arg(50)->Arg(200)->Arg(1000);

// ---------------------------------------------------------------------------
// BM_BranchHeavy
// Tight BNE loop — stresses the BTB and branch flush logic.
// ---------------------------------------------------------------------------
static void BM_BranchHeavy(benchmark::State& state) {
    const std::string src = R"(
        ADDI $t0, $zero, 50
        ADDI $t1, $zero, 0
LOOP:
        ADDI $t1, $t1, 1
        BNE  $t1, $t0, LOOP
    )";
    auto result = Assembler::assemble(src);
    if (!result) return;
    const auto& program = result->machineCode;

    const int cycles = static_cast<int>(state.range(0));
    MemoryBus bus;
    CPU cpu(bus);

    for (auto _ : state) {
        runCPU(cpu, program, cycles);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * cycles);
}
BENCHMARK(BM_BranchHeavy)->Arg(250)->Arg(1000);

// ---------------------------------------------------------------------------
// BM_CacheHeavy
// Sequential LW/SW operations — stresses L1D cache and load-use stall paths.
// ---------------------------------------------------------------------------
static void BM_CacheHeavy(benchmark::State& state) {
    // Write 10 values, then load them back
    std::string src;
    for (int i = 0; i < 10; ++i)
        src += "ADDI $t0, $zero, " + std::to_string(i * 10) + "\n"
             + "SW   $t0, " + std::to_string(i * 4) + "($zero)\n";
    for (int i = 0; i < 10; ++i)
        src += "LW   $t1, " + std::to_string(i * 4) + "($zero)\n";

    auto result = Assembler::assemble(src);
    if (!result) return;
    const auto& program = result->machineCode;

    const int cycles = static_cast<int>(state.range(0));
    MemoryBus bus;
    CPU cpu(bus);

    for (auto _ : state) {
        runCPU(cpu, program, cycles);
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * cycles);
}
BENCHMARK(BM_CacheHeavy)->Arg(150)->Arg(500);
