#include "Core/ALU.h"
#include <bit> // C++20/23 bit manipulation

namespace MIPS {

ALUResult ALU::execute(uint32_t src1, uint32_t src2, ALUControl control,
                       uint8_t shamt) {
  uint32_t result = 0;
  uint32_t hiResult = 0;
  bool writesHiLo = false;

  switch (control) {
  case ALUControl::NONE:
    break;
  case ALUControl::ADD:
    // Note: Standard MIPS 'ADD' traps on overflow. 'ADDU' wraps.
    // For this simulator, we default to wrapping behavior (ADDU) for
    // simplicity.
    result = src1 + src2;
    break;

  case ALUControl::SUB:
    result = src1 - src2;
    break;

  case ALUControl::AND:
    result = src1 & src2;
    break;

  case ALUControl::OR:
    result = src1 | src2;
    break;

  case ALUControl::XOR:
    result = src1 ^ src2;
    break;

  case ALUControl::NOR:
    result = ~(src1 | src2);
    break;

  case ALUControl::SLL:
    result = src2 << (shamt & 0x1F);
    break;

  case ALUControl::SRL:
    result = src2 >> (shamt & 0x1F);
    break;

  case ALUControl::SRA:
    // Arithmetic right shift uses signed integer
    result =
        static_cast<uint32_t>(static_cast<int32_t>(src2) >> (shamt & 0x1F));
    break;

  case ALUControl::MULT: {
    uint64_t prod =
        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(src1)) *
                              static_cast<int64_t>(static_cast<int32_t>(src2)));
    result = static_cast<uint32_t>(prod & 0xFFFFFFFF);
    hiResult = static_cast<uint32_t>(prod >> 32);
    writesHiLo = true;
    break;
  }

  case ALUControl::DIV: {
    if (src2 != 0) {
      result = static_cast<uint32_t>(static_cast<int32_t>(src1) /
                                     static_cast<int32_t>(src2));
      hiResult = static_cast<uint32_t>(static_cast<int32_t>(src1) %
                                       static_cast<int32_t>(src2));
    }
    writesHiLo = true;
    break;
  }

  case ALUControl::SLT:
    // SLT (Set Less Than) does a SIGNED comparison
    // If we treated them as uint32_t directly, -1 (0xFFFFFFFF) would be > 1.
    // So we cast to int32_t.
    result = (static_cast<int32_t>(src1) < static_cast<int32_t>(src2)) ? 1 : 0;
    break;
  }

  // Set the Zero Flag (True if result is 0)
  return {result, (result == 0), hiResult, writesHiLo};
}

} // namespace MIPS