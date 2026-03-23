// =============================================================================
// test_cpu.cpp — Google Test suite for the 5-Stage Pipeline
// Covers: forwarding, load-use hazards, branches, cache stats, perf counters.
// =============================================================================
#include "Assembler/Assembler.h"
#include "Core/CPU.h"
#include <gtest/gtest.h>

using namespace MIPS;

// ---------------------------------------------------------------------------
// Helper: assemble + run for N cycles, return final CPU state
// ---------------------------------------------------------------------------
static CpuState run(const std::string& src, int cycles = 50) {
    auto result = Assembler::assemble(src);
    EXPECT_TRUE(result.has_value()) << result.error();
    if (!result) return {};

    MemoryBus bus;
    CPU cpu(bus);
    if (!cpu.loadProgram(result->machineCode)) {
        ADD_FAILURE() << "Failed to load program.";
        return {};
    }
    for (int i = 0; i < cycles; ++i) {
        auto r = cpu.step();
        if (!r) { ADD_FAILURE() << "CPU error: " << r.error(); break; }
    }
    return cpu.getState();
}

// ---------------------------------------------------------------------------
// 1. Basic Arithmetic Correctness
// ---------------------------------------------------------------------------
TEST(CPU, ADDI_ADD_Chain) {
    auto state = run(R"(
        ADDI $t0, $zero, 15
        ADDI $t1, $zero, 25
        ADD  $t2, $t0, $t1
    )");
    EXPECT_EQ(state.regs[8],  15u);
    EXPECT_EQ(state.regs[9],  25u);
    EXPECT_EQ(state.regs[10], 40u);
}

TEST(CPU, SUB_Correct) {
    auto state = run(R"(
        ADDI $t0, $zero, 100
        ADDI $t1, $zero,  37
        SUB  $t2, $t0, $t1
    )");
    EXPECT_EQ(state.regs[10], 63u);
}

// ---------------------------------------------------------------------------
// 2. Forwarding (EX→EX and MEM→EX paths)
// ---------------------------------------------------------------------------
TEST(CPU, Forwarding_EX_to_EX) {
    // Without forwarding this would stall 2 cycles; with it, result is immediate.
    auto state = run(R"(
        ADDI $t0, $zero, 7
        ADD  $t1, $t0, $t0
        ADD  $t2, $t1, $t0
    )");
    // $t1 = 7+7 = 14,  $t2 = 14+7 = 21
    EXPECT_EQ(state.regs[9],  14u);
    EXPECT_EQ(state.regs[10], 21u);
}

TEST(CPU, Forwarding_MEM_to_EX) {
    // Chain of three dependent ADDs — exercises MEM→EX forwarding
    auto state = run(R"(
        ADDI $t0, $zero, 5
        ADD  $t1, $t0, $t0
        ADD  $t2, $t0, $t1
        ADD  $t3, $t1, $t2
    )");
    // $t1=10, $t2=15, $t3=25
    EXPECT_EQ(state.regs[11], 25u);
}

// ---------------------------------------------------------------------------
// 3. Load-Use Hazard (stall insertion)
// ---------------------------------------------------------------------------
TEST(CPU, LoadUse_Stall_Correct) {
    // LW followed IMMEDIATELY by a dependent instruction must insert a stall
    // but still produce the correct result.
    auto state = run(R"(
        ADDI $t0, $zero, 99
        SW   $t0, 0($zero)
        LW   $t1, 0($zero)
        ADD  $t2, $t1, $t1
    )");
    EXPECT_EQ(state.regs[9],  99u);  // $t1 = loaded value
    EXPECT_EQ(state.regs[10], 198u); // $t2 = 99+99 (stall handled correctly)
}

// ---------------------------------------------------------------------------
// 4. Branch — BEQ
// ---------------------------------------------------------------------------
TEST(CPU, BEQ_Taken) {
    // When taken, $t2 should NOT be written (instruction after BEQ is flushed)
    auto state = run(R"(
        ADDI $t0, $zero, 5
        ADDI $t1, $zero, 5
        BEQ  $t0, $t1, SKIP
        ADDI $t2, $zero, 99
SKIP:
        ADDI $t3, $zero, 42
    )");
    EXPECT_EQ(state.regs[10], 0u);  // $t2 was flushed — should still be 0
    EXPECT_EQ(state.regs[11], 42u); // $t3 written after branch target
}

TEST(CPU, BEQ_NotTaken) {
    auto state = run(R"(
        ADDI $t0, $zero, 5
        ADDI $t1, $zero, 6
        BEQ  $t0, $t1, SKIP
        ADDI $t2, $zero, 99
SKIP:
        ADDI $t3, $zero, 42
    )");
    EXPECT_EQ(state.regs[10], 99u); // branch not taken, ADDI executed
    EXPECT_EQ(state.regs[11], 42u);
}

// ---------------------------------------------------------------------------
// 5. Branch — BNE loop
// ---------------------------------------------------------------------------
TEST(CPU, BNE_Loop_FiveTimes) {
    auto state = run(R"(
        ADDI $t0, $zero, 5
        ADDI $t1, $zero, 0
LOOP:
        ADDI $t1, $t1, 1
        BNE  $t1, $t0, LOOP
    )", 150);
    EXPECT_EQ(state.regs[8], 5u);
    EXPECT_EQ(state.regs[9], 5u);
}

// ---------------------------------------------------------------------------
// 6. Memory (LW/SW) round-trip
// ---------------------------------------------------------------------------
TEST(CPU, SW_LW_RoundTrip) {
    auto state = run(R"(
        ADDI $t0, $zero, 0x1234
        SW   $t0, 4($zero)
        LW   $t1, 4($zero)
    )");
    EXPECT_EQ(state.regs[9], 0x1234u);
}

// ---------------------------------------------------------------------------
// 7. Logical Operations
// ---------------------------------------------------------------------------
TEST(CPU, AND_OR_XOR) {
    auto state = run(R"(
        ADDI $t0, $zero, 0xFF
        ADDI $t1, $zero, 0x0F
        AND  $t2, $t0, $t1
        OR   $t3, $t0, $t1
        XOR  $t4, $t0, $t1
    )");
    EXPECT_EQ(state.regs[10], 0x0Fu);
    EXPECT_EQ(state.regs[11], 0xFFu);
    EXPECT_EQ(state.regs[12], 0xF0u);
}

// ---------------------------------------------------------------------------
// 8. Multiply / Divide
// ---------------------------------------------------------------------------
TEST(CPU, MULT_MFLO) {
    auto state = run(R"(
        ADDI $t0, $zero, 11
        ADDI $t1, $zero, 13
        MULT $t0, $t1
        MFLO $t2
    )");
    EXPECT_EQ(state.regs[10], 143u);
}

TEST(CPU, DIV_MFLO_MFHI) {
    auto state = run(R"(
        ADDI $t0, $zero, 17
        ADDI $t1, $zero,  5
        DIV  $t0, $t1
        MFLO $t2
        MFHI $t3
    )");
    EXPECT_EQ(state.regs[10], 3u);  // quotient
    EXPECT_EQ(state.regs[11], 2u);  // remainder
}

// ---------------------------------------------------------------------------
// 9. Performance Counter — PerfCounters struct populated
// ---------------------------------------------------------------------------
TEST(CPU, PerfCounters_CyclesGTInstructions) {
    MemoryBus bus;
    CPU cpu(bus);
    auto result = Assembler::assemble(R"(
        ADDI $t0, $zero, 1
        ADDI $t1, $zero, 2
        ADD  $t2, $t0, $t1
    )");
    ASSERT_TRUE(result.has_value());
    cpu.loadProgram(result->machineCode);
    for (int i = 0; i < 30; ++i) (void)cpu.step();

    const auto& perf = cpu.getState().perf;
    EXPECT_GT(perf.cycles, 0u);
    EXPECT_GT(perf.instructions, 0u);
    // In a 5-stage pipeline, cycles >= instructions
    EXPECT_GE(perf.cycles, perf.instructions);
}
