#include "Assembler/Parser.h"
#include <cctype>
#include <format>

namespace MIPS {

Lexer::Lexer(const std::string &sourceStr) : source(sourceStr) {}

void Lexer::skipWhitespace() {
  while (pos < source.length() && std::isspace(static_cast<unsigned char>(source[pos]))) {
    if (source[pos] == '\n')
      lineNum++;
    pos++;
  }
  // Skip Comments
  if (pos < source.length() && source[pos] == '#') {
    while (pos < source.length() && source[pos] != '\n') {
      pos++;
    }
    skipWhitespace(); // Recursively skip trailing spaces/newlines
  }
}

Token Lexer::readIdentifierOrKeyword() {
  std::string val;
  while (pos < source.length() &&
         (std::isalnum(static_cast<unsigned char>(source[pos])) || source[pos] == '_')) {
    val += source[pos++];
  }

  if (pos < source.length() && source[pos] == ':') {
    val += ':';
    pos++;
    return {TokenType::Label, val, lineNum};
  }

  // Technically, this could be an Opcode or a Label Reference. Let parser
  // decide based on context, but for now, we'll label it as an Opcode/LabelRef.
  // Let's assume caps are opcodes for now? No, we shouldn't assume casing.
  // We'll just call it Opcode if it's the first token in a line, or LabelRef
  // otherwise. That's a parser job. Let's call them all Opcode/LabelRefs and
  // let Parser fix it. Let's just return LabelRef for now, as Opcode is
  // structurally identical to a LabelRef.
  return {TokenType::Opcode, val, lineNum};
}

Token Lexer::readNumber() {
  std::string val;
  if (source[pos] == '-') {
    val += source[pos++];
  }

  bool isHex = false;
  if (pos + 1 < source.length() && source[pos] == '0' &&
      (source[pos + 1] == 'x' || source[pos + 1] == 'X')) {
    isHex = true;
    val += "0x";
    pos += 2;
  }

  while (pos < source.length() &&
         (std::isdigit(static_cast<unsigned char>(source[pos])) ||
          (isHex && std::isxdigit(static_cast<unsigned char>(source[pos]))))) {
    val += source[pos++];
  }

  return {TokenType::Immediate, val, lineNum};
}

Token Lexer::readRegister() {
  std::string val;
  pos++; // Skip '$'
  while (pos < source.length() && std::isalnum(static_cast<unsigned char>(source[pos]))) {
    val += source[pos++];
  }
  return {TokenType::Register, val, lineNum};
}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;

  while (pos < source.length()) {
    skipWhitespace();
    if (pos >= source.length())
      break;

    char c = source[pos];
    if (c == '$') {
      tokens.push_back(readRegister());
    } else if (std::isdigit(static_cast<unsigned char>(c)) ||
               (c == '-' && pos + 1 < source.length() &&
                std::isdigit(static_cast<unsigned char>(source[pos + 1])))) {
      tokens.push_back(readNumber());
    } else if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      tokens.push_back(readIdentifierOrKeyword());
    } else if (c == ',') {
      tokens.push_back({TokenType::Comma, ",", lineNum});
      pos++;
    } else if (c == '(') {
      tokens.push_back({TokenType::LParen, "(", lineNum});
      pos++;
    } else if (c == ')') {
      tokens.push_back({TokenType::RParen, ")", lineNum});
      pos++;
    } else {
      // Unknown character
      pos++;
    }
  }

  tokens.push_back({TokenType::EOF_Token, "", lineNum});
  return tokens;
}

// --- Parser Implementation ---

Parser::Parser(const std::vector<Token> &tks) : tokens(tks) {}

const Token &Parser::peek(size_t offset) const {
  if (pos + offset >= tokens.size())
    return tokens.back();
  return tokens[pos + offset];
}

const Token &Parser::advance() {
  if (pos < tokens.size())
    pos++;
  return tokens[pos - 1];
}

bool Parser::match(TokenType expected) {
  if (peek().type == expected) {
    advance();
    return true;
  }
  return false;
}

std::expected<Token, std::string> Parser::expect(TokenType expected,
                                                 const std::string &errorMsg) {
  if (peek().type == expected) {
    return advance();
  }
  return std::unexpected(std::format("Line {}: {}", peek().lineNum, errorMsg));
}

std::expected<std::vector<ParsedInstruction>, std::string> Parser::parse() {
  std::vector<ParsedInstruction> instructions;

  while (peek().type != TokenType::EOF_Token) {
    if (peek().type == TokenType::Label) {
      // Add the label as a pseudo-instruction so Pass 1 can catch it
      instructions.push_back({peek().value, {}, peek().lineNum});
      advance();
      continue;
    }

    auto instrRes = parseInstruction();
    if (!instrRes)
      return std::unexpected(instrRes.error());
    instructions.push_back(*instrRes);
  }

  return instructions;
}

std::expected<ParsedInstruction, std::string> Parser::parseInstruction() {
  Token opToken = advance();
  if (opToken.type != TokenType::Opcode) {
    return std::unexpected(
        std::format("Line {}: Expected mnemonic (Opcode), found '{}'",
                    opToken.lineNum, opToken.value));
  }

  ParsedInstruction instr;
  instr.opcode = opToken.value;
  instr.lineNum = opToken.lineNum;

  // Parse operands.
  //
  // IMPORTANT: The lexer emits bare identifiers ("LOOP", "ADD" etc.) all as
  // TokenType::Opcode because it cannot distinguish a mnemonic from a label ref.
  // We disambiguate with a simple flag:
  //   expectOperand = true  → consume the next token as an operand regardless.
  //   expectOperand = false → an Opcode token is the NEXT instruction's mnemonic.
  //
  // We start with expectOperand = true only after the first comma-delimited token.
  bool expectOperand = false;
  while (peek().type != TokenType::EOF_Token && peek().type != TokenType::Label) {
    // A bare identifier (Opcode token) stops operand collection ONLY if we're
    // not expecting one (i.e., we haven't just seen a comma or this is the
    // very first peek after the mnemonic and registers are exhausted).
    if (peek().type == TokenType::Opcode && !expectOperand) {
      std::string opU = instr.opcode;
      for (auto &c : opU) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      
      if ((opU == "J" || opU == "JAL") && instr.operands.empty()) {
        // Special case: J and JAL take exactly one label operand. 
        // Do not break; consume the identifier as the operand.
      } else {
        break; // This is the next instruction's mnemonic
      }
    }

    // Memory offset operands like `10($t0)` parse as: Immediate, LParen,
    // Register, RParen
    if (peek().type == TokenType::Immediate &&
        peek(1).type == TokenType::LParen) {
      auto immToken = advance();
      advance(); // Consume '('

      auto regTokenOpt =
          expect(TokenType::Register, "Expected register inside parenthesis");
      if (!regTokenOpt)
        return std::unexpected(regTokenOpt.error());

      expect(TokenType::RParen, "Expected closing parenthesis ')'");

      // Memory offset uses 2 operands: Immediate then Source Base Register
      instr.operands.push_back(std::stoi(immToken.value, nullptr, 0));
      instr.operands.push_back(regTokenOpt->value);

      // After consuming an operand, check for comma
      expectOperand = false;
      if (match(TokenType::Comma)) expectOperand = true;
      continue;
    }

    auto opRes = parseOperand();
    if (!opRes)
      return std::unexpected(opRes.error());
    instr.operands.push_back(*opRes);

    // After consuming an operand, consume optional comma.
    // If a comma exists, the parser MUST consume the next token as an operand.
    expectOperand = false;
    if (match(TokenType::Comma)) expectOperand = true;
  }

  return instr;
}

std::expected<std::variant<std::string, int32_t>, std::string>
Parser::parseOperand() {
  Token t = advance();
  if (t.type == TokenType::Register) {
    return t.value;
  } else if (t.type == TokenType::Immediate) {
    // Parse immediate safely (supports hex and negative)
    return static_cast<int32_t>(std::stoi(t.value, nullptr, 0));
  } else if (t.type == TokenType::Opcode) {
    // A solitary identifier found as an operand is a Label Reference (e.g. `J
    // LOOP`)
    return t.value;
  }

  return std::unexpected(std::format(
      "Line {}: Unexpected token in operand list '{}'", t.lineNum, t.value));
}

} // namespace MIPS