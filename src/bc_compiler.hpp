#pragma once

#include "ast.hpp"
#include "runtime.hpp"
#include "bytecode.hpp"

#include <stdexcept>
#include <vector>
#include <map>

namespace sm {

class BcCompiler : public ASTVisitorBase<BcCompiler> {

    struct Function {
        explicit Function(const std::vector<std::string> &args) {
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

    void visit(const BodyNode&) override {
        throw std::runtime_error("unimplemented");
    }

    // void visit(const AssignNode&) override;
    // void visit(const IfNode&) override;
    // void visit(const IfShortNode&) override;
    // void visit(const WhileNode&) override;
    // void visit(const ForRangeNode&) override;
    // void visit(const ForIterNode&) override;
    // void visit(const LoopInfNode&) override;
    // void visit(const ExitNode&) override;
    // void visit(const ReturnNode&) override;
    // void visit(const BinOpNode&) override;
    // void visit(const UnaryOpNode&) override;
    // void visit(const IsNode&) override;
    // void visit(const IndexNode&) override;
    // void visit(const CallNode&) override;
    // void visit(const DotFieldNode&) override;
    // void visit(const DotIntNode&) override;
    // void visit(const RealLitNode&) override;
    // void visit(const StrLitNode&) override;
    // void visit(const BoolLitNode&) override;
    // void visit(const NoneLitNode&) override;
    // void visit(const ArrayLitNode&) override;
    // void visit(const TupleLitNode&) override;
    // void visit(const TupleElemNode&) override;
    // void visit(const FuncLitNode&) override;
    // void visit(const TypeNode&) override;
};

} // namespace sm
