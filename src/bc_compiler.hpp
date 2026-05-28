#pragma once

#include "ast.hpp"
#include "bytecode.hpp"

#include <map>
#include <optional>
#include <ranges>
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
    int                   label_counter = 0;
    std::vector<int>      loop_exit_labels;

    Function& current_function() { return telescope.back(); }
    void      emit(const Bytecode& bc) { code_buff.push_back(bc); }
    int       new_label() { return label_counter++; }
    void      emit_label(int id) { emit(bc_1op(BC_LABEL, static_cast<uint32_t>(id))); }

    static void             resolve_labels(std::vector<Bytecode>& code);
    void                    push_function(const std::vector<std::string>& args);
    int                     pop_function();
    std::optional<Location> capture(const std::string& name, size_t frame_index);
    void                    compileIfThen(const ASTNode& pred, const ASTNode& then);

public:
    explicit BcCompiler() = default;

    sm::BcFile              compile(const ASTNode& root);
    std::optional<Location> resolve(const std::string& name);

    void visit(const ProgramNode&) override;
    void visit(const VarDeclNode&) override;
    void visit(const VarDefNode&) override;
    void visit(const PrintNode&) override;
    void visit(const IdentNode&) override;
    void visit(const IntLitNode&) override;
    void visit(const BodyNode&) override;
    void visit(const FuncLitNode&) override;
    void visit(const CallNode&) override;
    void visit(const ReturnNode&) override;
    void visit(const RealLitNode&) override;
    void visit(const StrLitNode&) override;
    void visit(const BinOpNode&) override;
    void visit(const BoolLitNode&) override;
    void visit(const IsNode&) override;
    void visit(const AssignNode&) override;
    void visit(const NoneLitNode&) override;
    void visit(const ArrayLitNode&) override;
    void visit(const TupleLitNode&) override;
    void visit(const TupleElemNode&) override;
    void visit(const IndexNode&) override;
    void visit(const DotFieldNode&) override;
    void visit(const DotIntNode&) override;
    void visit(const IfNode&) override;
    void visit(const IfShortNode&) override;
    void visit(const WhileNode&) override;
    void visit(const ForRangeNode&) override;
    void visit(const ForIterNode&) override;
    void visit(const LoopInfNode&) override;
    void visit(const ExitNode&) override;
    void visit(const UnaryOpNode&) override;
};

} // namespace sm
