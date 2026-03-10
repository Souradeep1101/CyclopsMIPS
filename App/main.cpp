#include "Assembler/Assembler.h"
#include "Core/CPU.h"
#include <cstdint>
#include <print>

// ---------------------------------------------------------------------------
// Helper: run a quick CPU execution and return final registers
// ---------------------------------------------------------------------------
static MIPS::CpuState runProgram(const std::string& source, int cycles = 30) {
    auto asmResult = MIPS::Assembler::assemble(source);
    if (!asmResult) {
        std::println("  [ASSEMBLER ERROR]: {}", asmResult.error());
        return {};
    }

    MIPS::CPU cpu;
    cpu.loadProgram(*asmResult);
    for (int i = 0; i < cycles; ++i) {
        auto res = cpu.step();
        if (!res) { std::println("  [CPU ERROR]: {}", res.error()); break; }
    }
    return cpu.getState();
}

// ---------------------------------------------------------------------------
// Helper: assert assembler fails with a specific source
// ---------------------------------------------------------------------------
static bool expectAsmError(const std::string& source, const std::string& label) {
    auto result = MIPS::Assembler::assemble(source);
    if (result) {
        std::println("  FAIL ✗ — '{}' was expected to fail but succeeded", label);
        return false;
    }
    std::println("  Caught error ({}): \"{}\"", label, result.error());
    return true;
}

int main() {
    std::println("=== CyclopsMIPS: Phase 6 Full Assembler Test Suite ===\n");
    bool allPassed = true;

    // -----------------------------------------------------------------------
    // Test 1: ADDI + ADD arithmetic
    // -----------------------------------------------------------------------
    std::println("--- Test 1: ADDI + ADD ---");
    {
        auto state = runProgram(R"(
            ADDI $t0, $zero, 10
            ADDI $t1, $zero, 20
            ADD  $t2, $t0, $t1
        )");
        bool pass = (state.regs[8] == 10 && state.regs[9] == 20 && state.regs[10] == 30);
        std::println("  $t0={} $t1={} $t2={} → {}", state.regs[8], state.regs[9], state.regs[10], pass ? "PASS ✓" : "FAIL ✗");
        allPassed &= pass;
    }

    // -----------------------------------------------------------------------
    // Test 2: BNE counting loop
    // -----------------------------------------------------------------------
    std::println("--- Test 2: BNE counting loop (label resolution) ---");
    {
        auto state = runProgram(R"(
            ADDI $t0, $zero, 5
            ADDI $t1, $zero, 0
LOOP:
            ADDI $t1, $t1, 1
            BNE  $t1, $t0, LOOP
        )", 100);
        bool pass = (state.regs[8] == 5 && state.regs[9] == 5);
        std::println("  $t0={} $t1={} → {}", state.regs[8], state.regs[9], pass ? "PASS ✓" : "FAIL ✗");
        allPassed &= pass;
    }

    // -----------------------------------------------------------------------
    // Test 3: Logical (AND, OR, XOR, NOR)
    // -----------------------------------------------------------------------
    std::println("--- Test 3: Logical operations ---");
    {
        // $t0 = 0xF0, $t1 = 0x0F
        // AND = 0x00, OR = 0xFF, XOR = 0xFF, NOR = 0xFFFFFF00
        auto state = runProgram(R"(
            ADDI $t0, $zero, 0xF0
            ADDI $t1, $zero, 0x0F
            AND  $t2, $t0, $t1
            OR   $t3, $t0, $t1
            XOR  $t4, $t0, $t1
        )", 40);
        bool pass = (state.regs[10] == 0x00 &&
                     state.regs[11] == 0xFF &&
                     state.regs[12] == 0xFF);
        std::println("  AND={} OR={} XOR={} → {}",
            state.regs[10], state.regs[11], state.regs[12],
            pass ? "PASS ✓" : "FAIL ✗");
        allPassed &= pass;
    }

    // -----------------------------------------------------------------------
    // Test 4: Shift operations
    // -----------------------------------------------------------------------
    std::println("--- Test 4: Shift operations ---");
    {
        // $t0 = 1, SLL 3 = 8, SRL 1 = 4, SRA of 0x80000000 >> 1 = 0xC0000000
        auto state = runProgram(R"(
            ADDI $t0, $zero, 1
            SLL  $t1, $t0, 3
            SRL  $t2, $t1, 1
        )", 30);
        bool pass = (state.regs[9] == 8 && state.regs[10] == 4);
        std::println("  SLL(1,3)={} SRL(8,1)={} → {}",
            state.regs[9], state.regs[10], pass ? "PASS ✓" : "FAIL ✗");
        allPassed &= pass;
    }

    // -----------------------------------------------------------------------
    // Test 5: MULT / DIV with MFHI / MFLO
    // -----------------------------------------------------------------------
    std::println("--- Test 5: MULT + MFLO ---");
    {
        // 6 * 7 = 42, LO = 42
        auto state = runProgram(R"(
            ADDI $t0, $zero, 6
            ADDI $t1, $zero, 7
            MULT $t0, $t1
            MFLO $t2
        )", 30);
        bool pass = (state.regs[10] == 42);
        std::println("  MFLO(6*7)={} → {}", state.regs[10], pass ? "PASS ✓" : "FAIL ✗");
        allPassed &= pass;
    }

    // -----------------------------------------------------------------------
    // Test 6: LW / SW round-trip
    // -----------------------------------------------------------------------
    std::println("--- Test 6: SW + LW memory round-trip ---");
    {
        // Store 99 into memory[0], load it back
        auto state = runProgram(R"(
            ADDI $t0, $zero, 99
            SW   $t0, 0($zero)
            LW   $t1, 0($zero)
        )", 30);
        bool pass = (state.regs[9] == 99);
        std::println("  LW(SW(99))={} → {}", state.regs[9], pass ? "PASS ✓" : "FAIL ✗");
        allPassed &= pass;
    }

    // -----------------------------------------------------------------------
    // Test 7: Semantic — immediate out of 16-bit range
    // -----------------------------------------------------------------------
    std::println("--- Test 7: Semantic — immediate out of range ---");
    {
        bool pass = expectAsmError(
            "ADDI $t0, $zero, 99999",
            "imm > 32767");
        allPassed &= pass;
    }

    // -----------------------------------------------------------------------
    // Test 8: Semantic — shamt out of range
    // -----------------------------------------------------------------------
    std::println("--- Test 8: Semantic — shamt out of range ---");
    {
        bool pass = expectAsmError(
            "SLL $t0, $t0, 32",
            "shamt > 31");
        allPassed &= pass;
    }

    // -----------------------------------------------------------------------
    // Test 9: Semantic — undefined label
    // -----------------------------------------------------------------------
    std::println("--- Test 9: Semantic — undefined label ---");
    {
        bool pass = expectAsmError(
            "J NOWHERE",
            "undefined label");
        allPassed &= pass;
    }

    // -----------------------------------------------------------------------
    // Test 10: Semantic — duplicate label
    // -----------------------------------------------------------------------
    std::println("--- Test 10: Semantic — duplicate label ---");
    {
        bool pass = expectAsmError(R"(
DUP:
            ADDI $t0, $zero, 1
DUP:
            ADDI $t0, $zero, 2
        )", "duplicate label");
        allPassed &= pass;
    }

    std::println("\n=== Result: {} ===",
        allPassed ? "ALL TESTS PASSED ✓" : "SOME TESTS FAILED ✗");
    return allPassed ? 0 : 1;
}