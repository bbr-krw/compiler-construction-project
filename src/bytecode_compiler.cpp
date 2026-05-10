#include "bytecode_compiler.hpp"

#include "ast.hpp"
#include "ast_visitor.hpp"

#include <cassert>
#include <format>
#include <optional>
#include <stdexcept>

// ══════════════════════════════════════════════════════════════════════════════
// Pass 1: Capture Analysis
//
// Mirrors the semantic analyzer's scope-tracking, but additionally notes the
// function depth of each declaration.  When an IdentNode is visited whose
// declaration lives in an outer function, the owning function is marked as
// having that variable captured.
// ══════════════════════════════════════════════════════════════════════════════

struct CaptureAnalyzer : ASTVisitorBase<CaptureAnalyzer> {
    CaptureMap result;

    void run(const ASTNode& root) { root.accept(*this); }

    // ── Visitors mirror SemanticAnalyzer scope-push pattern ──────────────────

    void visit(const ProgramNode& n) override {
        push_scope();
        for (const auto& s : n.stmts)
            s->accept(*this);
        pop_scope();
    }

    void visit(const BodyNode& n) override {
        push_scope();
        for (const auto& s : n.stmts)
            s->accept(*this);
        pop_scope();
    }

    void visit(const VarDeclNode& n) override {
        for (const auto& d : n.defs)
            d->accept(*this);
    }

    void visit(const VarDefNode& n) override {
        // Mirror SemanticAnalyzer: func-literal inits are pre-declared
        const bool is_func = n.init && dynamic_cast<const FuncLitNode*>(n.init.get());
        if (is_func)
            declare(n.varname);
        if (n.init)
            n.init->accept(*this);
        if (!is_func)
            declare(n.varname);
    }

    void visit(const AssignNode& n) override {
        n.lhs->accept(*this);
        n.rhs->accept(*this);
    }
    void visit(const IfNode& n) override {
        n.cond->accept(*this);
        n.then_body->accept(*this);
        if (n.else_body)
            n.else_body->accept(*this);
    }
    void visit(const IfShortNode& n) override {
        n.cond->accept(*this);
        n.stmt->accept(*this);
    }
    void visit(const WhileNode& n) override {
        n.cond->accept(*this);
        n.body->accept(*this);
    }
    void visit(const ForRangeNode& n) override {
        n.from->accept(*this);
        n.to->accept(*this);
        push_scope();
        if (!n.iter.empty())
            declare(n.iter);
        n.body->accept(*this);
        pop_scope();
    }
    void visit(const ForIterNode& n) override {
        n.iterable->accept(*this);
        push_scope();
        if (!n.iter.empty())
            declare(n.iter);
        n.body->accept(*this);
        pop_scope();
    }
    void visit(const LoopInfNode& n) override { n.body->accept(*this); }
    void visit(const ExitNode&) override {}
    void visit(const ReturnNode& n) override {
        if (n.value)
            n.value->accept(*this);
    }
    void visit(const PrintNode& n) override {
        for (const auto& e : n.exprs)
            e->accept(*this);
    }
    void visit(const BinOpNode& n) override {
        n.left->accept(*this);
        n.right->accept(*this);
    }
    void visit(const UnaryOpNode& n) override { n.operand->accept(*this); }
    void visit(const IsNode& n) override { n.operand->accept(*this); }
    void visit(const IdentNode& n) override { check_capture(n.ident_name); }
    void visit(const IndexNode& n) override {
        n.base->accept(*this);
        n.index_expr->accept(*this);
    }
    void visit(const CallNode& n) override {
        n.callee->accept(*this);
        for (const auto& a : n.args)
            a->accept(*this);
    }
    void visit(const DotFieldNode& n) override { n.base->accept(*this); }
    void visit(const DotIntNode& n) override { n.base->accept(*this); }
    void visit(const ArrayLitNode& n) override {
        for (const auto& e : n.elems)
            e->accept(*this);
    }
    void visit(const TupleLitNode& n) override {
        for (const auto& e : n.elems)
            e->accept(*this);
    }
    void visit(const TupleElemNode& n) override { n.expr->accept(*this); }
    void visit(const ParamListNode& n) override {
        for (const auto& p : n.params) {
            const auto* id = static_cast<const IdentNode*>(p.get());
            declare(id->ident_name);
        }
    }
    void visit(const FuncLitNode& n) override {
        auto* saved    = current_func_;
        current_func_  = &n;
        ++func_depth_;
        push_scope(); // param scope
        if (n.params)
            n.params->accept(*this);
        if (n.body)
            n.body->accept(*this);
        pop_scope();
        --func_depth_;
        current_func_ = saved;
    }
    void visit(const IntLitNode&) override {}
    void visit(const RealLitNode&) override {}
    void visit(const StrLitNode&) override {}
    void visit(const BoolLitNode&) override {}
    void visit(const NoneLitNode&) override {}
    void visit(const TypeNode&) override {}

private:
    struct Entry {
        int                func_depth;
        const FuncLitNode* owner; // which function declared this variable
    };
    using Scope = std::unordered_map<std::string, Entry>;

    std::vector<Scope> scopes_;
    int                func_depth_   = 0;
    const FuncLitNode* current_func_ = nullptr;

    void push_scope() { scopes_.emplace_back(); }
    void pop_scope() { scopes_.pop_back(); }

    void declare(const std::string& name) {
        scopes_.back()[name] = {func_depth_, current_func_};
    }

    void check_capture(const std::string& name) {
        for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
            auto it = scopes_[i].find(name);
            if (it == scopes_[i].end())
                continue;
            const Entry& e = it->second;
            if (e.func_depth < func_depth_) {
                // Variable is declared in an outer function → captured
                result[e.owner].insert(name);
            }
            return;
        }
    }
};

CaptureMap analyze_captures(const ASTNode& root) {
    CaptureAnalyzer ca;
    ca.run(root);
    return std::move(ca.result);
}

// ══════════════════════════════════════════════════════════════════════════════
// Pass 2: Bytecode Compiler
// ══════════════════════════════════════════════════════════════════════════════

// ── Variable location descriptor ──────────────────────────────────────────────
enum class VarKind { REG, CELL, UPVAL };
struct VarInfo {
    VarKind kind = VarKind::REG;
    int32_t idx  = -1;
};

// ── Per-function compilation context ──────────────────────────────────────────
struct FuncCtx {
    Proto*             proto     = nullptr;
    FuncCtx*           parent    = nullptr;
    const FuncLitNode* func_node = nullptr; // nullptr for main proto

    int next_reg  = 0;
    int next_cell = 0;

    using Scope = std::unordered_map<std::string, VarInfo>;
    std::vector<Scope> scopes;

    struct LoopInfo {
        std::vector<int> break_sites;
    };
    std::vector<LoopInfo> loop_stack;

    void push_scope() { scopes.emplace_back(); }
    void pop_scope() { scopes.pop_back(); }
    void push_loop() { loop_stack.emplace_back(); }
    void pop_loop() { loop_stack.pop_back(); }

    void add_break_site(int pc) {
        if (!loop_stack.empty())
            loop_stack.back().break_sites.push_back(pc);
    }
    void patch_breaks(std::vector<Instr>& code, int target) {
        if (loop_stack.empty())
            return;
        for (int s : loop_stack.back().break_sites)
            code[s].b = target;
    }

    std::optional<VarInfo> find_var(const std::string& name) const {
        for (int i = static_cast<int>(scopes.size()) - 1; i >= 0; --i) {
            auto it = scopes[i].find(name);
            if (it != scopes[i].end())
                return it->second;
        }
        return std::nullopt;
    }
};

// ── Compiler ───────────────────────────────────────────────────────────────────
struct BytecodeCompiler : ASTVisitorBase<BytecodeCompiler> {
    explicit BytecodeCompiler(const CaptureMap& cm) : captures_(cm) {}

    std::unique_ptr<Module> compile(const ASTNode& root) {
        // Reserve to prevent vector reallocation which would invalidate
        // the raw pointers stored in FuncCtx::parent.
        ctx_stack_.reserve(64);

        auto mod       = std::make_unique<Module>();
        mod->main      = std::make_shared<Proto>();
        mod->main->name = "<main>";

        push_ctx(FuncCtx{mod->main.get(), nullptr, nullptr, 0, 0});
        ctx().push_scope();

        root.accept(*this);

        ctx().pop_scope();

        // Ensure a trailing RETURNNONE
        auto& code = current_proto().code;
        if (code.empty() ||
            (code.back().op != Opc::RETURN && code.back().op != Opc::RETURNNONE))
            emit({Opc::RETURNNONE});

        flush_regs_cells();
        pop_ctx();
        return mod;
    }

    // ── Statements ─────────────────────────────────────────────────────────────

    void visit(const ProgramNode& n) override {
        for (const auto& s : n.stmts)
            s->accept(*this);
    }

    void visit(const BodyNode& n) override {
        ctx().push_scope();
        for (const auto& s : n.stmts)
            s->accept(*this);
        ctx().pop_scope();
    }

    void visit(const VarDeclNode& n) override {
        for (const auto& d : n.defs)
            d->accept(*this);
    }

    void visit(const VarDefNode& n) override {
        const bool is_func = n.init && dynamic_cast<const FuncLitNode*>(n.init.get());
        VarInfo vi;
        if (is_func) {
            // Pre-declare so the closure can capture it
            vi = declare_var(n.varname);
            int none_r = alloc_reg();
            emit({Opc::LOADNONE, none_r});
            store_var(vi, none_r);
            // Compile the FuncLit (may capture the pre-declared slot)
            n.init->accept(*this);
            store_var(vi, result_reg_);
        } else {
            if (n.init) {
                n.init->accept(*this);
            } else {
                int rn = alloc_reg();
                emit({Opc::LOADNONE, rn});
                result_reg_ = rn;
            }
            vi = declare_var(n.varname);
            store_var(vi, result_reg_);
        }
    }

    void visit(const AssignNode& n) override {
        n.rhs->accept(*this);
        compile_store_lvalue(*n.lhs, result_reg_);
    }

    void visit(const IfNode& n) override {
        n.cond->accept(*this);
        int cond    = result_reg_;
        int jmpf_pc = emit_jmpf(cond, -1);
        n.then_body->accept(*this);
        if (n.else_body) {
            int jmp_end = emit_jmp(-1);
            patch(jmpf_pc, current_pc());
            n.else_body->accept(*this);
            patch(jmp_end, current_pc());
        } else {
            patch(jmpf_pc, current_pc());
        }
    }

    void visit(const IfShortNode& n) override {
        n.cond->accept(*this);
        int jmpf = emit_jmpf(result_reg_, -1);
        n.stmt->accept(*this);
        patch(jmpf, current_pc());
    }

    void visit(const WhileNode& n) override {
        ctx().push_loop();
        int loop_start = current_pc();
        n.cond->accept(*this);
        int jmpf = emit_jmpf(result_reg_, -1);
        n.body->accept(*this);
        emit({Opc::JMP, -1, loop_start});
        int exit_pc = current_pc();
        patch(jmpf, exit_pc);
        ctx().patch_breaks(current_proto().code, exit_pc);
        ctx().pop_loop();
    }

    void visit(const ForRangeNode& n) override {
        n.from->accept(*this);
        int from_r = result_reg_;
        n.to->accept(*this);
        int to_r = result_reg_;

        ctx().push_scope();
        ctx().push_loop();

        // Declare iterator variable; determine working register for the counter.
        // For a REG variable, iter_r == vi.idx (counter IS the variable register).
        // For a CELL variable, iter_r is a separate temp and we STORECELL every step.
        int     iter_r = -1;
        VarInfo iter_vi{VarKind::REG, -1};
        if (!n.iter.empty()) {
            iter_vi = declare_var(n.iter);
            if (iter_vi.kind == VarKind::REG) {
                iter_r = iter_vi.idx; // counter lives in the variable register
                emit({Opc::MOVE, iter_r, from_r});
            } else { // CELL
                iter_r = alloc_reg();
                emit({Opc::MOVE, iter_r, from_r});
                emit({Opc::STORECELL, iter_vi.idx, iter_r});
            }
        } else {
            iter_r = alloc_reg();
            emit({Opc::MOVE, iter_r, from_r});
        }

        // Loop header: if iter_r > to_r → exit
        int loop_start = current_pc();
        int cmp_r      = alloc_reg();
        emit({Opc::GT, cmp_r, iter_r, to_r});
        int jmpt = emit_jmpt(cmp_r, -1);

        // Body
        n.body->accept(*this);

        // Increment iter_r
        int one_r = alloc_reg();
        emit({Opc::LOADK, one_r, add_const(ConstVal{1LL})});
        emit({Opc::ADD, iter_r, iter_r, one_r});

        // For a cell iter variable, keep the cell in sync after increment
        if (!n.iter.empty() && iter_vi.kind == VarKind::CELL)
            emit({Opc::STORECELL, iter_vi.idx, iter_r});

        emit({Opc::JMP, -1, loop_start});
        int exit_pc = current_pc();
        patch(jmpt, exit_pc);
        ctx().patch_breaks(current_proto().code, exit_pc);
        ctx().pop_loop();
        ctx().pop_scope();
    }

    void visit(const ForIterNode& n) override {
        n.iterable->accept(*this);
        int iterable_r = result_reg_;
        int iter_state = alloc_reg();
        emit({Opc::ITERINIT, iter_state, iterable_r});

        ctx().push_scope();
        ctx().push_loop();

        // Determine the register that ITERNEXT will write into.
        // For REG vars: ITERNEXT writes directly to the variable register.
        // For CELL vars: ITERNEXT writes to a temp, then we STORECELL.
        // For unnamed loops: ITERNEXT writes to a temp that is unused.
        int     loop_var = -1;
        VarInfo iter_vi{VarKind::REG, -1};
        if (!n.iter.empty()) {
            iter_vi  = declare_var(n.iter);
            loop_var = (iter_vi.kind == VarKind::REG) ? iter_vi.idx : alloc_reg();
        } else {
            loop_var = alloc_reg();
        }

        int loop_start  = current_pc();
        int iternext_pc = emit_raw({Opc::ITERNEXT, loop_var, iter_state, -1});

        // For a cell iter variable, keep the cell in sync
        if (!n.iter.empty() && iter_vi.kind == VarKind::CELL)
            emit({Opc::STORECELL, iter_vi.idx, loop_var});

        n.body->accept(*this);
        emit({Opc::JMP, -1, loop_start});

        int exit_pc = current_pc();
        current_proto().code[iternext_pc].c = exit_pc;
        ctx().patch_breaks(current_proto().code, exit_pc);
        ctx().pop_loop();
        ctx().pop_scope();
    }

    void visit(const LoopInfNode& n) override {
        ctx().push_loop();
        int loop_start = current_pc();
        n.body->accept(*this);
        emit({Opc::JMP, -1, loop_start});
        int exit_pc = current_pc();
        ctx().patch_breaks(current_proto().code, exit_pc);
        ctx().pop_loop();
    }

    void visit(const ExitNode&) override {
        // Emit a JMP to be back-patched when the enclosing loop is closed
        ctx().add_break_site(emit_jmp(-1));
    }

    void visit(const ReturnNode& n) override {
        if (n.value) {
            n.value->accept(*this);
            emit({Opc::RETURN, result_reg_});
        } else {
            emit({Opc::RETURNNONE});
        }
    }

    void visit(const PrintNode& n) override {
        int count = static_cast<int>(n.exprs.size());
        if (count == 0) {
            emit({Opc::PRINT, 0, 0});
            return;
        }
        int base = alloc_regs(count);
        for (int i = 0; i < count; ++i) {
            n.exprs[i]->accept(*this);
            if (result_reg_ != base + i)
                emit({Opc::MOVE, base + i, result_reg_});
        }
        emit({Opc::PRINT, base, count});
    }

    // ── Expressions ────────────────────────────────────────────────────────────

    void visit(const BinOpNode& n) override {
        n.left->accept(*this);
        int l = result_reg_;
        n.right->accept(*this);
        int r   = result_reg_;
        int dst = alloc_reg();
        // BinOpNode::Op ordering: OR AND XOR LT LE GT GE EQ NEQ ADD SUB MUL DIV
        static constexpr Opc op_map[] = {
            Opc::OR,  Opc::AND, Opc::XOR, Opc::LT, Opc::LE,  Opc::GT,
            Opc::GE,  Opc::EQ,  Opc::NEQ, Opc::ADD, Opc::SUB, Opc::MUL,
            Opc::DIV,
        };
        emit({op_map[static_cast<int>(n.op)], dst, l, r});
        result_reg_ = dst;
    }

    void visit(const UnaryOpNode& n) override {
        n.operand->accept(*this);
        int dst = alloc_reg();
        Opc op;
        switch (n.op) {
        case UnaryOpNode::Op::UPLUS:  op = Opc::UPLUS;  break;
        case UnaryOpNode::Op::UMINUS: op = Opc::UMINUS; break;
        case UnaryOpNode::Op::NOT:    op = Opc::NOT;    break;
        default:                      op = Opc::NOP;    break;
        }
        emit({op, dst, result_reg_});
        result_reg_ = dst;
    }

    void visit(const IsNode& n) override {
        n.operand->accept(*this);
        int src = result_reg_;
        // Map TypeNode::Type → TypeTag
        static constexpr TypeTag type_map[] = {
            TypeTag::Int,   TypeTag::Real,  TypeTag::Bool,  TypeTag::Str,
            TypeTag::None,  TypeTag::Array, TypeTag::Tuple, TypeTag::Func,
        };
        const auto& tn = static_cast<const TypeNode&>(*n.type_node);
        int dst = alloc_reg();
        emit({Opc::ISTYPE, dst, src, static_cast<int32_t>(type_map[static_cast<int>(tn.type)])});
        result_reg_ = dst;
    }

    void visit(const IdentNode& n) override {
        VarInfo vi  = resolve_var(n.ident_name);
        int     dst = alloc_reg();
        emit_load_var(dst, vi);
        result_reg_ = dst;
    }

    void visit(const IndexNode& n) override {
        n.base->accept(*this);
        int base_r = result_reg_;
        n.index_expr->accept(*this);
        int dst = alloc_reg();
        emit({Opc::GETINDEX, dst, base_r, result_reg_});
        result_reg_ = dst;
    }

    void visit(const CallNode& n) override {
        n.callee->accept(*this);
        int func_r = result_reg_;
        int argc   = static_cast<int>(n.args.size());
        int base_r = alloc_regs(argc);
        for (int i = 0; i < argc; ++i) {
            n.args[i]->accept(*this);
            if (result_reg_ != base_r + i)
                emit({Opc::MOVE, base_r + i, result_reg_});
        }
        int dst = alloc_reg();
        emit({Opc::CALL, dst, func_r, base_r, argc});
        result_reg_ = dst;
    }

    void visit(const DotFieldNode& n) override {
        n.base->accept(*this);
        int kidx = add_const(ConstVal{n.field});
        int dst  = alloc_reg();
        emit({Opc::GETFIELD, dst, result_reg_, kidx});
        result_reg_ = dst;
    }

    void visit(const DotIntNode& n) override {
        n.base->accept(*this);
        // Encode as GETFIELD with an integer constant (1-based index)
        int kidx = add_const(ConstVal{n.index});
        int dst  = alloc_reg();
        emit({Opc::GETFIELD, dst, result_reg_, kidx});
        result_reg_ = dst;
    }

    void visit(const IntLitNode& n) override {
        int dst = alloc_reg();
        emit({Opc::LOADK, dst, add_const(ConstVal{n.value})});
        result_reg_ = dst;
    }
    void visit(const RealLitNode& n) override {
        int dst = alloc_reg();
        emit({Opc::LOADK, dst, add_const(ConstVal{n.value})});
        result_reg_ = dst;
    }
    void visit(const StrLitNode& n) override {
        int dst = alloc_reg();
        emit({Opc::LOADK, dst, add_const(ConstVal{n.value})});
        result_reg_ = dst;
    }
    void visit(const BoolLitNode& n) override {
        int dst = alloc_reg();
        emit({Opc::LOADBOOL, dst, n.value ? 1 : 0});
        result_reg_ = dst;
    }
    void visit(const NoneLitNode&) override {
        int dst = alloc_reg();
        emit({Opc::LOADNONE, dst});
        result_reg_ = dst;
    }

    void visit(const ArrayLitNode& n) override {
        int count = static_cast<int>(n.elems.size());
        int base  = alloc_regs(count);
        for (int i = 0; i < count; ++i) {
            n.elems[i]->accept(*this);
            if (result_reg_ != base + i)
                emit({Opc::MOVE, base + i, result_reg_});
        }
        int dst = alloc_reg();
        emit({Opc::NEWARRAY, dst, base, count});
        result_reg_ = dst;
    }

    void visit(const TupleLitNode& n) override {
        int count = static_cast<int>(n.elems.size());
        // Add element names to constant pool (consecutive slots)
        int nkidx = static_cast<int>(current_proto().consts.size());
        for (const auto& e : n.elems) {
            const auto& te = static_cast<const TupleElemNode&>(*e);
            current_proto().consts.emplace_back(ConstVal{te.elem_name});
        }
        // Compile element expressions into contiguous registers
        int base = alloc_regs(count);
        for (int i = 0; i < count; ++i) {
            const auto& te = static_cast<const TupleElemNode&>(*n.elems[i]);
            te.expr->accept(*this);
            if (result_reg_ != base + i)
                emit({Opc::MOVE, base + i, result_reg_});
        }
        int dst = alloc_reg();
        emit({Opc::NEWTUPLE, dst, base, count, nkidx});
        result_reg_ = dst;
    }

    void visit(const TupleElemNode& n) override { n.expr->accept(*this); }

    void visit(const ParamListNode&) override {
        // Handled inside visit(FuncLitNode)
    }

    void visit(const FuncLitNode& n) override {
        // Create and register a new sub-proto
        auto sub        = std::make_shared<Proto>();
        sub->name       = std::format("<func@{}:{}>", n.loc.line, n.loc.col);
        int proto_idx   = static_cast<int>(current_proto().protos.size());
        current_proto().protos.push_back(sub);

        // Save parent pointer before push (reserve(64) keeps it valid).
        FuncCtx* parent = &ctx_stack_.back();
        push_ctx(FuncCtx{sub.get(), parent, &n, 0, 0});
        ctx().push_scope(); // scope for parameters

        // Declare parameters in registers 0, 1, 2, ...
        if (n.params) {
            const auto& pl = static_cast<const ParamListNode&>(*n.params);
            sub->params    = static_cast<int>(pl.params.size());
            for (const auto& p : pl.params) {
                const auto* id  = static_cast<const IdentNode*>(p.get());
                bool        cap = is_captured(&n, id->ident_name);
                int         r   = alloc_reg(); // always reg 0, 1, ...
                if (cap) {
                    int cell = alloc_cell();
                    ctx().scopes.back()[id->ident_name] = VarInfo{VarKind::CELL, cell};
                    emit({Opc::STORECELL, cell, r});
                } else {
                    ctx().scopes.back()[id->ident_name] = VarInfo{VarKind::REG, r};
                }
            }
        }

        // Compile body (always a BodyNode from the parser)
        if (n.body)
            n.body->accept(*this);

        // Trailing RETURNNONE
        auto& code = current_proto().code;
        if (code.empty() ||
            (code.back().op != Opc::RETURN && code.back().op != Opc::RETURNNONE))
            emit({Opc::RETURNNONE});

        flush_regs_cells();
        ctx().pop_scope();
        pop_ctx();

        // Back in the parent: emit CLOSURE
        int dst = alloc_reg();
        emit({Opc::CLOSURE, dst, proto_idx});
        result_reg_ = dst;
    }

    void visit(const TypeNode&) override {
        int dst = alloc_reg();
        emit({Opc::LOADNONE, dst});
        result_reg_ = dst;
    }

private:
    // ── State ──────────────────────────────────────────────────────────────────
    const CaptureMap&  captures_;
    std::vector<FuncCtx> ctx_stack_;
    int                result_reg_ = -1;

    FuncCtx&  ctx()            { return ctx_stack_.back(); }
    Proto&    current_proto()  { return *ctx().proto; }

    void push_ctx(FuncCtx c) { ctx_stack_.push_back(std::move(c)); }
    void pop_ctx()           { ctx_stack_.pop_back(); }

    // ── Register / cell allocation ─────────────────────────────────────────────
    int alloc_reg() {
        return ctx().next_reg++;
    }
    int alloc_regs(int n) {
        int base = ctx().next_reg;
        ctx().next_reg += n;
        return base;
    }
    int alloc_cell() {
        return ctx().next_cell++;
    }
    void flush_regs_cells() {
        current_proto().regs  = ctx().next_reg;
        current_proto().cells = ctx().next_cell;
    }

    // ── Constant pool ──────────────────────────────────────────────────────────
    int add_const(ConstVal v) {
        return current_proto().add_const(std::move(v));
    }

    // ── Code emission ──────────────────────────────────────────────────────────
    int emit(Instr ins) {
        auto& code = current_proto().code;
        code.push_back(ins);
        return static_cast<int>(code.size()) - 1;
    }
    int emit_raw(Instr ins) { return emit(ins); } // alias for clarity
    int current_pc() const {
        return static_cast<int>(ctx_stack_.back().proto->code.size());
    }
    void patch(int pc, int target) {
        current_proto().code[pc].b = target;
    }
    int emit_jmp(int target)              { return emit({Opc::JMP,  -1, target}); }
    int emit_jmpt(int cond, int target)   { return emit({Opc::JMPT, cond, target}); }
    int emit_jmpf(int cond, int target)   { return emit({Opc::JMPF, cond, target}); }

    // ── Variable management ────────────────────────────────────────────────────
    bool is_captured(const FuncLitNode* f, const std::string& name) const {
        auto it = captures_.find(f);
        return it != captures_.end() && it->second.count(name) > 0;
    }

    // Declare a new variable in the current innermost scope.
    // Allocates a register (if not captured) or a cell (if captured).
    // Does NOT store any initial value.
    VarInfo declare_var(const std::string& name) {
        VarInfo vi;
        if (is_captured(ctx().func_node, name)) {
            vi.kind = VarKind::CELL;
            vi.idx  = alloc_cell();
        } else {
            vi.kind = VarKind::REG;
            vi.idx  = alloc_reg();
        }
        ctx().scopes.back()[name] = vi;
        return vi;
    }

    // Resolve a variable name to a VarInfo in the current function.
    // If not found locally, adds it as an upvalue via the parent chain.
    VarInfo resolve_var(const std::string& name) {
        if (auto v = ctx().find_var(name))
            return *v;
        // Walk the parent chain and create upvalue entries
        return VarInfo{VarKind::UPVAL, capture_upval(name, &ctx())};
    }

    // Ensure `name` is an upvalue of `tgt` (recursively through parents).
    // Returns the upvalue index in `tgt`.
    int capture_upval(const std::string& name, FuncCtx* tgt) {
        // Check if already added
        for (int i = 0; i < static_cast<int>(tgt->proto->upvals.size()); ++i)
            if (tgt->proto->upvals[i].name == name)
                return i;

        FuncCtx* parent = tgt->parent;
        if (!parent)
            throw std::runtime_error(
                "undefined variable (missed by semantic analysis): " + name);

        int uv_idx = static_cast<int>(tgt->proto->upvals.size());

        if (auto v = parent->find_var(name)) {
            // Parent has it as a local variable → must be a CELL
            assert(v->kind == VarKind::CELL &&
                   "captured variable must be allocated as a cell");
            tgt->proto->upvals.push_back({true, v->idx, name});
        } else {
            // Not in parent's locals → ensure it's an upvalue of parent first
            int parent_uv = capture_upval(name, parent);
            tgt->proto->upvals.push_back({false, parent_uv, name});
        }
        return uv_idx;
    }

    void emit_load_var(int dst, const VarInfo& vi) {
        switch (vi.kind) {
        case VarKind::REG:
            if (dst != vi.idx)
                emit({Opc::MOVE, dst, vi.idx});
            break;
        case VarKind::CELL:
            emit({Opc::LOADCELL, dst, vi.idx});
            break;
        case VarKind::UPVAL:
            emit({Opc::GETUPVAL, dst, vi.idx});
            break;
        }
    }

    void store_var(const VarInfo& vi, int src) {
        switch (vi.kind) {
        case VarKind::REG:
            if (vi.idx != src)
                emit({Opc::MOVE, vi.idx, src});
            break;
        case VarKind::CELL:
            emit({Opc::STORECELL, vi.idx, src});
            break;
        case VarKind::UPVAL:
            emit({Opc::SETUPVAL, vi.idx, src});
            break;
        }
    }

    void compile_store_lvalue(const ASTNode& lhs, int val_reg) {
        if (const auto* id = dynamic_cast<const IdentNode*>(&lhs)) {
            store_var(resolve_var(id->ident_name), val_reg);
        } else if (const auto* idx = dynamic_cast<const IndexNode*>(&lhs)) {
            idx->base->accept(*this);
            int base_r = result_reg_;
            idx->index_expr->accept(*this);
            emit({Opc::SETINDEX, base_r, result_reg_, val_reg});
        } else if (const auto* dot = dynamic_cast<const DotFieldNode*>(&lhs)) {
            dot->base->accept(*this);
            int kidx = add_const(ConstVal{dot->field});
            emit({Opc::SETFIELD, result_reg_, kidx, val_reg});
        } else if (const auto* di = dynamic_cast<const DotIntNode*>(&lhs)) {
            di->base->accept(*this);
            int kidx = add_const(ConstVal{di->index});
            emit({Opc::SETFIELD, result_reg_, kidx, val_reg});
        } else {
            throw std::runtime_error("invalid lvalue in assignment");
        }
    }
};

// ── Public API ─────────────────────────────────────────────────────────────────
std::unique_ptr<Module> compile_bytecode(const ASTNode& root) {
    CaptureMap    captures = analyze_captures(root);
    BytecodeCompiler compiler{captures};
    return compiler.compile(root);
}
