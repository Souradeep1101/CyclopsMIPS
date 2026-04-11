#pragma once
#include <cstdint>

namespace MIPS {

// The internal control signals the ALU understands
enum class ALUControl {
  NONE,
  ADD,
  SUB,
  AND,
  OR,
  XOR,
  NOR,
  SLT,
  SLL,
  SRL,
  SRA,
  MULT,
  MULTU,
  DIV,
  DIVU
};

struct ALUResult {
  uint32_t result;
  bool zero; // Critical for Branching

  // For Multiply/Divide
  uint32_t hiResult = 0;
  bool writesHiLo = false;
};

class ALU {
public:
  // Pure combinational logic: Inputs -> Output
  [[nodiscard]]
  static ALUResult execute(uint32_t src1, uint32_t src2, ALUControl control,
                           uint8_t shamt = 0);
};

} // namespace MIPS