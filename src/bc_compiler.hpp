#pragma once

#include "ast.hpp"
#include "bytecode.hpp"
#include "runtime.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <vector>

namespace sm {

class BcCompiler : public ASTVisitorBase<BcCompiler> {

    class Function {
    public:
        explicit Function(const std::vector<std::string>& args) {
            scheme.args_number   = 0;
            scheme.locals_number = 0;
            push_scope();
            for (auto& arg : args) {
                var_scopes.back()[arg] = Location{LocTypes::ARGUMENT, scheme.args_number++};
            }
        }
        sm::FunctionScheme scheme;

    private:
        std::vector<std::map<std::string, Location>> var_scopes;

    public:
        std::vector<Location> push_scope(std::vector<std::string> predefined_vars = {}) {
            var_scopes.push_back({});
            std::vector<Location> res;
            for (auto& var : predefined_vars) {
                auto loc               = Location{LocTypes::LOCAL, scheme.locals_number++};
                var_scopes.back()[var] = loc;
                res.push_back(loc);
            }
            return res;
        }
        void pop_scope() { var_scopes.pop_back(); }
        std::optional<Location> resolve(std::string name) {
            for (auto& scope : var_scopes | std::views::reverse) {
                if (scope.contains(name)) {
                    return scope[name];
                }
            }
            return std::nullopt;
        }
        Location addLocal(std::string name) {
            return var_scopes.back()[name] = Location{LocTypes::LOCAL, scheme.locals_number++};
        }
        Location addCaptured(std::string name, Location captured_loc) {
            auto location = var_scopes[0][name] = Location{
                .type = LocTypes::CAPTURED, .index = static_cast<uint16_t>(scheme.capture.size())};
            scheme.capture.push_back(captured_loc);
            return location;
        }
    };

private:
    sm::BcFile bc_file;
    std::vector<Function> telescope;
    std::vector<Bytecode> code_buff;

    inline Function& curFun() { return telescope.back(); }

    inline void emit(const Bytecode& bc) { code_buff.push_back(bc); }

    inline void emit(const std::vector<Bytecode>& bcs) {
        for (auto& bc : bcs) {
            emit(bc);
        }
    }

    inline void func_push(const std::vector<std::string>& args) {
        telescope.push_back(Function{args});
    }

    inline int func_pop() {
        bc_file.functions.push_back(telescope.back().scheme);
        telescope.pop_back();
        return bc_file.functions.size() - 1;
    }

    std::optional<Location> capture(const std::string& name, size_t frame_index) {
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

public:
    explicit BcCompiler() {};

    sm::BcFile compile(const ASTNode& root) {
        root.accept(*this);
        return bc_file;
    }

    std::optional<Location> resolve(const std::string& name) {
        auto local_location = curFun().resolve(name);
        if (local_location.has_value()) {
            return local_location.value();
        }

        return capture(name, telescope.size() - 1);
    }

    void visit(const ProgramNode& n) override {
        func_push({}); // main is a function without args

        for (const auto& s : n.stmts)
            s->accept(*this);

        telescope.back().scheme.code = std::move(code_buff);
        bc_file.main_function_index = func_pop();
    }

    void visit(const VarDeclNode& n) override {
        for (const auto& d : n.defs)
            d->accept(*this);
    }

    void visit(const VarDefNode& n) override {
        Location varLoc = curFun().addLocal(n.varname);
        if (n.init) {
            n.init->accept(*this); // loads init value onto stack
            emit(bc_1op(BC_ST, packLock(varLoc)));
        }
    }

    void visit(const PrintNode& n) override {
        for (auto& expr : n.exprs | std::views::reverse) {
            expr->accept(*this);
        }
        emit(bc_1op(BC_PRINT, n.exprs.size()));
    }

    void visit(const IdentNode& n) override {
        auto var_handle = resolve(n.ident_name);
        if (var_handle) {
            emit(bc_1op(BC_LD, packLock(*var_handle)));
        } else {
            throw std::runtime_error("undeclared ident");
        }
    }

    void visit(const IntLitNode& n) override { emit(bc_1op(BC_CONST, (uint32_t)n.value)); }

    void visit(const BodyNode& b) override {
        curFun().push_scope();
        for (const auto& stmt : b.stmts) {
            stmt->accept(*this);
        }
        curFun().pop_scope();
    }

    void visit(const FuncLitNode& fn) override {
        const auto pl = reinterpret_cast<const ParamListNode*>(fn.params.get());
        std::vector<std::string> args;
        for (size_t i = 0; i < pl->params.size(); i++) {
            const auto arg = reinterpret_cast<const IdentNode*>(pl->params[i].get());
            args.push_back(arg->ident_name);
        }

        func_push(args);
        telescope.back().scheme.code = compileIntoCodeBuff(*fn.body);
        emit(bc_1op(BC_CLOSURE, func_pop()));
    }

    void visit(const CallNode& call) override {
        for (const auto& arg : call.args) {
            arg->accept(*this);
        }

        call.callee->accept(*this);

        emit(bc_1op(BC_CALLC, call.args.size()));
    }

    void visit(const ReturnNode& r) override {
        r.value->accept(*this);
        emit(bc_0op(BC_RET));
    }

    void visit(const RealLitNode& n) override {
        float val = n.value;
        emit(bc_1op(BC_REAL, *reinterpret_cast<uint32_t*>(&val)));
    }

    void visit(const StrLitNode& n) override {
        emit(bc_1op(BC_STRING, bc_file.addString(n.value)));
    }

    void visit(const BinOpNode& b) override {
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

    void visit(const BoolLitNode& n) override { emit(bc_1op(BC_BOOL, n.value)); }

    void visit(const IsNode& n) override {
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

    void visit(const AssignNode& n) override {
        n.lhs->accept(*this);
        n.rhs->accept(*this);
        emit(bc_0op(BC_STD));
    }

    void visit(const NoneLitNode&) override { emit(bc_0op(BC_NONE)); }

    void visit(const ArrayLitNode& ar) override {
        emit(bc_0op(BC_ARRAY));
        for (size_t i = 0; i < ar.elems.size(); i++) {
            emit(bc_0op(BC_DUP));
            emit(bc_1op(BC_CONST, 1 + i));
            ar.elems[i]->accept(*this);
            emit(bc_0op(BC_STA));
        }
    }

    void visit(const TupleLitNode& t) override {
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

    void visit(const TupleElemNode&) override { throw std::runtime_error("Unreachable"); }

    void visit(const IndexNode& i) override {
        i.base->accept(*this);
        i.index_expr->accept(*this);

        emit(bc_0op(BC_LDA));
    }

    void visit(const DotFieldNode& f) override {
        size_t index = std::find(bc_file.strings.begin(), bc_file.strings.end(), f.field) -
                       bc_file.strings.begin();
        if (index == bc_file.strings.size()) {
            bc_file.strings.push_back(f.field);
        }

        f.base->accept(*this);
        emit(bc_1op(BC_LDT, index));
    }

    void visit(const DotIntNode& i) override {
        int32_t index = -i.index;
        i.base->accept(*this);
        emit(bc_1op(BC_LDT, index));
    }

    std::vector<Bytecode> compileIntoCodeBuff(const ASTNode& n) {
        std::vector<Bytecode> old_code_buff = std::move(code_buff);
        std::vector<Bytecode> node_code;
        code_buff = std::move(node_code);
        n.accept(*this);
        node_code = std::move(code_buff);
        code_buff = std::move(old_code_buff);
        return node_code;
    }

    void visit(const IfNode& n) override {
        if (n.else_body) {
            // if-then-else layout
            // <load pred>
            // CJMPZ L1
            //      <then_code>
            // JMP L2
            // L1:
            //      <else_code>
            // L2:

            std::vector<Bytecode> then_code = compileIntoCodeBuff(*n.then_body);
            std::vector<Bytecode> else_code = compileIntoCodeBuff(*n.else_body);

            n.cond->accept(*this);                   // loads boolean pred onto stack
            int l1_bcindex = code_buff.size() + 1    // CJMPZ
                             + then_code.size() + 1; // JMP
            int l2_bcindex = l1_bcindex + else_code.size();
            emit(bc_1op(BC_CJMPZ, l1_bcindex));
            emit(then_code);
            emit(bc_1op(BC_JMP, l2_bcindex));
            emit(else_code);
        } else {
            compileIfThen(*n.cond, *n.then_body);
        }
    }

    void visit(const IfShortNode& n) override { compileIfThen(*n.cond, *n.stmt); }

    void compileIfThen(const ASTNode& pred, const ASTNode& then) {
        // if-then-layout
        // <load pred>
        // CJMPZ L1
        //      <then_code>
        // L1:

        std::vector<Bytecode> then_code = compileIntoCodeBuff(then);
        pred.accept(*this);                   // loads boolean pred onto stack
        int l1_bcindex = code_buff.size() + 1 // CJMPZ
                         + then_code.size();
        emit(bc_1op(BC_CJMPZ, l1_bcindex));
        emit(then_code);
    }

    void visit(const WhileNode& n) override {
        // simple loop layout
        // L_ENTRY:
        // <pred_code>
        // CJMPZ L_EXIT
        //      <body_code>
        // JMP L_ENTRY
        // L_EXIT:

        std::vector<Bytecode> body_code = compileIntoCodeBuff(*n.body);
        std::vector<Bytecode> pred_code = compileIntoCodeBuff(*n.cond);

        int l_entry_bcindex = code_buff.size();
        int l_exit_bcindex  = l_entry_bcindex + pred_code.size() + 1 // CJMPZ L_EXIT
                             + body_code.size() + 1;                 // JMP L_ENTRY;
        emit(pred_code);
        emit(bc_1op(BC_CJMPZ, l_exit_bcindex));
        emit(body_code);
        emit(bc_1op(BC_JMP, l_entry_bcindex));
    }

    void visit(const ForRangeNode& n) override {
        // for from..to loop layout
        // <from_code>
        // ST FR_LOCAL
        // <to_code>    // stack: to
        // L1:
        // <body_code>  // stack: to
        // DUP          // stack: to, to
        // LD FR_LOCAL  // stack: to, to, fr
        // BINOP !=     // stack: to, pred
        // CJMPZ L2     // stack: to
        // DUP          // stack: to, to
        // LD FR_LOCAL  // stack: to, to, fr
        // RNG_SPEC     // stack: to, fr*
        // ST FR_LOCAL  // stack: to
        // JMP L1
        // L2:
        // DROP

        std::vector<Bytecode> from_code = compileIntoCodeBuff(*n.from);
        std::vector<Bytecode> to_code   = compileIntoCodeBuff(*n.to);

        Location fr_local               = curFun().push_scope({n.iter})[0];
        std::vector<Bytecode> body_code = compileIntoCodeBuff(*n.body);
        curFun().pop_scope();

        int l1_bcindex = code_buff.size() + from_code.size() + 1 // ST FR_LOCAL
                         + to_code.size();
        int l2_bcindex = l1_bcindex + body_code.size() + 1 // DUP          stack: to, to
                         + 1                               // LD FR_LOCAL  stack: to, to, fr
                         + 1                               // BINOP !=     stack: to, pred
                         + 1                               // CJMPZ L2     stack: to
                         + 1                               // DUP          stack: to, to
                         + 1                               // LD FR_LOCAL  stack: to, to, fr
                         + 1                               // RNGSPC       stack: to, fr*
                         + 1                               // ST FR_LOCAL  stack: to
                         + 1;                              // JMP L1

        emit(from_code);
        emit(bc_1op(BC_ST, packLock(fr_local)));
        emit(to_code);
        // l1:
        emit(body_code);
        emit(bc_0op(BC_DUP));
        emit(bc_1op(BC_LD, packLock(fr_local)));
        emit(bc_1op(BC_BINOP, 10)); // !=
        emit(bc_1op(BC_CJMPZ, l2_bcindex));
        // l2:
        emit(bc_0op(BC_DUP));
        emit(bc_1op(BC_LD, packLock(fr_local)));
        emit(bc_0op(BC_RNGSPC));
        emit(bc_1op(BC_ST, packLock(fr_local)));
        emit(bc_1op(BC_JMP, l1_bcindex));
        emit(bc_0op(BC_DROP));
    }

    void visit(const ForIterNode& n) override {
        // TODO make it work for tuples. (now we don't have suitable bytecode for it)
        // for var in iterable loop layout
        // <iter_code>
        // CONST 0
        // ST ITER_LOCAL      stack: iterable
        // L_ENTRY:
        // DUP                stack: iterable, iterable
        // LENGTH             stack: iterable, length
        // LD ITER_LOCAL      stack: iterable, length, iter
        // BINOP !=           stack: iterable, exit_pred
        // CJMPZ L_EXIT
        // DUP                stack: iterable, iterable
        // LD ITER_LOCAL      stack: iterable, iterable, iter
        // LDA                stack: iterable, elem
        // ST VAR_LOCAL       stack: iterable
        // <body_code>
        // LD ITER_LOCAL
        // CONST 0
        // BINOP +
        // ST ITER_LOCAL
        // JMP L_ENTRY
        // L_EXIT:
        // DROP

        std::vector<Bytecode> iter_code = compileIntoCodeBuff(*n.iterable);

        Location iter_local, var_local;
        {
            // FIXME ugly local alloc
            auto locals = curFun().push_scope({n.iter, ""});
            iter_local  = locals[0];
            var_local   = locals[1];
        }
        std::vector<Bytecode> body_code = compileIntoCodeBuff(*n.body);
        curFun().pop_scope();

        int lentry_bcindex = code_buff.size() + iter_code.size() + 2;
        int lexit_bcindex  = lentry_bcindex + body_code.size() + 14;

        emit(iter_code);
        emit(bc_1op(BC_CONST, 0));
        emit(bc_1op(BC_ST, packLock(iter_local)));
        // lentry:
        emit(bc_0op(BC_DUP));
        emit(bc_0op(BC_LENGTH));
        emit(bc_1op(BC_LD, packLock(iter_local)));
        emit(bc_1op(BC_BINOP, 10)); // !=
        emit(bc_1op(BC_CJMPZ, lexit_bcindex));
        emit(bc_0op(BC_DUP));
        emit(bc_1op(BC_LD, packLock(iter_local)));
        emit(bc_0op(BC_LDA));
        emit(bc_1op(BC_ST, packLock(var_local)));
        emit(body_code);
        emit(bc_1op(BC_LD, packLock(iter_local)));
        emit(bc_1op(BC_CONST, 1));
        emit(bc_1op(BC_BINOP, 0)); // +
        emit(bc_1op(BC_ST, packLock(iter_local)));
        emit(bc_1op(BC_JMP, lentry_bcindex));
        // lexit:
        emit(bc_0op(BC_DROP));
    }

    void visit(const LoopInfNode&) override {
        throw std::runtime_error("unimplemented loop inf");
    }
    void visit(const ExitNode&) override {
        throw std::runtime_error("unimplemented loop exit");
    }

    void visit(const UnaryOpNode& n) override {
        switch (n.op) {
        case UnaryOpNode::Op::UPLUS: {
            n.operand->accept(*this);
            break;
        }
        case UnaryOpNode::Op::UMINUS: {
            emit(bc_1op(BC_CONST, 0));
            n.operand->accept(*this);
            emit(bc_1op(BC_BINOP, 1)); // -;
            break;
        }
        case UnaryOpNode::Op::NOT: {
            emit(bc_1op(BC_BOOL, 1));
            n.operand->accept(*this);
            emit(bc_1op(BC_BINOP, 13)); // ^;
            break;
        }
        }
    }
};

} // namespace sm
