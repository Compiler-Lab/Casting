#pragma once
#include "diagnostic.h"
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>

struct CliOptions {
    std::string input_path;
    std::string output_path;
};

[[noreturn]] void cli_usage(int status);
CliOptions parse_args(int argc, char** argv);

class OutputStream {
public:
    explicit OutputStream(std::string_view path = "-") {
        if (!path.empty() && path != "-") {
            file_.open(std::string(path));
            if (!file_.is_open()) {
                diagnostic::fatal("cannot open output file: " + std::string(path));
            }
            out_ = &file_;
        }
    }

    OutputStream(const OutputStream&) = delete;
    OutputStream& operator=(const OutputStream&) = delete;
    OutputStream(OutputStream&&) = delete;
    OutputStream& operator=(OutputStream&&) = delete;

    std::ostream& get() { return *out_; }

    operator std::ostream&() & { return *out_; }
    operator std::ostream&() && = delete;

    void flush_and_check(std::string_view path = "-") {
        out_->flush();
        if (out_->fail()) {
            diagnostic::fatal("failed to write output to " + std::string(path));
        }
    }

private:
    std::ofstream file_;
    std::ostream* out_ = &std::cout;
};