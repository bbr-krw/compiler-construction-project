#include "bc_compiler.hpp"

#include <algorithm>
#include <bit>
#include <stdexcept>

namespace sm {

void BcCompiler::resolve_labels(std::vector<Bytecode>& code) {
    std::map<int32_t, int32_t> label_pos;
    int32_t pos = 0;
    for (const auto& bc : code) {
        if (sig(bc) == BC_LABEL)
            label_pos[imm32(bc)] = pos;
        else
            pos++;
    }
    std::vector<Bytecode> result;
    result.reserve(pos);
    for (const auto& bc : code) {
        const BytecodeSignatures s = sig(bc);
        if (s == BC_LABEL) {
            continue;
        } else if (s == BC_JMP || s == BC_CJMPZ) {
            result.push_back(bc_1op(s, static_cast<uint32_t>(label_pos.at(imm32(bc)))));
        } else {
            result.push_back(bc);
        }
    }
    code = std::move(result);
}

void BcCompiler::push_function(const std::vector<std::string>& args) {
    telescope.push_back(Function{args});
}

int BcCompiler::pop_function() {
    resolve_labels(code_buff);
    telescope.back().scheme.code = std::move(code_buff);
    bc_file.functions.push_back(telescope.back().scheme);
    telescope.pop_back();
    return bc_file.functions.size() - 1;
}

std::optional<Location> BcCompiler::capture(const std::string& name, size_t frame_index) {
    if (frame_index == 0) {
        return std::nullopt;
    }

    auto parent_location = telescope[frame_index - 1].resolve(name).or_else(
        [&] { return capture(name, frame_index - 1); });

    if (not parent_location.has_value()) {
        return std::nullopt;
    }

    return telescope[frame_index].addCaptured(name, parent_location.value());
}

void BcCompiler::compileIfThen(const ASTNode& pred, const ASTNode& then) {
    // <pred> CJMPZ L1 <then> L1:
    int L1 = new_label();
    pred.accept(*this);
    emit(bc_1op(BC_CJMPZ, L1));
    then.accept(*this);
    emit_label(L1);
}

sm::BcFile BcCompiler::compile(const ASTNode& root) {
    root.accept(*this);
    return bc_file;
}

std::optional<Location> BcCompiler::resolve(const std::string& name) {
    auto local_location = current_function().resolve(name);
    if (local_location.has_value()) {
        return local_location.value();
    }
    return capture(name, telescope.size() - 1);
}

void BcCompiler::visit(const ProgramNode& n) {
    push_function({}); // main is a function without args
    for (const auto& s : n.stmts)
        s->accept(*this);
    bc_file.main_function_index = pop_function();
}

void BcCompiler::visit(const VarDeclNode& n) {
    for (const auto& d : n.defs)
        d->accept(*this);
}

void BcCompiler::visit(const VarDefNode& n) {
    Location varLoc = current_function().addLocal(n.varname);
    if (n.init) {
        n.init->accept(*this);
        emit(bc_1op(BC_ST, packLock(varLoc)));
    }
}

void BcCompiler::visit(const PrintNode& n) {
    for (auto& expr : n.exprs | std::views::reverse) {
        expr->accept(*this);
    }
    emit(bc_1op(BC_PRINT, n.exprs.size()));
}

void BcCompiler::visit(const IdentNode& n) {
    auto var_handle = resolve(n.ident_name);
    if (var_handle) {
        emit(bc_1op(BC_LD, packLock(*var_handle)));
    } else {
        throw std::runtime_error("undeclared ident");
    }
}

void BcCompiler::visit(const IntLitNode& n) {
    emit(bc_1op(BC_CONST, (uint32_t)n.value));
}

void BcCompiler::visit(const BodyNode& b) {
    current_function().push_scope();
    for (const auto& stmt : b.stmts) {
        stmt->accept(*this);
    }
    current_function().pop_scope();
}

void BcCompiler::visit(const FuncLitNode& fn) {
    const auto pl = reinterpret_cast<const ParamListNode*>(fn.params.get());
    std::vector<std::string> args;
    for (size_t i = 0; i < pl->params.size(); i++) {
        const auto arg = reinterpret_cast<const IdentNode*>(pl->params[i].get());
        args.push_back(arg->ident_name);
    }

    std::vector<Bytecode> outer_buff = std::move(code_buff);
    code_buff                        = {};
    push_function(args);
    fn.body->accept(*this);
    int fn_idx = pop_function();
    code_buff  = std::move(outer_buff);
    emit(bc_1op(BC_CLOSURE, fn_idx));
}

void BcCompiler::visit(const CallNode& call) {
    for (const auto& arg : call.args) {
        arg->accept(*this);
    }
    call.callee->accept(*this);
    emit(bc_1op(BC_CALLC, call.args.size()));
}

void BcCompiler::visit(const ReturnNode& r) {
    r.value ? r.value->accept(*this) : emit(BC_NONE);
    emit(bc_0op(BC_RET));
}

void BcCompiler::visit(const RealLitNode& n) {
    emit(bc_1op(BC_REAL, std::bit_cast<uint32_t>(static_cast<float>(n.value))));
}

void BcCompiler::visit(const StrLitNode& n) {
    emit(bc_1op(BC_STRING, bc_file.addString(n.value)));
}

void BcCompiler::visit(const BinOpNode& b) {
    static int32_t binop_reencode[] = {
        12, // OR
        11, // AND
        13, // XOR
        5,  // LT
        6,  // LE
        7,  // GT
        8,  // GE
        9,  // EQ
        10, // NEQ
        0,  // ADD
        1,  // SUB
        2,  // MUL
        3,  // DIV
    };
    b.left->accept(*this);
    b.right->accept(*this);
    emit(bc_1op(BC_BINOP, binop_reencode[static_cast<size_t>(b.op)]));
}

void BcCompiler::visit(const BoolLitNode& n) {
    emit(bc_1op(BC_BOOL, n.value));
}

void BcCompiler::visit(const IsNode& n) {
    static uint32_t type_reencode[] = {
        1, // INT
        2, // REAL
        3, // BOOL
        4, // STRING
        0, // NONE
        5, // ARRAY
        6, // TUPLE
        7, // FUNC
    };
    n.operand->accept(*this);
    TypeNode::Type ast_expected_type = static_cast<const TypeNode&>(*n.type_node).type;
    emit(bc_1op(BC_ISTYPE, type_reencode[static_cast<uint32_t>(ast_expected_type)]));
}

void BcCompiler::visit(const AssignNode& n) {
    n.lhs->accept(*this);
    n.rhs->accept(*this);
    emit(bc_0op(BC_STD));
}

void BcCompiler::visit(const ExprStmtNode& n) {
    n.expr->accept(*this);
    emit(bc_0op(BC_DROP)); // drop value loaded by last expr
}


void BcCompiler::visit(const NoneLitNode&) {
    emit(bc_0op(BC_NONE));
}

void BcCompiler::visit(const ArrayLitNode& ar) {
    emit(bc_0op(BC_ARRAY));
    for (size_t i = 0; i < ar.elems.size(); i++) {
        emit(bc_0op(BC_DUP));
        emit(bc_1op(BC_CONST, 1 + i));
        ar.elems[i]->accept(*this);
        emit(bc_0op(BC_STA));
    }
}

void BcCompiler::visit(const TupleLitNode& t) {
    TupleScheme scheme;
    for (size_t i = 0; i < t.elems.size(); i++) {
        const auto elem = reinterpret_cast<const TupleElemNode*>(t.elems[i].get());
        if (elem->elem_name.empty()) {
            scheme.field_names.push_back(std::nullopt);
        } else {
            scheme.field_names.push_back(elem->elem_name);
        }
    }

    const size_t scheme_index = bc_file.tuples.size();
    bc_file.tuples.push_back(scheme);

    for (size_t i = 0; i < t.elems.size(); i++) {
        const auto elem = reinterpret_cast<const TupleElemNode*>(t.elems[i].get());
        elem->expr->accept(*this);
    }

    emit(bc_1op(BC_TUPLE, scheme_index));
}

void BcCompiler::visit(const TupleElemNode&) {
    throw std::runtime_error("Unreachable");
}

void BcCompiler::visit(const IndexNode& i) {
    i.base->accept(*this);
    i.index_expr->accept(*this);
    emit(bc_0op(BC_LDA));
}

void BcCompiler::visit(const DotFieldNode& f) {
    size_t index = std::find(bc_file.strings.begin(), bc_file.strings.end(), f.field) -
                   bc_file.strings.begin();
    if (index == bc_file.strings.size()) {
        bc_file.strings.push_back(f.field);
    }
    f.base->accept(*this);
    emit(bc_1op(BC_LDT, index));
}

void BcCompiler::visit(const DotIntNode& i) {
    int32_t index = -i.index;
    i.base->accept(*this);
    emit(bc_1op(BC_LDT, index));
}

void BcCompiler::visit(const IfNode& n) {
    if (n.else_body) {
        // <cond> CJMPZ L1 <then> JMP L2 L1: <else> L2:
        int L1 = new_label(), L2 = new_label();
        n.cond->accept(*this);
        emit(bc_1op(BC_CJMPZ, L1));
        n.then_body->accept(*this);
        emit(bc_1op(BC_JMP, L2));
        emit_label(L1);
        n.else_body->accept(*this);
        emit_label(L2);
    } else {
        compileIfThen(*n.cond, *n.then_body);
    }
}

void BcCompiler::visit(const IfShortNode& n) {
    compileIfThen(*n.cond, *n.stmt);
}

void BcCompiler::visit(const WhileNode& n) {
    // L_ENTRY: <cond> CJMPZ L_EXIT <body> JMP L_ENTRY L_EXIT:
    int L_entry = new_label(), L_exit = new_label();
    emit_label(L_entry);
    n.cond->accept(*this);
    emit(bc_1op(BC_CJMPZ, L_exit));
    n.body->accept(*this);
    emit(bc_1op(BC_JMP, L_entry));
    emit_label(L_exit);
}

void BcCompiler::visit(const ForRangeNode& n) {
    // <from> ST_FR <to>
    // L1: <body> DUP LD_FR BINOP!= CJMPZ_L2 DUP LD_FR RNGSPC ST_FR JMP_L1
    // L2: DROP
    int L1 = new_label(), L2 = new_label();

    n.from->accept(*this);
    Location fr_local = current_function().push_scope({n.iter})[0];
    emit(bc_1op(BC_ST, packLock(fr_local)));
    n.to->accept(*this);

    emit_label(L1);
    n.body->accept(*this);
    emit(bc_0op(BC_DUP));
    emit(bc_1op(BC_LD, packLock(fr_local)));
    emit(bc_1op(BC_BINOP, 10)); // !=
    emit(bc_1op(BC_CJMPZ, L2));
    emit(bc_0op(BC_DUP));
    emit(bc_1op(BC_LD, packLock(fr_local)));
    emit(bc_0op(BC_RNGSPC));
    emit(bc_1op(BC_ST, packLock(fr_local)));
    emit(bc_1op(BC_JMP, L1));

    current_function().pop_scope();
    emit_label(L2);
    emit(bc_0op(BC_DROP));
}

void BcCompiler::visit(const ForIterNode& n) {
    // TODO make it work for tuples. (now we don't have suitable bytecode for it)
    // <iterable> CONST 0 ST_ITER
    // L_ENTRY: DUP LENGTH LD_ITER BINOP!= CJMPZ_L_EXIT
    //   DUP LD_ITER CONST 1 BINOP+ ST_ITER LD_ITER LDA ST_VAR <body>
    // JMP L_ENTRY
    // L_EXIT: DROP
    int L_entry = new_label(), L_exit = new_label();

    n.iterable->accept(*this);
    Location iter_local, var_local;
    {
        auto locals = current_function().push_scope({n.iter, ""});
        var_local   = locals[0];
        iter_local  = locals[1];
    }
    emit(bc_1op(BC_CONST, 0));
    emit(bc_1op(BC_ST, packLock(iter_local)));

    emit_label(L_entry);
    emit(bc_0op(BC_DUP));
    emit(bc_0op(BC_LENGTH));
    emit(bc_1op(BC_LD, packLock(iter_local)));
    emit(bc_1op(BC_BINOP, 10)); // !=
    emit(bc_1op(BC_CJMPZ, L_exit));
    emit(bc_0op(BC_DUP));
    emit(bc_1op(BC_LD, packLock(iter_local)));
    emit(bc_1op(BC_CONST, 1));
    emit(bc_1op(BC_BINOP, 0)); // +
    emit(bc_1op(BC_ST, packLock(iter_local)));
    emit(bc_1op(BC_LD, packLock(iter_local)));
    emit(bc_0op(BC_LDA));
    emit(bc_1op(BC_ST, packLock(var_local)));
    n.body->accept(*this);
    current_function().pop_scope();

    emit(bc_1op(BC_JMP, L_entry));
    emit_label(L_exit);
    emit(bc_0op(BC_DROP));
}

void BcCompiler::visit(const LoopInfNode& n) {
    // L_ENTRY: <body> JMP L_ENTRY  L_EXIT: (exit jumps here)
    int L_entry = new_label(), L_exit = new_label();
    loop_exit_labels.push_back(L_exit);
    emit_label(L_entry);
    n.body->accept(*this);
    loop_exit_labels.pop_back();
    emit(bc_1op(BC_JMP, L_entry));
    emit_label(L_exit);
}

void BcCompiler::visit(const ExitNode&) {
    emit(bc_1op(BC_JMP, loop_exit_labels.back()));
}

void BcCompiler::visit(const UnaryOpNode& n) {
    switch (n.op) {
    case UnaryOpNode::Op::UPLUS:
        n.operand->accept(*this);
        break;
    case UnaryOpNode::Op::UMINUS:
        emit(bc_1op(BC_CONST, 0));
        n.operand->accept(*this);
        emit(bc_1op(BC_BINOP, 1)); // -
        break;
    case UnaryOpNode::Op::NOT:
        emit(bc_1op(BC_BOOL, 1));
        n.operand->accept(*this);
        emit(bc_1op(BC_BINOP, 13)); // ^
        break;
    }
}

} // namespace sm
