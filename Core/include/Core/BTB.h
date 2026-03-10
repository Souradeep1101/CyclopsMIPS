#pragma once
#include <array>
#include <cstdint>


namespace MIPS {

enum class BranchPrediction : uint8_t {
  StronglyNotTaken = 0,
  WeaklyNotTaken = 1,
  WeaklyTaken = 2,
  StronglyTaken = 3
};

class BTB {
public:
  // Size of the BTB in realistic hardware (e.g., 256 entries)
  static constexpr size_t NUM_ENTRIES = 256;

  bool predict(uint32_t pc, uint32_t &outTargetPC) const {
    // Map PC to index (Instructions are 4-byte aligned, so shift right by 2)
    size_t index = (pc >> 2) % NUM_ENTRIES;
    const auto &entry = entries[index];

    if (entry.valid && entry.tagPC == pc) {
      if (entry.state == BranchPrediction::WeaklyTaken ||
          entry.state == BranchPrediction::StronglyTaken) {
        outTargetPC = entry.targetPC;
        return true;
      }
    }
    return false;
  }

  void update(uint32_t pc, uint32_t actualTarget, bool actuallyTaken) {
    size_t index = (pc >> 2) % NUM_ENTRIES;
    auto &entry = entries[index];

    if (!entry.valid || entry.tagPC != pc) {
      // New branch identified. Replace whatever is here.
      entry.valid = true;
      entry.tagPC = pc;
      entry.targetPC = actualTarget;
      // Seed the 2-bit counter
      entry.state = actuallyTaken ? BranchPrediction::WeaklyTaken
                                  : BranchPrediction::WeaklyNotTaken;
    } else {
      // Update existing branch execution result
      entry.targetPC = actualTarget; // Update in case target changed (e.g. JR)

      if (actuallyTaken) {
        if (entry.state != BranchPrediction::StronglyTaken) {
          entry.state =
              static_cast<BranchPrediction>(static_cast<int>(entry.state) + 1);
        }
      } else {
        if (entry.state != BranchPrediction::StronglyNotTaken) {
          entry.state =
              static_cast<BranchPrediction>(static_cast<int>(entry.state) - 1);
        }
      }
    }
  }

  void reset() { entries.fill(BTBEntry{}); }

private:
  struct BTBEntry {
    bool valid = false;
    uint32_t tagPC = 0;
    uint32_t targetPC = 0;
    BranchPrediction state = BranchPrediction::WeaklyNotTaken;
  };

  std::array<BTBEntry, NUM_ENTRIES> entries;
};

} // namespace MIPS
