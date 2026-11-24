#pragma once

#include <string>
#include <string_view>

class Token;

namespace diagnostic {

void set_source(std::string_view filename, std::string_view source);

// Print message (and optional location), then exit. No line/column info yet.
[[noreturn]] void error_at(std::string_view loc, const std::string& msg);
[[noreturn]] void error_tok(Token* tok, const std::string& msg);
[[noreturn]] void fatal(const std::string& msg);

} // namespace diagnostic
