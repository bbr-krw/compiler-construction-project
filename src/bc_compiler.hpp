#pragma once

#include "runtime.hpp"
#include "bytecode.hpp"

class BcCompiler : public ASTVisitorBase<BcCompiler> {

public:
    explicit BcCompiler();

    BcFile compile(const ASTNode& root) {
        root.accept(*this);
        return bc_file;
    }

    void visit(const ProgramNode& n) override {
        for (const auto& s : n.stmts)
            s->accept(*this);
    }

    void visit(const BodyNode&) override {
        throw std::runtime_error("unimplemented");
    }

    void visit(const VarDeclNode& n) override {
        for (const auto& d : n.defs)
            d->accept(*this);
    }

    void visit(const VarDefNode& n) override {
        // const bool is_func_init = n.init && dynamic_cast<const FuncLitNode*>(n.init.get()) != nullptr;
        // if (is_func_init)
        //     declare(n.varname); // initially none
        // DValue v = n.init ? eval(*n.init) : DValue{};
        // if (is_func_init)
        //     lookup_ref(n.varname) = std::move(v);
        // else
        //     declare(n.varname, std::move(v));
    }

    void visit(const PrintNode& n) override {
        // TODO
    }

    void visit(const AssignNode&) override;
    void visit(const IfNode&) override;
    void visit(const IfShortNode&) override;
    void visit(const WhileNode&) override;
    void visit(const ForRangeNode&) override;
    void visit(const ForIterNode&) override;
    void visit(const LoopInfNode&) override;
    void visit(const ExitNode&) override;
    void visit(const ReturnNode&) override;
    void visit(const BinOpNode&) override;
    void visit(const UnaryOpNode&) override;
    void visit(const IsNode&) override;
    void visit(const IdentNode&) override;
    void visit(const IndexNode&) override;
    void visit(const CallNode&) override;
    void visit(const DotFieldNode&) override;
    void visit(const DotIntNode&) override;
    void visit(const IntLitNode&) override;
    void visit(const RealLitNode&) override;
    void visit(const StrLitNode&) override;
    void visit(const BoolLitNode&) override;
    void visit(const NoneLitNode&) override;
    void visit(const ArrayLitNode&) override;
    void visit(const TupleLitNode&) override;
    void visit(const TupleElemNode&) override;
    void visit(const FuncLitNode&) override;
    void visit(const TypeNode&) override;

private:
    BcFile bc_file;

};
