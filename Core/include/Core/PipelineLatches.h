#pragma once
#include "ALU.h"
#include "Instruction.h"
#include <cstdint>

namespace MIPS {

struct IF_ID {
  uint32_t pcPlus4 = 0;
  Instruction instr{0};
  bool valid = false; 

  // Branch Prediction Information
  uint32_t pc = 0;              
  bool predictedTaken = false;  
  uint32_t predictedTarget = 0; 
};

// Instruction Decode to Execute
struct ID_EX {
  uint32_t pc = 0; 
  uint32_t pcPlus4 = 0;
  uint32_t regData1 = 0;
  uint32_t regData2 = 0;
  uint32_t signExtImm = 0;

  uint8_t rs = 0;
  uint8_t rt = 0;
  uint8_t rd = 0;
  uint8_t shamt = 0;

  // --- EX Control Bundle ---
  ALUControl aluCtrl = ALUControl::NONE;
  bool regDst = false; 
  bool aluSrc = false; 

  // --- M Control Bundle ---
  bool memRead = false;
  bool memWrite = false;

  // --- WB Control Bundle ---
  bool regWrite = false;
  bool memToReg = false;

  // Early Branch Resolution Tracking (Moved from EX_MEM for UI)
  bool branch = false;
  bool isBranchTaken = false; // Result of branch AND equality
  uint32_t branchTarget = 0;
  bool isSyscall = false;
  bool valid = false;
};

// Execute to Memory
struct EX_MEM {
  uint32_t pc = 0; 
  uint32_t aluResult = 0;
  uint32_t writeData = 0; 
  uint8_t destReg = 0;    

  // --- M Control Bundle ---
  bool memRead = false;
  bool memWrite = false;

  // --- WB Control Bundle ---
  bool regWrite = false;
  bool memToReg = false;

  bool valid = false;
};

// Memory to Writeback
struct MEM_WB {
  uint32_t pc = 0; 
  uint32_t readData = 0;  
  uint32_t aluResult = 0; 
  uint8_t destReg = 0;

  // --- WB Control Bundle ---
  bool regWrite = false;
  bool memToReg = false;

  bool valid = false;
};

} // namespace MIPS