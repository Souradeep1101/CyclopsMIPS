#pragma once
#include <cstdint>
#include <expected>
#include <string>
#include <variant>
#include <vector>


namespace MIPS {

enum class TokenType {
  Opcode,    // "ADD", "ADDI", "J"
  Register,  // "$t0", "$zero", "$8"
  Immediate, // "10", "-5", "0x1F"
  Label,     // "LOOP:"
  LabelRef,  // "LOOP"
  Comma,     // ","
  LParen,    // "("
  RParen,    // ")"
  EOF_Token
};

struct Token {
  TokenType type;
  std::string value;
  int lineNum;
};

class Lexer {
public:
  Lexer(const std::string &source);
  std::vector<Token> tokenize();

private:
  std::string source;
  size_t pos = 0;
  int lineNum = 1;

  void skipWhitespace();
  Token readIdentifierOrKeyword();
  Token readNumber();
  Token readRegister();
};

// --- Parser Structures ---
struct ParsedInstruction {
  std::string opcode;
  std::vector<std::variant<std::string, int32_t>>
      operands; // Can hold registers ("$t0"), immediates (10), or label refs
                // ("LOOP")
  int lineNum;
};

class Parser {
public:
  Parser(const std::vector<Token> &tokens);
  std::expected<std::vector<ParsedInstruction>, std::string> parse();

private:
  std::vector<Token> tokens;
  size_t pos = 0;

  const Token &peek(size_t offset = 0) const;
  const Token &advance();
  bool match(TokenType expected);
  std::expected<Token, std::string> expect(TokenType expected,
                                           const std::string &errorMsg);

  std::expected<ParsedInstruction, std::string> parseInstruction();
  std::expected<std::variant<std::string, int32_t>, std::string> parseOperand();
};

} // namespace MIPS
