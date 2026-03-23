#pragma once
#include "ALU.h"
#include "Instruction.h"
#include <cstdint>


namespace MIPS {

struct IF_ID {
  uint32_t pcPlus4 = 0;
  Instruction instr{0};
  bool valid =
      false; // True if holding valid instruction, False if stalled/flushed

  // Branch Prediction Information
  uint32_t pc = 0;              // PC of the fetched instruction
  bool predictedTaken = false;  // Did the BTB predict a branch was taken?
  uint32_t predictedTarget = 0; // Where did the BTB predict we went?
};

// Instruction Decode to Execute
struct ID_EX {
  uint32_t pc = 0; // Architectural tracker
  uint32_t pcPlus4 = 0;
  uint32_t regData1 = 0;
  uint32_t regData2 = 0;
  uint32_t signExtImm = 0;

  uint8_t rs = 0;
  uint8_t rt = 0;
  uint8_t rd = 0;
  uint8_t shamt = 0;

  // Execute Control Signals
  ALUControl aluCtrl = ALUControl::NONE;
  bool regDst = false; // false = rt, true = rd
  bool aluSrc = false; // false = regData2, true = signExtImm

  // Memory Control Signals
  bool memRead = false;
  bool memWrite = false;

  // Writeback Control Signals
  bool regWrite = false;
  bool memToReg = false;

  bool valid = false;
};

// Execute to Memory
struct EX_MEM {
  uint32_t pc = 0; // Architectural tracker
  uint32_t aluResult = 0;
  uint32_t writeData = 0; // Data to write to memory (from ID_EX.regData2)
  uint8_t destReg = 0;    // Muxed destination register (rt or rd)

  // Memory Control Signals
  bool memRead = false;
  bool memWrite = false;

  // Writeback Control Signals
  bool regWrite = false;
  bool memToReg = false;

  bool valid = false;
};

// Memory to Writeback
struct MEM_WB {
  uint32_t pc = 0; // Architectural tracker
  uint32_t readData = 0;  // Data read from memory
  uint32_t aluResult = 0; // Passed through from EX
  uint8_t destReg = 0;

  // Writeback Control Signals
  bool regWrite = false;
  bool memToReg = false;

  bool valid = false;
};

} // namespace MIPS
