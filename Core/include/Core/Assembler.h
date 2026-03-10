#pragma once
#include <vector>
#include <string>
#include <expected>
#include <unordered_map>
#include "Parser.h"
#include "Instruction.h"

namespace MIPS {

    class Assembler {
    public:
        // Main API: Converts a list of tokens (e.g., ["ADD", "$t0", "$t1", "$t2"]) into binary
        [[nodiscard]]
        static std::expected<Instruction, std::string> assemble(const std::vector<Token>& tokens);

    private:
        // Helpers to map string names to hardware constants
        static std::expected<uint8_t, std::string> parseRegister(const std::string& regName);
        static std::expected<uint16_t, std::string> parseImmediate(const std::string& immStr);

        // Internal lookup tables
        // Note: In a full engine, these might be in a separate "ISA Definition" file
        static const std::unordered_map<std::string, int> registerMap;
    };

}