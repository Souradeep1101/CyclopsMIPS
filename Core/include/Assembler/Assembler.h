#pragma once
#include "Assembler/Parser.h"
#include "Core/Instruction.h"
#include "Core/OpCode.h"
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

namespace MIPS {

// The result from a successful assembly: a program ready to be loaded into CPU
using MachineCode = std::vector<uint32_t>;

// The top-level Assembler: converts a MIPS assembly source string → MachineCode
class Assembler {
public:
    // Main entry point.  Returns machine code or a human-readable error string.
    static std::expected<MachineCode, std::string>
    assemble(const std::string& source);

private:
    // Symbol table: label name → byte address
    using SymbolTable = std::unordered_map<std::string, uint32_t>;

    // Pass 1 – walk the parsed instructions and record every label's address.
    // Returns the symbol table on success.
    static std::expected<SymbolTable, std::string>
    buildSymbolTable(const std::vector<ParsedInstruction>& instructions);

    // Pass 2 – translate each ParsedInstruction into a 32-bit word.
    static std::expected<MachineCode, std::string>
    generateCode(const std::vector<ParsedInstruction>& instructions,
                 const SymbolTable& symbols);

    // --- Helpers used by Pass 2 ---
    // Resolve a register name ("t0", "zero", "8", …) to its number (0-31).
    static std::expected<uint8_t, std::string>
    resolveRegister(const std::string& name, int lineNum);

    // Pull a register operand from the operand list at the given index.
    static std::expected<uint8_t, std::string>
    getRegOp(const ParsedInstruction& instr, size_t idx);

    // Pull a signed-immediate operand from the operand list at the given index.
    static std::expected<int32_t, std::string>
    getImmOp(const ParsedInstruction& instr, size_t idx);

    // Pull a label-reference operand (string variant) from the operand list.
    static std::expected<std::string, std::string>
    getLabelOp(const ParsedInstruction& instr, size_t idx);

    // --- Semantic Range Checks ---
    // Verify an immediate fits in a signed 16-bit field [-32768, 32767].
    static std::expected<int16_t, std::string>
    checkImm16(int32_t value, int lineNum, const std::string& opcode);

    // Verify a shift amount is in [0, 31].
    static std::expected<uint8_t, std::string>
    checkShamt(int32_t value, int lineNum, const std::string& opcode);

    // True if the ParsedInstruction is a label pseudo-instruction (ends in ':')
    static bool isLabel(const ParsedInstruction& instr);
};

} // namespace MIPS
