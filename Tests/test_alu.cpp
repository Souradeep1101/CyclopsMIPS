// =============================================================================
// test_alu.cpp — Google Test suite for the ALU in isolation
// =============================================================================
#include "Core/ALU.h"
#include <gtest/gtest.h>

using namespace MIPS;

// Shorthand helper
static ALUResult exec(ALUControl op, uint32_t a, uint32_t b, uint8_t shamt = 0) {
    return ALU::execute(a, b, op, shamt);
}

// ---------------------------------------------------------------------------
// Arithmetic
// ---------------------------------------------------------------------------
TEST(ALU, ADD_Basic) {
    auto r = exec(ALUControl::ADD, 10, 32);
    EXPECT_EQ(r.result, 42u);
    EXPECT_FALSE(r.zero);
}

TEST(ALU, ADD_Wrap) {
    auto r = exec(ALUControl::ADD, 0xFFFFFFFFu, 1u);
    EXPECT_EQ(r.result, 0u);
    EXPECT_TRUE(r.zero);
}

TEST(ALU, SUB_Positive) {
    auto r = exec(ALUControl::SUB, 50, 8);
    EXPECT_EQ(r.result, 42u);
    EXPECT_FALSE(r.zero);
}

TEST(ALU, SUB_ZeroFlag) {
    auto r = exec(ALUControl::SUB, 7, 7);
    EXPECT_EQ(r.result, 0u);
    EXPECT_TRUE(r.zero);
}

// ---------------------------------------------------------------------------
// Logical
// ---------------------------------------------------------------------------
TEST(ALU, AND) {
    auto r = exec(ALUControl::AND, 0xFF00u, 0x0FF0u);
    EXPECT_EQ(r.result, 0x0F00u);
}

TEST(ALU, OR) {
    auto r = exec(ALUControl::OR, 0xFF00u, 0x00FFu);
    EXPECT_EQ(r.result, 0xFFFFu);
}

TEST(ALU, XOR) {
    auto r = exec(ALUControl::XOR, 0xFFFFu, 0x0F0Fu);
    EXPECT_EQ(r.result, 0xF0F0u);
}

TEST(ALU, NOR) {
    // NOR(0,0) = ~0 = 0xFFFFFFFF
    auto r = exec(ALUControl::NOR, 0u, 0u);
    EXPECT_EQ(r.result, 0xFFFFFFFFu);
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------
TEST(ALU, SLT_True) {
    // Signed: -1 < 1  → set = 1
    auto r = exec(ALUControl::SLT, static_cast<uint32_t>(-1), 1u);
    EXPECT_EQ(r.result, 1u);
}

TEST(ALU, SLT_False_Equal) {
    auto r = exec(ALUControl::SLT, 10u, 10u);
    EXPECT_EQ(r.result, 0u);
}

TEST(ALU, SLT_False_Greater) {
    auto r = exec(ALUControl::SLT, 5u, 3u);
    EXPECT_EQ(r.result, 0u);
}

// ---------------------------------------------------------------------------
// Shifts (shamt passed as 4th argument)
// ---------------------------------------------------------------------------
TEST(ALU, SLL) {
    auto r = exec(ALUControl::SLL, 0u, 1u, 4u); // 1 << 4 = 16
    EXPECT_EQ(r.result, 16u);
}

TEST(ALU, SRL) {
    // Logical shift — MSB not propagated
    auto r = exec(ALUControl::SRL, 0u, 0x80000000u, 1u);
    EXPECT_EQ(r.result, 0x40000000u);
}

TEST(ALU, SRA) {
    // Arithmetic shift — sign bit IS propagated
    auto r = exec(ALUControl::SRA, 0u, 0x80000000u, 1u);
    EXPECT_EQ(r.result, 0xC0000000u);
}

// ---------------------------------------------------------------------------
// Multiply / Divide
// ---------------------------------------------------------------------------
TEST(ALU, MULT_Small) {
    auto r = exec(ALUControl::MULT, 7u, 6u);
    EXPECT_EQ(r.result,   42u); // LO
    EXPECT_EQ(r.hiResult,  0u); // HI = 0 for small product
    EXPECT_TRUE(r.writesHiLo);
}

TEST(ALU, MULT_LargeHI) {
    // 2000000000 * 3 = 6000000000 = 0x165A0BC00 (overflows into HI)
    auto r = exec(ALUControl::MULT, 2000000000u, 3u);
    EXPECT_EQ(r.hiResult, 1u);
    EXPECT_EQ(r.result,   1705032704u);
}

TEST(ALU, DIV_QuotientRemainder) {
    auto r = exec(ALUControl::DIV, 17u, 5u);
    EXPECT_EQ(r.result,   3u); // LO = quotient
    EXPECT_EQ(r.hiResult, 2u); // HI = remainder
    EXPECT_TRUE(r.writesHiLo);
}

TEST(ALU, DIV_Exact) {
    auto r = exec(ALUControl::DIV, 20u, 5u);
    EXPECT_EQ(r.result,   4u);
    EXPECT_EQ(r.hiResult, 0u);
}
