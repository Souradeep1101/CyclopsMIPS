#pragma once
#include <vector>
#include <string>
#include <string_view>
#include <expected> // C++23 Feature
#include <format>

namespace MIPS {

    enum class TokenType {
        Label,          // "main:"
        Instruction,    // "ADD", "LW"
        Register,       // "$t0", "$sp"
        Immediate,      // "100", "-5", "0xFF"
        Comma,          // "," (Optional, often ignored)
        Unknown
    };

    struct Token {
        TokenType type;
        std::string value;

        // Helper for debugging print
        std::string toString() const {
            switch (type) {
            case TokenType::Label: return std::format("[Label: {}]", value);
            case TokenType::Instruction: return std::format("[Op: {}]", value);
            case TokenType::Register: return std::format("[Reg: {}]", value);
            case TokenType::Immediate: return std::format("[Imm: {}]", value);
            default: return std::format("[Unknown: {}]", value);
            }
        }
    };

    class Parser {
    public:
        // Returns tokens OR an error string if parsing fails
        // [[nodiscard]] means the compiler warns us if we ignore the error
        [[nodiscard]]
        static std::expected<std::vector<Token>, std::string> tokenize(std::string_view line);

    private:
        static TokenType identifyToken(std::string_view str);
    };

}