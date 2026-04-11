#pragma once
#include "DataMemory.h"
#include <atomic>
#include <mutex>
#include <expected>

namespace MIPS {

// The MemoryBus owns the system's Physical RAM and coordinates MMIO.
// It allows Symmetric Multi-Processing (SMP) if multiple CPUs connect to it.
class MemoryBus {
public:
    explicit MemoryBus();
    
    void reset();

    // DataMemory interface passthrough for Cache misses
    [[nodiscard]] std::expected<uint32_t, std::string> readWord(uint32_t address) const;
    [[nodiscard]] std::expected<void, std::string> writeWord(uint32_t address, uint32_t value);
    
    // Direct memory inspection for UI and syscalls without triggering cache hits
    [[nodiscard]] uint32_t readWordDirect(uint32_t address) const;
    [[nodiscard]] uint8_t readByteDirect(uint32_t address) const;

    bool loadProgram(const std::vector<uint32_t>& binary, uint32_t startAddress = 0x0000);
    
    // Allows inspection
    const DataMemory& getMemory() const { return ram; }
    DataMemory& getMemory() { return ram; }

private:
    DataMemory ram;
};

} // namespace MIPS
