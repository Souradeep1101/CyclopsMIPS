#pragma once
#include <cstdint>

namespace MIPS {

// MIPS R2000 Opcode Map (Top 6 bits)
enum class OpCode : uint8_t {
  R_TYPE = 0x00, // All R-Types have opcode 0, distinguished by "Funct"
  ADDI = 0x08,
  LW = 0x23,
  SW = 0x2B,
  BEQ = 0x04,
  BNE = 0x05,
  J = 0x02,
  JAL = 0x03
};

// For R-Type instructions, the logic is in the bottom 6 bits (Funct)
enum class Funct : uint8_t {
  SLL = 0x00, // Shift Left Logical
  SRL = 0x02, // Shift Right Logical
  SRA = 0x03, // Shift Right Arithmetic
  JR = 0x08,
  SYSCALL = 0x0C,
  MFHI = 0x10,
  MFLO = 0x12,
  MULT = 0x18,
  DIV = 0x1A,
  ADD = 0x20,
  SUB = 0x22,
  AND = 0x24,
  OR = 0x25,
  XOR = 0x26,
  NOR = 0x27,
  SLT = 0x2A
};

} // namespace MIPS