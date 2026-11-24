#include "codegen.h"
#include "context.h"
#include "parser.h"
#include "tokenize.h"
#include "utils.h"
#include <cstdlib>

int main(int argc, char** argv) {
    CliOptions clis = parse_args(argc, argv);
    ASTContext ctx;
    Token* token = TokenizeFile(clis.input_path, ctx);
    Parser parser(token, ctx);
    Program* program_translation_unit = parser.parse();
    OutputStream os(clis.output_path);
    CodeGen codegen(os.get());
    codegen.generate(program_translation_unit);
    os.flush_and_check(clis.output_path);
    return 0;
}