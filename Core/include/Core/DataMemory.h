#pragma once
#include <cstdint>
#include <expected>
#include <memory_resource>
#include <string>
#include <vector>


namespace MIPS {

class DataMemory {
public:
  // Initialize with size (default 4MB)
  explicit DataMemory(size_t size = 4 * 1024 * 1024);

  // Read a 32-bit word (checks alignment)
  [[nodiscard]]
  std::expected<uint32_t, std::string> readWord(uint32_t address) const;

  // Write a 32-bit word (checks alignment)
  [[nodiscard]]
  std::expected<void, std::string> writeWord(uint32_t address, uint32_t value);

  // Debug helper: Load a binary blob (like a compiled program) into memory
  bool loadProgram(const std::vector<uint32_t> &program, uint32_t startAddress);

private:
  std::pmr::monotonic_buffer_resource memoryPool;
  std::pmr::vector<uint8_t> memory;
};

} // namespace MIPS