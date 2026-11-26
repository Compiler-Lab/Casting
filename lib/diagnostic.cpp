#include "diagnostic.h"
#include "tokenize.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
std::string current_file = "<input>";
std::string_view current_source;

[[noreturn]] void verror_at(int line_no, std::string_view loc, const std::string& msg) {
    const char* base = current_source.data();
    const char* end_all = base + current_source.size();
    const char* p = loc.data();

    if (current_source.empty() || !p || p < base || p > end_all) {
        std::cerr << "error: " << msg;
        if (!loc.empty()) {
            std::cerr << " ('" << loc << "')";
        }
        std::cerr << "\n";
        std::exit(1);
    }

    const char* line = p;
    while (line > base && line[-1] != '\n') {
        --line;
    }

    const char* line_end = p;
    while (line_end < end_all && *line_end != '\n') {
        ++line_end;
    }

    std::string prefix = current_file + ":" + std::to_string(line_no) + ": ";
    std::cerr << prefix << std::string_view(line, line_end - line) << "\n";
    std::size_t indent = prefix.size() + static_cast<std::size_t>(p - line);
    std::cerr << std::string(indent, ' ') << "^ " << msg << "\n";
    std::exit(1);
}
} // namespace

namespace diagnostic {

void set_source(std::string_view filename, std::string_view source) {
    current_file = filename.empty() ? "<input>" : std::string(filename);
    current_source = source;
}

[[noreturn]] void error_at(std::string_view loc, const std::string& msg) {
    const char* base = current_source.data();
    const char* end_all = base + current_source.size();
    const char* p = loc.data();

    int line_no = 1;
    if (!current_source.empty() && p && p >= base && p <= end_all) {
        for (const char* cur = base; cur < p; ++cur) {
            if (*cur == '\n') {
                ++line_no;
            }
        }
    }

    verror_at(line_no, loc, msg);
}

[[noreturn]] void error_tok(const Token* tok, const std::string& msg) {
    if (!tok) {
        fatal(msg);
    }
    verror_at(tok->get_line_no(), tok->get_content(), msg);
}

[[noreturn]] void fatal(const std::string& msg) {
    std::cerr << "error: " << msg << "\n";
    std::exit(1);
}

} // namespace diagnostic
