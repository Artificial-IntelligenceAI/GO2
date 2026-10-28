// The checker: resolves every name, gives every expression its type, and enforces GO2's rules.
// It rewrites the tree in place (a Call of fmt.print becomes a Print, a Name becomes a LocalRef, ...),
// so gen.cpp only ever sees resolved kinds.

#include "g2c2.h"

#include <cmath>
#include <cstdlib>

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

static Reason* make_reason(const char* name) {
    Reason* r = new Reason();
    r->name = name;
    return r;
}

Reason* r_out_of_range = make_reason("out_of_range");
Reason* r_not_a_number = make_reason("not_a_number");

std::string type_name(Type* type) { return type->name; }
bool is_int(Type* type) { return type->kind == TypeKind::Int; }
bool is_float(Type* type) { return type->kind == TypeKind::Float; }
bool is_number(Type* type) { return is_int(type) || is_float(type); }

static std::string q(const std::string& s) { return "`" + s + "`"; }

// "an i64", "a u8", "a Token": a type's name with its article, for messages.
static std::string an(Type* t) {
    char c = t->name[0];
    bool vowel = c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'A' || c == 'E' || c == 'I' || c == 'O' ||
                 t == t_f32 || t == t_f64;
    return (vowel ? "an " : "a ") + t->name;
}

// ---- Context ----

struct Scope {
    std::map<std::string, Local*> names;
};

struct Ctx {
    Package* pkg = nullptr;
    FileAst* file = nullptr;
    Item* fn = nullptr;
    std::vector<Scope> scopes;
    int loops = 0;               // how many fors we are inside
    std::vector<Local*> vars;    // every var declared, to check each changes
};


static const Import* find_import(FileAst* file, const std::string& name) {
    for (const Import& imp : file->imports) {
        if (imp.name == name) return &imp;
    }
    return nullptr;
}

static Local* find_local(Ctx& c, const std::string& name) {
    for (size_t i = c.scopes.size(); i > 0; i--) {
        auto it = c.scopes[i - 1].names.find(name);
        if (it != c.scopes[i - 1].names.end()) return it->second;
    }
    return nullptr;
}

static Item* find_item(Package* pkg, const std::string& name) {
    auto it = pkg->items.find(name);
    return it == pkg->items.end() ? nullptr : it->second;
}

static Local* declare(Ctx& c, const std::string& name, Pos pos, LocalKind kind, bool is_var, Type* type) {
    if (find_local(c, name) || find_item(c.pkg, name) || find_import(c.file, name)) {
        error_at(pos, q(name) + " is already a name here; reusing a name (shadowing) is not decided yet");
    }
    Local* l = new Local();
    l->name = name;
    l->pos = pos;
    l->kind = kind;
    l->is_var = is_var;
    l->type = type;
    c.scopes.back().names[name] = l;
    if (is_var) c.vars.push_back(l);
    return l;
}

// ---- Types ----

static Type* builtin_type(const std::string& name) {
    static Type** all[] = {&t_i8, &t_i16, &t_i32, &t_i64, &t_u8, &t_u16, &t_u32, &t_u64,
                           &t_f32, &t_f64, &t_bool, &t_str, &t_strc};
    for (Type** t : all) {
        if ((*t)->name == name) return *t;
    }
    return nullptr;
}

static bool is_type_item(Item* it) {
    return it->kind == ItemKind::Struct || it->kind == ItemKind::Enum || it->kind == ItemKind::ExternType;
}

// The package an import names, or null for fmt and os.
static Package* import_pkg(FileAst* file, const std::string& name, Pos pos) {
    const Import* imp = find_import(file, name);
    if (!imp) return nullptr;
    if (!imp->pkg) error_at(pos, q(name) + " has no types");
    return imp->pkg;
}

static Item* pub_item(Package* pkg, const std::string& name, Pos pos) {
    Item* it = find_item(pkg, name);
    if (!it) error_at(pos, "package " + q(pkg->name) + " has no " + q(name));
    if (!it->is_pub) error_at(pos, q(pkg->name + "." + name) + " is not `pub`, so it stays in its package");
    return it;
}

static Type* resolve_type(Package* pkg, FileAst* file, TypeRef* t) {
    if (t->pkg.empty()) {
        if (Type* b = builtin_type(t->name)) return b;
        Item* it = find_item(pkg, t->name);
        if (!it || !is_type_item(it)) error_at(t->pos, "there is no type " + q(t->name));
        return it->type;
    }
    if (!find_import(file, t->pkg)) error_at(t->pos, "there is no package " + q(t->pkg) + " imported here");
    Package* p = import_pkg(file, t->pkg, t->pos);
    Item* it = pub_item(p, t->name, t->pos);
    if (!is_type_item(it)) error_at(t->pos, q(t->pkg + "." + t->name) + " is not a type");
    return it->type;
}

// If e names a type (Token, lexer.Token), its item; else null.
static Item* type_named(Ctx& c, Expr* e) {
    if (e->kind == ExprKind::Name) {
        if (find_local(c, e->text)) return nullptr;
        Item* it = find_item(c.pkg, e->text);
        return it && is_type_item(it) ? it : nullptr;
    }
    if (e->kind == ExprKind::Field && e->kids[0]->kind == ExprKind::Name && !find_local(c, e->kids[0]->text)) {
        const Import* imp = find_import(c.file, e->kids[0]->text);
        if (!imp || !imp->pkg) return nullptr;
        Item* it = pub_item(imp->pkg, e->text, e->pos);
        return is_type_item(it) ? it : nullptr;
    }
    return nullptr;
}

// ---- Literals ----

// Whether the integer literal (value, negative) fits in t.
static bool int_fits(uint64_t value, bool negative, Type* t) {
    if (t->is_signed) {
        uint64_t limit = (uint64_t)1 << (t->bits - 1);
        return negative ? value <= limit : value < limit;
    }
    if (negative) return value == 0;
    return t->bits == 64 || value < ((uint64_t)1 << t->bits);
}

// A literal, or arithmetic of literals: its type comes from what it meets.
static bool is_untyped(Expr* e) {
    switch (e->kind) {
    case ExprKind::Int:
    case ExprKind::Float:
        return true;
    case ExprKind::Paren:
        return is_untyped(e->kids[0]);
    case ExprKind::Unary:
        return e->text == "-" && is_untyped(e->kids[0]);
    case ExprKind::Binary:
        return is_untyped(e->kids[0]) && is_untyped(e->kids[1]);
    default:
        return false;
    }
}

static std::string int_text(Expr* e) { return (e->negative ? "-" : "") + e->text; }

static Type* check_int_lit(Expr* e, Type* hint) {
    Type* t = hint && is_int(hint) ? hint : t_i64;
    if (hint && is_float(hint)) {
        error_at(e->pos, "the integer " + int_text(e) + " meets " + an(hint) +
                             "; an integer literal taking a float type is not decided yet (write " + int_text(e) + ".0)");
    }
    if (!int_fits(e->value, e->negative, t)) {
        error_at(e->pos, int_text(e) + " does not fit in " + t->name + "; a literal that does not fit is not decided yet");
    }
    e->type = t;
    return t;
}

static Type* check_float_lit(Expr* e, Type* hint) {
    Type* t = hint && is_float(hint) ? hint : t_f64;
    std::string digits;
    for (char ch : e->text) {
        if (ch != '_') digits += ch;
    }
    double v = std::strtod(digits.c_str(), nullptr);
    bool too_big = std::isinf(v) || (t == t_f32 && std::fabs(v) > 3.4028234663852886e38);
    if (too_big) {
        error_at(e->pos, e->text + " does not fit in " + t->name + "; a literal that does not fit is not decided yet");
    }
    e->type = t;
    return t;
}

// ---- Places: what can be changed ----

// The local at the root of a place: a local, or fields of one. Null if e is not a place.
static Local* place_root(Expr* e) {
    if (e->kind == ExprKind::LocalRef) return e->local;
    if (e->kind == ExprKind::FieldGet) return place_root(e->kids[0]);
    return nullptr;
}

static std::string place_text(Expr* e) {
    if (e->kind == ExprKind::LocalRef) return e->local->name;
    if (e->kind == ExprKind::FieldGet) return place_text(e->kids[0]) + "." + e->text;
    return "this";
}

// Checks that the place e may change, and marks it changed. what: "changed", or "used by a var self method".
static void change_place(Expr* e, Pos pos, const std::string& what) {
    Local* root = place_root(e);
    if (!root) error_at(pos, "only a variable, or a field of one, can be " + what);
    if (!root->is_var) {
        switch (root->kind) {
        case LocalKind::Loop: error_at(pos, q(root->name) + " is a loop's name, a let; it cannot be " + what);
        case LocalKind::Binding: error_at(pos, q(root->name) + " names a case's data, a let; it cannot be " + what);
        case LocalKind::Const: error_at(pos, q(root->name) + " is a const; it cannot be " + what);
        case LocalKind::Self: error_at(pos, "this method has `let self`; `self` cannot be " + what + " (make it `var self`)");
        default: error_at(pos, q(root->name) + " is a let; it cannot be " + what + " (make it `var`)");
        }
    }
    for (Expr* f = e; f->kind == ExprKind::FieldGet; f = f->kids[0]) {
        Field& field = f->kids[0]->type->item->fields[f->index];
        if (!field.is_var) {
            error_at(pos, q(place_text(f)) + " is a let field, set only when the struct is built; it cannot be " + what);
        }
        field.changed = true;
    }
    root->changed = true;
}

// ---- Expressions ----

static Type* check_expr(Ctx& c, Expr* e, Type* hint);
static void check_block(Ctx& c, Block* b);

static void want_type(Expr* e, Type* got, Type* want, const std::string& what) {
    if (got != want) error_at(e->pos, what + " is " + an(want) + ", but this is " + an(got));
}

static void must_not_fail(Expr* e) {
    if (e->kind == ExprKind::FnCall || e->kind == ExprKind::MethodCall) {
        if (!e->reasons.empty() && !e->handled) {
            error_at(e->pos, q(e->item->name) + " can fail; a call to it stands alone and says what happens, as in "
                             "`let x = " + e->item->name + "(…) ?? 0;`");
        }
    }
}

// Checks a value the caller will use: never nothing, never a call left unhandled.
static Type* check_value(Ctx& c, Expr* e, Type* hint) {
    Type* t = check_expr(c, e, hint);
    if (t == t_void) error_at(e->pos, "this gives nothing, so it has no value to use");
    must_not_fail(e);
    return t;
}

static void check_args(Ctx& c, Expr* call, const std::vector<Param>& params, size_t first, const std::string& name) {
    size_t want = params.size() - first;
    size_t got = call->kids.size() - 1;
    if (want != got) {
        error_at(call->pos, q(name) + " takes " + std::to_string(want) + " argument" + (want == 1 ? "" : "s") + ", but is given " +
                                std::to_string(got));
    }
    for (size_t i = 0; i < got; i++) {
        Expr* a = call->kids[i + 1];
        Type* pt = params[first + i].local->type;
        Type* t = check_value(c, a, pt);
        want_type(a, t, pt, "parameter " + q(params[first + i].name));
    }
}

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
        Type* t = check_value(c, args[i], nullptr);
        if (t->kind != TypeKind::Str && t->kind != TypeKind::Int) {
            error_at(args[i]->pos, "how `{}` prints " + an(t) + " is not decided yet");
        }
        values.push_back(args[i]);
    }
    e->kind = ExprKind::Print;
    e->kids = values;
    e->pieces = pieces;
    e->type = t_void;
}

// os.exit(code)
static void check_exit(Ctx& c, Expr* e) {
    if (e->kids.size() != 2) error_at(e->pos, "`os.exit` takes one exit code");
    Expr* a = e->kids[1];
    if (a->kind != ExprKind::Int || a->negative || a->value > 255) {
        error_at(a->pos, "`os.exit` takes a code from 0 to 255 written as a number; the type of its parameter is not decided yet");
    }
    a->type = t_i32;
    e->kind = ExprKind::Exit;
    e->kids = {a};
    e->type = t_void;
}

// T(x): a conversion.
static Type* check_conv(Ctx& c, Expr* e) {
    const std::string& name = e->kids[0]->text;
    if (e->kids.size() != 2) error_at(e->pos, "a conversion takes one value: " + q(name + "(x)"));
    Expr* a = e->kids[1];
    if (name == "str") error_at(e->pos, "`str(…)` needs the garbage collector, which G2C2 does not have yet");
    if (name == "ls" || name == "map") error_at(e->pos, q(name) + " needs the garbage collector, which G2C2 does not have yet");
    Type* to = builtin_type(name);
    if (name == "str.c") {
        Type* t = check_value(c, a, nullptr);
        want_type(a, t, t_str, "`str.c(…)` takes a str; it");
        e->kind = ExprKind::StrC;
        e->kids = {a};
        e->type = t_strc;
        return t_strc;
    }
    Type* from = check_value(c, a, nullptr);
    e->kind = ExprKind::Conv;
    e->kids = {a};
    e->type = to;
    e->can_fail = false;
    e->reasons.clear();
    if (is_number(to) && is_number(from)) {
        if (is_int(to) && is_int(from)) {
            // fails when the source's range is not inside the target's
            bool fits = from == to || (from->is_signed == to->is_signed && from->bits <= to->bits) ||
                        (!from->is_signed && to->is_signed && from->bits < to->bits);
            e->can_fail = !fits;
        } else if (is_int(to)) {
            e->can_fail = true;
        }
        if (e->can_fail) e->reasons = {r_out_of_range};
        return to;
    }
    if (is_int(to) && from == t_str) {
        e->can_fail = true;
        e->reasons = {r_not_a_number, r_out_of_range};
        if (!e->handled) {
            error_at(e->pos, "text to a number can fail (`not_a_number`, `out_of_range`) and must be handled, as in "
                             "`let n = " + name + "(text) ?? 0;`");
        }
        return to;
    }
    if (is_float(to) && from == t_str) error_at(e->pos, "text to a float is not decided yet");
    if (to == t_bool || from == t_bool) error_at(e->pos, "converting between bool and numbers is not decided yet");
    error_at(e->pos, "there is no conversion from " + from->name + " to " + to->name);
}

static Type* check_fn_call(Ctx& c, Expr* e, Item* fn) {
    if (fn->kind != ItemKind::Fn && fn->kind != ItemKind::ExternFn) {
        if (fn->kind == ItemKind::Struct) error_at(e->pos, "a struct is built with `[ ]`: " + q(fn->name + "[…]"));
        error_at(e->pos, q(fn->name) + " is not a function");
    }
    if (!fn->recv_name.empty()) error_at(e->pos, q(fn->name) + " is a method, called on a value: `v." + fn->name + "(…)`");
    check_args(c, e, fn->params, 0, fn->name);
    e->kind = ExprKind::FnCall;
    e->item = fn;
    e->kids.erase(e->kids.begin());
    e->type = fn->ret;
    e->reasons = fn->fails;
    return e->type;
}

static Type* check_method_call(Ctx& c, Expr* e) {
    Expr* callee = e->kids[0];
    Expr* base = callee->kids[0];
    Type* bt = check_value(c, base, nullptr);
    const std::string& name = callee->text;
    if (bt == t_str) {
        if (name == "len") {
            if (e->kids.size() != 1) error_at(e->pos, "`len()` takes nothing");
            e->kind = ExprKind::StrLen;
            e->kids = {base};
            e->type = t_i64;
            return t_i64;
        }
        if (name == "chars") error_at(e->pos, "`s.chars()` is walked by a `for`: `for c in s.chars() { … }`");
        error_at(callee->pos, "a str has no method " + q(name));
    }
    if (!bt->item) {
        error_at(callee->pos, an(bt) + " has no method " + q(name));
    }
    auto it = bt->item->methods.find(name);
    if (it == bt->item->methods.end()) {
        bool is_field = false;
        for (Field& f : bt->item->fields) is_field = is_field || f.name == name;
        if (is_field) error_at(callee->pos, q(name) + " is a field, read without `()`");
        error_at(callee->pos, bt->name + " has no method " + q(name));
    }
    Item* m = it->second;
    if (m->self_var) change_place(base, base->pos, "used by " + q(name) + ", a `var self` method");
    check_args(c, e, m->params, 1, name);
    e->kind = ExprKind::MethodCall;
    e->item = m;
    e->kids[0] = base;
    e->type = m->ret;
    e->reasons = m->fails;
    return e->type;
}

static Type* check_call(Ctx& c, Expr* e) {
    Expr* callee = e->kids[0];
    if (callee->kind == ExprKind::TypeName) return check_conv(c, e);
    if (callee->kind == ExprKind::Name) {
        if (find_local(c, callee->text)) error_at(callee->pos, q(callee->text) + " is a variable, not a function");
        Item* it = find_item(c.pkg, callee->text);
        if (!it) {
            if (find_import(c.file, callee->text)) error_at(callee->pos, q(callee->text) + " is a package");
            error_at(callee->pos, "there is no " + q(callee->text));
        }
        return check_fn_call(c, e, it);
    }
    if (callee->kind == ExprKind::Field && callee->kids[0]->kind == ExprKind::Name && !find_local(c, callee->kids[0]->text)) {
        const std::string& head = callee->kids[0]->text;
        const Import* imp = find_import(c.file, head);
        if (imp && imp->path == "fmt") {
            if (callee->text != "print") error_at(callee->pos, "`fmt` has no " + q(callee->text));
            check_print(c, e);
            return t_void;
        }
        if (imp && imp->path == "os") {
            if (callee->text != "exit") error_at(callee->pos, "`os` has no " + q(callee->text));
            check_exit(c, e);
            return t_void;
        }
        if (imp) return check_fn_call(c, e, pub_item(imp->pkg, callee->text, callee->pos));
        if (Item* t = type_named(c, callee->kids[0])) {
            error_at(callee->pos, "a method is called on a value of " + q(t->name) + ", not on the type");
        }
        if (!find_item(c.pkg, head) && (head == "fmt" || head == "os")) {
            error_at(callee->kids[0]->pos, q(head) + " is used but not imported: `import \"" + head + "\";`");
        }
    }
    if (callee->kind == ExprKind::Field) return check_method_call(c, e);
    error_at(callee->pos, "this cannot be called");
}

// Token.plus, or a field read p.x
static Type* check_field(Ctx& c, Expr* e) {
    Expr* base = e->kids[0];
    if (Item* t = type_named(c, base)) {
        if (t->kind != ItemKind::Enum) error_at(e->pos, q(t->name) + " is a type; its fields are read from a value");
        for (size_t i = 0; i < t->cases.size(); i++) {
            if (t->cases[i].name == e->text) {
                if (t->cases[i].type) {
                    error_at(e->pos, q(t->name + "." + e->text) + " carries " + an(t->cases[i].type) + ": " +
                                         q(t->name + "." + e->text + "[…]"));
                }
                e->kind = ExprKind::EnumLit;
                e->item = t;
                e->index = (int)i;
                e->kids.clear();
                e->type = t->type;
                return e->type;
            }
        }
        error_at(e->pos, q(t->name) + " has no case " + q(e->text));
    }
    if (base->kind == ExprKind::Name && !find_local(c, base->text)) {
        const Import* imp = find_import(c.file, base->text);
        if (imp && imp->pkg) {
            Item* it = pub_item(imp->pkg, e->text, e->pos);
            if (it->kind == ItemKind::Fn || it->kind == ItemKind::ExternFn) error_at(e->pos, q(base->text + "." + e->text) + " is a function, called with `( )`");
            error_at(e->pos, q(base->text + "." + e->text) + " is not a value");
        }
        if (imp) error_at(e->pos, q(base->text + "." + e->text) + " is not a value");
    }
    Type* bt = check_value(c, base, nullptr);
    if (bt->kind != TypeKind::Struct) error_at(e->pos, an(bt) + " has no field " + q(e->text));
    Item* s = bt->item;
    for (size_t i = 0; i < s->fields.size(); i++) {
        if (s->fields[i].name == e->text) {
            e->kind = ExprKind::FieldGet;
            e->index = (int)i;
            e->type = s->fields[i].type;
            return e->type;
        }
    }
    if (s->methods.count(e->text)) error_at(e->pos, q(e->text) + " is a method, called with `( )`");
    error_at(e->pos, q(s->name) + " has no field " + q(e->text));
}

// Point[x = 1, y = 2], Token.number[42], s[i]
static Type* check_bracket(Ctx& c, Expr* e) {
    Expr* base = e->kids[0];
    std::vector<Expr*> items(e->kids.begin() + 1, e->kids.end());
    if (Item* t = type_named(c, base)) {
        if (t->kind != ItemKind::Struct) error_at(e->pos, q(t->name) + " is not a struct, so it is not built with `[ ]`");
        std::vector<Expr*> ordered(t->fields.size(), nullptr);
        for (size_t i = 0; i < items.size(); i++) {
            const std::string& fname = e->names[i];
            if (fname.empty()) error_at(items[i]->pos, "a struct is built field by name: " + q(t->name + "[x = …]"));
            int index = -1;
            for (size_t k = 0; k < t->fields.size(); k++) {
                if (t->fields[k].name == fname) index = (int)k;
            }
            if (index < 0) error_at(e->name_pos[i], q(t->name) + " has no field " + q(fname));
            if (ordered[index]) error_at(e->name_pos[i], q(fname) + " is given twice");
            Type* ft = t->fields[index].type;
            Type* got = check_value(c, items[i], ft);
            want_type(items[i], got, ft, "field " + q(fname));
            ordered[index] = items[i];
        }
        for (size_t k = 0; k < t->fields.size(); k++) {
            if (!ordered[k]) error_at(e->pos, "building " + q(t->name) + " needs every field; " + q(t->fields[k].name) + " is missing");
        }
        e->kind = ExprKind::StructLit;
        e->item = t;
        e->kids = ordered;
        e->type = t->type;
        return e->type;
    }
    if (base->kind == ExprKind::Field) {
        if (Item* t = type_named(c, base->kids[0])) {
            if (t->kind != ItemKind::Enum) error_at(e->pos, q(t->name) + " is not an enum");
            for (size_t i = 0; i < t->cases.size(); i++) {
                if (t->cases[i].name != base->text) continue;
                Type* ct = t->cases[i].type;
                if (!ct) error_at(e->pos, q(t->name + "." + base->text) + " carries nothing: " + q(t->name + "." + base->text));
                if (items.size() != 1 || !e->names[0].empty()) error_at(e->pos, q(t->name + "." + base->text) + " takes one value");
                Type* got = check_value(c, items[0], ct);
                want_type(items[0], got, ct, q(t->name + "." + base->text) + "'s data");
                e->kind = ExprKind::EnumLit;
                e->item = t;
                e->index = (int)i;
                e->kids = {items[0]};
                e->type = t->type;
                return e->type;
            }
            error_at(base->pos, q(t->name) + " has no case " + q(base->text));
        }
    }
    Type* bt = check_value(c, base, nullptr);
    if (bt == t_str) {
        if (items.size() != 1 || !e->names[0].empty()) error_at(e->pos, "a str is indexed by one position: `s[i]`");
        Type* it = check_value(c, items[0], t_i64);
        if (it != t_i64) error_at(items[0]->pos, "indexing a str by " + an(it) + " is not decided yet; use an i64");
        if (e->handled) error_at(e->pos, "handling `s[i]` out of range with `??` is not decided yet");
        e->kind = ExprKind::StrIndex;
        e->kids = {base, items[0]};
        e->type = t_u8;
        return t_u8;
    }
    error_at(e->pos, an(bt) + " cannot be indexed");
}

static Type* check_binary(Ctx& c, Expr* e, Type* hint) {
    Expr* a = e->kids[0];
    Expr* b = e->kids[1];
    Type* h = hint && is_number(hint) ? hint : nullptr;
    Type* ta;
    Type* tb;
    if (is_untyped(a) && !is_untyped(b)) {
        tb = check_value(c, b, h);
        ta = check_value(c, a, tb);
    } else {
        ta = check_value(c, a, h);
        tb = check_value(c, b, ta);
    }
    const std::string& op = e->text;
    if (ta != tb) error_at(e->pos, "both sides of " + q(op) + " have one type, but these are " + ta->name + " and " + tb->name);
    if (ta == t_str && op == "+") error_at(e->pos, "strings are joined with `concat`, not `+`");
    if (!is_number(ta)) error_at(e->pos, q(op) + " works on numbers, not on " + an(ta));
    if (op == "%" && is_float(ta)) error_at(e->pos, "there is no `%` on floats");
    e->type = ta;
    return ta;
}

static Type* check_compare(Ctx& c, Expr* e) {
    // the first operand with a type of its own gives literals theirs
    size_t first = 0;
    while (first < e->kids.size() && is_untyped(e->kids[first])) first++;
    if (first == e->kids.size()) first = 0;
    Type* t = check_value(c, e->kids[first], nullptr);
    for (size_t i = 0; i < e->kids.size(); i++) {
        if (i == first) continue;
        Type* ti = check_value(c, e->kids[i], t);
        if (ti != t) error_at(e->kids[i]->pos, "a comparison is between values of one type, but these are " + t->name + " and " + ti->name);
    }
    for (const std::string& op : e->ops) {
        if (t == t_strc) error_at(e->pos, "comparing str.c values is not decided yet");
        if (op != "==" && op != "!=" && !is_number(t) && t != t_str) {
            error_at(e->pos, q(op) + " works on numbers and str, not on " + an(t));
        }
    }
    e->type = t_bool;
    return t_bool;
}

static Type* check_expr(Ctx& c, Expr* e, Type* hint) {
    switch (e->kind) {
    case ExprKind::Int:
        return check_int_lit(e, hint);
    case ExprKind::Float:
        return check_float_lit(e, hint);
    case ExprKind::Str:
        e->type = t_str;
        return t_str;
    case ExprKind::Bool:
        e->type = t_bool;
        return t_bool;
    case ExprKind::Name: {
        if (Local* l = find_local(c, e->text)) {
            e->kind = ExprKind::LocalRef;
            e->local = l;
            e->type = l->type;
            return e->type;
        }
        if (Item* it = find_item(c.pkg, e->text)) {
            if (it->kind == ItemKind::Fn || it->kind == ItemKind::ExternFn) error_at(e->pos, q(e->text) + " is a function, called with `( )`");
            if (it->kind == ItemKind::Const) error_at(e->pos, "a const outside a function is not decided yet");
            error_at(e->pos, q(e->text) + " is a type, not a value");
        }
        if (find_import(c.file, e->text)) error_at(e->pos, q(e->text) + " is a package, not a value");
        error_at(e->pos, "there is no " + q(e->text));
    }
    case ExprKind::SelfRef: {
        Local* l = find_local(c, "self");
        if (!l) error_at(e->pos, "`self` is only in a method");
        e->kind = ExprKind::LocalRef;
        e->local = l;
        e->type = l->type;
        return e->type;
    }
    case ExprKind::Paren: {
        Type* t = check_expr(c, e->kids[0], hint);
        must_not_fail(e->kids[0]);
        e->type = t;
        return t;
    }
    case ExprKind::Unary: {
        if (e->text == "!") {
            Type* t = check_value(c, e->kids[0], t_bool);
            if (t != t_bool) error_at(e->pos, "`!` works on a bool, not on " + an(t));
            e->type = t_bool;
            return t_bool;
        }
        Type* t = check_value(c, e->kids[0], hint && is_number(hint) ? hint : nullptr);
        if (!is_number(t)) error_at(e->pos, "`-` works on numbers, not on " + an(t));
        e->type = t;
        return t;
    }
    case ExprKind::Binary:
        return check_binary(c, e, hint);
    case ExprKind::Compare:
        return check_compare(c, e);
    case ExprKind::Logic:
        for (Expr* k : e->kids) {
            Type* t = check_value(c, k, t_bool);
            if (t != t_bool) error_at(k->pos, q(e->text) + " works on bools, but this is " + an(t));
        }
        e->type = t_bool;
        return t_bool;
    case ExprKind::Call:
        return check_call(c, e);
    case ExprKind::Field:
        return check_field(c, e);
    case ExprKind::Bracket:
        return check_bracket(c, e);
    case ExprKind::TypeName:
        error_at(e->pos, q(e->text) + " is a type; converting to it is " + q(e->text + "(x)"));
    case ExprKind::Handle:
        error_at(e->pos, "a call with `??` stands alone, as a statement or a declaration's whole value");
    default:
        error_at(e->pos, "internal error: this expression is checked twice");
    }
}

// ---- Handling failures: ?? value, ?? fail, ?? coll.member ----

static bool block_ends(Block* b, bool loop_exits);

static Reason* find_reason(const std::vector<Reason*>& reasons, const std::string& name, int* index) {
    for (size_t i = 0; i < reasons.size(); i++) {
        if (reasons[i]->name == name) {
            if (index) *index = (int)i;
            return reasons[i];
        }
    }
    return nullptr;
}

static std::string what_fails(Expr* call) {
    if (call->kind == ExprKind::FnCall || call->kind == ExprKind::MethodCall) return q(call->item->name);
    return "this conversion";
}

// If h names a coll.handler member (coll.member or coll.member(args)), its collection and member; else null.
static Coll* handler_coll(Ctx& c, Expr* h) {
    Expr* f = h->kind == ExprKind::Call ? h->kids[0] : h;
    if (f->kind != ExprKind::Field || f->kids[0]->kind != ExprKind::Name) return nullptr;
    const std::string& name = f->kids[0]->text;
    auto it = c.pkg->colls.find(name);
    if (it == c.pkg->colls.end()) return nullptr;
    if (find_local(c, name)) {
        error_at(f->kids[0]->pos, q(name) + " is both a `coll.handler` and a value here; which one `??` uses is not decided yet");
    }
    return it->second;
}

static void check_member_use(Ctx& c, Expr* h, Expr* call, Coll* coll, Expr* use) {
    Expr* f = use->kind == ExprKind::Call ? use->kids[0] : use;
    Member* m = nullptr;
    for (Member* mm : coll->members) {
        if (mm->name == f->text) m = mm;
    }
    if (!m) error_at(f->pos, "`coll.handler " + coll->name + "` has no member " + q(f->text));
    std::vector<Expr*> args;
    if (use->kind == ExprKind::Call) args.assign(use->kids.begin() + 1, use->kids.end());
    if (use->kind != ExprKind::Call && !m->params.empty()) {
        error_at(use->pos, q(coll->name + "." + m->name) + " takes arguments: " + q(coll->name + "." + m->name + "(…)"));
    }
    if (use->kind == ExprKind::Call && m->params.empty()) {
        error_at(use->pos, q(coll->name + "." + m->name) + " takes nothing, so it is used bare: " + q(coll->name + "." + m->name));
    }
    if (args.size() != m->params.size()) {
        error_at(use->pos, q(coll->name + "." + m->name) + " takes " + std::to_string(m->params.size()) + " arguments, but is given " +
                               std::to_string(args.size()));
    }
    // the arguments are worked out where the handler is used
    std::vector<Type*> ptypes;
    for (size_t i = 0; i < args.size(); i++) {
        Type* pt = resolve_type(c.pkg, coll->file, m->params[i].tref);
        Type* t = check_value(c, args[i], pt);
        want_type(args[i], t, pt, "parameter " + q(m->params[i].name));
        ptypes.push_back(pt);
    }
    // each arm covers exactly one reason the call can fail with
    const std::vector<Reason*>& reasons = call->reasons;
    std::vector<Arm*> arms(reasons.size(), nullptr);
    for (Arm* a : m->arms) {
        int index = 0;
        if (!find_reason(reasons, a->name, &index)) {
            error_at(use->pos, q(coll->name + "." + m->name) + " has an arm for " + q(a->name) + ", which " + what_fails(call) +
                                   " cannot fail with");
        }
        if (arms[index]) error_at(a->pos, q(a->name) + " has two arms");
        arms[index] = a;
    }
    for (size_t i = 0; i < reasons.size(); i++) {
        if (!arms[i]) {
            error_at(use->pos, q(coll->name + "." + m->name) + " does not cover " + q(reasons[i]->name) + ", which " +
                                   what_fails(call) + " can fail with");
        }
    }
    // the arms are checked afresh here: they see the member's parameters, not the caller's names
    Ctx ac;
    ac.pkg = c.pkg;
    ac.file = coll->file;
    ac.fn = c.fn;
    ac.loops = c.loops;
    ac.scopes.push_back(Scope());
    for (size_t i = 0; i < m->params.size(); i++) {
        Local* l = declare(ac, m->params[i].name, m->params[i].pos, LocalKind::Param, m->params[i].is_var, ptypes[i]);
        h->member_params.push_back(l);
    }
    for (size_t i = 0; i < reasons.size(); i++) {
        Arm* a = new Arm(*arms[i]);
        a->body = clone_block(arms[i]->body);
        a->index = (int)i;
        ac.scopes.push_back(Scope());
        if (!a->binding.empty()) {
            if (!reasons[i]->type) error_at(a->binding_pos, q(a->name) + " carries nothing to name");
            a->local = declare(ac, a->binding, a->binding_pos, LocalKind::Binding, false, reasons[i]->type);
        } else if (reasons[i]->type) {
            error_at(a->pos, q(a->name) + " carries " + an(reasons[i]->type) + "; an arm not naming it is not decided yet");
        }
        check_block(ac, a->body);
        ac.scopes.pop_back();
        if (call->type != t_void && !block_ends(a->body, true)) {
            error_at(a->pos, "the arm for " + q(a->name) + " must leave (`ret`, `fail`, `stop` or `next`), as " +
                                 what_fails(call) + " gives a value");
        }
        h->arms.push_back(a);
    }
    for (Local* v : ac.vars) {
        if (!v->changed) error_at(v->pos, q(v->name) + " is a var but never changes; make it `let`");
    }
    h->member = m;
    h->kids.resize(1);
    for (Expr* a : args) h->kids.push_back(a);
}

// call ?? handler, where a statement or declaration allows it. Returns the value's type.
static Type* check_handle(Ctx& c, Expr* h, Type* hint) {
    Expr* call = h->kids[0];
    if (call->kind != ExprKind::Call && call->kind != ExprKind::Bracket) {
        error_at(h->pos, "`??` handles a call or a conversion that can fail");
    }
    call->handled = true;
    Type* t = check_expr(c, call, hint);
    if (call->reasons.empty()) {
        error_at(h->pos, "`??` on something that cannot fail is not decided yet");
    }
    if (h->handle == HandleKind::Fail) {
        Item* fn = c.fn;
        for (Reason* r : call->reasons) {
            int index = 0;
            Reason* mine = find_reason(fn->fails, r->name, &index);
            if (!mine) {
                error_at(h->pos, "`?? fail` passes " + q(r->name) + " up, but " + q(fn->name) + " does not list it in its `fail[…]`");
            }
            if (mine->type != r->type) {
                error_at(h->pos, "`?? fail` passes " + q(r->name) + " up, but " + q(fn->name) + " lists it with different data");
            }
        }
        h->type = t;
        return t;
    }
    Expr* use = h->kids[1];
    if (Coll* coll = handler_coll(c, use)) {
        h->handle = HandleKind::Member;
        check_member_use(c, h, call, coll, use);
        h->type = t;
        return t;
    }
    if (t == t_void) {
        error_at(use->pos, what_fails(call) + " gives nothing, so `??` has no value to give; use `?? fail` or a handler");
    }
    Type* vt = check_value(c, use, t);
    want_type(use, vt, t, "the value after `??`");
    h->type = t;
    return t;
}

// The value of a statement that allows `??`: a declaration's or a call statement's.
static Type* check_stmt_value(Ctx& c, Expr* e, Type* hint) {
    if (e->kind == ExprKind::Handle) return check_handle(c, e, hint);
    return check_value(c, e, hint);
}

// ---- Statements ----

static void check_cond(Ctx& c, Expr* e) {
    Type* t = check_value(c, e, t_bool);
    if (t != t_bool) {
        std::string zero = is_float(t) ? "0.0" : "0";
        if (is_number(t)) error_at(e->pos, "a condition must be a bool; write `… != " + zero + "`");
        error_at(e->pos, "a condition must be a bool, but this is " + an(t));
    }
}

static void check_loop_body(Ctx& c, Block* b) {
    c.loops++;
    check_block(c, b);
    c.loops--;
}

static bool is_literal(Expr* e) {
    return e->kind == ExprKind::Int || e->kind == ExprKind::Float || e->kind == ExprKind::Str || e->kind == ExprKind::Bool;
}

static void check_stmt(Ctx& c, Stmt* s) {
    switch (s->kind) {
    case StmtKind::Let:
    case StmtKind::Var:
    case StmtKind::Const: {
        Type* want = s->tref ? resolve_type(c.pkg, c.file, s->tref) : nullptr;
        if (s->kind == StmtKind::Const) {
            if (s->value->kind == ExprKind::Handle || !is_literal(s->value)) {
                error_at(s->value->pos, "a const whose value is not a literal is not decided yet");
            }
            if (s->value->kind == ExprKind::Str && want && want != t_str) want_type(s->value, t_str, want, q(s->name));
        }
        Type* t = check_stmt_value(c, s->value, want);
        if (want) want_type(s->value, t, want, q(s->name));
        LocalKind kind = s->kind == StmtKind::Let ? LocalKind::Let : s->kind == StmtKind::Var ? LocalKind::Var : LocalKind::Const;
        s->local = declare(c, s->name, s->name_pos, kind, s->kind == StmtKind::Var, t);
        return;
    }
    case StmtKind::Assign: {
        Type* t = check_value(c, s->target, nullptr);
        change_place(s->target, s->target->pos, "changed");
        if (s->value->kind == ExprKind::Handle) error_at(s->value->pos, "`??` in an assignment is not decided yet");
        Type* v = check_value(c, s->value, t);
        want_type(s->value, v, t, q(place_text(s->target)));
        return;
    }
    case StmtKind::Expr: {
        Expr* e = s->value->kind == ExprKind::Handle ? s->value->kids[0] : s->value;
        if (e->kind != ExprKind::Call) error_at(e->pos, "only a call stands as a statement; this value is unused");
        if (s->value->kind == ExprKind::Handle) {
            check_handle(c, s->value, nullptr);
        } else {
            check_expr(c, e, nullptr);
            must_not_fail(e);
        }
        return;
    }
    case StmtKind::If:
        for (size_t i = 0; i < s->conds.size(); i++) {
            check_cond(c, s->conds[i]);
            check_block(c, s->blocks[i]);
        }
        if (s->else_block) check_block(c, s->else_block);
        return;
    case StmtKind::ForEver:
        check_loop_body(c, s->blocks[0]);
        return;
    case StmtKind::ForWhile:
        check_cond(c, s->conds[0]);
        check_loop_body(c, s->blocks[0]);
        return;
    case StmtKind::ForRange: {
        Expr* from = s->conds[0];
        Expr* to = s->conds[1];
        Type* t;
        if (is_untyped(from) && !is_untyped(to)) {
            Type* tt = check_value(c, to, nullptr);
            t = check_value(c, from, tt);
        } else {
            t = check_value(c, from, nullptr);
            check_value(c, to, t);
        }
        if (to->type != t) error_at(to->pos, "both ends of a range have one type, but these are " + t->name + " and " + to->type->name);
        if (is_float(t)) error_at(from->pos, "a range of floats is not decided yet");
        if (!is_int(t)) error_at(from->pos, "a range is of integers, not of " + t->name);
        c.scopes.push_back(Scope());
        s->local = declare(c, s->name, s->name_pos, LocalKind::Loop, false, t);
        check_loop_body(c, s->blocks[0]);
        c.scopes.pop_back();
        return;
    }
    case StmtKind::ForEach: {
        Expr* w = s->value;
        bool chars = w->kind == ExprKind::Call && w->kids.size() == 1 && w->kids[0]->kind == ExprKind::Field && w->kids[0]->text == "chars";
        if (!chars) error_at(w->pos, "walking lists and maps needs the garbage collector; G2C2 walks numbers (`1..n`) and `s.chars()`");
        Expr* str = w->kids[0]->kids[0];
        Type* t = check_value(c, str, nullptr);
        if (t != t_str) error_at(w->pos, "`chars()` walks a str, not " + an(t));
        if (!s->name2.empty()) error_at(s->name2_pos, "what the first name is in `for i, c in s.chars()` is not decided yet");
        s->value = str;
        c.scopes.push_back(Scope());
        s->local = declare(c, s->name, s->name_pos, LocalKind::Loop, false, t_u32);
        check_loop_body(c, s->blocks[0]);
        c.scopes.pop_back();
        return;
    }
    case StmtKind::Match: {
        Type* t = check_value(c, s->value, nullptr);
        if (t->kind != TypeKind::Enum) error_at(s->value->pos, "`match` tells an enum's cases apart, not " + an(t) + "'s");
        Item* en = t->item;
        s->type = t;
        std::vector<bool> covered(en->cases.size(), false);
        for (Arm* a : s->arms) {
            int index = -1;
            for (size_t i = 0; i < en->cases.size(); i++) {
                if (en->cases[i].name == a->name) index = (int)i;
            }
            if (index < 0) error_at(a->pos, q(en->name) + " has no case " + q(a->name));
            if (covered[index]) error_at(a->pos, q(a->name) + " has two arms");
            covered[index] = true;
            a->index = index;
            Type* data = en->cases[index].type;
            c.scopes.push_back(Scope());
            if (!a->binding.empty()) {
                if (!data) error_at(a->binding_pos, q(a->name) + " carries nothing to name");
                a->local = declare(c, a->binding, a->binding_pos, LocalKind::Binding, false, data);
            } else if (data) {
                error_at(a->pos, q(a->name) + " carries " + an(data) + "; an arm not naming it is not decided yet");
            }
            check_block(c, a->body);
            c.scopes.pop_back();
        }
        std::string missing;
        for (size_t i = 0; i < covered.size(); i++) {
            if (!covered[i]) missing += (missing.empty() ? "" : ", ") + q(en->cases[i].name);
        }
        if (s->else_block) {
            if (missing.empty()) error_at(s->else_block->pos, "this `else` covers nothing: every case is named above");
            check_block(c, s->else_block);
        } else if (!missing.empty()) {
            error_at(s->pos, "this `match` does not cover " + missing + "; name each, or add `else`");
        }
        return;
    }
    case StmtKind::Ret: {
        Item* fn = c.fn;
        if (!s->value) {
            if (fn->ret != t_void) error_at(s->pos, q(fn->name) + " returns " + an(fn->ret) + "; `ret` needs a value");
            return;
        }
        if (fn->ret == t_void) error_at(s->value->pos, q(fn->name) + " returns nothing; use `ret;`");
        if (s->value->kind == ExprKind::Handle) error_at(s->value->pos, "`??` in a `ret` is not decided yet");
        Type* t = check_value(c, s->value, fn->ret);
        want_type(s->value, t, fn->ret, q(fn->name) + "'s result");
        return;
    }
    case StmtKind::Fail: {
        Item* fn = c.fn;
        int index = 0;
        Reason* r = find_reason(fn->fails, s->name, &index);
        if (!r) error_at(s->name_pos, q(fn->name) + " does not list " + q(s->name) + " in its `fail[…]`");
        s->reason = r;
        s->reason_index = index;
        if (r->type && !s->value) error_at(s->pos, q(s->name) + " carries " + an(r->type) + ": `fail " + s->name + "[…];`");
        if (!r->type && s->value) error_at(s->value->pos, q(s->name) + " carries nothing: `fail " + s->name + ";`");
        if (s->value) {
            Type* t = check_value(c, s->value, r->type);
            want_type(s->value, t, r->type, q(s->name) + "'s data");
        }
        return;
    }
    case StmtKind::Stop:
    case StmtKind::Next:
        if (c.loops == 0) error_at(s->pos, std::string(s->kind == StmtKind::Stop ? "`stop`" : "`next`") + " is only inside a `for`");
        return;
    }
}

static void check_block(Ctx& c, Block* b) {
    c.scopes.push_back(Scope());
    for (Stmt* s : b->stmts) check_stmt(c, s);
    c.scopes.pop_back();
}

// ---- Whether a block always leaves ----

// Whether a block holds a stop for the loop it is in (not for a loop inside it).
static bool has_stop(Block* b);

static bool stmt_has_stop(Stmt* s) {
    if (s->kind == StmtKind::Stop) return true;
    if (s->kind == StmtKind::If) {
        for (Block* b : s->blocks) {
            if (has_stop(b)) return true;
        }
        return s->else_block && has_stop(s->else_block);
    }
    if (s->kind == StmtKind::Match) {
        for (Arm* a : s->arms) {
            if (has_stop(a->body)) return true;
        }
        return s->else_block && has_stop(s->else_block);
    }
    if (s->value && s->value->kind == ExprKind::Handle) {
        for (Arm* a : s->value->arms) {
            if (has_stop(a->body)) return true;
        }
    }
    return false;
}

static bool has_stop(Block* b) {
    for (Stmt* s : b->stmts) {
        if (stmt_has_stop(s)) return true;
    }
    return false;
}

// Go's rule: a block ends when its last statement does. loop_exits: stop and next count as leaving.
static bool stmt_ends(Stmt* s, bool loop_exits) {
    switch (s->kind) {
    case StmtKind::Ret:
    case StmtKind::Fail:
        return true;
    case StmtKind::Stop:
    case StmtKind::Next:
        return loop_exits;
    case StmtKind::If:
        if (!s->else_block) return false;
        for (Block* b : s->blocks) {
            if (!block_ends(b, loop_exits)) return false;
        }
        return block_ends(s->else_block, loop_exits);
    case StmtKind::Match:
        for (Arm* a : s->arms) {
            if (!block_ends(a->body, loop_exits)) return false;
        }
        return !s->else_block || block_ends(s->else_block, loop_exits);
    case StmtKind::ForEver:
        return !has_stop(s->blocks[0]);
    default:
        return false;
    }
}

static bool block_ends(Block* b, bool loop_exits) {
    return !b->stmts.empty() && stmt_ends(b->stmts.back(), loop_exits);
}

// ---- Declarations ----

static void resolve_signature(Item* fn) {
    Package* pkg = fn->pkg;
    FileAst* file = fn->file;
    fn->ret = fn->ret_ref ? resolve_type(pkg, file, fn->ret_ref) : t_void;
    for (size_t i = 0; i < fn->fails.size(); i++) {
        Reason* r = fn->fails[i];
        for (size_t k = 0; k < i; k++) {
            if (fn->fails[k]->name == r->name) error_at(r->pos, q(r->name) + " is listed twice");
        }
        if (r->tref) r->type = resolve_type(pkg, file, r->tref);
    }
    for (size_t i = 0; i < fn->params.size(); i++) {
        Param& p = fn->params[i];
        Local* l = new Local();
        l->name = p.name;
        l->pos = p.pos;
        l->is_var = p.is_var;
        l->kind = p.name == "self" && !p.tref ? LocalKind::Self : LocalKind::Param;
        l->type = p.tref ? resolve_type(pkg, file, p.tref) : fn->recv->type;
        for (size_t k = 0; k < i; k++) {
            if (fn->params[k].name == p.name) error_at(p.pos, "parameter " + q(p.name) + " is named twice");
        }
        p.local = l;
    }
    if (fn->kind == ItemKind::ExternFn) {
        if (fn->has_fail_list) error_at(fn->pos, "an `extern.c fn` that can fail is not decided yet");
        auto c_type = [](Type* t, Pos pos) {
            if (is_int(t) || t == t_strc || t->kind == TypeKind::Opaque) return;
            if (is_float(t)) error_at(pos, "an `extern.c fn` taking or giving a float is not decided yet");
            if (t == t_bool) error_at(pos, "an `extern.c fn` taking or giving a bool is not decided yet");
            error_at(pos, "an `extern.c fn` takes and gives integers, `str.c` and `extern.c` types, not " + an(t));
        };
        for (Param& p : fn->params) {
            c_type(p.local->type, p.tref->pos);
            if (p.is_var) error_at(p.pos, "parameter " + q(p.name) + " is var, but an `extern.c fn` has no body to change it in");
        }
        if (fn->ret != t_void) c_type(fn->ret, fn->ret_ref->pos);
    }
}

static void check_fn(Item* fn) {
    Ctx c;
    c.pkg = fn->pkg;
    c.file = fn->file;
    c.fn = fn;
    c.scopes.push_back(Scope());
    for (Param& p : fn->params) {
        if (find_item(c.pkg, p.name) || find_import(c.file, p.name)) {
            error_at(p.pos, q(p.name) + " is already a name here; reusing a name (shadowing) is not decided yet");
        }
        c.scopes.back().names[p.name] = p.local;
    }
    check_block(c, fn->body);
    if (fn->ret != t_void && !block_ends(fn->body, false)) {
        error_at(fn->pos, q(fn->name) + " returns " + an(fn->ret) + ", but its end can be reached without `ret`");
    }
    for (Param& p : fn->params) {
        if (p.is_var && !p.local->changed) {
            if (p.local->kind == LocalKind::Self) error_at(p.pos, "`var self`, but `self` never changes; make it `let self`");
            error_at(p.pos, "parameter " + q(p.name) + " is var but never changes; make it `let`");
        }
    }
    for (Local* v : c.vars) {
        if (!v->changed) error_at(v->pos, q(v->name) + " is a var but never changes; make it `let`");
    }
}

// A struct or enum that holds itself, directly or not, needs the collector.
static void check_contains(Item* it) {
    if (it->state == 2) return;
    if (it->state == 1) {
        error_at(it->pos, "type " + q(it->name) + " contains itself, which needs the garbage collector; G2C2 does not have it yet");
    }
    it->state = 1;
    for (Field& f : it->fields) {
        if (f.type->item && f.type->kind != TypeKind::Opaque) check_contains(f.type->item);
    }
    for (Case& k : it->cases) {
        if (k.type && k.type->item && k.type->kind != TypeKind::Opaque) check_contains(k.type->item);
    }
    it->state = 2;
}

static void make_user_type(Item* it) {
    Type* t = new Type();
    t->kind = it->kind == ItemKind::Struct ? TypeKind::Struct : it->kind == ItemKind::Enum ? TypeKind::Enum : TypeKind::Opaque;
    t->name = it->pkg->name == "main" ? it->name : it->pkg->name + "." + it->name;
    t->item = it;
    it->type = t;
}

void check_program(Program* program) {
    // every top-level name, per package
    for (Package* pkg : program->packages) {
        for (FileAst* f : pkg->files) {
            for (Item* it : f->items) {
                it->pkg = pkg;
                it->file = f;
                if (!it->recv_name.empty()) {
                    pkg->methods.push_back(it);
                    continue;
                }
                if (pkg->items.count(it->name)) error_at(it->pos, q(it->name) + " is declared twice in package " + q(pkg->name));
                pkg->items[it->name] = it;
                if (is_type_item(it)) make_user_type(it);
                if (it->kind == ItemKind::Const) error_at(it->pos, "a const outside a function is not decided yet");
            }
            for (Coll* coll : f->colls) {
                coll->file = f;
                if (pkg->colls.count(coll->name)) error_at(coll->pos, "`coll.handler " + coll->name + "` is declared twice");
                for (size_t i = 0; i < coll->members.size(); i++) {
                    for (size_t k = 0; k < i; k++) {
                        if (coll->members[k]->name == coll->members[i]->name) error_at(coll->members[i]->pos, "member " + q(coll->members[i]->name) + " is declared twice");
                    }
                }
                pkg->colls[coll->name] = coll;
            }
            for (const Import& imp : f->imports) {
                if (pkg->items.count(imp.name)) error_at(imp.pos, q(imp.name) + " is both an import and a name in this package; that is not decided yet");
            }
        }
    }
    // fields, cases, signatures; methods join their types
    for (Package* pkg : program->packages) {
        for (auto& [name, it] : pkg->items) {
            for (size_t i = 0; i < it->fields.size(); i++) {
                Field& f = it->fields[i];
                for (size_t k = 0; k < i; k++) {
                    if (it->fields[k].name == f.name) error_at(f.pos, "field " + q(f.name) + " is declared twice");
                }
                f.type = resolve_type(pkg, it->file, f.tref);
            }
            for (size_t i = 0; i < it->cases.size(); i++) {
                Case& k = it->cases[i];
                for (size_t j = 0; j < i; j++) {
                    if (it->cases[j].name == k.name) error_at(k.pos, "case " + q(k.name) + " is declared twice");
                }
                if (k.tref) k.type = resolve_type(pkg, it->file, k.tref);
            }
        }
        for (Item* m : pkg->methods) {
            Item* t = find_item(pkg, m->recv_name);
            if (!t || !is_type_item(t)) {
                error_at(m->recv_pos, "there is no type " + q(m->recv_name) + " in this package; a method is declared on a type of its own package");
            }
            if (t->methods.count(m->name)) error_at(m->pos, q(t->name) + " already has a method " + q(m->name));
            m->recv = t;
            m->self_var = m->params[0].is_var;
            t->methods[m->name] = m;
        }
    }
    for (Package* pkg : program->packages) {
        for (auto& [name, it] : pkg->items) {
            if (it->kind == ItemKind::Fn || it->kind == ItemKind::ExternFn) resolve_signature(it);
            if (it->kind == ItemKind::Struct || it->kind == ItemKind::Enum) check_contains(it);
        }
        for (Item* m : pkg->methods) resolve_signature(m);
    }

    Package* main_pkg = program->packages[0];
    Item* main_fn = find_item(main_pkg, "main");
    if (!main_fn || main_fn->kind != ItemKind::Fn) {
        error_at(Pos{main_pkg->files[0]->file, 0, 0}, "package main has no `fn main[]`");
    }
    if (!main_fn->params.empty()) error_at(main_fn->pos, "`main` takes nothing: `fn main[]`");
    if (main_fn->ret_ref || main_fn->has_fail_list) error_at(main_fn->pos, "`main` returns nothing and lists no `fail[…]`; it exits 0 at its end");

    for (Package* pkg : program->packages) {
        for (auto& [name, it] : pkg->items) {
            if (it->kind == ItemKind::Fn) check_fn(it);
        }
        for (Item* m : pkg->methods) check_fn(m);
    }
    // a var field must change somewhere in the program
    for (Package* pkg : program->packages) {
        for (auto& [name, it] : pkg->items) {
            for (Field& f : it->fields) {
                if (f.is_var && !f.changed) {
                    error_at(f.pos, "field " + q(f.name) + " of " + q(it->name) + " is var but never changes anywhere; make it `let`");
                }
            }
        }
    }
}
