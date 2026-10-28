// The generator: the checked tree into LLVM IR, optimised at -O1 and written as an object file.

#include "g2c2.h"

#include <llvm/IR/IRBuilder.h>
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

// ---- Types ----

static llvm::Type* lt(Type* t) {
    if (t->llvm) return t->llvm;
    switch (t->kind) {
    case TypeKind::Str:
        t->llvm = llvm::StructType::create(*g_ctx, {llvm::PointerType::get(*g_ctx, 0), llvm::Type::getInt64Ty(*g_ctx)}, "str");
        break;
    case TypeKind::Void:
        t->llvm = llvm::Type::getVoidTy(*g_ctx);
        break;
    default:
        std::abort();
    }
    return t->llvm;
}

// ---- Runtime functions ----

static llvm::FunctionCallee runtime_fn(const char* name, llvm::Type* ret, std::vector<llvm::Type*> params) {
    return g_mod->getOrInsertFunction(name, llvm::FunctionType::get(ret, params, false));
}

static llvm::Type* i64_ty() { return llvm::Type::getInt64Ty(*g_ctx); }
static llvm::Type* ptr_ty() { return llvm::PointerType::get(*g_ctx, 0); }
static llvm::Type* void_ty() { return llvm::Type::getVoidTy(*g_ctx); }

// A string literal: its bytes and a NUL, so str.c of it needs no copy.
static llvm::Value* gen_str_lit(const std::string& bytes) {
    llvm::Constant* data = llvm::ConstantDataArray::getString(*g_ctx, bytes, true);
    auto* gv = new llvm::GlobalVariable(*g_mod, data->getType(), true, llvm::GlobalValue::PrivateLinkage, data, "str");
    gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
    return llvm::ConstantStruct::get((llvm::StructType*)lt(t_str), {gv, llvm::ConstantInt::get(i64_ty(), bytes.size())});
}

static void print_bytes(llvm::Value* s) {
    auto fn = runtime_fn("g2_print_str", void_ty(), {ptr_ty(), i64_ty()});
    g_b->CreateCall(fn, {g_b->CreateExtractValue(s, 0), g_b->CreateExtractValue(s, 1)});
}

// ---- Expressions ----

static llvm::Value* gen_expr(Expr* e) {
    switch (e->kind) {
    case ExprKind::Str:
        return gen_str_lit(e->text);
    case ExprKind::Print: {
        for (size_t i = 0; i < e->pieces.size(); i++) {
            if (!e->pieces[i].empty()) print_bytes(gen_str_lit(e->pieces[i]));
            if (i < e->kids.size()) print_bytes(gen_expr(e->kids[i]));
        }
        return nullptr;
    }
    default:
        std::abort();
    }
}

// ---- Statements ----

static void gen_block(Block* b);

static void gen_stmt(Stmt* s) {
    switch (s->kind) {
    case StmtKind::Expr:
        gen_expr(s->value);
        return;
    default:
        std::abort();
    }
}

static void gen_block(Block* b) {
    for (Stmt* s : b->stmts) gen_stmt(s);
}

// ---- Functions ----

static std::string symbol(Item* fn) {
    if (fn->kind == ItemKind::ExternFn) return fn->name;
    std::string s = fn->pkg->name + ".";
    if (!fn->recv_name.empty()) s += fn->recv_name + ".";
    return s + fn->name;
}

static void declare_fn(Item* fn) {
    auto* ft = llvm::FunctionType::get(lt(fn->ret), {}, false);
    fn->llvm_fn = llvm::Function::Create(ft, llvm::GlobalValue::InternalLinkage, symbol(fn), g_mod);
}

static void gen_fn(Item* fn) {
    auto* entry = llvm::BasicBlock::Create(*g_ctx, "entry", fn->llvm_fn);
    g_b->SetInsertPoint(entry);
    gen_block(fn->body);
    g_b->CreateRetVoid();
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
        for (FileAst* f : pkg->files) {
            for (Item* it : f->items) {
                if (it->kind == ItemKind::Fn) fns.push_back(it);
            }
        }
    }
    for (Item* fn : fns) declare_fn(fn);
    for (Item* fn : fns) gen_fn(fn);

    // C's main calls GO2's main, and exits 0 at its end.
    Item* go2_main = program->packages[0]->items["main"];
    auto* main_ty = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), {}, false);
    auto* c_main = llvm::Function::Create(main_ty, llvm::GlobalValue::ExternalLinkage, "main", &mod);
    builder.SetInsertPoint(llvm::BasicBlock::Create(ctx, "entry", c_main));
    builder.CreateCall(go2_main->llvm_fn);
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));

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
