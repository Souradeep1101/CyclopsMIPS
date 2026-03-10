#pragma once
#include <cstdint>
#include "OpCode.h"

namespace MIPS {

    struct Instruction {
        uint32_t data; // The raw 32-bit machine code

        // --- Decode Helpers (Extracting Bits) ---

        // Opcode is top 6 bits (31-26)
        [[nodiscard]] constexpr uint8_t opcode() const {
            return (data >> 26) & 0x3F;
        }

        // RS (Source Register) is bits 25-21
        [[nodiscard]] constexpr uint8_t rs() const {
            return (data >> 21) & 0x1F;
        }

        // RT (Target Register) is bits 20-16
        [[nodiscard]] constexpr uint8_t rt() const {
            return (data >> 16) & 0x1F;
        }

        // RD (Destination Register) is bits 15-11 (R-Type only)
        [[nodiscard]] constexpr uint8_t rd() const {
            return (data >> 11) & 0x1F;
        }

        // Shamt (Shift Amount) is bits 10-6
        [[nodiscard]] constexpr uint8_t shamt() const {
            return (data >> 6) & 0x1F;
        }

        // Funct is bottom 6 bits (5-0)
        [[nodiscard]] constexpr uint8_t funct() const {
            return data & 0x3F;
        }

        // Immediate is bottom 16 bits (I-Type)
        [[nodiscard]] constexpr uint16_t imm() const {
            return data & 0xFFFF;
        }

        // Target Address is bottom 26 bits (J-Type)
        [[nodiscard]] constexpr uint32_t target() const {
            return data & 0x03FFFFFF;
        }

        // --- Encode Helpers (Building Bits) ---
        // We use the "Builder Pattern" logic here usually, but static factories are cleaner.

        static constexpr Instruction makeRType(uint8_t rs, uint8_t rt, uint8_t rd, uint8_t shamt, Funct funct) {
            return {
                (static_cast<uint32_t>(OpCode::R_TYPE) << 26) |
                (static_cast<uint32_t>(rs) << 21) |
                (static_cast<uint32_t>(rt) << 16) |
                (static_cast<uint32_t>(rd) << 11) |
                (static_cast<uint32_t>(shamt) << 6) |
                static_cast<uint32_t>(funct)
            };
        }

        static constexpr Instruction makeIType(OpCode op, uint8_t rs, uint8_t rt, int16_t imm) {
            return {
                (static_cast<uint32_t>(op) << 26) |
                (static_cast<uint32_t>(rs) << 21) |
                (static_cast<uint32_t>(rt) << 16) |
                static_cast<uint32_t>(static_cast<uint16_t>(imm))
            };
        }

        // J-Type: opcode[31:26] | target[25:0]
        static constexpr Instruction makeJType(OpCode op, uint32_t target26) {
            return {
                (static_cast<uint32_t>(op) << 26) |
                (target26 & 0x03FFFFFF)
            };
        }

        // Sign-extended immediate (used in ID stage)
        [[nodiscard]] constexpr int32_t signExtImm() const {
            return static_cast<int32_t>(static_cast<int16_t>(imm()));
        }
    };

}