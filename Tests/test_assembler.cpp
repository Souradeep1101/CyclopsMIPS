// =============================================================================
// test_assembler.cpp — Google Test suite for the Two-Pass Assembler
// Covers: all R-type, I-type, J-type, and full semantic error detection.
// =============================================================================
#include "Assembler/Assembler.h"
#include <gtest/gtest.h>

using namespace MIPS;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static MachineCode assemble(const std::string& src) {
    auto result = Assembler::assemble(src);
    EXPECT_TRUE(result.has_value()) << "Assembler error: " << (result ? "" : result.error());
    return result->machineCode;
}

static void expectAsmError(const std::string& src, const std::string& hint = "") {
    auto result = Assembler::assemble(src);
    EXPECT_FALSE(result.has_value())
        << "Expected an assembler error for: " << hint << "\n  Source: " << src;
}

// ---------------------------------------------------------------------------
// R-Type instructions
// ---------------------------------------------------------------------------
TEST(Assembler, ADD) {
    // ADD $t2, $t0, $t1  →  rs=8, rt=9, rd=10, funct=0x20
    auto code = assemble("ADD $t2, $t0, $t1");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0] >> 26, 0u);               // opcode = R-type
    EXPECT_EQ((code[0] >> 21) & 0x1F,  8u);     // rs = $t0
    EXPECT_EQ((code[0] >> 16) & 0x1F,  9u);     // rt = $t1
    EXPECT_EQ((code[0] >> 11) & 0x1F, 10u);     // rd = $t2
    EXPECT_EQ(code[0] & 0x3F, 0x20u);           // funct = ADD
}

TEST(Assembler, SUB) {
    auto code = assemble("SUB $s0, $s1, $s2");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ((code[0] >> 11) & 0x1F, 16u);     // rd = $s0
    EXPECT_EQ(code[0] & 0x3F, 0x22u);           // funct = SUB
}

TEST(Assembler, AND) {
    auto code = assemble("AND $t0, $t1, $t2");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0] & 0x3F, 0x24u);
}

TEST(Assembler, OR) {
    auto code = assemble("OR $t0, $t1, $t2");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0] & 0x3F, 0x25u);
}

TEST(Assembler, XOR) {
    auto code = assemble("XOR $t0, $t1, $t2");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0] & 0x3F, 0x26u);
}

TEST(Assembler, NOR) {
    auto code = assemble("NOR $t0, $t1, $t2");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0] & 0x3F, 0x27u);
}

TEST(Assembler, SLT) {
    auto code = assemble("SLT $t0, $t1, $t2");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0] & 0x3F, 0x2Au);
}

TEST(Assembler, SLL) {
    // SLL $t1, $t0, 3  →  shamt=3, funct=0x00
    auto code = assemble("SLL $t1, $t0, 3");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ((code[0] >> 6) & 0x1F, 3u);       // shamt
    EXPECT_EQ(code[0] & 0x3F, 0x00u);           // funct = SLL
}

TEST(Assembler, SRL) {
    auto code = assemble("SRL $t1, $t0, 1");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ((code[0] >> 6) & 0x1F, 1u);
    EXPECT_EQ(code[0] & 0x3F, 0x02u);
}

TEST(Assembler, SRA) {
    auto code = assemble("SRA $t1, $t0, 2");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ((code[0] >> 6) & 0x1F, 2u);
    EXPECT_EQ(code[0] & 0x3F, 0x03u);
}

TEST(Assembler, MULT_DIV_MFLO_MFHI) {
    auto code = assemble(R"(
        MULT $t0, $t1
        DIV  $t0, $t1
        MFLO $t2
        MFHI $t3
    )");
    ASSERT_EQ(code.size(), 4u);
    EXPECT_EQ(code[0] & 0x3F, 0x18u); // MULT funct
    EXPECT_EQ(code[1] & 0x3F, 0x1Au); // DIV funct
    EXPECT_EQ(code[2] & 0x3F, 0x12u); // MFLO funct
    EXPECT_EQ(code[3] & 0x3F, 0x10u); // MFHI funct
}

TEST(Assembler, JR) {
    auto code = assemble("JR $ra");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ((code[0] >> 21) & 0x1F, 31u);  // rs = $ra
    EXPECT_EQ(code[0] & 0x3F, 0x08u);        // funct = JR
}

TEST(Assembler, NOP) {
    auto code = assemble("NOP");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0], 0u);  // NOP = all zeros (SLL $0,$0,0)
}

// ---------------------------------------------------------------------------
// I-Type instructions
// ---------------------------------------------------------------------------
TEST(Assembler, ADDI) {
    // ADDI $t0, $zero, 42  →  opcode=0x08, rs=0, rt=8, imm=42
    auto code = assemble("ADDI $t0, $zero, 42");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0] >> 26, 0x08u);
    EXPECT_EQ((code[0] >> 21) & 0x1F, 0u);
    EXPECT_EQ((code[0] >> 16) & 0x1F, 8u);
    EXPECT_EQ(code[0] & 0xFFFF, 42u);
}

TEST(Assembler, ADDI_Negative) {
    auto code = assemble("ADDI $t0, $zero, -1");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0] & 0xFFFF, 0xFFFFu); // -1 in 16-bit two's complement
}

TEST(Assembler, LW) {
    // LW $t0, 4($sp)  →  opcode=0x23, rs=29, rt=8, imm=4
    auto code = assemble("LW $t0, 4($sp)");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0] >> 26, 0x23u);
    EXPECT_EQ((code[0] >> 21) & 0x1F, 29u); // base = $sp
    EXPECT_EQ((code[0] >> 16) & 0x1F, 8u);  // rt = $t0
    EXPECT_EQ(code[0] & 0xFFFF, 4u);
}

TEST(Assembler, SW) {
    auto code = assemble("SW $t0, 8($sp)");
    ASSERT_EQ(code.size(), 1u);
    EXPECT_EQ(code[0] >> 26, 0x2Bu);
    EXPECT_EQ(code[0] & 0xFFFF, 8u);
}

TEST(Assembler, BEQ_Forward) {
    // BEQ $t0, $zero, END  →  offset should be +1 (next instruction after BEQ is END)
    auto code = assemble(R"(
        BEQ $t0, $zero, END
        NOP
END:
        NOP
    )");
    ASSERT_EQ(code.size(), 3u);
    // Branch offset from BEQ (at addr 0): target=8, PC+4=4, offset=(8-4)/4 = 1
    int16_t offset = static_cast<int16_t>(code[0] & 0xFFFF);
    EXPECT_EQ(offset, 1);
}

TEST(Assembler, BNE_Backward) {
    auto code = assemble(R"(
LOOP:
        ADDI $t1, $t1, 1
        BNE  $t1, $t0, LOOP
    )");
    ASSERT_EQ(code.size(), 2u);
    // BNE at addr=4: target=0, PC+4=8, offset=(0-8)/4 = -2
    int16_t offset = static_cast<int16_t>(code[1] & 0xFFFF);
    EXPECT_EQ(offset, -2);
}

// ---------------------------------------------------------------------------
// J-Type instructions
// ---------------------------------------------------------------------------
TEST(Assembler, J) {
    // J TARGET  → opcode=0x02, target = TARGET_addr >> 2
    auto code = assemble(R"(
TARGET:
        NOP
        J TARGET
    )");
    ASSERT_EQ(code.size(), 2u);
    EXPECT_EQ(code[1] >> 26, 0x02u);
    EXPECT_EQ(code[1] & 0x03FFFFFF, 0u); // target = addr 0 >> 2 = 0
}

TEST(Assembler, JAL) {
    auto code = assemble(R"(
        JAL FUNC
        NOP
FUNC:
        JR $ra
    )");
    ASSERT_EQ(code.size(), 3u);
    EXPECT_EQ(code[0] >> 26, 0x03u); // opcode = JAL
    // Target = addr of FUNC (byte 8) >> 2 = 2
    EXPECT_EQ(code[0] & 0x03FFFFFF, 2u);
}

// ---------------------------------------------------------------------------
// Semantic errors — assembler must REJECT these
// ---------------------------------------------------------------------------
TEST(Assembler, Semantic_ImmTooLarge) {
    expectAsmError("ADDI $t0, $zero, 40000", "imm > 32767");
}

TEST(Assembler, Semantic_ImmTooNegative) {
    expectAsmError("ADDI $t0, $zero, -40000", "imm < -32768");
}

TEST(Assembler, Semantic_ShamtTooLarge) {
    expectAsmError("SLL $t0, $t0, 32", "shamt > 31");
}

TEST(Assembler, Semantic_ShamtNegative) {
    expectAsmError("SLL $t0, $t0, -1", "shamt < 0");
}

TEST(Assembler, Semantic_UndefinedLabel) {
    expectAsmError("J NOWHERE", "undefined label");
}

TEST(Assembler, Semantic_DuplicateLabel) {
    expectAsmError(R"(
A:  NOP
A:  NOP
    )", "duplicate label");
}

TEST(Assembler, Semantic_UnknownRegister) {
    expectAsmError("ADD $xyz, $t0, $t1", "unknown register");
}

TEST(Assembler, Semantic_UnknownOpcode) {
    expectAsmError("FOOBAR $t0, $t1, $t2", "unknown opcode");
}
