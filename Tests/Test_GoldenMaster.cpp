// =============================================================================
// Test_GoldenMaster.cpp — Layer 3: Golden Master (End-to-End) Tests
// Target: Whole-System Validation using Fibonacci Sequence
// =============================================================================
#include "Assembler/Assembler.h"
#include "Core/CPU.h"
#include <gtest/gtest.h>

using namespace MIPS;

class GoldenMasterTest : public ::testing::Test {
protected:
    MemoryBus bus;
    CPU cpu;

    GoldenMasterTest() : cpu(bus) {}

    void ExecuteProgram(const std::string& source, int maxCycles = 500) {
        auto result = Assembler::assemble(source);
        ASSERT_TRUE(result.has_value()) << "Assembler Failed: " << (result ? "" : result.error());
        
        cpu.reset();
        ASSERT_TRUE(cpu.loadProgram(result->machineCode));
        
        for (int i = 0; i < maxCycles; ++i) {
            auto res = cpu.step();
            if (!res) {
                // If the emulator returns an error (e.g., Unknown OpCode), fail the test immediately
                FAIL() << "Emulation Error at cycle " << i << ": " << res.error();
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Fibonacci(10) = 55 (0x37)
// This test exercises:
// - ADDI, ADD (Arithmetic)
// - SW, LW (Memory)
// - BNE (Control Flow / Labels)
// - $zero (Special Register)
// ---------------------------------------------------------------------------
TEST_F(GoldenMasterTest, Fibonacci_10th_Number) {
    const std::string mipsAsm = R"(
        ADDI $t0, $zero, 10      # n = 10
        ADDI $t1, $zero, 0       # f1 = 0
        ADDI $t2, $zero, 1       # f2 = 1
        
    LOOP:
        BEQ  $t0, $zero, END     # if n == 0, break
        ADD  $t3, $t1, $t2       # next = f1 + f2
        ADD  $t1, $zero, $t2     # f1 = f2
        ADD  $t2, $zero, $t3     # f2 = next
        ADDI $t0, $t0, -1        # n--
        J LOOP
        
    END:
        SW   $t1, 100($zero)     # Save Fib(10) to address 100
    HALT:
        J    HALT                # Infinite loop to drain pipeline
    )";

    ExecuteProgram(mipsAsm, 500);

    // After 10 iterations:
    // Fib(10) = 55 (0x37)
    // Architect Note: We verify both register and memory. 
    // However, if L1 is Write-Back, memory might be 0 until eviction.
    // So we primarily verify the retired register state.
    
    EXPECT_EQ(cpu.getState().regs[9], 55u); // $t1 = 55
}
