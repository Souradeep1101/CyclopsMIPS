// =============================================================================
// Test_Instruction.cpp — Layer 1: Component Unit Tests
// Target: MIPS Instruction decoding (bit masking)
// =============================================================================
#include "Core/Instruction.h"
#include <gtest/gtest.h>

using namespace MIPS;

TEST(Instruction, RType_Decode) {
    // 0x012A4020 -> add $t0, $t1, $t2
    // opcode=0, rs=9, rt=10, rd=8, shmt=0, funct=32
    Instruction inst{ 0x012A4020 };
    
    EXPECT_EQ(inst.opcode(), 0u);
    EXPECT_EQ(inst.rs(),     9u); // $t1
    EXPECT_EQ(inst.rt(),     10u); // $t2
    EXPECT_EQ(inst.rd(),     8u); // $t0
    EXPECT_EQ(inst.shamt(),  0u);
    EXPECT_EQ(inst.funct(),  32u); // ADD
}

TEST(Instruction, IType_Decode) {
    // 0x2128000A -> addi $t0, $t1, 10
    // opcode=8 (ADDI), rs=9, rt=8, imm=10
    Instruction inst{ 0x2128000A };
    
    EXPECT_EQ(inst.opcode(), 8u);
    EXPECT_EQ(inst.rs(),     9u);
    EXPECT_EQ(inst.rt(),     8u);
    EXPECT_EQ(inst.imm(),    10u);
}

TEST(Instruction, SignExtension) {
    // Negative immediate: -1 (0xFFFF)
    Instruction inst{ 0x2128FFFF };
    EXPECT_EQ(inst.imm(), 0xFFFFu);
    EXPECT_EQ(inst.signExtImm(), -1);
}

TEST(Instruction, JType_Decode) {
    // 0x08000005 -> j 5
    // opcode=2 (J), target=5
    Instruction inst{ 0x08000005 };
    
    EXPECT_EQ(inst.opcode(), 2u);
    EXPECT_EQ(inst.target(), 5u);
}

TEST(Instruction, Factory_RType) {
    auto inst = Instruction::makeRType(9, 10, 8, 0, Funct::ADD);
    EXPECT_EQ(inst.data, 0x012A4020u);
}

TEST(Instruction, Factory_IType) {
    auto inst = Instruction::makeIType(OpCode::ADDI, 9, 8, 10);
    EXPECT_EQ(inst.data, 0x2128000Au);
}
