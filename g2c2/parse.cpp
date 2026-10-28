// The parser: a file's tokens into a syntax tree.
//
// Precedence follows maths, tightest first: unary - and !; * / %; + -; comparisons; logic. Where maths has no
// answer the parser refuses until parenthesised: / followed by * or /, % next to other arithmetic, mixed logic
// operators, and a comparison chain that changes direction.

#include "g2c2.h"

struct Parser {
    std::vector<Token> toks;
    size_t at = 0;
};

static const Token& peek(Parser& p, int ahead = 0) {
    size_t i = p.at + ahead;
    if (i >= p.toks.size()) i = p.toks.size() - 1;
    return p.toks[i];
}

static Token next(Parser& p) {
    Token t = peek(p);
    if (p.at < p.toks.size() - 1) p.at++;
    return t;
}

static bool accept(Parser& p, Tok kind) {
    if (peek(p).kind == kind) {
        next(p);
        return true;
    }
    return false;
}

static std::string found(const Token& t) {
    if (t.kind == Tok::Name) return "`" + t.text + "`";
    if (t.kind == Tok::KwType) return "`" + t.text + "`";
    if (t.kind == Tok::Int || t.kind == Tok::Float) return "`" + t.text + "`";
    if (t.kind == Tok::String) return "a string";
    return tok_name(t.kind);
}

static Token expect(Parser& p, Tok kind, const std::string& what) {
    if (peek(p).kind != kind) {
        error_at(peek(p).pos, "expected " + what + ", found " + found(peek(p)));
    }
    return next(p);
}

static Token expect_name(Parser& p, const std::string& what) {
    const Token& t = peek(p);
    if (t.kind == Tok::KwType || (t.kind != Tok::Name && t.kind >= Tok::KwPkg && t.kind <= Tok::KwOr)) {
        error_at(t.pos, found(t) + " is reserved and can never be a name");
    }
    return expect(p, Tok::Name, what);
}

static Expr* new_expr(ExprKind kind, Pos pos) {
    Expr* e = new Expr();
    e->kind = kind;
    e->pos = pos;
    return e;
}

static Expr* parse_expr(Parser& p);
static Expr* parse_unary(Parser& p);
static Block* parse_block(Parser& p);

// ---- Types ----

static TypeRef* parse_type(Parser& p) {
    TypeRef* t = new TypeRef();
    t->pos = peek(p).pos;
    if (peek(p).kind == Tok::KwType) {
        t->name = next(p).text;
        if (t->name == "str" && peek(p).kind == Tok::Dot && peek(p, 1).kind == Tok::Name && peek(p, 1).text == "c") {
            next(p);
            next(p);
            t->name = "str.c";
        }
        if (t->name == "ls" || t->name == "map") {
            error_at(t->pos, "`" + t->name + "` needs the garbage collector, which G2C2 does not have yet");
        }
        return t;
    }
    Token name = expect_name(p, "a type");
    t->name = name.text;
    if (accept(p, Tok::Dot)) {
        t->pkg = t->name;
        t->name = expect_name(p, "a type name after `.`").text;
    }
    return t;
}

// ---- Expressions ----

static Expr* parse_primary(Parser& p) {
    const Token& t = peek(p);
    switch (t.kind) {
    case Tok::Int: {
        Expr* e = new_expr(ExprKind::Int, t.pos);
        e->value = t.value;
        e->text = t.text;
        next(p);
        return e;
    }
    case Tok::Float: {
        Expr* e = new_expr(ExprKind::Float, t.pos);
        e->text = t.text;
        next(p);
        return e;
    }
    case Tok::String: {
        Expr* e = new_expr(ExprKind::Str, t.pos);
        e->text = t.text;
        next(p);
        return e;
    }
    case Tok::KwTrue:
    case Tok::KwFalse: {
        Expr* e = new_expr(ExprKind::Bool, t.pos);
        e->value = t.kind == Tok::KwTrue ? 1 : 0;
        next(p);
        return e;
    }
    case Tok::Name: {
        Expr* e = new_expr(ExprKind::Name, t.pos);
        e->text = t.text;
        next(p);
        return e;
    }
    case Tok::KwSelf: {
        Expr* e = new_expr(ExprKind::SelfRef, t.pos);
        e->text = "self";
        next(p);
        return e;
    }
    case Tok::KwType: {
        Expr* e = new_expr(ExprKind::TypeName, t.pos);
        e->text = t.text;
        next(p);
        if (e->text == "str" && peek(p).kind == Tok::Dot && peek(p, 1).kind == Tok::Name && peek(p, 1).text == "c") {
            next(p);
            next(p);
            e->text = "str.c";
        }
        return e;
    }
    case Tok::LParen: {
        Expr* e = new_expr(ExprKind::Paren, t.pos);
        next(p);
        e->kids.push_back(parse_expr(p));
        expect(p, Tok::RParen, "`)`");
        return e;
    }
    case Tok::LBracket:
        error_at(t.pos, "a list literal needs the garbage collector, which G2C2 does not have yet");
    default:
        error_at(t.pos, "expected a value, found " + found(t));
    }
}

static Expr* parse_postfix(Parser& p) {
    Expr* e = parse_primary(p);
    while (true) {
        const Token& t = peek(p);
        if (t.kind == Tok::Dot) {
            next(p);
            Expr* f = new_expr(ExprKind::Field, t.pos);
            Token name = expect_name(p, "a name after `.`");
            f->text = name.text;
            f->pos = name.pos;
            f->kids.push_back(e);
            e = f;
        } else if (t.kind == Tok::LParen) {
            next(p);
            Expr* c = new_expr(ExprKind::Call, e->pos);
            c->kids.push_back(e);
            if (peek(p).kind != Tok::RParen) {
                while (true) {
                    c->kids.push_back(parse_expr(p));
                    if (!accept(p, Tok::Comma)) break;
                }
            }
            expect(p, Tok::RParen, "`,` or `)`");
            e = c;
        } else if (t.kind == Tok::LBracket) {
            next(p);
            Expr* b = new_expr(ExprKind::Bracket, e->pos);
            b->kids.push_back(e);
            if (peek(p).kind != Tok::RBracket) {
                while (true) {
                    if (peek(p).kind == Tok::Name && peek(p, 1).kind == Tok::Assign) {
                        Token name = next(p);
                        next(p);
                        b->names.push_back(name.text);
                        b->name_pos.push_back(name.pos);
                    } else {
                        b->names.push_back("");
                        b->name_pos.push_back(peek(p).pos);
                    }
                    b->kids.push_back(parse_expr(p));
                    if (!accept(p, Tok::Comma)) break;
                }
            }
            expect(p, Tok::RBracket, "`,` or `]`");
            e = b;
        } else {
            return e;
        }
    }
}

static Expr* parse_unary(Parser& p) {
    const Token& t = peek(p);
    if (t.kind == Tok::Minus || t.kind == Tok::Bang) {
        Pos pos = t.pos;
        std::string op = t.kind == Tok::Minus ? "-" : "!";
        next(p);
        Expr* operand = parse_unary(p);
        // -7 is folded to a literal
        if (op == "-" && (operand->kind == ExprKind::Int || operand->kind == ExprKind::Float) && !operand->negative) {
            operand->negative = true;
            operand->pos = pos;
            return operand;
        }
        Expr* e = new_expr(ExprKind::Unary, pos);
        e->text = op;
        e->kids.push_back(operand);
        return e;
    }
    return parse_postfix(p);
}

static bool is_rem(Expr* e) { return e->kind == ExprKind::Binary && e->text == "%"; }

static Expr* parse_mult(Parser& p) {
    Expr* left = parse_unary(p);
    std::string prev;
    while (peek(p).kind == Tok::Star || peek(p).kind == Tok::Slash || peek(p).kind == Tok::Percent) {
        Token t = next(p);
        std::string op = t.text;
        if (prev == "/" && (op == "*" || op == "/")) {
            error_at(t.pos, "`/` followed by `" + op + "` needs parentheses, as in `(a / b) " + op + " c`");
        }
        if (op == "%" || prev == "%") {
            error_at(t.pos, "`%` next to other arithmetic needs parentheses");
        }
        Expr* e = new_expr(ExprKind::Binary, t.pos);
        e->text = op;
        e->kids.push_back(left);
        e->kids.push_back(parse_unary(p));
        left = e;
        prev = op;
    }
    return left;
}

static Expr* parse_add(Parser& p) {
    Expr* left = parse_mult(p);
    while (peek(p).kind == Tok::Plus || peek(p).kind == Tok::Minus) {
        Token t = next(p);
        Expr* right = parse_mult(p);
        if (is_rem(left) || is_rem(right)) {
            error_at(t.pos, "`%` next to other arithmetic needs parentheses");
        }
        Expr* e = new_expr(ExprKind::Binary, t.pos);
        e->text = t.text;
        e->kids.push_back(left);
        e->kids.push_back(right);
        left = e;
    }
    return left;
}

static bool is_compare(Tok k) {
    return k == Tok::Eq || k == Tok::Ne || k == Tok::Lt || k == Tok::Le || k == Tok::Gt || k == Tok::Ge;
}

static char compare_way(const std::string& op) {
    if (op == "<" || op == "<=") return '<';
    if (op == ">" || op == ">=") return '>';
    if (op == "==") return '=';
    return '!';
}

static Expr* parse_compare(Parser& p) {
    Expr* first = parse_add(p);
    if (!is_compare(peek(p).kind)) return first;
    Expr* e = new_expr(ExprKind::Compare, peek(p).pos);
    e->kids.push_back(first);
    while (is_compare(peek(p).kind)) {
        Token t = next(p);
        if (!e->ops.empty()) {
            char way = compare_way(e->ops[0]);
            if (way == '!' || compare_way(t.text) != way) {
                error_at(t.pos, "a comparison chain goes one way: all `<` and `<=`, all `>` and `>=`, or all `==`");
            }
        }
        e->ops.push_back(t.text);
        e->kids.push_back(parse_add(p));
    }
    return e;
}

static bool is_logic(Tok k) { return k == Tok::AndAnd || k == Tok::OrOr || k == Tok::KwAnd || k == Tok::KwOr; }

static Expr* parse_logic(Parser& p) {
    Expr* first = parse_compare(p);
    if (!is_logic(peek(p).kind)) return first;
    Token t = peek(p);
    Expr* e = new_expr(ExprKind::Logic, t.pos);
    e->text = t.text;
    e->kids.push_back(first);
    while (is_logic(peek(p).kind)) {
        Token op = next(p);
        if (op.text != e->text) {
            error_at(op.pos, "`" + e->text + "` mixed with `" + op.text + "` needs parentheses, as in `(a " + e->text +
                                 " b) " + op.text + " c`");
        }
        e->kids.push_back(parse_compare(p));
    }
    return e;
}

static Expr* parse_expr(Parser& p) { return parse_logic(p); }

static bool is_binary_op(Tok k) {
    return k == Tok::Plus || k == Tok::Minus || k == Tok::Star || k == Tok::Slash || k == Tok::Percent ||
           is_compare(k) || is_logic(k) || k == Tok::QQ;
}

// After an expression that may be followed by ?? and its handler.
static Expr* parse_maybe_handled(Parser& p, Expr* e) {
    if (peek(p).kind != Tok::QQ) return e;
    Token qq = next(p);
    Expr* h = new_expr(ExprKind::Handle, qq.pos);
    h->kids.push_back(e);
    if (peek(p).kind == Tok::KwFail) {
        next(p);
        h->handle = HandleKind::Fail;
    } else {
        h->handle = HandleKind::Value;
        h->kids.push_back(parse_unary(p));
    }
    if (is_binary_op(peek(p).kind)) {
        error_at(peek(p).pos, "a `??` stands alone: after it comes one value, a handler, or `fail`");
    }
    return h;
}

// ---- Statements ----

static Stmt* new_stmt(StmtKind kind, Pos pos) {
    Stmt* s = new Stmt();
    s->kind = kind;
    s->pos = pos;
    return s;
}

static Arm* parse_arm(Parser& p) {
    Token c = expect(p, Tok::KwCase, "`case`");
    Arm* a = new Arm();
    a->pos = c.pos;
    a->name = expect_name(p, "a case name after `case`").text;
    if (peek(p).kind != Tok::LBrace) {
        Token b = expect_name(p, "a name for the case's data, or `{`");
        a->binding = b.text;
        a->binding_pos = b.pos;
    }
    a->body = parse_block(p);
    return a;
}

static Stmt* parse_stmt(Parser& p) {
    const Token& t = peek(p);
    Pos pos = t.pos;
    switch (t.kind) {
    case Tok::KwLet:
    case Tok::KwVar:
    case Tok::KwConst: {
        StmtKind kind = t.kind == Tok::KwLet ? StmtKind::Let : t.kind == Tok::KwVar ? StmtKind::Var : StmtKind::Const;
        std::string word = t.text;
        next(p);
        Stmt* s = new_stmt(kind, pos);
        Token name = expect_name(p, "a name after `" + word + "`");
        s->name = name.text;
        s->name_pos = name.pos;
        if (accept(p, Tok::Colon)) s->tref = parse_type(p);
        if (peek(p).kind != Tok::Assign) {
            error_at(peek(p).pos, "`" + s->name + "` needs a value: `" + word + " " + s->name + " = …;` (there are no zero values)");
        }
        next(p);
        s->value = parse_maybe_handled(p, parse_expr(p));
        expect(p, Tok::Semi, "`;`");
        return s;
    }
    case Tok::KwIf: {
        Stmt* s = new_stmt(StmtKind::If, pos);
        next(p);
        s->conds.push_back(parse_expr(p));
        s->blocks.push_back(parse_block(p));
        while (peek(p).kind == Tok::KwElse) {
            next(p);
            if (accept(p, Tok::KwIf)) {
                s->conds.push_back(parse_expr(p));
                s->blocks.push_back(parse_block(p));
            } else {
                s->else_block = parse_block(p);
                break;
            }
        }
        return s;
    }
    case Tok::KwFor: {
        next(p);
        if (peek(p).kind == Tok::LBrace) {
            Stmt* s = new_stmt(StmtKind::ForEver, pos);
            s->blocks.push_back(parse_block(p));
            return s;
        }
        bool is_in = (peek(p).kind == Tok::Name && peek(p, 1).kind == Tok::KwIn) ||
                     (peek(p).kind == Tok::Name && peek(p, 1).kind == Tok::Comma && peek(p, 2).kind == Tok::Name &&
                      peek(p, 3).kind == Tok::KwIn);
        if (!is_in) {
            Stmt* s = new_stmt(StmtKind::ForWhile, pos);
            s->conds.push_back(parse_expr(p));
            s->blocks.push_back(parse_block(p));
            return s;
        }
        Token name = next(p);
        Stmt* s = new_stmt(StmtKind::ForEach, pos);
        s->name = name.text;
        s->name_pos = name.pos;
        if (accept(p, Tok::Comma)) {
            Token name2 = next(p);
            s->name2 = name2.text;
            s->name2_pos = name2.pos;
        }
        expect(p, Tok::KwIn, "`in`");
        Expr* walked = parse_expr(p);
        if (accept(p, Tok::DotDot)) {
            s->kind = StmtKind::ForRange;
            s->conds.push_back(walked);
            s->conds.push_back(parse_expr(p));
            if (!s->name2.empty()) error_at(s->name2_pos, "a range of numbers gives one name: `for i in a..b`");
        } else {
            s->value = walked;
        }
        s->blocks.push_back(parse_block(p));
        return s;
    }
    case Tok::KwMatch: {
        Stmt* s = new_stmt(StmtKind::Match, pos);
        next(p);
        s->value = parse_expr(p);
        expect(p, Tok::LBrace, "`{`");
        while (peek(p).kind == Tok::KwCase) s->arms.push_back(parse_arm(p));
        if (accept(p, Tok::KwElse)) s->else_block = parse_block(p);
        expect(p, Tok::RBrace, "`case`, `else` or `}`");
        return s;
    }
    case Tok::KwRet: {
        Stmt* s = new_stmt(StmtKind::Ret, pos);
        next(p);
        if (!accept(p, Tok::Semi)) {
            s->value = parse_maybe_handled(p, parse_expr(p));
            expect(p, Tok::Semi, "`;`");
        }
        return s;
    }
    case Tok::KwFail: {
        Stmt* s = new_stmt(StmtKind::Fail, pos);
        next(p);
        Token name = expect_name(p, "a reason after `fail`");
        s->name = name.text;
        s->name_pos = name.pos;
        if (accept(p, Tok::LBracket)) {
            s->value = parse_expr(p);
            expect(p, Tok::RBracket, "`]`");
        }
        expect(p, Tok::Semi, "`;`");
        return s;
    }
    case Tok::KwStop:
    case Tok::KwNext: {
        Stmt* s = new_stmt(t.kind == Tok::KwStop ? StmtKind::Stop : StmtKind::Next, pos);
        next(p);
        expect(p, Tok::Semi, "`;`");
        return s;
    }
    case Tok::LBrace:
        error_at(pos, "a block standing alone as a statement is not decided yet");
    default:
        break;
    }
    Expr* e = parse_expr(p);
    if (peek(p).kind == Tok::Assign) {
        Stmt* s = new_stmt(StmtKind::Assign, pos);
        next(p);
        s->target = e;
        s->value = parse_maybe_handled(p, parse_expr(p));
        expect(p, Tok::Semi, "`;`");
        return s;
    }
    Stmt* s = new_stmt(StmtKind::Expr, pos);
    s->value = parse_maybe_handled(p, e);
    expect(p, Tok::Semi, "`;`");
    return s;
}

static Block* parse_block(Parser& p) {
    Block* b = new Block();
    b->pos = peek(p).pos;
    expect(p, Tok::LBrace, "`{`");
    while (peek(p).kind != Tok::RBrace) {
        if (peek(p).kind == Tok::End) expect(p, Tok::RBrace, "`}`");
        b->stmts.push_back(parse_stmt(p));
    }
    next(p);
    return b;
}

// ---- Declarations ----

// [let a: i32, var b: i64] or [let self, ...]
static std::vector<Param> parse_params(Parser& p, bool allow_self) {
    std::vector<Param> params;
    expect(p, Tok::LBracket, "`[`");
    if (accept(p, Tok::RBracket)) return params;
    while (true) {
        Param prm;
        prm.pos = peek(p).pos;
        if (accept(p, Tok::KwLet)) {
            prm.is_var = false;
        } else if (accept(p, Tok::KwVar)) {
            prm.is_var = true;
        } else {
            error_at(peek(p).pos, "every parameter says `let` or `var`, as in `let a: i64`");
        }
        if (peek(p).kind == Tok::KwSelf) {
            Token s = next(p);
            if (!allow_self || !params.empty()) error_at(s.pos, "`self` comes first, and only in a method: `fn Type.name[let self, …]`");
            prm.name = "self";
        } else {
            Token name = expect_name(p, "a parameter name");
            prm.name = name.text;
            prm.pos = name.pos;
            if (peek(p).kind != Tok::Colon) {
                error_at(peek(p).pos, "each parameter says its own type: `" + prm.name + ": …`");
            }
            next(p);
            prm.tref = parse_type(p);
        }
        params.push_back(prm);
        if (!accept(p, Tok::Comma)) break;
    }
    expect(p, Tok::RBracket, "`,` or `]`");
    return params;
}

static std::vector<Reason*> parse_fail_list(Parser& p) {
    std::vector<Reason*> reasons;
    expect(p, Tok::LBracket, "`[` after `fail`");
    while (true) {
        Reason* r = new Reason();
        Token name = expect_name(p, "a reason");
        r->name = name.text;
        r->pos = name.pos;
        if (accept(p, Tok::Colon)) r->tref = parse_type(p);
        reasons.push_back(r);
        if (!accept(p, Tok::Comma)) break;
    }
    expect(p, Tok::RBracket, "`,` or `]`");
    return reasons;
}

// -> T, -> T or fail[...], -> fail[...]
static void parse_returns(Parser& p, Item* fn) {
    if (!accept(p, Tok::Arrow)) return;
    if (accept(p, Tok::KwFail)) {
        fn->has_fail_list = true;
        fn->fails = parse_fail_list(p);
        return;
    }
    fn->ret_ref = parse_type(p);
    if (accept(p, Tok::KwOr)) {
        expect(p, Tok::KwFail, "`fail` after `or`");
        fn->has_fail_list = true;
        fn->fails = parse_fail_list(p);
    }
}

static Item* parse_fn(Parser& p, bool is_pub) {
    Token f = expect(p, Tok::KwFn, "`fn`");
    Item* it = new Item();
    it->kind = ItemKind::Fn;
    it->is_pub = is_pub;
    Token name = expect_name(p, "a function name");
    it->pos = name.pos;
    it->name = name.text;
    if (accept(p, Tok::Dot)) {
        it->recv_name = it->name;
        it->recv_pos = it->pos;
        Token m = expect_name(p, "a method name after `.`");
        it->name = m.text;
        it->pos = m.pos;
    }
    it->params = parse_params(p, !it->recv_name.empty());
    if (!it->recv_name.empty() && (it->params.empty() || it->params[0].name != "self" || it->params[0].tref)) {
        error_at(it->pos, "a method's first parameter is `let self` or `var self`");
    }
    parse_returns(p, it);
    (void)f;
    it->body = parse_block(p);
    return it;
}

static Item* parse_struct(Parser& p, bool is_pub) {
    next(p);
    Item* it = new Item();
    it->kind = ItemKind::Struct;
    it->is_pub = is_pub;
    Token name = expect_name(p, "a struct name");
    it->name = name.text;
    it->pos = name.pos;
    expect(p, Tok::LBrace, "`{`");
    while (!accept(p, Tok::RBrace)) {
        Field f;
        f.pos = peek(p).pos;
        if (accept(p, Tok::KwLet)) {
            f.is_var = false;
        } else if (accept(p, Tok::KwVar)) {
            f.is_var = true;
        } else {
            error_at(peek(p).pos, "every field says `let` or `var`, as in `let x: i64;`");
        }
        Token fname = expect_name(p, "a field name");
        f.name = fname.text;
        f.pos = fname.pos;
        expect(p, Tok::Colon, "`:` and the field's type");
        f.tref = parse_type(p);
        expect(p, Tok::Semi, "`;`");
        it->fields.push_back(f);
    }
    return it;
}

static Item* parse_enum(Parser& p, bool is_pub) {
    next(p);
    Item* it = new Item();
    it->kind = ItemKind::Enum;
    it->is_pub = is_pub;
    Token name = expect_name(p, "an enum name");
    it->name = name.text;
    it->pos = name.pos;
    expect(p, Tok::LBrace, "`{`");
    while (!accept(p, Tok::RBrace)) {
        Case c;
        Token cname = expect_name(p, "a case name");
        c.name = cname.text;
        c.pos = cname.pos;
        if (accept(p, Tok::Colon)) c.tref = parse_type(p);
        expect(p, Tok::Semi, "`;`");
        it->cases.push_back(c);
    }
    return it;
}

static Item* parse_extern(Parser& p, bool is_pub) {
    Token ext = next(p);
    if (!accept(p, Tok::Dot) || peek(p).kind != Tok::Name || peek(p).text != "c") {
        error_at(ext.pos, "`extern` is written `extern.c`");
    }
    next(p);
    Item* it = new Item();
    it->is_pub = is_pub;
    if (peek(p).kind == Tok::Name && peek(p).text == "type") {
        next(p);
        it->kind = ItemKind::ExternType;
        Token name = expect_name(p, "a type name");
        it->name = name.text;
        it->pos = name.pos;
        expect(p, Tok::Semi, "`;`");
        return it;
    }
    expect(p, Tok::KwFn, "`fn` or `type` after `extern.c`");
    it->kind = ItemKind::ExternFn;
    Token name = expect_name(p, "a C function name");
    it->name = name.text;
    it->pos = name.pos;
    it->params = parse_params(p, false);
    parse_returns(p, it);
    if (peek(p).kind == Tok::LBrace) error_at(peek(p).pos, "an `extern.c fn` has no body");
    expect(p, Tok::Semi, "`;`");
    return it;
}

static Coll* parse_coll(Parser& p) {
    Token c = next(p);
    if (!accept(p, Tok::Dot) || peek(p).kind != Tok::Name || peek(p).text != "handler") {
        error_at(c.pos, "`coll` is written `coll.handler`, its one kind");
    }
    next(p);
    Coll* coll = new Coll();
    Token name = expect_name(p, "a collection name");
    coll->name = name.text;
    coll->pos = name.pos;
    expect(p, Tok::LBrace, "`{`");
    while (!accept(p, Tok::RBrace)) {
        expect(p, Tok::KwMember, "`member` or `}`");
        Member* m = new Member();
        Token mname = expect_name(p, "a member name");
        m->name = mname.text;
        m->pos = mname.pos;
        if (peek(p).kind == Tok::LBracket) m->params = parse_params(p, false);
        expect(p, Tok::LBrace, "`{`");
        while (peek(p).kind == Tok::KwCase) m->arms.push_back(parse_arm(p));
        expect(p, Tok::RBrace, "`case` or `}`");
        coll->members.push_back(m);
    }
    return coll;
}

FileAst* parse_file(int file) {
    Parser p;
    p.toks = lex(file);
    FileAst* f = new FileAst();
    f->file = file;
    if (peek(p).kind != Tok::KwPkg) error_at(peek(p).pos, "a file starts with its package: `pkg main;`");
    next(p);
    Token name = expect_name(p, "a package name after `pkg`");
    f->pkg_name = name.text;
    f->pkg_pos = name.pos;
    expect(p, Tok::Semi, "`;`");
    while (peek(p).kind == Tok::KwImport) {
        next(p);
        Token path = expect(p, Tok::String, "a package path in quotes, as in `import \"fmt\";`");
        Import imp;
        imp.pos = path.pos;
        imp.path = path.text;
        size_t slash = imp.path.rfind('/');
        imp.name = slash == std::string::npos ? imp.path : imp.path.substr(slash + 1);
        expect(p, Tok::Semi, "`;`");
        f->imports.push_back(imp);
    }
    while (peek(p).kind != Tok::End) {
        Pos pos = peek(p).pos;
        bool is_pub = accept(p, Tok::KwPub);
        switch (peek(p).kind) {
        case Tok::KwFn: f->items.push_back(parse_fn(p, is_pub)); break;
        case Tok::KwStruct: f->items.push_back(parse_struct(p, is_pub)); break;
        case Tok::KwEnum: f->items.push_back(parse_enum(p, is_pub)); break;
        case Tok::KwExtern: f->items.push_back(parse_extern(p, is_pub)); break;
        case Tok::KwColl:
            if (is_pub) error_at(pos, "a `pub coll.handler` is not decided yet");
            f->colls.push_back(parse_coll(p));
            break;
        case Tok::KwImport:
            error_at(peek(p).pos, "an `import` after other declarations is not decided yet; imports come right after `pkg`");
        case Tok::KwConst: {
            Token c = next(p);
            Item* it = new Item();
            it->kind = ItemKind::Const;
            it->is_pub = is_pub;
            Token cname = expect_name(p, "a name after `const`");
            it->name = cname.text;
            it->pos = c.pos;
            if (accept(p, Tok::Colon)) it->const_ref = parse_type(p);
            expect(p, Tok::Assign, "`=` and the constant's value");
            it->const_value = parse_expr(p);
            expect(p, Tok::Semi, "`;`");
            f->items.push_back(it);
            break;
        }
        default:
            error_at(peek(p).pos, "expected `fn`, `struct`, `enum`, `const`, `extern.c` or `coll.handler`, found " +
                                      found(peek(p)));
        }
    }
    return f;
}

Expr* parse_project_file(int file) {
    Parser p;
    p.toks = lex(file);
    Expr* e = parse_expr(p);
    expect(p, Tok::Semi, "`;`");
    expect(p, Tok::End, "the end of the file");
    return e;
}

// ---- Cloning, for handler members, which are checked afresh at each use ----

static Expr* clone_expr(Expr* e) {
    if (!e) return nullptr;
    Expr* c = new Expr(*e);
    for (size_t i = 0; i < c->kids.size(); i++) c->kids[i] = clone_expr(e->kids[i]);
    return c;
}

static Stmt* clone_stmt(Stmt* s);

Block* clone_block(Block* b) {
    if (!b) return nullptr;
    Block* c = new Block(*b);
    for (size_t i = 0; i < c->stmts.size(); i++) c->stmts[i] = clone_stmt(b->stmts[i]);
    return c;
}

static Arm* clone_arm(Arm* a) {
    Arm* c = new Arm(*a);
    c->body = clone_block(a->body);
    return c;
}

static Stmt* clone_stmt(Stmt* s) {
    Stmt* c = new Stmt(*s);
    c->value = clone_expr(s->value);
    c->target = clone_expr(s->target);
    for (size_t i = 0; i < c->conds.size(); i++) c->conds[i] = clone_expr(s->conds[i]);
    for (size_t i = 0; i < c->blocks.size(); i++) c->blocks[i] = clone_block(s->blocks[i]);
    c->else_block = clone_block(s->else_block);
    for (size_t i = 0; i < c->arms.size(); i++) c->arms[i] = clone_arm(s->arms[i]);
    return c;
}
