// =============================================================================
// Test_Hazards.cpp — Layer 2: Pipeline Integration Tests
// Target: Data Hazards (Forwarding, Load-Use) & Control Hazards (Flush)
// =============================================================================
#include "Assembler/Assembler.h"
#include "Core/CPU.h"
#include <gtest/gtest.h>

using namespace MIPS;

class HazardTest : public ::testing::Test {
protected:
    MemoryBus bus;
    CPU cpu;

    HazardTest() : cpu(bus) {}

    void RunProgram(const std::string& source, int cycles) {
        auto result = Assembler::assemble(source);
        ASSERT_TRUE(result.has_value()) << "Assembler Error: " << result.error();
        
        cpu.reset();
        ASSERT_TRUE(cpu.loadProgram(result->machineCode));
        
        for (int i = 0; i < cycles; ++i) {
            auto r = cpu.step();
            ASSERT_TRUE(r.has_value()) << "CPU Error at cycle " << i << ": " << r.error();
        }
    }
};

// ---------------------------------------------------------------------------
// 1. Data Hazards: Forwarding (EX -> EX)
// ---------------------------------------------------------------------------
TEST_F(HazardTest, Forwarding_EX_to_EX) {
    // $t1 depends on $t0 (written in previous cycle)
    // $t2 depends on $t1 (written in previous cycle)
    RunProgram(R"(
        ADDI $t0, $zero, 10
        ADD  $t1, $t0, $t0
        ADD  $t2, $t1, $t0
    )", 10);

    // After enough cycles: $t1=20, $t2=30
    EXPECT_EQ(cpu.getState().regs[9], 20u);
    EXPECT_EQ(cpu.getState().regs[10], 30u);
}

// ---------------------------------------------------------------------------
// 2. Data Hazards: Load-Use Stall
// ---------------------------------------------------------------------------
TEST_F(HazardTest, LoadUse_Stall) {
    // $t1 is loaded from memory, used IMMEDIATELY by ADD
    // HDU must insert 1 stall (NOP)
    RunProgram(R"(
        ADDI $t0, $zero, 123
        SW   $t0, 0($zero)
        LW   $t1, 0($zero)
        ADD  $t2, $t1, $t1
    )", 15);

    EXPECT_EQ(cpu.getState().regs[9], 123u);
    EXPECT_EQ(cpu.getState().regs[10], 246u);
    // Verify hazard flag was set at some point (check architectural trace or flags)
    // For now, correct result proves stall + forwarding worked.
}

// ---------------------------------------------------------------------------
// 3. Control Hazards: Branch Flush
// ---------------------------------------------------------------------------
TEST_F(HazardTest, Branch_Taken_Flush) {
    // BEQ is taken. The "ADDI $t2" must be FLUSHED (never execute).
    RunProgram(R"(
        ADDI $t0, $zero, 5
        ADDI $t1, $zero, 5
        BEQ  $t0, $t1, TARGET
        ADDI $t2, $zero, 99
    TARGET:
        ADDI $t3, $zero, 42
    )", 20);

    EXPECT_EQ(cpu.getState().regs[10], 0u);   // $t2 must remain 0
    EXPECT_EQ(cpu.getState().regs[11], 42u);  // $t3 must be 42
}

// ---------------------------------------------------------------------------
// 4. Control Hazards: Unconditional Jump
// ---------------------------------------------------------------------------
TEST_F(HazardTest, JUMP_Resolution) {
    RunProgram(R"(
        J START
        ADDI $t0, $zero, 99
    START:
        ADDI $t1, $zero, 7
    )", 15);

    EXPECT_EQ(cpu.getState().regs[8], 0u); // $t0 skipped
    EXPECT_EQ(cpu.getState().regs[9], 7u); // $t1 executes
}
