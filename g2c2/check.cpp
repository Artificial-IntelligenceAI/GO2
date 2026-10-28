// The checker: resolves every name, gives every expression its type, and enforces GO2's rules.
// It rewrites the tree in place (a Call of fmt.print becomes a Print, a Name becomes a LocalRef, ...),
// so gen.cpp only ever sees resolved kinds.

#include "g2c2.h"

static Type* make_type(TypeKind kind, int bits, bool is_signed, const char* name) {
    Type* t = new Type();
    t->kind = kind;
    t->bits = bits;
    t->is_signed = is_signed;
    t->name = name;
    return t;
}

Type* t_i8 = make_type(TypeKind::Int, 8, true, "i8");
Type* t_i16 = make_type(TypeKind::Int, 16, true, "i16");
Type* t_i32 = make_type(TypeKind::Int, 32, true, "i32");
Type* t_i64 = make_type(TypeKind::Int, 64, true, "i64");
Type* t_u8 = make_type(TypeKind::Int, 8, false, "u8");
Type* t_u16 = make_type(TypeKind::Int, 16, false, "u16");
Type* t_u32 = make_type(TypeKind::Int, 32, false, "u32");
Type* t_u64 = make_type(TypeKind::Int, 64, false, "u64");
Type* t_f32 = make_type(TypeKind::Float, 32, true, "f32");
Type* t_f64 = make_type(TypeKind::Float, 64, true, "f64");
Type* t_bool = make_type(TypeKind::Bool, 1, false, "bool");
Type* t_str = make_type(TypeKind::Str, 0, false, "str");
Type* t_strc = make_type(TypeKind::StrC, 0, false, "str.c");
Type* t_void = make_type(TypeKind::Void, 0, false, "nothing");

std::string type_name(Type* type) { return type->name; }
bool is_int(Type* type) { return type->kind == TypeKind::Int; }
bool is_float(Type* type) { return type->kind == TypeKind::Float; }
bool is_number(Type* type) { return is_int(type) || is_float(type); }

// ---- Context ----

struct Scope {
    std::map<std::string, Local*> names;
};

struct Ctx {
    Package* pkg = nullptr;
    FileAst* file = nullptr;
    Item* fn = nullptr;
    std::vector<Scope> scopes;
};

static Program* g_prog = nullptr;

static const Import* find_import(Ctx& c, const std::string& name) {
    for (const Import& imp : c.file->imports) {
        if (imp.name == name) return &imp;
    }
    return nullptr;
}

// ---- Expressions ----

static Type* check_expr(Ctx& c, Expr* e, Type* hint);

// fmt.print(format, values...): the format is split at each {} into pieces.
static void check_print(Ctx& c, Expr* e) {
    std::vector<Expr*> args(e->kids.begin() + 1, e->kids.end());
    if (args.empty() || args[0]->kind != ExprKind::Str) {
        error_at(e->pos, "`fmt.print` takes a string literal first; a format that is not a literal is not decided yet");
    }
    const std::string& f = args[0]->text;
    std::vector<std::string> pieces;
    std::string piece;
    for (size_t i = 0; i < f.size(); i++) {
        if (f[i] == '{' && i + 1 < f.size() && f[i + 1] == '{') {
            piece += '{';
            i++;
        } else if (f[i] == '{' && i + 1 < f.size() && f[i + 1] == '}') {
            pieces.push_back(piece);
            piece.clear();
            i++;
        } else if (f[i] == '{') {
            error_at(args[0]->pos, "in `fmt.print`, a `{` not followed by `}` or `{` is not decided yet");
        } else {
            piece += f[i];
        }
    }
    pieces.push_back(piece);
    size_t marks = pieces.size() - 1;
    if (marks != args.size() - 1) {
        error_at(e->pos, "`fmt.print` has " + std::to_string(marks) + " `{}` and " + std::to_string(args.size() - 1) +
                             " values; a mismatch is not decided yet");
    }
    std::vector<Expr*> values;
    for (size_t i = 1; i < args.size(); i++) {
        Type* t = check_expr(c, args[i], nullptr);
        if (t->kind != TypeKind::Str) {
            error_at(args[i]->pos, "G2C2 does not print a " + type_name(t) + " yet");
        }
        values.push_back(args[i]);
    }
    e->kind = ExprKind::Print;
    e->kids = values;
    e->pieces = pieces;
    e->type = t_void;
}

static Type* check_call(Ctx& c, Expr* e, Type* hint) {
    Expr* callee = e->kids[0];
    if (callee->kind == ExprKind::Field && callee->kids[0]->kind == ExprKind::Name) {
        const std::string& head = callee->kids[0]->text;
        const Import* imp = find_import(c, head);
        if (imp && imp->path == "fmt") {
            if (callee->text != "print") error_at(callee->pos, "`fmt` has no `" + callee->text + "`");
            check_print(c, e);
            return e->type;
        }
    }
    error_at(callee->pos, "G2C2 cannot call this yet");
}

static Type* check_expr(Ctx& c, Expr* e, Type* hint) {
    switch (e->kind) {
    case ExprKind::Str:
        e->type = t_str;
        return e->type;
    case ExprKind::Call:
        return check_call(c, e, hint);
    default:
        error_at(e->pos, "G2C2 does not build this expression yet");
    }
}

// ---- Statements ----

static void check_block(Ctx& c, Block* b);

static void check_stmt(Ctx& c, Stmt* s) {
    switch (s->kind) {
    case StmtKind::Expr: {
        Expr* e = s->value;
        if (e->kind != ExprKind::Call) {
            error_at(e->pos, "only a call stands as a statement");
        }
        check_expr(c, e, nullptr);
        return;
    }
    default:
        error_at(s->pos, "G2C2 does not build this statement yet");
    }
}

static void check_block(Ctx& c, Block* b) {
    c.scopes.push_back(Scope());
    for (Stmt* s : b->stmts) check_stmt(c, s);
    c.scopes.pop_back();
}

// ---- Packages ----

static void check_fn(Package* pkg, FileAst* file, Item* fn) {
    Ctx c;
    c.pkg = pkg;
    c.file = file;
    c.fn = fn;
    fn->ret = t_void;
    check_block(c, fn->body);
}

void check_program(Program* program) {
    g_prog = program;
    for (Package* pkg : program->packages) {
        for (FileAst* f : pkg->files) {
            for (Item* it : f->items) {
                it->pkg = pkg;
                pkg->items[it->name] = it;
            }
        }
    }
    Package* main_pkg = program->packages[0];
    auto found = main_pkg->items.find("main");
    if (found == main_pkg->items.end() || found->second->kind != ItemKind::Fn) {
        error_at(Pos{main_pkg->files[0]->file, 0, 0}, "package main has no `fn main[]`");
    }
    for (Package* pkg : program->packages) {
        for (FileAst* f : pkg->files) {
            for (Item* it : f->items) {
                if (it->kind == ItemKind::Fn) check_fn(pkg, f, it);
            }
        }
    }
}
