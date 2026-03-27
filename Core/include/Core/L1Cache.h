#pragma once
#include "Core/MemoryBus.h"
#include <array>
#include <cstdint>

namespace MIPS {

// Simulates an L1 Direct-Mapped Cache
class L1Cache {
public:
  // Configurable latency properties
  static constexpr uint32_t CACHE_HIT_CYCLES = 0;
  static constexpr uint32_t CACHE_MISS_CYCLES = 0; // Ideal pipeline for education

  // Cache architecture: 4KB total size, 16 Byte Lines = 256 Lines
  static constexpr size_t LINE_SIZE = 16;
  static constexpr size_t NUM_LINES = 256;

  struct CacheLine {
    bool valid = false;
    bool dirty = false;
    uint32_t tag = 0;
    std::array<uint8_t, LINE_SIZE> data = {0};
  };

  L1Cache(MemoryBus &mainBus) : bus(mainBus) { reset(); }

  void reset() {
    for (auto &line : lines) {
      line.valid = false;
      line.dirty = false;
      line.tag = 0;
    }
    stats_hits = 0;
    stats_misses = 0;
  }

  // Returns {data, stall_cycles}
  // If stall_cycles > 0, it means it was a cache miss. The actual C++ read
  // happens instantly but the CPU must simulate the latency.
  struct CacheResult {
    uint32_t data;
    uint32_t stallCycles;
  };

  CacheResult readWord(uint32_t address) {
    uint32_t index = (address / LINE_SIZE) % NUM_LINES;
    uint32_t tag = address / (LINE_SIZE * NUM_LINES);
    uint32_t offset = address % LINE_SIZE;

    auto &line = lines[index];

    if (line.valid && line.tag == tag) {
      // Cache HIT
      stats_hits++;
      uint32_t value;
      std::memcpy(&value, &line.data[offset], sizeof(uint32_t));
      return {value, CACHE_HIT_CYCLES};
    }

    // Cache MISS
    stats_misses++;

    // 1. If old line is dirty, simulate write-back to Main Memory
    if (line.valid && line.dirty) {
      uint32_t oldAddress =
          (line.tag * NUM_LINES * LINE_SIZE) + (index * LINE_SIZE);
      for (size_t i = 0; i < LINE_SIZE; i += 4) {
        uint32_t writeVal;
        std::memcpy(&writeVal, &line.data[i], sizeof(uint32_t));
        (void)bus.writeWord(oldAddress + static_cast<uint32_t>(i), writeVal);
      }
    }

    // 2. Fetch new line from Main Memory
    uint32_t baseAddress = address - offset;
    for (size_t i = 0; i < LINE_SIZE; i += 4) {
      auto valOpt = bus.readWord(baseAddress + static_cast<uint32_t>(i));
      uint32_t readVal = valOpt.value_or(0); // If out of bounds, read 0
      std::memcpy(&line.data[i], &readVal, sizeof(uint32_t));
    }

    // 3. Update Cache Line Metadata
    line.valid = true;
    line.dirty = false;
    line.tag = tag;

    // 4. Return Requested Word
    uint32_t requestedValue;
    std::memcpy(&requestedValue, &line.data[offset], sizeof(uint32_t));

    return {requestedValue, CACHE_MISS_CYCLES};
  }

  // Returns stall cycles
  uint32_t writeWord(uint32_t address, uint32_t value) {
    uint32_t index = (address / LINE_SIZE) % NUM_LINES;
    uint32_t tag = address / (LINE_SIZE * NUM_LINES);
    uint32_t offset = address % LINE_SIZE;

    auto &line = lines[index];

    if (line.valid && line.tag == tag) {
      // Cache HIT (Write)
      stats_hits++;
      std::memcpy(&line.data[offset], &value, sizeof(uint32_t));
      line.dirty = true;
      return CACHE_HIT_CYCLES;
    }

    // Cache MISS (Write-Allocate protocol)
    stats_misses++;

    // 1. Evict if dirty
    if (line.valid && line.dirty) {
      uint32_t oldAddress =
          (line.tag * NUM_LINES * LINE_SIZE) + (index * LINE_SIZE);
      for (size_t i = 0; i < LINE_SIZE; i += 4) {
        uint32_t writeVal;
        std::memcpy(&writeVal, &line.data[i], sizeof(uint32_t));
        (void)bus.writeWord(oldAddress + static_cast<uint32_t>(i), writeVal);
      }
    }

    // 2. Fetch line from memory
    uint32_t baseAddress = address - offset;
    for (size_t i = 0; i < LINE_SIZE; i += 4) {
      auto valOpt = bus.readWord(baseAddress + static_cast<uint32_t>(i));
      uint32_t readVal = valOpt.value_or(0);
      std::memcpy(&line.data[i], &readVal, sizeof(uint32_t));
    }

    // 3. Apply the write to the fetched line
    std::memcpy(&line.data[offset], &value, sizeof(uint32_t));

    // 4. Update Metadata
    line.valid = true;
    line.dirty = true; // It's now dirty!
    line.tag = tag;

    return CACHE_MISS_CYCLES;
  }

  uint64_t getHits() const { return stats_hits; }
  uint64_t getMisses() const { return stats_misses; }

private:
  MemoryBus &bus;
  std::array<CacheLine, NUM_LINES> lines;

  uint64_t stats_hits = 0;
  uint64_t stats_misses = 0;
};

} // namespace MIPS
