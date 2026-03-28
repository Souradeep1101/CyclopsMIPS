// =============================================================================
// Test_ALU.cpp — Layer 1: Component Unit Tests
// Target: ALU logic (Arithmetic, Logical, Comparison, Shifts)
// =============================================================================
#include "Core/ALU.h"
#include <gtest/gtest.h>

using namespace MIPS;

// Shorthand helper for tests
static ALUResult exec(uint32_t a, uint32_t b, ALUControl op, uint8_t shamt = 0) {
    return ALU::execute(a, b, op, shamt);
}

// ---------------------------------------------------------------------------
// 1. Arithmetic Correctness
// ---------------------------------------------------------------------------
TEST(ALU, ADD_Basic) {
    auto r = exec(10, 32, ALUControl::ADD);
    EXPECT_EQ(r.result, 42u);
    EXPECT_FALSE(r.zero);
}

TEST(ALU, SUB_Basic) {
    auto r = exec(50, 8, ALUControl::SUB);
    EXPECT_EQ(r.result, 42u);
    EXPECT_FALSE(r.zero);
}

TEST(ALU, Zero_Flag) {
    auto r = exec(7, 7, ALUControl::SUB);
    EXPECT_EQ(r.result, 0u);
    EXPECT_TRUE(r.zero);
}

// ---------------------------------------------------------------------------
// 2. Logical & Comparison
// ---------------------------------------------------------------------------
TEST(ALU, Logical_AND_OR_XOR_NOR) {
    EXPECT_EQ(exec(0xFF00u, 0x0FF0u, ALUControl::AND).result, 0x0F00u);
    EXPECT_EQ(exec(0xFF00u, 0x00FFu, ALUControl::OR).result, 0xFFFFu);
    EXPECT_EQ(exec(0xFFFFu, 0x0F0Fu, ALUControl::XOR).result, 0xF0F0u);
    EXPECT_EQ(exec(0u, 0u, ALUControl::NOR).result, 0xFFFFFFFFu);
}

TEST(ALU, Comparison_SLT) {
    // Signed: -1 < 1
    EXPECT_EQ(exec(static_cast<uint32_t>(-1), 1u, ALUControl::SLT).result, 1u);
    EXPECT_EQ(exec(10u, 10u, ALUControl::SLT).result, 0u);
}

// ---------------------------------------------------------------------------
// 3. Shifts (shamt field)
// ---------------------------------------------------------------------------
TEST(ALU, Shift_SLL_SRL_SRA) {
    EXPECT_EQ(exec(0u, 1u, ALUControl::SLL, 4u).result, 16u);
    EXPECT_EQ(exec(0u, 0x80000000u, ALUControl::SRL, 1u).result, 0x40000000u);
    EXPECT_EQ(exec(0u, 0x80000000u, ALUControl::SRA, 1u).result, 0xC0000000u);
}

// ---------------------------------------------------------------------------
// 4. Multiply & Divide (HI/LO logic)
// ---------------------------------------------------------------------------
TEST(ALU, MULT_64bit_Product) {
    // 2^31 * 2 = 2^32 (HI=1, LO=0) - Using MULTU (Unsigned)
    auto r = exec(0x80000000u, 2u, ALUControl::MULTU);
    EXPECT_EQ(r.hiResult, 1u);
    EXPECT_EQ(r.result,   0u);
    EXPECT_TRUE(r.writesHiLo);
}

TEST(ALU, DIV_Quotient_Remainder) {
    auto r = exec(17u, 5u, ALUControl::DIV);
    EXPECT_EQ(r.result,   3u); // LO: quotient
    EXPECT_EQ(r.hiResult, 2u); // HI: remainder
    EXPECT_TRUE(r.writesHiLo);
}
