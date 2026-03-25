#pragma once
#include <expected>
#include <string>
#include <vector>

#include "ALU.h"
#include "Core/BTB.h" // Added for Branch Target Buffer
#include "DataMemory.h"
#include "Instruction.h"
#include "L1Cache.h"
#include "PipelineLatches.h"
#include "RegisterFile.h"
#include "State.h"
#include "Core/MemoryBus.h"
#include <atomic>


namespace MIPS {

enum class StallSource { None, Fetch, Memory };

class CPU {
public:
  explicit CPU(MemoryBus& bus);

  // Resets PC to 0, clears registers and latches
  void reset();

  // Loads binary machine code into memory starting at 0x0000
  bool loadProgram(const std::vector<uint32_t> &binary);

  // The Heartbeat: Executes ONE clock cycle
  [[nodiscard]]
  std::expected<void, std::string> step();

  // Inspection/Mutation for UI/Tests
  const CpuState &getState() const { return state; }
  void setRegister(uint8_t reg, uint32_t val) { regFile.write(reg, val); }
  
  const MemoryBus &getMemoryBus() const { return bus; }
  // Pipeline Optimization Toggles (for educational visualization)
  bool enableForwarding = true;
  bool enableHazardDetection = true;
  bool enableBranchPrediction = true;

  // Pipeline Latches (Made public temporarily for debugging)
  IF_ID if_id;
  ID_EX id_ex;
  EX_MEM ex_mem;
  MEM_WB mem_wb;
  MEM_WB mem_wb_old;

  uint32_t hazardFlags = 0; // 0x1: Load-Use, 0x2: Branch Flush

  // Forwarding state (set by execute(), read by UI)
  // 0: no forward, 1: MEM/WB forward, 2: EX/MEM forward
  uint8_t forwardA = 0;
  uint8_t forwardB = 0;

private:
  CpuState state;
  RegisterFile regFile;
  MemoryBus& bus;
  
  // Thread-safe interrupt flag for Phase 8 MMIO integration
  std::atomic<bool> hardwareInterruptPending{false};

  // Branch Target Buffer (Dynamic Branch Prediction)
  BTB btb;

  // L1 Caches
  L1Cache l1i_cache;
  L1Cache l1d_cache;

  // Pipeline Stall Signals
  uint32_t stallCycles = 0;
  StallSource stallSource = StallSource::None;

  // Branch Resolution Signals (for cycle-accurate branch penalties)
  bool pcOverride = false;
  uint32_t pcOverrideValue = 0;
  bool flushNextFetch = false;
  bool loadUseHazard = false;

  // Pipeline Stages
  void writeBack();
  std::expected<void, std::string> memoryAccess();
  void execute();
  std::expected<void, std::string> decode();
  std::expected<void, std::string> fetch();
};

} // namespace MIPS