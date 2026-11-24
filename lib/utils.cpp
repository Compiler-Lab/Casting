#include "utils.h"
#include "diagnostic.h"
#include <iostream>
#include <string>
#include <string_view>

void cli_usage(int status) {
    std::cerr << "Casting [ -o <path> ] <file>\n";
    std::exit(status);
}

CliOptions parse_args(int argc, char** argv) {
    if (argc < 2) {
        cli_usage(1);
    }

    CliOptions opts;
    for (int num_agrs = 1; num_agrs < argc; num_agrs++) {
        std::string_view arg = argv[num_agrs];

        if (arg == "--help") {
            cli_usage(0);
        }

        if (arg == "-o") {
            if (num_agrs + 1 >= argc) {
                cli_usage(2);
            }
            opts.output_path = std::string(argv[++num_agrs]);
            continue;
        }

        if (arg.starts_with("-o")) {
            opts.output_path = std::string(arg.substr(2));
            continue;
        }

        if (arg.starts_with("-") && arg != "-") {
            diagnostic::fatal("unknown argument:" + std::string(arg));
        }

        opts.input_path = std::string(arg);
    }

    if (opts.input_path.empty()) {
        diagnostic::fatal("no input files");
    }

    return opts;
}
