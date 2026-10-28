// The generator: the checked tree into LLVM IR, optimised at -O1 and written as an object file.
//
// How values are laid out:
//   iN, uN -> iN; f32 -> float; f64 -> double; bool -> i1
//   str -> { ptr, i64 }: its bytes (a literal, stored with a NUL after it) and their count
//   str.c and extern.c types -> ptr
//   a struct -> an LLVM struct of its fields
//   an enum -> { i32 case, [n x i64] data }, the data area big enough for the largest case
//   a function that can fail returns { i32 status, T value, [n x i64] data }: status 0 is success, k + 1 is reason k
// Every local lives in an alloca; LLVM's -O1 turns them into registers.

#include "g2c2.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#include <cstdlib>

static llvm::LLVMContext* g_ctx = nullptr;
static llvm::Module* g_mod = nullptr;
static llvm::IRBuilder<>* g_b = nullptr;
static bool g_release = false;

// Why a program stops; the runtime's g2_stop names it. Kept in step with runtime/runtime.cpp.
enum StopKind { StopOverflow = 1, StopDivZero = 2, StopIndex = 3, StopConv = 4, StopFloatConvRelease = 5 };

struct LoopTarget {
    llvm::BasicBlock* stop = nullptr;
    llvm::BasicBlock* next = nullptr;
};

// The function being generated.
struct FnGen {
    Item* fn = nullptr;
    llvm::Function* f = nullptr;
    llvm::StructType* result = nullptr;  // when the function can fail
    std::vector<LoopTarget> loops;
};

static FnGen* g_fn = nullptr;
static std::map<Item*, llvm::Function*> g_eq_fns;

static llvm::Type* i1_ty() { return llvm::Type::getInt1Ty(*g_ctx); }
static llvm::Type* i8_ty() { return llvm::Type::getInt8Ty(*g_ctx); }
static llvm::Type* i32_ty() { return llvm::Type::getInt32Ty(*g_ctx); }
static llvm::Type* i64_ty() { return llvm::Type::getInt64Ty(*g_ctx); }
static llvm::Type* ptr_ty() { return llvm::PointerType::get(*g_ctx, 0); }
static llvm::Type* void_ty() { return llvm::Type::getVoidTy(*g_ctx); }
static llvm::Constant* i32_const(int64_t v) { return llvm::ConstantInt::get(i32_ty(), v); }
static llvm::Constant* i64_const(int64_t v) { return llvm::ConstantInt::get(i64_ty(), v); }

// ---- Types ----

static llvm::Type* lt(Type* t);

static uint64_t size_of(llvm::Type* t) { return g_mod->getDataLayout().getTypeAllocSize(t); }

// An area of i64s big enough for any of the types given.
static llvm::ArrayType* data_area(const std::vector<Type*>& types) {
    uint64_t bytes = 0;
    for (Type* t : types) {
        if (t) bytes = std::max(bytes, size_of(lt(t)));
    }
    return llvm::ArrayType::get(i64_ty(), (bytes + 7) / 8);
}

static llvm::Type* lt(Type* t) {
    if (t->llvm) return t->llvm;
    switch (t->kind) {
    case TypeKind::Int:
        t->llvm = llvm::IntegerType::get(*g_ctx, t->bits);
        break;
    case TypeKind::Float:
        t->llvm = t->bits == 32 ? llvm::Type::getFloatTy(*g_ctx) : llvm::Type::getDoubleTy(*g_ctx);
        break;
    case TypeKind::Bool:
        t->llvm = i1_ty();
        break;
    case TypeKind::Str:
        t->llvm = llvm::StructType::create(*g_ctx, {ptr_ty(), i64_ty()}, "str");
        break;
    case TypeKind::StrC:
    case TypeKind::Opaque:
        t->llvm = ptr_ty();
        break;
    case TypeKind::Void:
        t->llvm = void_ty();
        break;
    case TypeKind::Struct: {
        auto* st = llvm::StructType::create(*g_ctx, t->name);
        t->llvm = st;
        std::vector<llvm::Type*> fields;
        for (Field& f : t->item->fields) fields.push_back(lt(f.type));
        st->setBody(fields);
        break;
    }
    case TypeKind::Enum: {
        auto* st = llvm::StructType::create(*g_ctx, t->name);
        t->llvm = st;
        std::vector<Type*> datas;
        for (Case& c : t->item->cases) datas.push_back(c.type);
        st->setBody({i32_ty(), data_area(datas)});
        break;
    }
    }
    return t->llvm;
}

// { i32 status, T value, data } for a function that can fail.
static llvm::StructType* result_type(Item* fn) {
    std::vector<Type*> datas;
    for (Reason* r : fn->fails) datas.push_back(r->type);
    std::vector<llvm::Type*> fields = {i32_ty()};
    if (fn->ret != t_void) fields.push_back(lt(fn->ret));
    fields.push_back(data_area(datas));
    return llvm::StructType::get(*g_ctx, fields);
}

// ---- Small helpers ----

static llvm::AllocaInst* entry_alloca(llvm::Type* t, const std::string& name) {
    llvm::BasicBlock& entry = g_fn->f->getEntryBlock();
    llvm::IRBuilder<> b(&entry, entry.begin());
    return b.CreateAlloca(t, nullptr, name);
}

static llvm::BasicBlock* new_block(const std::string& name) {
    return llvm::BasicBlock::Create(*g_ctx, name, g_fn->f);
}

// After a ret or br, later statements go into a fresh block nothing jumps to.
static void start_dead_block() {
    g_b->SetInsertPoint(new_block("dead"));
}

static bool terminated() { return g_b->GetInsertBlock()->hasTerminator(); }

static void br_to(llvm::BasicBlock* bb) {
    if (!terminated()) g_b->CreateBr(bb);
}

// Stores a value in a data area, and reads one back.
static llvm::Value* to_data(llvm::Value* v, llvm::ArrayType* area) {
    llvm::AllocaInst* slot = entry_alloca(area, "data");
    g_b->CreateStore(llvm::Constant::getNullValue(area), slot);
    if (v) g_b->CreateStore(v, slot);
    return g_b->CreateLoad(area, slot);
}

static llvm::Value* from_data(llvm::Value* data, Type* t) {
    llvm::AllocaInst* slot = entry_alloca(data->getType(), "data");
    g_b->CreateStore(data, slot);
    return g_b->CreateLoad(lt(t), slot);
}

static llvm::FunctionCallee runtime_fn(const char* name, llvm::Type* ret, std::vector<llvm::Type*> params) {
    llvm::FunctionCallee fn = g_mod->getOrInsertFunction(name, llvm::FunctionType::get(ret, params, false));
    return fn;
}

static llvm::Value* file_name_const(Pos pos) {
    return g_b->CreateGlobalString(g_files[pos.file].path, "file");
}

// Stops the program, naming the line, if bad holds.
static void stop_if(llvm::Value* bad, StopKind kind, Pos pos) {
    llvm::BasicBlock* stop = new_block("stop");
    llvm::BasicBlock* ok = new_block("ok");
    g_b->CreateCondBr(bad, stop, ok);
    g_b->SetInsertPoint(stop);
    auto fn = runtime_fn("g2_stop", void_ty(), {i32_ty(), ptr_ty(), i64_ty()});
    llvm::CallInst* call = g_b->CreateCall(fn, {i32_const(kind), file_name_const(pos), i64_const(pos.line)});
    call->setDoesNotReturn();
    g_b->CreateUnreachable();
    g_b->SetInsertPoint(ok);
}

// A string literal: its bytes and a NUL, so str.c of it needs no copy.
static llvm::Value* gen_str_lit(const std::string& bytes) {
    llvm::Constant* data = llvm::ConstantDataArray::getString(*g_ctx, bytes, true);
    auto* gv = new llvm::GlobalVariable(*g_mod, data->getType(), true, llvm::GlobalValue::PrivateLinkage, data, "str");
    gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    return llvm::ConstantStruct::get((llvm::StructType*)lt(t_str), {gv, i64_const((int64_t)bytes.size())});
}

// ---- Expressions ----

static llvm::Value* gen_expr(Expr* e);
static llvm::Value* gen_handle(Expr* h);
static void gen_block(Block* b);

static llvm::Value* gen_int_lit(Expr* e) {
    llvm::Type* t = lt(e->type);
    if (e->negative) return llvm::ConstantInt::get(t, ~e->value + 1, true);
    return llvm::ConstantInt::get(t, e->value);
}

static llvm::Value* gen_float_lit(Expr* e) {
    std::string digits;
    for (char ch : e->text) {
        if (ch != '_') digits += ch;
    }
    double v = e->type->bits == 32 ? (double)std::strtof(digits.c_str(), nullptr) : std::strtod(digits.c_str(), nullptr);
    if (e->negative) v = -v;
    return llvm::ConstantFP::get(lt(e->type), v);
}

// An address for a place: a local, or a field of one.
static llvm::Value* gen_place(Expr* e) {
    if (e->kind == ExprKind::LocalRef) return e->local->slot;
    if (e->kind == ExprKind::FieldGet) {
        llvm::Value* base = gen_place(e->kids[0]);
        return g_b->CreateStructGEP(lt(e->kids[0]->type), base, e->index);
    }
    std::abort();
}

// a op b on integers, stopping a debug build on overflow, and any build on dividing by zero.
static llvm::Value* gen_int_arith(const std::string& op, llvm::Value* a, llvm::Value* b, Type* t, Pos pos) {
    bool s = t->is_signed;
    if (op == "/" || op == "%") {
        stop_if(g_b->CreateICmpEQ(b, llvm::ConstantInt::get(a->getType(), 0)), StopDivZero, pos);
        if (!s) return op == "/" ? g_b->CreateUDiv(a, b) : g_b->CreateURem(a, b);
        // the smallest value divided by -1 overflows (its remainder, 0, does not); LLVM leaves both undefined,
        // so -1 is done apart
        llvm::Value* minus_one = g_b->CreateICmpEQ(b, llvm::ConstantInt::getSigned(a->getType(), -1));
        if (!g_release && op == "/") {
            llvm::Value* min = llvm::ConstantInt::get(a->getType(), llvm::APInt::getSignedMinValue(t->bits));
            stop_if(g_b->CreateAnd(minus_one, g_b->CreateICmpEQ(a, min)), StopOverflow, pos);
        }
        llvm::Value* safe_b = g_b->CreateSelect(minus_one, llvm::ConstantInt::get(a->getType(), 1), b);
        if (op == "/") return g_b->CreateSelect(minus_one, g_b->CreateSub(llvm::ConstantInt::get(a->getType(), 0), a), g_b->CreateSDiv(a, safe_b));
        return g_b->CreateSelect(minus_one, llvm::ConstantInt::get(a->getType(), 0), g_b->CreateSRem(a, safe_b));
    }
    if (g_release) {
        if (op == "+") return g_b->CreateAdd(a, b);
        if (op == "-") return g_b->CreateSub(a, b);
        return g_b->CreateMul(a, b);
    }
    llvm::Intrinsic::ID id;
    if (op == "+") id = s ? llvm::Intrinsic::sadd_with_overflow : llvm::Intrinsic::uadd_with_overflow;
    else if (op == "-") id = s ? llvm::Intrinsic::ssub_with_overflow : llvm::Intrinsic::usub_with_overflow;
    else id = s ? llvm::Intrinsic::smul_with_overflow : llvm::Intrinsic::umul_with_overflow;
    llvm::Value* r = g_b->CreateBinaryIntrinsic(id, a, b);
    stop_if(g_b->CreateExtractValue(r, 1), StopOverflow, pos);
    return g_b->CreateExtractValue(r, 0);
}

static llvm::Value* gen_binary(Expr* e) {
    llvm::Value* a = gen_expr(e->kids[0]);
    llvm::Value* b = gen_expr(e->kids[1]);
    const std::string& op = e->text;
    if (is_float(e->type)) {
        if (op == "+") return g_b->CreateFAdd(a, b);
        if (op == "-") return g_b->CreateFSub(a, b);
        if (op == "*") return g_b->CreateFMul(a, b);
        return g_b->CreateFDiv(a, b);
    }
    return gen_int_arith(op, a, b, e->type, e->pos);
}

static llvm::Value* gen_eq(llvm::Value* a, llvm::Value* b, Type* t);

// The == function of a struct or enum, made once.
static llvm::Function* eq_fn(Type* t) {
    auto found = g_eq_fns.find(t->item);
    if (found != g_eq_fns.end()) return found->second;
    auto* ft = llvm::FunctionType::get(i1_ty(), {lt(t), lt(t)}, false);
    auto* f = llvm::Function::Create(ft, llvm::GlobalValue::InternalLinkage, "eq." + t->name, g_mod);
    g_eq_fns[t->item] = f;
    FnGen* saved_fn = g_fn;
    llvm::IRBuilderBase::InsertPoint saved = g_b->saveIP();
    FnGen fg;
    fg.f = f;
    g_fn = &fg;
    g_b->SetInsertPoint(llvm::BasicBlock::Create(*g_ctx, "entry", f));
    llvm::Value* a = f->getArg(0);
    llvm::Value* b = f->getArg(1);
    if (t->kind == TypeKind::Struct) {
        llvm::Value* all = llvm::ConstantInt::getTrue(*g_ctx);
        for (size_t i = 0; i < t->item->fields.size(); i++) {
            Type* ft2 = t->item->fields[i].type;
            all = g_b->CreateAnd(all, gen_eq(g_b->CreateExtractValue(a, (unsigned)i), g_b->CreateExtractValue(b, (unsigned)i), ft2));
        }
        g_b->CreateRet(all);
    } else {
        llvm::Value* ta = g_b->CreateExtractValue(a, 0);
        llvm::Value* tb = g_b->CreateExtractValue(b, 0);
        llvm::BasicBlock* same = new_block("same");
        llvm::BasicBlock* differ = new_block("differ");
        g_b->CreateCondBr(g_b->CreateICmpEQ(ta, tb), same, differ);
        g_b->SetInsertPoint(differ);
        g_b->CreateRet(llvm::ConstantInt::getFalse(*g_ctx));
        g_b->SetInsertPoint(same);
        llvm::BasicBlock* nodata = new_block("nodata");
        llvm::SwitchInst* sw = g_b->CreateSwitch(ta, nodata);
        for (size_t i = 0; i < t->item->cases.size(); i++) {
            Type* ct = t->item->cases[i].type;
            if (!ct) continue;
            llvm::BasicBlock* bb = new_block("case");
            sw->addCase((llvm::ConstantInt*)i32_const((int64_t)i), bb);
            g_b->SetInsertPoint(bb);
            llvm::Value* da = from_data(g_b->CreateExtractValue(a, 1), ct);
            llvm::Value* db = from_data(g_b->CreateExtractValue(b, 1), ct);
            g_b->CreateRet(gen_eq(da, db, ct));
        }
        g_b->SetInsertPoint(nodata);
        g_b->CreateRet(llvm::ConstantInt::getTrue(*g_ctx));
    }
    g_fn = saved_fn;
    g_b->restoreIP(saved);
    return f;
}

static llvm::Value* gen_eq(llvm::Value* a, llvm::Value* b, Type* t) {
    switch (t->kind) {
    case TypeKind::Int:
    case TypeKind::Bool:
    case TypeKind::Opaque:
        return g_b->CreateICmpEQ(a, b);
    case TypeKind::Float:
        return g_b->CreateFCmpOEQ(a, b);
    case TypeKind::Str: {
        auto fn = runtime_fn("g2_str_eq", i1_ty(), {ptr_ty(), i64_ty(), ptr_ty(), i64_ty()});
        return g_b->CreateCall(fn, {g_b->CreateExtractValue(a, 0), g_b->CreateExtractValue(a, 1), g_b->CreateExtractValue(b, 0),
                                    g_b->CreateExtractValue(b, 1)});
    }
    case TypeKind::Struct:
    case TypeKind::Enum:
        return g_b->CreateCall(eq_fn(t), {a, b});
    default:
        std::abort();
    }
}

static llvm::Value* gen_compare_one(const std::string& op, llvm::Value* a, llvm::Value* b, Type* t) {
    if (op == "==") return gen_eq(a, b, t);
    if (op == "!=") {
        if (is_float(t)) return g_b->CreateFCmpUNE(a, b);
        return g_b->CreateNot(gen_eq(a, b, t));
    }
    if (is_float(t)) {
        if (op == "<") return g_b->CreateFCmpOLT(a, b);
        if (op == "<=") return g_b->CreateFCmpOLE(a, b);
        if (op == ">") return g_b->CreateFCmpOGT(a, b);
        return g_b->CreateFCmpOGE(a, b);
    }
    if (t == t_str) {
        auto fn = runtime_fn("g2_str_cmp", i32_ty(), {ptr_ty(), i64_ty(), ptr_ty(), i64_ty()});
        a = g_b->CreateCall(fn, {g_b->CreateExtractValue(a, 0), g_b->CreateExtractValue(a, 1), g_b->CreateExtractValue(b, 0),
                                 g_b->CreateExtractValue(b, 1)});
        b = i32_const(0);
        t = t_i32;
    }
    bool s = t->is_signed;
    if (op == "<") return s ? g_b->CreateICmpSLT(a, b) : g_b->CreateICmpULT(a, b);
    if (op == "<=") return s ? g_b->CreateICmpSLE(a, b) : g_b->CreateICmpULE(a, b);
    if (op == ">") return s ? g_b->CreateICmpSGT(a, b) : g_b->CreateICmpUGT(a, b);
    return s ? g_b->CreateICmpSGE(a, b) : g_b->CreateICmpUGE(a, b);
}

// a < b < c is a < b && b < c, each value worked out once, stopping early.
static llvm::Value* gen_compare(Expr* e) {
    Type* t = e->kids[0]->type;
    llvm::BasicBlock* done = new_block("cmp.done");
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> results;
    llvm::Value* a = gen_expr(e->kids[0]);
    for (size_t i = 0; i < e->ops.size(); i++) {
        llvm::Value* b = gen_expr(e->kids[i + 1]);
        llvm::Value* r = gen_compare_one(e->ops[i], a, b, t);
        if (i + 1 == e->ops.size()) {
            results.push_back({r, g_b->GetInsertBlock()});
            g_b->CreateBr(done);
        } else {
            llvm::BasicBlock* more = new_block("cmp.more");
            results.push_back({llvm::ConstantInt::getFalse(*g_ctx), g_b->GetInsertBlock()});
            g_b->CreateCondBr(r, more, done);
            g_b->SetInsertPoint(more);
        }
        a = b;
    }
    g_b->SetInsertPoint(done);
    llvm::PHINode* phi = g_b->CreatePHI(i1_ty(), (unsigned)results.size());
    for (auto& [v, bb] : results) phi->addIncoming(v, bb);
    return phi;
}

static llvm::Value* gen_logic(Expr* e) {
    const std::string& op = e->text;
    if (op == "and" || op == "or") {
        llvm::Value* r = gen_expr(e->kids[0]);
        for (size_t i = 1; i < e->kids.size(); i++) {
            llvm::Value* v = gen_expr(e->kids[i]);
            r = op == "and" ? g_b->CreateAnd(r, v) : g_b->CreateOr(r, v);
        }
        return r;
    }
    // && and || stop early
    bool is_and = op == "&&";
    llvm::BasicBlock* done = new_block("logic.done");
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> results;
    for (size_t i = 0; i < e->kids.size(); i++) {
        llvm::Value* v = gen_expr(e->kids[i]);
        if (i + 1 == e->kids.size()) {
            results.push_back({v, g_b->GetInsertBlock()});
            g_b->CreateBr(done);
        } else {
            llvm::BasicBlock* more = new_block("logic.more");
            results.push_back({llvm::ConstantInt::getBool(*g_ctx, !is_and), g_b->GetInsertBlock()});
            if (is_and) g_b->CreateCondBr(v, more, done);
            else g_b->CreateCondBr(v, done, more);
            g_b->SetInsertPoint(more);
        }
    }
    g_b->SetInsertPoint(done);
    llvm::PHINode* phi = g_b->CreatePHI(i1_ty(), (unsigned)results.size());
    for (auto& [v, bb] : results) phi->addIncoming(v, bb);
    return phi;
}

static llvm::Value* gen_unary(Expr* e) {
    llvm::Value* v = gen_expr(e->kids[0]);
    if (e->text == "!") return g_b->CreateNot(v);
    if (is_float(e->type)) return g_b->CreateFNeg(v);
    return gen_int_arith("-", llvm::ConstantInt::get(v->getType(), 0), v, e->type, e->pos);
}

// ---- Conversions ----

// Whether the number v of type from fits in to (both numbers; to an integer).
static llvm::Value* conv_fits(llvm::Value* v, Type* from, Type* to) {
    if (is_int(from)) {
        llvm::Type* wide = llvm::IntegerType::get(*g_ctx, 128);
        llvm::Value* w = from->is_signed ? g_b->CreateSExt(v, wide) : g_b->CreateZExt(v, wide);
        llvm::APInt lo = to->is_signed ? llvm::APInt::getSignedMinValue(to->bits).sext(128) : llvm::APInt(128, 0);
        llvm::APInt hi = to->is_signed ? llvm::APInt::getSignedMaxValue(to->bits).sext(128) : llvm::APInt::getMaxValue(to->bits).zext(128);
        return g_b->CreateAnd(g_b->CreateICmpSGE(w, llvm::ConstantInt::get(wide, lo)), g_b->CreateICmpSLE(w, llvm::ConstantInt::get(wide, hi)));
    }
    // a float fits when dropping its fraction toward zero gives a value in range; NaN never fits
    llvm::Value* d = from->bits == 32 ? g_b->CreateFPExt(v, llvm::Type::getDoubleTy(*g_ctx)) : v;
    llvm::Type* dt = llvm::Type::getDoubleTy(*g_ctx);
    double hi = to->is_signed ? std::ldexp(1.0, to->bits - 1) : std::ldexp(1.0, to->bits);
    llvm::Value* below = g_b->CreateFCmpOLT(d, llvm::ConstantFP::get(dt, hi));
    llvm::Value* above;
    if (!to->is_signed) above = g_b->CreateFCmpOGT(d, llvm::ConstantFP::get(dt, -1.0));
    else if (to->bits == 64) above = g_b->CreateFCmpOGE(d, llvm::ConstantFP::get(dt, -std::ldexp(1.0, 63)));
    else above = g_b->CreateFCmpOGT(d, llvm::ConstantFP::get(dt, -std::ldexp(1.0, to->bits - 1) - 1.0));
    return g_b->CreateAnd(above, below);
}

// The converted value; for an integer narrowing, the wrapped one.
static llvm::Value* conv_value(llvm::Value* v, Type* from, Type* to) {
    llvm::Type* t = lt(to);
    if (from == to) return v;
    if (is_int(from) && is_int(to)) {
        if (to->bits < from->bits) return g_b->CreateTrunc(v, t);
        if (to->bits > from->bits) return from->is_signed ? g_b->CreateSExt(v, t) : g_b->CreateZExt(v, t);
        return v;
    }
    if (is_float(from) && is_int(to)) {
        llvm::Value* r = to->is_signed ? g_b->CreateFPToSI(v, t) : g_b->CreateFPToUI(v, t);
        return g_b->CreateFreeze(r);
    }
    if (is_int(from) && is_float(to)) return from->is_signed ? g_b->CreateSIToFP(v, t) : g_b->CreateUIToFP(v, t);
    if (to->bits > from->bits) return g_b->CreateFPExt(v, t);
    return g_b->CreateFPTrunc(v, t);
}

// The outcome of something that can fail: status 0 on success, k + 1 for reason k.
struct Outcome {
    llvm::Value* status = nullptr;
    llvm::Value* value = nullptr;  // null when it gives nothing
    llvm::Value* data = nullptr;   // the reasons' data area, or null
};

static Outcome gen_outcome(Expr* call) {
    Outcome o;
    if (call->kind == ExprKind::Conv) {
        Type* from = call->kids[0]->type;
        Type* to = call->type;
        llvm::Value* v = gen_expr(call->kids[0]);
        if (from == t_str) {
            auto fn = runtime_fn("g2_text_to_int", i32_ty(), {ptr_ty(), i64_ty(), i32_ty(), i1_ty(), ptr_ty()});
            llvm::AllocaInst* out = entry_alloca(i64_ty(), "n");
            o.status = g_b->CreateCall(fn, {g_b->CreateExtractValue(v, 0), g_b->CreateExtractValue(v, 1), i32_const(to->bits),
                                            llvm::ConstantInt::getBool(*g_ctx, to->is_signed), out});
            o.value = g_b->CreateTrunc(g_b->CreateLoad(i64_ty(), out), lt(to));
            if (to->bits == 64) o.value = g_b->CreateLoad(i64_ty(), out);
            return o;
        }
        o.status = g_b->CreateSelect(conv_fits(v, from, to), i32_const(0), i32_const(1));
        o.value = conv_value(v, from, to);
        return o;
    }
    llvm::Value* r = gen_expr(call);
    o.status = g_b->CreateExtractValue(r, 0);
    unsigned last = ((llvm::StructType*)r->getType())->getNumElements() - 1;
    if (call->type != t_void) o.value = g_b->CreateExtractValue(r, 1);
    o.data = g_b->CreateExtractValue(r, last);
    return o;
}

static llvm::Value* gen_conv(Expr* e) {
    Type* from = e->kids[0]->type;
    Type* to = e->type;
    llvm::Value* v = gen_expr(e->kids[0]);
    if (e->can_fail) {
        llvm::Value* bad = g_b->CreateNot(conv_fits(v, from, to));
        if (!g_release) stop_if(bad, StopConv, e->pos);
        else if (is_float(from)) stop_if(bad, StopFloatConvRelease, e->pos);
    }
    return conv_value(v, from, to);
}

// ---- Calls ----

static llvm::Value* gen_call(Expr* e) {
    Item* fn = e->item;
    std::vector<llvm::Value*> args;
    size_t first = 0;
    if (e->kind == ExprKind::MethodCall) {
        args.push_back(fn->self_var ? gen_place(e->kids[0]) : gen_expr(e->kids[0]));
        first = 1;
    }
    for (size_t i = first; i < e->kids.size(); i++) args.push_back(gen_expr(e->kids[i]));
    return g_b->CreateCall(fn->llvm_fn, args);
}

static void print_value(llvm::Value* v, Type* t) {
    if (t == t_str) {
        auto fn = runtime_fn("g2_print_str", void_ty(), {ptr_ty(), i64_ty()});
        g_b->CreateCall(fn, {g_b->CreateExtractValue(v, 0), g_b->CreateExtractValue(v, 1)});
    } else if (is_int(t)) {
        if (t->is_signed) {
            auto fn = runtime_fn("g2_print_i64", void_ty(), {i64_ty()});
            g_b->CreateCall(fn, {g_b->CreateSExt(v, i64_ty())});
        } else {
            auto fn = runtime_fn("g2_print_u64", void_ty(), {i64_ty()});
            g_b->CreateCall(fn, {g_b->CreateZExt(v, i64_ty())});
        }
    } else {
        std::abort();
    }
}

static llvm::Value* gen_expr(Expr* e) {
    switch (e->kind) {
    case ExprKind::Int:
        return gen_int_lit(e);
    case ExprKind::Float:
        return gen_float_lit(e);
    case ExprKind::Str:
        return gen_str_lit(e->text);
    case ExprKind::Bool:
        return llvm::ConstantInt::getBool(*g_ctx, e->value != 0);
    case ExprKind::LocalRef:
        return g_b->CreateLoad(lt(e->type), e->local->slot, e->local->name);
    case ExprKind::Paren:
        return gen_expr(e->kids[0]);
    case ExprKind::Unary:
        return gen_unary(e);
    case ExprKind::Binary:
        return gen_binary(e);
    case ExprKind::Compare:
        return gen_compare(e);
    case ExprKind::Logic:
        return gen_logic(e);
    case ExprKind::FnCall:
    case ExprKind::MethodCall:
        return gen_call(e);
    case ExprKind::Conv:
        return gen_conv(e);
    case ExprKind::StrC:
        return g_b->CreateExtractValue(gen_expr(e->kids[0]), 0);
    case ExprKind::StrLen:
        return g_b->CreateExtractValue(gen_expr(e->kids[0]), 1);
    case ExprKind::StrIndex: {
        llvm::Value* s = gen_expr(e->kids[0]);
        llvm::Value* i = gen_expr(e->kids[1]);
        llvm::Value* len = g_b->CreateExtractValue(s, 1);
        llvm::Value* bad = g_b->CreateOr(g_b->CreateICmpSLT(i, i64_const(1)), g_b->CreateICmpSGT(i, len));
        stop_if(bad, StopIndex, e->pos);
        llvm::Value* p = g_b->CreateGEP(i8_ty(), g_b->CreateExtractValue(s, 0), g_b->CreateSub(i, i64_const(1)));
        return g_b->CreateLoad(i8_ty(), p);
    }
    case ExprKind::FieldGet:
        return g_b->CreateExtractValue(gen_expr(e->kids[0]), (unsigned)e->index);
    case ExprKind::StructLit: {
        llvm::Value* v = llvm::UndefValue::get(lt(e->type));
        for (size_t i = 0; i < e->kids.size(); i++) v = g_b->CreateInsertValue(v, gen_expr(e->kids[i]), (unsigned)i);
        return v;
    }
    case ExprKind::EnumLit: {
        auto* st = (llvm::StructType*)lt(e->type);
        auto* area = (llvm::ArrayType*)st->getElementType(1);
        llvm::Value* data = e->kids.empty() ? llvm::Constant::getNullValue(area) : to_data(gen_expr(e->kids[0]), area);
        llvm::Value* v = llvm::UndefValue::get(st);
        v = g_b->CreateInsertValue(v, i32_const(e->index), 0);
        return g_b->CreateInsertValue(v, data, 1);
    }
    case ExprKind::Print:
        for (size_t i = 0; i < e->pieces.size(); i++) {
            if (!e->pieces[i].empty()) print_value(gen_str_lit(e->pieces[i]), t_str);
            if (i < e->kids.size()) print_value(gen_expr(e->kids[i]), e->kids[i]->type);
        }
        return nullptr;
    case ExprKind::Exit: {
        auto fn = runtime_fn("g2_exit", void_ty(), {i32_ty()});
        llvm::CallInst* call = g_b->CreateCall(fn, {gen_expr(e->kids[0])});
        call->setDoesNotReturn();
        g_b->CreateUnreachable();
        start_dead_block();
        return nullptr;
    }
    case ExprKind::Handle:
        return gen_handle(e);
    default:
        std::abort();
    }
}

// ---- Leaving a function ----

static void gen_ret(llvm::Value* v) {
    if (!g_fn->result) {
        if (v) g_b->CreateRet(v);
        else g_b->CreateRetVoid();
        return;
    }
    llvm::Value* r = llvm::UndefValue::get(g_fn->result);
    r = g_b->CreateInsertValue(r, i32_const(0), 0);
    unsigned last = g_fn->result->getNumElements() - 1;
    if (v) r = g_b->CreateInsertValue(r, v, 1);
    r = g_b->CreateInsertValue(r, llvm::Constant::getNullValue(g_fn->result->getElementType(last)), last);
    g_b->CreateRet(r);
}

// Leaves the function failing with its reason index (data may be null).
static void gen_fail(int index, llvm::Value* data) {
    unsigned last = g_fn->result->getNumElements() - 1;
    auto* area = (llvm::ArrayType*)g_fn->result->getElementType(last);
    llvm::Value* r = llvm::UndefValue::get(g_fn->result);
    r = g_b->CreateInsertValue(r, i32_const(index + 1), 0);
    if (g_fn->result->getNumElements() == 3) r = g_b->CreateInsertValue(r, llvm::Constant::getNullValue(g_fn->result->getElementType(1)), 1);
    r = g_b->CreateInsertValue(r, data ? to_data(data, area) : llvm::Constant::getNullValue(area), last);
    g_b->CreateRet(r);
}

// call ?? value, call ?? fail, call ?? coll.member. Returns the value, or null when it gives nothing.
static llvm::Value* gen_handle(Expr* h) {
    Expr* call = h->kids[0];
    Outcome o = gen_outcome(call);
    llvm::BasicBlock* failed = new_block("failed");
    llvm::BasicBlock* ok = new_block("ok");
    g_b->CreateCondBr(g_b->CreateICmpEQ(o.status, i32_const(0)), ok, failed);
    llvm::BasicBlock* ok_end = ok;
    g_b->SetInsertPoint(failed);

    if (h->handle == HandleKind::Value) {
        // the value is worked out only when the call fails
        llvm::Value* fallback = gen_expr(h->kids[1]);
        llvm::BasicBlock* failed_end = g_b->GetInsertBlock();
        llvm::BasicBlock* join = new_block("handled");
        g_b->CreateBr(join);
        g_b->SetInsertPoint(ok);
        g_b->CreateBr(join);
        g_b->SetInsertPoint(join);
        llvm::PHINode* phi = g_b->CreatePHI(o.value->getType(), 2);
        phi->addIncoming(o.value, ok_end);
        phi->addIncoming(fallback, failed_end);
        return phi;
    }

    const std::vector<Reason*>& reasons = call->reasons;
    if (h->handle == HandleKind::Fail) {
        llvm::SwitchInst* sw = g_b->CreateSwitch(o.status, ok);
        for (size_t i = 0; i < reasons.size(); i++) {
            llvm::BasicBlock* bb = new_block("pass_up");
            sw->addCase((llvm::ConstantInt*)i32_const((int64_t)i + 1), bb);
            g_b->SetInsertPoint(bb);
            int mine = 0;
            for (size_t k = 0; k < g_fn->fn->fails.size(); k++) {
                if (g_fn->fn->fails[k]->name == reasons[i]->name) mine = (int)k;
            }
            llvm::Value* data = reasons[i]->type ? from_data(o.data, reasons[i]->type) : nullptr;
            gen_fail(mine, data);
        }
        g_b->SetInsertPoint(ok);
        return o.value;
    }

    // a member: its parameters, then one arm per reason, written here as if at the call
    for (size_t i = 0; i < h->member_params.size(); i++) {
        Local* l = h->member_params[i];
        l->slot = entry_alloca(lt(l->type), l->name);
        g_b->CreateStore(gen_expr(h->kids[i + 1]), l->slot);
    }
    llvm::BasicBlock* after = new_block("after_arms");
    llvm::SwitchInst* sw = g_b->CreateSwitch(o.status, after);
    for (size_t i = 0; i < h->arms.size(); i++) {
        Arm* a = h->arms[i];
        llvm::BasicBlock* bb = new_block("arm." + a->name);
        sw->addCase((llvm::ConstantInt*)i32_const((int64_t)i + 1), bb);
        g_b->SetInsertPoint(bb);
        if (a->local) {
            a->local->slot = entry_alloca(lt(a->local->type), a->local->name);
            g_b->CreateStore(from_data(o.data, a->local->type), a->local->slot);
        }
        gen_block(a->body);
        br_to(after);
    }
    g_b->SetInsertPoint(after);
    if (call->type == t_void) {
        // an arm may finish, and the program carries on
        g_b->CreateBr(ok);
    } else {
        g_b->CreateUnreachable();
    }
    g_b->SetInsertPoint(ok);
    return o.value;
}

// ---- Statements ----

static void gen_stmt(Stmt* s);

static void gen_block(Block* b) {
    for (Stmt* s : b->stmts) {
        if (terminated()) start_dead_block();
        gen_stmt(s);
    }
}

static void gen_loop_body(Block* body, llvm::BasicBlock* stop, llvm::BasicBlock* next) {
    g_fn->loops.push_back(LoopTarget{stop, next});
    gen_block(body);
    g_fn->loops.pop_back();
    br_to(next);
}

static void gen_stmt(Stmt* s) {
    switch (s->kind) {
    case StmtKind::Let:
    case StmtKind::Var:
    case StmtKind::Const: {
        llvm::Value* v = gen_expr(s->value);
        s->local->slot = entry_alloca(lt(s->local->type), s->name);
        g_b->CreateStore(v, s->local->slot);
        return;
    }
    case StmtKind::Assign: {
        llvm::Value* v = gen_expr(s->value);
        g_b->CreateStore(v, gen_place(s->target));
        return;
    }
    case StmtKind::Expr:
        gen_expr(s->value);
        return;
    case StmtKind::If: {
        llvm::BasicBlock* end = new_block("if.end");
        for (size_t i = 0; i < s->conds.size(); i++) {
            llvm::Value* c = gen_expr(s->conds[i]);
            llvm::BasicBlock* then = new_block("if.then");
            llvm::BasicBlock* other = new_block("if.else");
            g_b->CreateCondBr(c, then, other);
            g_b->SetInsertPoint(then);
            gen_block(s->blocks[i]);
            br_to(end);
            g_b->SetInsertPoint(other);
        }
        if (s->else_block) gen_block(s->else_block);
        br_to(end);
        g_b->SetInsertPoint(end);
        return;
    }
    case StmtKind::ForEver: {
        llvm::BasicBlock* body = new_block("for.body");
        llvm::BasicBlock* end = new_block("for.end");
        g_b->CreateBr(body);
        g_b->SetInsertPoint(body);
        gen_loop_body(s->blocks[0], end, body);
        g_b->SetInsertPoint(end);
        return;
    }
    case StmtKind::ForWhile: {
        llvm::BasicBlock* cond = new_block("for.cond");
        llvm::BasicBlock* body = new_block("for.body");
        llvm::BasicBlock* end = new_block("for.end");
        g_b->CreateBr(cond);
        g_b->SetInsertPoint(cond);
        g_b->CreateCondBr(gen_expr(s->conds[0]), body, end);
        g_b->SetInsertPoint(body);
        gen_loop_body(s->blocks[0], end, cond);
        g_b->SetInsertPoint(end);
        return;
    }
    case StmtKind::ForRange: {
        // both ends are worked out once; the loop stops at the last number without stepping past it
        Type* t = s->local->type;
        llvm::Value* from = gen_expr(s->conds[0]);
        llvm::Value* to = gen_expr(s->conds[1]);
        s->local->slot = entry_alloca(lt(t), s->name);
        g_b->CreateStore(from, s->local->slot);
        llvm::BasicBlock* body = new_block("for.body");
        llvm::BasicBlock* step = new_block("for.step");
        llvm::BasicBlock* end = new_block("for.end");
        llvm::Value* empty = t->is_signed ? g_b->CreateICmpSGT(from, to) : g_b->CreateICmpUGT(from, to);
        g_b->CreateCondBr(empty, end, body);
        g_b->SetInsertPoint(body);
        gen_loop_body(s->blocks[0], end, step);
        g_b->SetInsertPoint(step);
        llvm::Value* i = g_b->CreateLoad(lt(t), s->local->slot);
        llvm::BasicBlock* more = new_block("for.more");
        g_b->CreateCondBr(g_b->CreateICmpEQ(i, to), end, more);
        g_b->SetInsertPoint(more);
        g_b->CreateStore(g_b->CreateAdd(i, llvm::ConstantInt::get(lt(t), 1)), s->local->slot);
        g_b->CreateBr(body);
        g_b->SetInsertPoint(end);
        return;
    }
    case StmtKind::ForEach: {
        // for c in s.chars(): each character, a u32
        llvm::Value* str = gen_expr(s->value);
        llvm::Value* bytes = g_b->CreateExtractValue(str, 0);
        llvm::Value* len = g_b->CreateExtractValue(str, 1);
        llvm::AllocaInst* at = entry_alloca(i64_ty(), "at");
        g_b->CreateStore(i64_const(0), at);
        s->local->slot = entry_alloca(lt(t_u32), s->name);
        llvm::BasicBlock* cond = new_block("for.cond");
        llvm::BasicBlock* body = new_block("for.body");
        llvm::BasicBlock* end = new_block("for.end");
        g_b->CreateBr(cond);
        g_b->SetInsertPoint(cond);
        g_b->CreateCondBr(g_b->CreateICmpSLT(g_b->CreateLoad(i64_ty(), at), len), body, end);
        g_b->SetInsertPoint(body);
        auto fn = runtime_fn("g2_utf8_next", i32_ty(), {ptr_ty(), ptr_ty()});
        g_b->CreateStore(g_b->CreateCall(fn, {bytes, at}), s->local->slot);
        gen_loop_body(s->blocks[0], end, cond);
        g_b->SetInsertPoint(end);
        return;
    }
    case StmtKind::Match: {
        llvm::Value* v = gen_expr(s->value);
        llvm::Value* tag = g_b->CreateExtractValue(v, 0);
        llvm::BasicBlock* end = new_block("match.end");
        llvm::BasicBlock* other = new_block("match.else");
        llvm::SwitchInst* sw = g_b->CreateSwitch(tag, other);
        for (Arm* a : s->arms) {
            llvm::BasicBlock* bb = new_block("case." + a->name);
            sw->addCase((llvm::ConstantInt*)i32_const(a->index), bb);
            g_b->SetInsertPoint(bb);
            if (a->local) {
                a->local->slot = entry_alloca(lt(a->local->type), a->local->name);
                g_b->CreateStore(from_data(g_b->CreateExtractValue(v, 1), a->local->type), a->local->slot);
            }
            gen_block(a->body);
            br_to(end);
        }
        g_b->SetInsertPoint(other);
        if (s->else_block) {
            gen_block(s->else_block);
            br_to(end);
        } else {
            g_b->CreateUnreachable();
        }
        g_b->SetInsertPoint(end);
        return;
    }
    case StmtKind::Ret:
        gen_ret(s->value ? gen_expr(s->value) : nullptr);
        return;
    case StmtKind::Fail:
        gen_fail(s->reason_index, s->value ? gen_expr(s->value) : nullptr);
        return;
    case StmtKind::Stop:
        g_b->CreateBr(g_fn->loops.back().stop);
        return;
    case StmtKind::Next:
        g_b->CreateBr(g_fn->loops.back().next);
        return;
    }
}

// ---- Functions ----

static std::string symbol(Item* fn) {
    if (fn->kind == ItemKind::ExternFn) return fn->name;
    std::string s = fn->pkg->name + ".";
    if (!fn->recv_name.empty()) s += fn->recv_name + ".";
    return s + fn->name;
}

static void declare_fn(Item* fn) {
    std::vector<llvm::Type*> params;
    for (Param& p : fn->params) {
        bool by_pointer = p.local->kind == LocalKind::Self && p.is_var;
        params.push_back(by_pointer ? ptr_ty() : lt(p.local->type));
    }
    llvm::Type* ret = fn->has_fail_list ? (llvm::Type*)result_type(fn) : lt(fn->ret);
    auto* ft = llvm::FunctionType::get(ret, params, false);
    if (fn->kind == ItemKind::ExternFn) {
        fn->llvm_fn = llvm::cast<llvm::Function>(g_mod->getOrInsertFunction(fn->name, ft).getCallee());
        return;
    }
    fn->llvm_fn = llvm::Function::Create(ft, llvm::GlobalValue::InternalLinkage, symbol(fn), g_mod);
}

static void gen_fn(Item* fn) {
    FnGen fg;
    fg.fn = fn;
    fg.f = fn->llvm_fn;
    if (fn->has_fail_list) fg.result = (llvm::StructType*)fn->llvm_fn->getReturnType();
    g_fn = &fg;
    g_b->SetInsertPoint(llvm::BasicBlock::Create(*g_ctx, "entry", fn->llvm_fn));
    for (size_t i = 0; i < fn->params.size(); i++) {
        Param& p = fn->params[i];
        llvm::Value* arg = fn->llvm_fn->getArg((unsigned)i);
        if (p.local->kind == LocalKind::Self && p.is_var) {
            p.local->slot = arg;  // var self: the caller's own value
        } else {
            p.local->slot = entry_alloca(lt(p.local->type), p.name);
            g_b->CreateStore(arg, p.local->slot);
        }
    }
    gen_block(fn->body);
    if (!terminated()) {
        if (fn->ret == t_void) gen_ret(nullptr);
        else g_b->CreateUnreachable();
    }
    g_fn = nullptr;
}

// ---- The module ----

bool gen_program(Program* program, bool release, const std::string& obj_path, std::string& err) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::LLVMContext ctx;
    llvm::Module mod("go2", ctx);
    llvm::IRBuilder<> builder(ctx);
    g_ctx = &ctx;
    g_mod = &mod;
    g_b = &builder;
    g_release = release;

    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (!target) return false;
    llvm::TargetOptions options;
    llvm::TargetMachine* tm = target->createTargetMachine(triple, llvm::sys::getHostCPUName(), "", options,
                                                          llvm::Reloc::PIC_, std::nullopt, llvm::CodeGenOptLevel::Less);
    mod.setTargetTriple(triple);
    mod.setDataLayout(tm->createDataLayout());

    std::vector<Item*> fns;
    for (Package* pkg : program->packages) {
        for (auto& [name, it] : pkg->items) {
            if (it->kind == ItemKind::Fn || it->kind == ItemKind::ExternFn) fns.push_back(it);
        }
        for (Item* m : pkg->methods) fns.push_back(m);
    }
    for (Item* fn : fns) declare_fn(fn);
    for (Item* fn : fns) {
        if (fn->kind == ItemKind::Fn) gen_fn(fn);
    }

    // C's main calls GO2's main, and exits 0 at its end.
    Item* go2_main = program->packages[0]->items["main"];
    auto* main_ty = llvm::FunctionType::get(i32_ty(), {}, false);
    auto* c_main = llvm::Function::Create(main_ty, llvm::GlobalValue::ExternalLinkage, "main", &mod);
    builder.SetInsertPoint(llvm::BasicBlock::Create(ctx, "entry", c_main));
    builder.CreateCall(go2_main->llvm_fn);
    builder.CreateRet(i32_const(0));

    std::string verify;
    llvm::raw_string_ostream vs(verify);
    if (llvm::verifyModule(mod, &vs)) {
        if (std::getenv("G2C2_IR")) mod.print(llvm::errs(), nullptr);
        err = "internal error, G2C2 made invalid IR:\n" + verify;
        return false;
    }

    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;
    llvm::PassBuilder pb(tm);
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    llvm::ModulePassManager mpm = pb.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O1);
    mpm.run(mod, mam);
    if (std::getenv("G2C2_IR")) mod.print(llvm::errs(), nullptr);

    std::error_code ec;
    llvm::raw_fd_ostream out(obj_path, ec, llvm::sys::fs::OF_None);
    if (ec) {
        err = "cannot write " + obj_path + ": " + ec.message();
        return false;
    }
    llvm::legacy::PassManager pm;
    if (tm->addPassesToEmitFile(pm, out, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        err = "LLVM cannot write an object file for " + triple.str();
        return false;
    }
    pm.run(mod);
    out.flush();
    delete tm;
    return true;
}
