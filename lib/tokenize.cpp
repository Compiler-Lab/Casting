#include "tokenize.h"
#include "context.h"
#include "diagnostic.h"
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

static constexpr std::array<std::string_view, 4> multi_char_ops = {"==", "!=", ">=", "<="};
static constexpr std::array<std::string_view, 8> keywords = {"if",    "else", "return", "for",
                                                             "while", "int",  "sizeof", "char"};

bool Tkequal(Token* TK, const char* op) {
    if (!TK || TK->get_content().empty())
        return false;
    return TK->get_content() == op;
}

int Token::get_number() const {
    if (this->TKind != TokenKind::NUM) {
        diagnostic::error_at(this->TKContent, "expected a number");
    }
    return this->TKval;
}

void Tkskip(Token*& TK, const char* op) {
    if (!Tkequal(TK, op)) {
        diagnostic::error_at(TK->get_content(), "expected '" + std::string(op) + "'");
    }
    TK = TK->get_next();
}

static bool if_keyword(std::string_view tok_content) {
    for (const std::string_view keyword : keywords) {
        if (tok_content == keyword) {
            return true;
        }
    }
    return false;
}

static int from_hex(char c) {
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}

static int read_escaped_char(char** new_pos, char* p) {
    // Octal escape sequences have a length limit of three octal digits
    if ('0' <= *p && *p <= '7') {
        int c = *p++ - '0';
        if ('0' <= *p && *p <= '7') {
            c = (c << 3) + static_cast<int>(*p++ - '0');
            if ('0' <= *p && *p <= '7')
                c = (c << 3) + static_cast<int>(*p++ - '0');
        }
        *new_pos = p;
        return c;
    }

    if (*p == 'x') {
        ++p;
        if (!std::isxdigit(static_cast<unsigned char>(*p))) {
            diagnostic::error_at(std::string_view(p, 1), "invalid hex escape sequence");
        }

        unsigned int c = 0;
        while (std::isxdigit(static_cast<unsigned char>(*p))) {
            c = (c << 4) + static_cast<unsigned int>(from_hex(*p));
            ++p;
        }
        *new_pos = p;
        return static_cast<unsigned char>(c);
    }

    *new_pos = p + 1;
    switch (*p) {
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'v':
        return '\v';
    case 'f':
        return '\f';
    case 'r':
        return '\r';
    case 'e':
        return 27;
    default:
        return *p;
    }
}

static char* string_literal_end(char* q) {
    char* start = q;
    while (*q != '"') {

        if (*q == '\\')
            ++q;

        if (*q == '\0' || *q == '\n') {
            diagnostic::error_at(std::string_view(start, q - start), "unterminated string literal");
        }
        ++q;
    }
    return q;
}

static Token* read_string_literal(char*& p, ASTContext& ctx) {
    char* start = p;
    char* end = string_literal_end(start + 1);
    std::string bytes;
    bytes.reserve(end - start - 1);

    for (char* cur = start + 1; cur < end;) {
        if (*cur == '\\') {
            bytes.push_back(static_cast<char>(read_escaped_char(&cur, cur + 1)));
        } else {
            bytes.push_back(*cur);
            cur++;
        }
    }
    bytes.push_back('\0'); // end of string literal
    int len = static_cast<int>(bytes.size());
    Token* new_token = ctx.make_token(TokenKind::STR, std::string_view(start, end - start + 1));
    new_token->set_type(ctx.make_array_type(ctx.get_char_type(), len));
    new_token->set_string(std::move(bytes));

    p = end + 1;
    return new_token;
}

static void convert_keyword(Token* TK) {
    while (TK->get_kind() != TokenKind::EOF_TK) {
        if (if_keyword(TK->get_content()) && TK->get_kind() == TokenKind::IDENT) {
            TK->set_kind(TokenKind::KEYWORD);
        }
        TK = TK->get_next();
    }
}

static bool starts_with(const char* p, std::string_view prefix) {
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (p[i] == '\0')
            return false;
    }
    return std::string_view(p, prefix.size()) == prefix;
}

// Returns true if c is valid as the first character of an identifier.
static bool is_ident1(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

// Returns true if c is valid as a non-first character of an identifier.
static bool is_ident2(char c) { return is_ident1(c) || ('0' <= c && c <= '9'); }

Token* Tokenize(char* Input, ASTContext& ctx, const char* filename) {
    diagnostic::set_source(filename ? filename : "<input>", std::string_view(Input));
    Token head(TokenKind::EOF_TK, {}, 0);
    Token* current = &head;

    while (*Input) {
        if (starts_with(Input, "//")) {
            Input += 2;
            while (*Input != '\n' && *Input) {
                Input++;
            }
            continue;
        }
        if (starts_with(Input, "/*")) {
            char* temp = Input + 2;
            while (*temp && !starts_with(temp, "*/")) {
                ++temp;
            }
            if (!*temp) {
                diagnostic::error_at(std::string_view(Input, 2), "unclosed block comment");
            }
            Input = temp + 2;
            continue;
        }

        if (std::isspace(*Input)) {
            Input++;
            continue;
        }

        if (*Input == '"') {
            Token* new_token = read_string_literal(Input, ctx);
            current->set_next(new_token);
            current = current->get_next();
            continue;
        }

        if (std::isdigit(*Input)) {
            char* temp = Input;
            int val = std::strtol(Input, &Input, 10);
            int len = static_cast<int>(Input - temp);
            Token* new_token = ctx.make_token(TokenKind::NUM, std::string_view(temp, len), val);
            current->set_next(new_token);
            current = current->get_next();
            continue;
        }
        if (is_ident1(*Input)) {
            char* start = Input;
            do {
                Input++;
            } while (is_ident2(*Input));
            Token* new_token =
                ctx.make_token(TokenKind::IDENT, std::string_view(start, Input - start));
            current->set_next(new_token);
            current = current->get_next();
            continue;
        }

        bool found_multi_ops = false;
        for (std::string_view op : multi_char_ops) {
            if (starts_with(Input, op)) {
                Token* new_token =
                    ctx.make_token(TokenKind::PUNCT, std::string_view(Input, op.size()));
                current->set_next(new_token);
                current = current->get_next();
                Input += op.size();
                found_multi_ops = true;
                break;
            }
        }
        if (found_multi_ops)
            continue;

        if (std::ispunct(*Input)) {
            Token* new_token = ctx.make_token(TokenKind::PUNCT, std::string_view(Input, 1));
            current->set_next(new_token);
            current = current->get_next();
            Input++;
            continue;
        }
        diagnostic::error_at(std::string_view(Input, 1), "invalid token");
    }
    Token* eof_token = ctx.make_token(TokenKind::EOF_TK, std::string_view(Input, 0));
    current->set_next(eof_token);
    convert_keyword(head.get_next());
    return head.get_next();
}

static std::string read_file(std::string_view path) {
    std::ifstream file;
    std::istream* in = nullptr;

    if (path == "-") {
        in = &std::cin;
    } else {
        file.open(std::string(path), std::ios::binary | std::ios::in);
        if (!file.is_open()) {
            diagnostic::fatal("cannot open file: " + std::string(path) + std::strerror(errno));
        }
        in = &file;
    }

    std::ostringstream oss;
    oss << in->rdbuf();
    if (in->bad()) {
        diagnostic::fatal("failed to read " + std::string(path));
    }
    std::string content = std::move(oss).str();
    if (content.empty() || content.back() != '\n') {
        content.push_back('\n');
    }
    return content;
}

Token* TokenizeFile(std::string_view path, ASTContext& ctx) {
    std::string content = read_file(path);
    SourceFile* srcf = ctx.read_source_file();
    srcf->name = std::string(path);
    srcf->content = std::move(content);
    return Tokenize(srcf->content.data(), ctx, srcf->name.data());
}