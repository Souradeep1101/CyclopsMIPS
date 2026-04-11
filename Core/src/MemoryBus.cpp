#include "Core/MemoryBus.h"

namespace MIPS {

MemoryBus::MemoryBus() : ram(4 * 1024 * 1024) { // 4MB RAM
}

void MemoryBus::reset() {
    ram.reset();
}

std::expected<uint32_t, std::string> MemoryBus::readWord(uint32_t address) const {
    return ram.readWord(address);
}

std::expected<void, std::string> MemoryBus::writeWord(uint32_t address, uint32_t value) {
    return ram.writeWord(address, value);
}

uint32_t MemoryBus::readWordDirect(uint32_t address) const {
    auto result = ram.readWord(address);
    if (result) return *result;
    return 0;
}

bool MemoryBus::loadProgram(const std::vector<uint32_t>& binary, uint32_t startAddress) {
    return ram.loadProgram(binary, startAddress);
}

uint8_t MemoryBus::readByteDirect(uint32_t address) const {
    return ram.readByteDirect(address);
}

} // namespace MIPS
