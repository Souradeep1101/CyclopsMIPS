#include "Core/DataMemory.h"
#include <cstring> // for memcpy
#include <format>


namespace MIPS {

DataMemory::DataMemory(size_t size) : memoryPool(size), memory(&memoryPool) {
  memory.resize(size, 0); // Zero-initialize RAM
}

void DataMemory::reset() {
  std::fill(memory.begin(), memory.end(), static_cast<uint8_t>(0));
}

std::expected<uint32_t, std::string>
DataMemory::readWord(uint32_t address) const {
  // 1. Check Bounds
  if (address + 4 > memory.size()) {
    return std::unexpected(std::format(
        "Segmentation Fault: Address {:#x} out of bounds", address));
  }

  // 2. Check Alignment (Address must be divisible by 4)
  if (address % 4 != 0) {
    return std::unexpected(std::format(
        "Alignment Error: Address {:#x} is not 4-byte aligned", address));
  }

  // 3. Read Data (Little Endian: Direct bit copy)
  // We use memcpy to handle the bytes safely
  uint32_t value;
  std::memcpy(&value, &memory[address], sizeof(uint32_t));
  return value;
}

std::expected<void, std::string> DataMemory::writeWord(uint32_t address,
                                                       uint32_t value) {
  if (address + 4 > memory.size()) {
    return std::unexpected(std::format(
        "Segmentation Fault: Address {:#x} out of bounds", address));
  }

  if (address % 4 != 0) {
    return std::unexpected(std::format(
        "Alignment Error: Address {:#x} is not 4-byte aligned", address));
  }

  std::memcpy(&memory[address], &value, sizeof(uint32_t));
  return {};
}

bool DataMemory::loadProgram(const std::vector<uint32_t> &program,
                             uint32_t startAddress) {
  if (startAddress + (program.size() * 4) > memory.size())
    return false;

  for (uint32_t instruction : program) {
    if (!writeWord(startAddress, instruction))
      return false;
    startAddress += 4;
  }
  return true;
}

} // namespace MIPS