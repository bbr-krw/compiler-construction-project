#pragma once

#include "ast.hpp"
#include "runtime.hpp"
#include "bytecode.hpp"

#include <memory>
#include <stdexcept>
#include <vector>
#include <map>

namespace sm {

class BcCompiler : public ASTVisitorBase<BcCompiler> {

    struct Function {
        explicit Function(const std::vector<std::string> &args) {
            scheme.args_number = 0;
            scheme.locals_number = 0;
            for (auto &arg : args) {
                addArg(arg);
            }
        }
        sm::FunctionScheme scheme;
        std::map<std::string, Location> var_scope;
        std::optional<Location> resolve(std::string name) {
            if (var_scope.contains(name)) {
                return var_scope[name];
            } else {
                return std::nullopt;
            }
        }
        Location addLocal(std::string name) {
            return var_scope[name] = Location{ LocTypes::LOCAL, scheme.locals_number++ };
        }
        Location addArg(std::string name) {
            return var_scope[name] = Location{ LocTypes::ARGUMENT, scheme.args_number++ };
        }
    };

private:
    sm::BcFile bc_file;
    std::vector<Function> telescope;

    inline Function& curFun() {
        return telescope.back();
    }

    inline void emit(const Bytecode& bc) {
        curFun().scheme.code.push_back(bc);
    }

    inline void telescope_push(const std::vector<std::string>& args) {
        telescope.push_back(Function{args});
    }

    inline int telescope_pop() {
        bc_file.functions.push_back(telescope.back().scheme);
        telescope.pop_back();
        return bc_file.functions.size() - 1;
    }

    std::optional<Location> capture(const std::string& name, size_t frame_index) {
        if (frame_index == 0) {
            return std::nullopt;
        }

        auto parent_location = telescope[frame_index - 1].resolve(name).or_else([&] {
            return capture(name, frame_index - 1);
        });
        
        if (not parent_location.has_value()) {
            return std::nullopt;
        }
        
        auto& captured = telescope[frame_index].scheme.capture;

        auto location = telescope[frame_index].var_scope[name] = Location{
            .type = LocTypes::CAPTURED, .index = static_cast<uint16_t>(captured.size())
        };
        telescope[frame_index].scheme.capture.push_back(parent_location.value());

        return location;
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
        telescope_push({}); // main is a function without args

        for (const auto& s : n.stmts)
            s->accept(*this);

        bc_file.main_function_index = telescope_pop();
    }

    void visit(const VarDeclNode& n) override {
        for (const auto& d : n.defs)
            d->accept(*this);
    }

    void visit(const VarDefNode& n) override {
        // TODO: handle closure creation
        Location varLoc = curFun().addLocal(n.varname);
        if (n.init) {
            n.init->accept(*this); // loads init value onto stack
            emit(bc_1op(BC_ST, packLock(varLoc)));
        }
    }

    void visit(const PrintNode& n) override {
        for (auto &expr : n.exprs) {
            expr->accept(*this);
            emit(bc_0op(BC_PRINT));
        }
    }

    void visit(const IdentNode& n) override {
        auto var_handle = resolve(n.ident_name);
        if (var_handle) {
            emit(bc_1op(BC_LD, packLock(*var_handle)));
        } else {
            throw std::runtime_error("undeclared ident");
        }
    }

    void visit(const IntLitNode& n) override {
        emit(bc_1op(BC_CONST, (uint32_t) n.value));
    }

    void visit(const BodyNode& b) override {
        for (const auto& stmt : b.stmts) {
            stmt->accept(*this);
        }
    }

    void visit(const FuncLitNode& fn) override {
        const auto pl = reinterpret_cast<const ParamListNode*>(fn.params.get());
        std::vector<std::string> args;
        for (size_t i = 0; i < pl->params.size(); i++) {
            const auto arg = reinterpret_cast<const IdentNode*>(pl->params[i].get());
            args.push_back(arg->ident_name);
        }

        telescope.push_back(Function(args));

        fn.body->accept(*this);

        bc_file.functions.push_back(telescope.back().scheme);
        telescope.pop_back();

        emit(bc_1op(BC_CLOSURE, bc_file.functions.size() - 1));
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
            12,     // OR
            11,     // AND
            13,     // XOR
            5,      // LT
            6,      // LE
            7,      // GT
            8,      // GE
            9,      // EQ
            10,     // NEQ
            0,      // ADD
            1,     // SUB
            2,     // MUL
            3,     // DIV
        };

        b.left->accept(*this);
        b.right->accept(*this);
        emit(bc_1op(BC_BINOP, binop_reencode[static_cast<size_t>(b.op)]));
    }

    void visit(const BoolLitNode& n) override {
        emit(bc_1op(BC_BOOL, n.value));
    }

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
        // if (n.init) {
        //     n.init->accept(*this); // loads init value onto stack
        //     emit(bc_1op(BC_ST, packLock(varLoc)));
        // }

    }
    // void visit(const IfNode&) override;
    // void visit(const IfShortNode&) override;
    // void visit(const WhileNode&) override;
    // void visit(const ForRangeNode&) override;
    // void visit(const ForIterNode&) override;
    // void visit(const LoopInfNode&) override;
    // void visit(const ExitNode&) override;
    // void visit(const UnaryOpNode&) override;
    // void visit(const IndexNode&) override;
    // void visit(const DotFieldNode&) override;
    // void visit(const DotIntNode&) override;
    // void visit(const NoneLitNode&) override;
    // void visit(const ArrayLitNode&) override;
    // void visit(const TupleLitNode&) override;
    // void visit(const TupleElemNode&) override;
    // void visit(const TypeNode&) override;
};

} // namespace sm
