#include "Assembler/Assembler.h"
#include "Assembler/Parser.h"
#include "Core/Instruction.h"
#include "Core/OpCode.h"
#include <algorithm>
#include <cctype>
#include <format>
#include <unordered_map>

namespace MIPS {

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
std::expected<AssembledProgram, std::string>
Assembler::assemble(const std::string& source) {
    // Lex
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    // Parse
    Parser parser(tokens);
    auto parseResult = parser.parse();
    if (!parseResult)
        return std::unexpected(parseResult.error());

    const auto& instructions = *parseResult;

    // Pass 1 — build symbol table
    auto symResult = buildSymbolTable(instructions);
    if (!symResult)
        return std::unexpected(symResult.error());

    // Pass 2 — emit machine code
    return generateCode(instructions, *symResult);
}

// ---------------------------------------------------------------------------
// Pass 1: Symbol Table
// ---------------------------------------------------------------------------
std::expected<Assembler::SymbolTable, std::string>
Assembler::buildSymbolTable(const std::vector<ParsedInstruction>& instructions) {
    SymbolTable table;
    uint32_t address = 0x0000;

    for (const auto& instr : instructions) {
        if (isLabel(instr)) {
            // Strip trailing ':'
            std::string labelName = instr.opcode.substr(0, instr.opcode.size() - 1);
            if (table.count(labelName)) {
                return std::unexpected(
                    std::format("Line {}: Duplicate label '{}'", instr.lineNum, labelName));
            }
            table[labelName] = address;
        } else {
            address += 4; // Every real instruction is exactly 4 bytes
        }
    }

    return table;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
bool Assembler::isLabel(const ParsedInstruction& instr) {
    return !instr.opcode.empty() && instr.opcode.back() == ':';
}

std::expected<uint8_t, std::string>
Assembler::resolveRegister(const std::string& name, int lineNum) {
    // Handle numeric: "8" → 8
    if (!name.empty() && std::isdigit(name[0])) {
        int n = std::stoi(name);
        if (n < 0 || n > 31)
            return std::unexpected(std::format("Line {}: Register number {} out of range", lineNum, n));
        return static_cast<uint8_t>(n);
    }

    // Symbolic register names (case-insensitive)
    static const std::unordered_map<std::string, uint8_t> regMap = {
        {"zero", uint8_t{0}},  {"at",   uint8_t{1}},
        {"v0",   uint8_t{2}},  {"v1",   uint8_t{3}},
        {"a0",   uint8_t{4}},  {"a1",   uint8_t{5}},  {"a2",  uint8_t{6}},  {"a3",  uint8_t{7}},
        {"t0",   uint8_t{8}},  {"t1",   uint8_t{9}},  {"t2",  uint8_t{10}}, {"t3",  uint8_t{11}},
        {"t4",   uint8_t{12}}, {"t5",   uint8_t{13}}, {"t6",  uint8_t{14}}, {"t7",  uint8_t{15}},
        {"s0",   uint8_t{16}}, {"s1",   uint8_t{17}}, {"s2",  uint8_t{18}}, {"s3",  uint8_t{19}},
        {"s4",   uint8_t{20}}, {"s5",   uint8_t{21}}, {"s6",  uint8_t{22}}, {"s7",  uint8_t{23}},
        {"t8",   uint8_t{24}}, {"t9",   uint8_t{25}},
        {"k0",   uint8_t{26}}, {"k1",   uint8_t{27}},
        {"gp",   uint8_t{28}}, {"sp",   uint8_t{29}}, {"fp",  uint8_t{30}}, {"ra",  uint8_t{31}},
    };

    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto it = regMap.find(lower);
    if (it == regMap.end())
        return std::unexpected(std::format("Line {}: Unknown register '${}'", lineNum, name));

    return it->second;
}

std::expected<uint8_t, std::string>
Assembler::getRegOp(const ParsedInstruction& instr, size_t idx) {
    if (idx >= instr.operands.size())
        return std::unexpected(
            std::format("Line {}: '{}' expected register operand at position {}", instr.lineNum, instr.opcode, idx));

    const auto& op = instr.operands[idx];
    if (!std::holds_alternative<std::string>(op))
        return std::unexpected(
            std::format("Line {}: '{}' operand {} must be a register", instr.lineNum, instr.opcode, idx));

    return resolveRegister(std::get<std::string>(op), instr.lineNum);
}

std::expected<int32_t, std::string>
Assembler::getImmOp(const ParsedInstruction& instr, size_t idx) {
    if (idx >= instr.operands.size())
        return std::unexpected(
            std::format("Line {}: '{}' expected immediate operand at position {}", instr.lineNum, instr.opcode, idx));

    const auto& op = instr.operands[idx];
    if (!std::holds_alternative<int32_t>(op))
        return std::unexpected(
            std::format("Line {}: '{}' operand {} must be an immediate", instr.lineNum, instr.opcode, idx));

    return std::get<int32_t>(op);
}

std::expected<std::string, std::string>
Assembler::getLabelOp(const ParsedInstruction& instr, size_t idx) {
    if (idx >= instr.operands.size())
        return std::unexpected(
            std::format("Line {}: '{}' expected label at position {}", instr.lineNum, instr.opcode, idx));

    const auto& op = instr.operands[idx];
    if (!std::holds_alternative<std::string>(op))
        return std::unexpected(
            std::format("Line {}: '{}' operand {} must be a label reference", instr.lineNum, instr.opcode, idx));

    return std::get<std::string>(op);
}

// ---------------------------------------------------------------------------
// Semantic Range Checks
// ---------------------------------------------------------------------------
std::expected<int16_t, std::string>
Assembler::checkImm16(int32_t value, int lineNum, const std::string& opcode) {
    if (value < -32768 || value > 32767)
        return std::unexpected(std::format(
            "Line {}: '{}' immediate value {} does not fit in 16 bits (range -32768..32767)",
            lineNum, opcode, value));
    return static_cast<int16_t>(value);
}

std::expected<uint8_t, std::string>
Assembler::checkShamt(int32_t value, int lineNum, const std::string& opcode) {
    if (value < 0 || value > 31)
        return std::unexpected(std::format(
            "Line {}: '{}' shift amount {} is out of range (must be 0-31)",
            lineNum, opcode, value));
    return static_cast<uint8_t>(value);
}


// ---------------------------------------------------------------------------
// Pass 2: Machine Code Generation
// ---------------------------------------------------------------------------
std::expected<AssembledProgram, std::string>
Assembler::generateCode(const std::vector<ParsedInstruction>& instructions,
                         const SymbolTable& symbols) {
    AssembledProgram program;
    uint32_t currentAddress = 0x0000;

    for (const auto& instr : instructions) {
        if (isLabel(instr))
            continue; // Labels occupy no machine code words

        const std::string& op = instr.opcode;
        uint32_t word = 0;

        // Reconstruct Source String String 
        std::string sourceStr = op;
        for (size_t i = 0; i < instr.operands.size(); ++i) {
            sourceStr += (i == 0) ? " " : ", ";
            if (std::holds_alternative<std::string>(instr.operands[i])) {
                sourceStr += std::get<std::string>(instr.operands[i]);
            } else {
                sourceStr += std::to_string(std::get<int32_t>(instr.operands[i]));
            }
        }
        program.sourceMap[currentAddress] = { instr.lineNum, sourceStr };

        // ---------------------------------------------------------------
        // R-TYPE instructions: rd, rs, rt, shamt, funct
        // ---------------------------------------------------------------
        if (op == "ADD" || op == "ADDU") {
            auto rd = getRegOp(instr, 0); if (!rd) return std::unexpected(rd.error());
            auto rs = getRegOp(instr, 1); if (!rs) return std::unexpected(rs.error());
            auto rt = getRegOp(instr, 2); if (!rt) return std::unexpected(rt.error());
            word = Instruction::makeRType(*rs, *rt, *rd, 0, Funct::ADD).data;

        } else if (op == "SUB" || op == "SUBU") {
            auto rd = getRegOp(instr, 0); if (!rd) return std::unexpected(rd.error());
            auto rs = getRegOp(instr, 1); if (!rs) return std::unexpected(rs.error());
            auto rt = getRegOp(instr, 2); if (!rt) return std::unexpected(rt.error());
            word = Instruction::makeRType(*rs, *rt, *rd, 0, Funct::SUB).data;

        } else if (op == "AND") {
            auto rd = getRegOp(instr, 0); if (!rd) return std::unexpected(rd.error());
            auto rs = getRegOp(instr, 1); if (!rs) return std::unexpected(rs.error());
            auto rt = getRegOp(instr, 2); if (!rt) return std::unexpected(rt.error());
            word = Instruction::makeRType(*rs, *rt, *rd, 0, Funct::AND).data;

        } else if (op == "OR") {
            auto rd = getRegOp(instr, 0); if (!rd) return std::unexpected(rd.error());
            auto rs = getRegOp(instr, 1); if (!rs) return std::unexpected(rs.error());
            auto rt = getRegOp(instr, 2); if (!rt) return std::unexpected(rt.error());
            word = Instruction::makeRType(*rs, *rt, *rd, 0, Funct::OR).data;

        } else if (op == "XOR") {
            auto rd = getRegOp(instr, 0); if (!rd) return std::unexpected(rd.error());
            auto rs = getRegOp(instr, 1); if (!rs) return std::unexpected(rs.error());
            auto rt = getRegOp(instr, 2); if (!rt) return std::unexpected(rt.error());
            word = Instruction::makeRType(*rs, *rt, *rd, 0, Funct::XOR).data;

        } else if (op == "NOR") {
            auto rd = getRegOp(instr, 0); if (!rd) return std::unexpected(rd.error());
            auto rs = getRegOp(instr, 1); if (!rs) return std::unexpected(rs.error());
            auto rt = getRegOp(instr, 2); if (!rt) return std::unexpected(rt.error());
            word = Instruction::makeRType(*rs, *rt, *rd, 0, Funct::NOR).data;

        } else if (op == "SLT") {
            auto rd = getRegOp(instr, 0); if (!rd) return std::unexpected(rd.error());
            auto rs = getRegOp(instr, 1); if (!rs) return std::unexpected(rs.error());
            auto rt = getRegOp(instr, 2); if (!rt) return std::unexpected(rt.error());
            word = Instruction::makeRType(*rs, *rt, *rd, 0, Funct::SLT).data;

        } else if (op == "SLL") {
            auto rd      = getRegOp(instr, 0);  if (!rd)      return std::unexpected(rd.error());
            auto rt      = getRegOp(instr, 1);  if (!rt)      return std::unexpected(rt.error());
            auto rawShamt = getImmOp(instr, 2); if (!rawShamt) return std::unexpected(rawShamt.error());
            auto shamt   = checkShamt(*rawShamt, instr.lineNum, op); if (!shamt) return std::unexpected(shamt.error());
            word = Instruction::makeRType(0, *rt, *rd, *shamt, Funct::SLL).data;

        } else if (op == "SRL") {
            auto rd      = getRegOp(instr, 0);  if (!rd)      return std::unexpected(rd.error());
            auto rt      = getRegOp(instr, 1);  if (!rt)      return std::unexpected(rt.error());
            auto rawShamt = getImmOp(instr, 2); if (!rawShamt) return std::unexpected(rawShamt.error());
            auto shamt   = checkShamt(*rawShamt, instr.lineNum, op); if (!shamt) return std::unexpected(shamt.error());
            word = Instruction::makeRType(0, *rt, *rd, *shamt, Funct::SRL).data;

        } else if (op == "SRA") {
            auto rd      = getRegOp(instr, 0);  if (!rd)      return std::unexpected(rd.error());
            auto rt      = getRegOp(instr, 1);  if (!rt)      return std::unexpected(rt.error());
            auto rawShamt = getImmOp(instr, 2); if (!rawShamt) return std::unexpected(rawShamt.error());
            auto shamt   = checkShamt(*rawShamt, instr.lineNum, op); if (!shamt) return std::unexpected(shamt.error());
            word = Instruction::makeRType(0, *rt, *rd, *shamt, Funct::SRA).data;

        } else if (op == "MULT") {
            // MULT rs, rt  (no destination register — result goes to HI/LO)
            auto rs = getRegOp(instr, 0); if (!rs) return std::unexpected(rs.error());
            auto rt = getRegOp(instr, 1); if (!rt) return std::unexpected(rt.error());
            word = Instruction::makeRType(*rs, *rt, 0, 0, Funct::MULT).data;

        } else if (op == "DIV") {
            auto rs = getRegOp(instr, 0); if (!rs) return std::unexpected(rs.error());
            auto rt = getRegOp(instr, 1); if (!rt) return std::unexpected(rt.error());
            word = Instruction::makeRType(*rs, *rt, 0, 0, Funct::DIV).data;

        } else if (op == "MFHI") {
            auto rd = getRegOp(instr, 0); if (!rd) return std::unexpected(rd.error());
            word = Instruction::makeRType(0, 0, *rd, 0, Funct::MFHI).data;

        } else if (op == "MFLO") {
            auto rd = getRegOp(instr, 0); if (!rd) return std::unexpected(rd.error());
            word = Instruction::makeRType(0, 0, *rd, 0, Funct::MFLO).data;

        } else if (op == "JR") {
            auto rs = getRegOp(instr, 0); if (!rs) return std::unexpected(rs.error());
            word = Instruction::makeRType(*rs, 0, 0, 0, Funct::JR).data;

        } else if (op == "NOP" || op == "nop") {
            word = Instruction::makeRType(0, 0, 0, 0, Funct::SLL).data;

        // ---------------------------------------------------------------
        // I-TYPE instructions: rt, rs, imm
        // ---------------------------------------------------------------
        } else if (op == "ADDI" || op == "ADDIU") {
            // Syntax: ADDI $rt, $rs, imm
            auto rt     = getRegOp(instr, 0);  if (!rt)     return std::unexpected(rt.error());
            auto rs     = getRegOp(instr, 1);  if (!rs)     return std::unexpected(rs.error());
            auto rawImm = getImmOp(instr, 2);  if (!rawImm) return std::unexpected(rawImm.error());
            auto imm    = checkImm16(*rawImm, instr.lineNum, op); if (!imm) return std::unexpected(imm.error());
            word = Instruction::makeIType(OpCode::ADDI, *rs, *rt, *imm).data;

        } else if (op == "LW") {
            // Syntax: LW $rt, offset($base)
            auto rt        = getRegOp(instr, 0);  if (!rt)        return std::unexpected(rt.error());
            auto rawOffset = getImmOp(instr, 1);  if (!rawOffset) return std::unexpected(rawOffset.error());
            auto base      = getRegOp(instr, 2);  if (!base)      return std::unexpected(base.error());
            auto offset    = checkImm16(*rawOffset, instr.lineNum, op); if (!offset) return std::unexpected(offset.error());
            word = Instruction::makeIType(OpCode::LW, *base, *rt, *offset).data;

        } else if (op == "SW") {
            // Syntax: SW $rt, offset($base)
            auto rt        = getRegOp(instr, 0);  if (!rt)        return std::unexpected(rt.error());
            auto rawOffset = getImmOp(instr, 1);  if (!rawOffset) return std::unexpected(rawOffset.error());
            auto base      = getRegOp(instr, 2);  if (!base)      return std::unexpected(base.error());
            auto offset    = checkImm16(*rawOffset, instr.lineNum, op); if (!offset) return std::unexpected(offset.error());
            word = Instruction::makeIType(OpCode::SW, *base, *rt, *offset).data;

        } else if (op == "BEQ") {
            // Syntax: BEQ $rs, $rt, label
            auto rs    = getRegOp(instr, 0);   if (!rs)    return std::unexpected(rs.error());
            auto rt    = getRegOp(instr, 1);   if (!rt)    return std::unexpected(rt.error());
            auto label = getLabelOp(instr, 2); if (!label) return std::unexpected(label.error());

            auto it = symbols.find(*label);
            if (it == symbols.end())
                return std::unexpected(std::format("Line {}: Undefined label '{}'", instr.lineNum, *label));

            // Branch offset = (target_addr - (current_addr + 4)) / 4
            int32_t rawOffset = (static_cast<int32_t>(it->second) - static_cast<int32_t>(currentAddress + 4)) / 4;
            auto offset = checkImm16(rawOffset, instr.lineNum, op); if (!offset) return std::unexpected(offset.error());
            word = Instruction::makeIType(OpCode::BEQ, *rs, *rt, *offset).data;

        } else if (op == "BNE") {
            auto rs    = getRegOp(instr, 0);   if (!rs)    return std::unexpected(rs.error());
            auto rt    = getRegOp(instr, 1);   if (!rt)    return std::unexpected(rt.error());
            auto label = getLabelOp(instr, 2); if (!label) return std::unexpected(label.error());

            auto it = symbols.find(*label);
            if (it == symbols.end())
                return std::unexpected(std::format("Line {}: Undefined label '{}'", instr.lineNum, *label));

            int32_t rawOffset = (static_cast<int32_t>(it->second) - static_cast<int32_t>(currentAddress + 4)) / 4;
            auto offset = checkImm16(rawOffset, instr.lineNum, op); if (!offset) return std::unexpected(offset.error());
            word = Instruction::makeIType(OpCode::BNE, *rs, *rt, *offset).data;

        // ---------------------------------------------------------------
        // J-TYPE instructions
        // ---------------------------------------------------------------
        } else if (op == "J") {
            auto label = getLabelOp(instr, 0); if (!label) return std::unexpected(label.error());

            auto it = symbols.find(*label);
            if (it == symbols.end())
                return std::unexpected(std::format("Line {}: Undefined label '{}'", instr.lineNum, *label));

            // J-type target = bits[27:2] of the absolute target address
            word = Instruction::makeJType(OpCode::J, it->second >> 2).data;

        } else if (op == "JAL") {
            auto label = getLabelOp(instr, 0); if (!label) return std::unexpected(label.error());

            auto it = symbols.find(*label);
            if (it == symbols.end())
                return std::unexpected(std::format("Line {}: Undefined label '{}'", instr.lineNum, *label));

            word = Instruction::makeJType(OpCode::JAL, it->second >> 2).data;

        } else {
            return std::unexpected(
                std::format("Line {}: Unknown opcode '{}'", instr.lineNum, instr.opcode));
        }

        program.machineCode.push_back(word);
        currentAddress += 4;
    }

    return program;
}

} // namespace MIPS