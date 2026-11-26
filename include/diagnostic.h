#pragma once

#include <string>
#include <string_view>

class Token;

namespace diagnostic {

void set_source(std::string_view filename, std::string_view source);

// Print source location with line/column caret, then exit.
[[noreturn]] void error_at(std::string_view loc, const std::string& msg);
[[noreturn]] void error_tok(const Token* tok, const std::string& msg);
[[noreturn]] void fatal(const std::string& msg);

} // namespace diagnostic
