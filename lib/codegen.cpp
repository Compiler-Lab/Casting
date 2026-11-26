#include "codegen.h"
#include "astnode.h"
#include "diagnostic.h"
#include <iostream>

// Round up n to the nearest multiple of align (e.g. 5,8 -> 8; 11,8 -> 16).
int CodeGen::align_to(int n, int align) { return (n + align - 1) / align * align; }

void CodeGen::load(Type* ty) {
    if (ty->get_kind() == TypeKind::TY_ARRAY) {
        return;
    }
    if (ty->get_size() == 1) {
        out_ << "    movsbq (%rax), %rax\n";
    } else {
        out_ << "    mov (%rax), %rax\n";
    }
}

void CodeGen::store(Type* ty) {
    pop("%rdi");

    if (ty->get_kind() == TypeKind::TY_ARRAY) {
        return;
    }
    if (ty->get_size() == 1) {
        out_ << "    movb %al, (%rdi)\n";
    } else {
        out_ << "    mov %rax, (%rdi)\n";
    }
}

void CodeGen::push() {
    out_ << "    push %rax\n";
    depth++;
}
void CodeGen::pop(const char* reg) {
    out_ << "    pop " << reg << "\n";
    depth--;
}

void CodeGen::emit_data(Program* program) {
    if (program->get_global_variable_begin()) {
        out_ << "    .data\n";
    } else {
        return;
    }
    for (GlobalVariable* gv = program->get_global_variable_begin(); gv; gv = gv->get_next()) {
        out_ << ".globl " << gv->get_name() << "\n";
        out_ << gv->get_name() << ":\n";
        if (gv->has_init_data()) {
            for (unsigned char c : gv->get_init_string())
                out_ << "    .byte " << static_cast<int>(c) << "\n";
        } else if (gv->get_has_init()) {
            out_ << "    .quad " << gv->get_init_val() << "\n";
        } else {
            out_ << "    .zero " << gv->get_ty()->get_size() << "\n";
        }
    }
}

// The SysV ABI passes the first six integer arguments in registers. We spill
// them into the stack slots assigned to the function's parameter variables so
// the rest of the compiler can treat parameters exactly like ordinary locals.
void CodeGen::store_function_params(Function* fn) {
    if (fn->get_params().size() > args_regs.size())
        diagnostic::error_tok(fn->get_tok(), "too many function parameters");

    for (std::size_t i = 0; i < fn->get_params().size(); ++i) {
        LocalVariable* param = fn->get_params()[i];
        if (param->get_ty()->get_kind() == TypeKind::TY_CHAR) {
            out_ << "    mov " << char_args_regs[i] << ", " << param->get_offset() << "(%rbp)\n";
        } else {
            out_ << "    mov " << args_regs[i] << ", " << param->get_offset() << "(%rbp)\n";
        }
    }
}

// Emit code to leave the address of the node in %rax. Valid for ND_VAR, ND_DEREF, ND_ADDR.
void CodeGen::gen_addr(Node* node) {
    switch (node->get_nodekind()) {
    case NodeKind::ND_VAR:
        out_ << "    lea " << node->get_var()->get_offset() << "(%rbp), %rax\n";
        break;
    case NodeKind::ND_GVAR:
        out_ << "    lea " << node->get_gvar()->get_name() << "(%rip), %rax\n";
        break;
    case NodeKind::ND_DEREF:
        gen_expr(node->get_lhs());
        return;
    case NodeKind::ND_ADDR:
        gen_addr(node->get_lhs());
        return;
    default:
        diagnostic::error_tok(node->get_tok(), "lvalue required for address-of or dereference");
    }
}

// Set each function's local var offsets and stack_size (16-byte aligned).
void CodeGen::assign_lvar_offsets(Function* prog) {
    for (Function* fn = prog; fn; fn = fn->get_next()) {
        int offset = 0;
        for (LocalVariable* var = fn->get_locals(); var; var = var->get_next()) {
            offset += var->get_ty()->get_size();
            var->set_offset(-offset);
        }
        fn->set_stack_size(align_to(offset, 16));
    }
}

void CodeGen::gen_expr(Node* node) {
    switch (node->get_nodekind()) {
    case NodeKind::ND_NUM:
        out_ << "    mov $" << node->get_val() << ", %rax\n";
        return;
    case NodeKind::ND_NEG:
        gen_expr(node->get_lhs());
        out_ << "    neg %rax\n";
        return;
    case NodeKind::ND_VAR:
    case NodeKind::ND_GVAR:
        gen_addr(node);
        load(node->get_ty());
        return;
    case NodeKind::ND_ASSIGN:
        gen_addr(node->get_lhs());
        push();
        gen_expr(node->get_rhs());
        store(node->get_lhs()->get_ty());
        return;
    case NodeKind::ND_ADDR:
        gen_addr(node->get_lhs());
        return;
    case NodeKind::ND_DEREF:
        gen_addr(node);
        load(node->get_ty());
        return;
    case NodeKind::ND_STMT_EXPR:
        for (Node* n = node->get_body(); n; n = n->get_nextstmt())
            gen_stmt(n);
        return;
    case NodeKind::ND_FUNCALL:
        int arg_count = 0;
        for (Node* arg = node->get_args(); arg; arg = arg->get_nextstmt()) {
            gen_expr(arg);
            push();
            arg_count++;
        }
        for (int i = arg_count - 1; i >= 0; i--) {
            pop(args_regs[i].data());
        }
        out_ << "    call " << node->get_func_name() << "\n";
        return;
    }

    gen_expr(node->get_rhs());
    push();
    gen_expr(node->get_lhs());
    pop("%rdi");

    switch (node->get_nodekind()) {
    case NodeKind::ND_ADD:
        out_ << "    add %rdi, %rax\n";
        break;
    case NodeKind::ND_SUB:
        out_ << "    sub %rdi, %rax\n";
        break;
    case NodeKind::ND_MUL:
        out_ << "    imul %rdi, %rax\n";
        break;
    case NodeKind::ND_DIV:
        out_ << "    cqo\n";
        out_ << "    idiv %rdi\n";
        break;

    case NodeKind::ND_LT:
    case NodeKind::ND_EQ:
    case NodeKind::ND_NE:
    case NodeKind::ND_LE:
        out_ << "    cmp %rdi, %rax\n";
        if (node->get_nodekind() == NodeKind::ND_EQ)
            out_ << "    sete %al\n";
        else if (node->get_nodekind() == NodeKind::ND_NE)
            out_ << "    setne %al\n";
        else if (node->get_nodekind() == NodeKind::ND_LT)
            out_ << "    setl %al\n";
        else if (node->get_nodekind() == NodeKind::ND_LE)
            out_ << "    setle %al\n";

        out_ << "    movzbq %al, %rax\n";
        break;
    default:
        diagnostic::error_tok(node->get_tok(), "invalid expression");
    }
}

void CodeGen::gen_stmt(Node* node) {
    switch (node->get_nodekind()) {
    case NodeKind::ND_IF: {
        int seq = gen_label_seq();
        gen_expr(node->get_condition());
        out_ << "    cmp $0, %rax\n";
        out_ << "    je .L.else" << seq << "\n";
        gen_stmt(node->get_then());
        out_ << "    jmp .L.end" << seq << "\n";
        out_ << ".L.else" << seq << ":\n";
        if (node->get_els()) {
            gen_stmt(node->get_els());
        }
        out_ << ".L.end" << seq << ":\n";
        return;
    }
    case NodeKind::ND_FOR: {
        int seq = gen_label_seq();
        if (node->get_init()) {
            gen_stmt(node->get_init());
        }
        out_ << ".L.begin" << seq << ":\n";
        if (node->get_condition()) {
            gen_expr(node->get_condition());
            out_ << "    cmp $0, %rax\n";
            out_ << "    je .L.end" << seq << "\n";
        }
        gen_stmt(node->get_then());
        if (node->get_inc()) {
            gen_expr(node->get_inc());
        }
        out_ << "    jmp .L.begin" << seq << "\n";
        out_ << ".L.end" << seq << ":\n";
        return;
    }
    case NodeKind::ND_EXPR_STMT:
        gen_expr(node->get_lhs());
        return;
    case NodeKind::ND_RETURN:
        gen_expr(node->get_lhs());
        out_ << "    jmp .L.return." << current_fn_->get_name() << "\n";
        return;
    case NodeKind::ND_BLOCK:
        for (Node* n = node->get_body(); n; n = n->get_nextstmt()) {
            gen_stmt(n);
        }
        return;
    default:
        diagnostic::error_tok(node->get_tok(), "invalid statement");
    }
    diagnostic::error_tok(node->get_tok(), "invalid statement");
}

void CodeGen::generate(Program* program_translation_unit) {

    emit_data(program_translation_unit);
    out_ << "    .text\n";
    assign_lvar_offsets(program_translation_unit->get_function_begin());

    for (Function* fn = program_translation_unit->get_function_begin(); fn; fn = fn->get_next()) {
        current_fn_ = fn;
        out_ << "    .globl " << fn->get_name() << "\n";
        out_ << fn->get_name() << ":\n";
        out_ << "    push %rbp\n";
        out_ << "    mov %rsp, %rbp\n";
        out_ << "    sub $" << fn->get_stack_size() << ", %rsp\n";
        store_function_params(fn);

        for (Node* n = fn->get_body(); n; n = n->get_nextstmt()) {
            gen_stmt(n);
            assert(depth == 0);
        }

        out_ << ".L.return." << fn->get_name() << ":\n";
        out_ << "    mov %rbp, %rsp\n";
        out_ << "    pop %rbp\n";
        out_ << "    ret\n";
    }
}
