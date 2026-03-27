#include "Core/CPU.h"
#include "Core/OpCode.h"
#include <format>

namespace MIPS {

CPU::CPU(MemoryBus& bus_ref)
    : regFile(state), bus(bus_ref), l1i_cache(bus_ref),
      l1d_cache(bus_ref) {
  reset();
}

void CPU::reset() {
  state.pc = 0;
  state.next_pc = 0;
  state.regs.fill(0);
  state.hi = 0;
  state.lo = 0;
  state.perf.reset();

  stallCycles = 0;
  stallSource = StallSource::None;
  pcOverride = false;
  pcOverrideValue = 0;
  flushNextFetch = false;
  loadUseHazard = false;

  btb.reset();
  l1i_cache.reset();
  l1d_cache.reset();

  if_id = IF_ID{};
  id_ex = ID_EX{};
  ex_mem = EX_MEM{};
  mem_wb = MEM_WB{};
  mem_wb_old = MEM_WB{}; // Snapshot for correct pipeline forwarding
}

bool CPU::loadProgram(const std::vector<uint32_t> &binary) {
  return bus.loadProgram(binary, 0x0000);
}

std::expected<void, std::string> CPU::step() {
  state.perf.cycles++; // Every call to step() = 1 clock cycle
  mem_wb_old = mem_wb; // Snapshot for pipeline forwarding

  // Clear last-changed tracking at start of cycle
  state.lastChangedReg = 0xFF;
  state.lastChangedAddr = 0xFFFFFFFF;

  if (stallCycles > 0) {
    stallCycles--;
    state.perf.stallCycles++;

    // Drain pipeline stages AFTER the stall point.
    if (stallSource == StallSource::Memory) {
      writeBack();
      mem_wb.valid = false; // Bubble
    } else if (stallSource == StallSource::Fetch) {
      writeBack();
      auto memRes = memoryAccess();
      if (!memRes)
        return std::unexpected(memRes.error());
      execute();
      auto decRes = decode();
      if (!decRes)
        return std::unexpected(decRes.error());
      
      // If a load-use hazard was detected while draining the Fetch stall,
      // KEEP if_id so it can be decoded next cycle.
      if (!loadUseHazard) {
        if_id.valid = false; // Bubble
      }
    }
    return {};
  }

  writeBack();

  auto memRes = memoryAccess();
  if (!memRes)
    return std::unexpected(memRes.error());

  if (stallCycles > 0) {
    // memoryAccess requested a stall!
    mem_wb.valid = false;
    return {}; // Freeze EX, ID, IF
  }

  execute();

  auto decRes = decode();
  if (!decRes)
    return std::unexpected(decRes.error());

  if (loadUseHazard) {
    // 1-cycle pipeline bubble injected by decode. IF and PC are frozen.
    hazardFlags |= 0x1;
    loadUseHazard = false;
  } else {
    auto fetRes = fetch();
    if (!fetRes)
      return std::unexpected(fetRes.error());

    if (stallCycles > 0) {
      // fetch requested a multi-cycle stall (I-Cache miss)
      if_id.valid = false;
      return {}; // Freeze PC
    }
  }

  if (flushNextFetch) {
      hazardFlags |= 0x2;
  }

  // Latch PC at the end of the clock cycle
  state.pc = state.next_pc;

  return {};
}

void CPU::writeBack() {
  if (!mem_wb.valid)
    return;

  state.perf.instructions++; // Count instructions retiring from the pipeline

  uint32_t writeData = mem_wb.memToReg ? mem_wb.readData : mem_wb.aluResult;

  if (mem_wb.regWrite && mem_wb.destReg != 0) {
    regFile.write(mem_wb.destReg, writeData);
    state.lastChangedReg = mem_wb.destReg;
    state.lastChangedCycle = state.perf.cycles;
  }
}

std::expected<void, std::string> CPU::memoryAccess() {
  mem_wb.valid = ex_mem.valid;
  if (!ex_mem.valid)
    return {};

  // Pass-through signals
  mem_wb.pc = ex_mem.pc;
  mem_wb.aluResult = ex_mem.aluResult;
  mem_wb.destReg = ex_mem.destReg;
  mem_wb.regWrite = ex_mem.regWrite;
  mem_wb.memToReg = ex_mem.memToReg;
  mem_wb.readData = 0;

  if (ex_mem.memRead) {
    auto cacheRes = l1d_cache.readWord(ex_mem.aluResult);
    if (cacheRes.stallCycles > 0) {
      stallCycles = cacheRes.stallCycles;
      stallSource = StallSource::Memory;
      return {}; // Stall request
    }
    mem_wb.readData = cacheRes.data;
  }

  if (ex_mem.memWrite) {
    uint32_t stallCyclesNeeded =
        l1d_cache.writeWord(ex_mem.aluResult, ex_mem.writeData);
    if (stallCyclesNeeded > 0) {
      stallCycles = stallCyclesNeeded;
      stallSource = StallSource::Memory;
      return {}; // Stall request
    }
    state.lastChangedAddr = ex_mem.aluResult;
    state.lastChangedCycle = state.perf.cycles;
  }

  return {};
}

void CPU::execute() {
  if (!id_ex.valid) {
    ex_mem.valid = false;
    return;
  }

  // Forwarding Unit — write directly to public members for UI observability
  forwardA = 0; // 0: ID/EX, 1: MEM/WB, 2: EX/MEM
  forwardB = 0;

  // EX Hazard (Forward from EX/MEM)
  if (enableForwarding) {
    if (ex_mem.valid && ex_mem.regWrite && ex_mem.destReg != 0 && ex_mem.destReg == id_ex.rs) {
      forwardA = 2;
    }
    if (ex_mem.valid && ex_mem.regWrite && ex_mem.destReg != 0 && ex_mem.destReg == id_ex.rt) {
      forwardB = 2;
    }

    // MEM Hazard (Forward from MEM/WB using the old snapshot BEFORE memoryAccess)
    if (mem_wb_old.valid && mem_wb_old.regWrite && mem_wb_old.destReg != 0 &&
        !(ex_mem.valid && ex_mem.regWrite && ex_mem.destReg != 0 && ex_mem.destReg == id_ex.rs) &&
        mem_wb_old.destReg == id_ex.rs) {
      forwardA = 1;
    }
    if (mem_wb_old.valid && mem_wb_old.regWrite && mem_wb_old.destReg != 0 &&
        !(ex_mem.valid && ex_mem.regWrite && ex_mem.destReg != 0 && ex_mem.destReg == id_ex.rt) &&
        mem_wb_old.destReg == id_ex.rt) {
      forwardB = 1;
    }
  }

  // ALU Source 1 Mux
  uint32_t operand1 = id_ex.regData1;
  if (forwardA == 1)
    operand1 = (mem_wb_old.memToReg) ? mem_wb_old.readData : mem_wb_old.aluResult;
  else if (forwardA == 2)
    operand1 = ex_mem.aluResult;

  // ForwardB Mux Data (Used for Memory Write Data)
  uint32_t forwardBData = id_ex.regData2;
  if (forwardB == 1)
    forwardBData = (mem_wb_old.memToReg) ? mem_wb_old.readData : mem_wb_old.aluResult;
  else if (forwardB == 2)
    forwardBData = ex_mem.aluResult;

  // ALU Source 2 Mux
  uint32_t operand2 = id_ex.aluSrc ? id_ex.signExtImm : forwardBData;

  // Execute ALU
  auto aluRes = ALU::execute(operand1, operand2, id_ex.aluCtrl, id_ex.shamt);
  ex_mem.aluResult = aluRes.result;

  // HI/LO Register Write
  if (aluRes.writesHiLo) {
    state.hi = aluRes.hiResult;
    state.lo = aluRes.result;
  }

  // Destination Register Mux
  ex_mem.destReg = id_ex.regDst ? id_ex.rd : id_ex.rt;

  // Pass-through control signals mapping to the new EX_MEM struct
  ex_mem.pc = id_ex.pc;
  ex_mem.writeData = forwardBData; 
  ex_mem.memRead = id_ex.memRead;
  ex_mem.memWrite = id_ex.memWrite;
  ex_mem.regWrite = id_ex.regWrite;
  ex_mem.memToReg = id_ex.memToReg;
  ex_mem.valid = true;
}

std::expected<void, std::string> CPU::decode() {
  if (!if_id.valid) {
    id_ex.valid = false;
    return {};
  }

  Instruction instr = if_id.instr;
  uint8_t op = instr.opcode();
  uint8_t rs = instr.rs();
  uint8_t rt = instr.rt();

  // Hazard Detection Unit evaluates BEFORE id_ex is overwritten
  if (enableHazardDetection && id_ex.valid && id_ex.memRead && ((id_ex.rt == rs) || (id_ex.rt == rt))) {
    // Stall the pipeline: Prevent PC from incrementing
    state.next_pc = state.pc; 
    loadUseHazard = true;

    // Preserving IF/ID latch is handled by skipping fetch() in step()
    
    // Insert a bubble into ID_EX
    id_ex.regDst = false;
    id_ex.aluSrc = false;
    id_ex.memRead = false;
    id_ex.memWrite = false;
    id_ex.regWrite = false;
    id_ex.memToReg = false;
    id_ex.aluCtrl = ALUControl::NONE;
    id_ex.valid = false;

    return {};
  }

  id_ex.valid = true; // Write ID/EX Latch
  id_ex.pc = if_id.pc;
  id_ex.pcPlus4 = if_id.pcPlus4;
  id_ex.rs = rs;
  id_ex.rt = rt;
  id_ex.rd = instr.rd();
  id_ex.shamt = instr.shamt();

  // Read Register File (in a real CPU, this happens concurrently with control logic)
  uint32_t regData1_raw = regFile.read(rs);
  uint32_t regData2_raw = regFile.read(rt);

  // Early Forwarding for Branch Resolution (in ID Stage)
  id_ex.regData1 = regData1_raw;
  if (enableForwarding) {
    if (ex_mem.valid && ex_mem.regWrite && ex_mem.destReg != 0 && ex_mem.destReg == rs) {
      id_ex.regData1 = ex_mem.aluResult;
    } else if (mem_wb.valid && mem_wb.regWrite && mem_wb.destReg != 0 && mem_wb.destReg == rs) {
      id_ex.regData1 = mem_wb.memToReg ? mem_wb.readData : mem_wb.aluResult;
    }
  }

  id_ex.regData2 = regData2_raw;
  if (enableForwarding) {
    if (ex_mem.valid && ex_mem.regWrite && ex_mem.destReg != 0 && ex_mem.destReg == rt) {
      id_ex.regData2 = ex_mem.aluResult;
    } else if (mem_wb.valid && mem_wb.regWrite && mem_wb.destReg != 0 && mem_wb.destReg == rt) {
      id_ex.regData2 = mem_wb.memToReg ? mem_wb.readData : mem_wb.aluResult;
    }
  }

  uint16_t imm = instr.imm();
  id_ex.signExtImm = static_cast<int32_t>(static_cast<int16_t>(imm));

  // Default Control Signals (NOP)
  id_ex.regDst = false;
  id_ex.aluSrc = false;
  id_ex.memRead = false;
  id_ex.memWrite = false;
  id_ex.regWrite = false;
  id_ex.memToReg = false;
  id_ex.branch = false;
  id_ex.isBranchTaken = false; 
  id_ex.branchTarget = 0;
  id_ex.aluCtrl = ALUControl::NONE;

  // Decode Logic & Branch Resolution
  if (op == static_cast<uint8_t>(OpCode::R_TYPE)) {
    uint8_t funct = instr.funct();
    if (funct == static_cast<uint8_t>(Funct::JR)) {
      // JR is an unconditional jump, but target is from register.
      // It's a control hazard.
      pcOverride = true;
      pcOverrideValue = id_ex.regData1;
      flushNextFetch = true; // Flush IF stage
      id_ex.valid = false;   // Bubble in EX stage
      return {};
    }

    id_ex.regDst = true;
    if (funct == static_cast<uint8_t>(Funct::ADD)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::ADD;
    } else if (funct == static_cast<uint8_t>(Funct::SUB)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::SUB;
    } else if (funct == static_cast<uint8_t>(Funct::SLT)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::SLT;
    } else if (funct == static_cast<uint8_t>(Funct::AND)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::AND;
    } else if (funct == static_cast<uint8_t>(Funct::OR)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::OR;
    } else if (funct == static_cast<uint8_t>(Funct::XOR)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::XOR;
    } else if (funct == static_cast<uint8_t>(Funct::NOR)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::NOR;
    } else if (funct == static_cast<uint8_t>(Funct::SLL)) {
      if (id_ex.rd != 0) {
        id_ex.regWrite = true;
        id_ex.aluCtrl = ALUControl::SLL;
      } else {
        id_ex.regWrite = false;
        id_ex.aluCtrl = ALUControl::NONE; 
      }
    } else if (funct == static_cast<uint8_t>(Funct::SRL)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::SRL;
    } else if (funct == static_cast<uint8_t>(Funct::SRA)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::SRA;
    } else if (funct == static_cast<uint8_t>(Funct::MULT)) {
      id_ex.regWrite = false;
      id_ex.aluCtrl = ALUControl::MULT;
    } else if (funct == static_cast<uint8_t>(Funct::DIV)) {
      id_ex.regWrite = false;
      id_ex.aluCtrl = ALUControl::DIV;
    } else if (funct == static_cast<uint8_t>(Funct::MFHI)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::ADD;
      id_ex.regData1 = state.hi;
      id_ex.aluSrc = true;
      id_ex.signExtImm = 0;
    } else if (funct == static_cast<uint8_t>(Funct::MFLO)) {
      id_ex.regWrite = true;
      id_ex.aluCtrl = ALUControl::ADD;
      id_ex.regData1 = state.lo;
      id_ex.aluSrc = true;
      id_ex.signExtImm = 0;
    } else {
      return std::unexpected(std::format("Unknown R-Type Funct: {:#x}", funct));
    }
  } else if (op == static_cast<uint8_t>(OpCode::ADDI)) {
    id_ex.aluSrc = true;
    id_ex.regWrite = true;
    id_ex.aluCtrl = ALUControl::ADD;
  } else if (op == static_cast<uint8_t>(OpCode::LW)) {
    id_ex.aluSrc = true;
    id_ex.memToReg = true;
    id_ex.regWrite = true;
    id_ex.memRead = true;
    id_ex.aluCtrl = ALUControl::ADD;
  } else if (op == static_cast<uint8_t>(OpCode::SW)) {
    id_ex.aluSrc = true;
    id_ex.memWrite = true;
    id_ex.aluCtrl = ALUControl::ADD;
  } else if (op == static_cast<uint8_t>(OpCode::BEQ) ||
             op == static_cast<uint8_t>(OpCode::BNE)) {
    bool shouldBranch = false;
    if (op == static_cast<uint8_t>(OpCode::BEQ))
      shouldBranch = (id_ex.regData1 == id_ex.regData2);
    else
      shouldBranch = (id_ex.regData1 != id_ex.regData2);

    uint32_t target = id_ex.pcPlus4 + (id_ex.signExtImm << 2);

    // Early branch resolution data mapping for the UI
    id_ex.branch = true;
    id_ex.isBranchTaken = shouldBranch;
    id_ex.branchTarget = target;

    // Update BTB
    if (enableBranchPrediction) {
      btb.update(if_id.pc, target, shouldBranch);
    }

    if (shouldBranch) {
      if (enableBranchPrediction && if_id.predictedTaken && if_id.predictedTarget == target) {
        // Correct prediction! No flush.
      } else {
        // Mispredicted not taken (or wrong target). Flush IF, jump.
        pcOverride = true;
        pcOverrideValue = target;
        flushNextFetch = true;
      }
    } else {
      if (enableBranchPrediction && if_id.predictedTaken) {
        // Mispredicted taken. Flush IF, resume normal execution.
        pcOverride = true;
        pcOverrideValue = id_ex.pcPlus4;
        flushNextFetch = true;
      } else {
        // Correct prediction (Not Taken). No flush.
      }
    }

    id_ex.regDst = false;
    id_ex.aluSrc = false;
    id_ex.memRead = false;
    id_ex.memWrite = false;
    id_ex.regWrite = false;
    id_ex.memToReg = false;
    id_ex.aluCtrl = ALUControl::NONE;
    id_ex.valid = true;

  } else if (op == static_cast<uint8_t>(OpCode::J) ||
             op == static_cast<uint8_t>(OpCode::JAL)) {
    uint32_t target = (id_ex.pcPlus4 & 0xF0000000) | (instr.target() << 2);

    // Update BTB
    if (enableBranchPrediction) {
      btb.update(if_id.pc, target, true);
    }

    if (enableBranchPrediction && if_id.predictedTaken && if_id.predictedTarget == target) {
      // Correct prediction!
    } else {
      // Mispredict!
      pcOverride = true;
      pcOverrideValue = target;
      flushNextFetch = true;
    }

    if (op == static_cast<uint8_t>(OpCode::JAL)) {
      id_ex.regWrite = true;
      id_ex.rd = 31; // $ra
      id_ex.regDst = true;
      id_ex.memToReg = false;
      id_ex.aluCtrl = ALUControl::ADD;
      id_ex.regData1 = id_ex.pcPlus4 + 4;
      id_ex.aluSrc = true;
      id_ex.signExtImm = 0;
    } else {
      id_ex.regWrite = false;
      id_ex.aluCtrl = ALUControl::NONE;
    }

    id_ex.memRead = false;
    id_ex.memWrite = false;
    id_ex.valid = true;
  } else {
    return std::unexpected(std::format("Unknown OpCode: {:#x}", op));
  }

  return {};
}

std::expected<void, std::string> CPU::fetch() {
  // Instruction Fetch
  if (flushNextFetch) {
    if_id.valid = false;
    state.next_pc = pcOverrideValue;
    flushNextFetch = false;
    pcOverride = false;
    return {};
  }

  auto cacheRes = l1i_cache.readWord(state.pc);
  if (cacheRes.stallCycles > 0) {
    stallCycles = cacheRes.stallCycles;
    stallSource = StallSource::Fetch;
    return {}; // Stall request
  }

  uint32_t instrData = cacheRes.data;
  if_id.instr = Instruction{instrData};
  if_id.pcPlus4 = state.pc + 4;
  if_id.pc = state.pc;
  if_id.valid = true;

  if (pcOverride) {
    state.next_pc = pcOverrideValue;
    pcOverride = false;
  } else {
    // BTB Prediction
    uint32_t predictedTarget = 0;
    if (enableBranchPrediction && btb.predict(state.pc, predictedTarget)) {
      if_id.predictedTaken = true;
      if_id.predictedTarget = predictedTarget;
      state.next_pc = predictedTarget;
    } else {
      if_id.predictedTaken = false;
      state.next_pc = state.pc + 4;
    }
  }

  return {};
}

} // namespace MIPS