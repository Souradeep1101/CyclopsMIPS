// =============================================================================
// Test_RegFile.cpp — Layer 1: Component Unit Tests
// Target: RegisterFile (R0 hardwired to 0)
// =============================================================================
#include "Core/RegisterFile.h"
#include <gtest/gtest.h>

using namespace MIPS;

class RegFileTest : public ::testing::Test {
protected:
    CpuState state;
    RegisterFile rf;

    RegFileTest() : rf(state) {
        state.reset();
    }
};

TEST_F(RegFileTest, Zero_Is_Hardwired) {
    // Attempt write to register 0
    rf.write(0, 0xFFFFFFFF);
    EXPECT_EQ(rf.read(0), 0u);
}

TEST_F(RegFileTest, Basic_ReadWrite) {
    // $t0 (Index 8)
    rf.write(8, 0x12345678);
    EXPECT_EQ(rf.read(8), 0x12345678u);
}

TEST_F(RegFileTest, Multiple_Registers) {
    rf.write(1, 100);
    rf.write(16, 200);
    rf.write(31, 300);
    
    EXPECT_EQ(rf.read(1),  100u);
    EXPECT_EQ(rf.read(16), 200u);
    EXPECT_EQ(rf.read(31), 300u);
}
