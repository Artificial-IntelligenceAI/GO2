// G2C2, the bootstrap compiler for GO2.
//
// One header for the whole compiler. The stages are plain functions over plain structs:
// lex (lex.cpp) -> parse (parse.cpp) -> check (check.cpp) -> gen (gen.cpp), driven by main.cpp.
// G2C2 will be ported to GO2 as GoTo, so it keeps to what GO2 has: structs, enums, lists, maps,
// functions and methods. No inheritance, no templates of its own, no exceptions.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace llvm {
class Value;
class Function;
class Type;
class StructType;
}

// ---- Source positions and errors ----

struct Pos {
    int file = 0;  // index into g_files
    int line = 0;
    int col = 0;
};

struct SourceFile {
    std::string path;  // as shown in messages
    std::string text;
};

extern std::vector<SourceFile> g_files;

// Prints "path:line:col: message" and ends G2C2 with exit code 1. G2C2 stops at the first error.
[[noreturn]] void error_at(Pos pos, const std::string& message);

// ---- Tokens ----

enum class Tok {
    End,
    Name,
    Int,
    Float,
    String,
    // reserved words
    KwPkg, KwImport, KwPub, KwFn, KwRet, KwLet, KwVar, KwConst, KwStruct, KwEnum, KwMatch, KwCase,
    KwElse, KwIf, KwFor, KwIn, KwStop, KwNext, KwFail, KwColl, KwMember, KwExtern, KwSelf,
    KwTrue, KwFalse, KwAnd, KwOr,
    // built-in type names, also reserved
    KwType,  // i8 ... u64, f32, f64, bool, str, ls, map: the name is in Token.text
    // punctuation
    LParen, RParen, LBracket, RBracket, LBrace, RBrace,
    Semi, Colon, Comma, Dot, DotDot, Arrow, Assign, QQ,
    Plus, Minus, Star, Slash, Percent,
    Eq, Ne, Lt, Le, Gt, Ge,
    AndAnd, OrOr, Bang,
};

struct Token {
    Tok kind = Tok::End;
    Pos pos;
    std::string text;      // the source text; for a String, its bytes with escapes resolved
    uint64_t value = 0;    // for an Int
};

std::vector<Token> lex(int file);
std::string tok_name(Tok kind);

// ---- Types ----

enum class TypeKind { Int, Float, Bool, Str, StrC, Struct, Enum, Opaque, Void };

struct Item;

struct Type {
    TypeKind kind = TypeKind::Void;
    int bits = 0;            // Int and Float
    bool is_signed = false;  // Int
    std::string name;        // as written in messages: "i64", "Token", "lexer.Token"
    Item* item = nullptr;    // Struct, Enum, Opaque
    llvm::Type* llvm = nullptr;
};

extern Type* t_i8;
extern Type* t_i16;
extern Type* t_i32;
extern Type* t_i64;
extern Type* t_u8;
extern Type* t_u16;
extern Type* t_u32;
extern Type* t_u64;
extern Type* t_f32;
extern Type* t_f64;
extern Type* t_bool;
extern Type* t_str;
extern Type* t_strc;
extern Type* t_void;

// A type as written in the source, resolved by the checker.
struct TypeRef {
    Pos pos;
    std::string pkg;   // "lexer" in lexer.Token, else empty
    std::string name;  // "i64", "str.c", "Token"
};

// ---- Expressions ----

enum class ExprKind {
    // as parsed
    Int, Float, Str, Bool, Name, SelfRef, Paren, Unary, Binary, Compare, Logic, Call, Field, Bracket,
    TypeName,  // a built-in type name used as a value, as in i64(x) or str.c(s)
    Handle,    // call ?? handler; only at the top of a statement
    // as resolved by the checker
    LocalRef, FnCall, MethodCall, Conv, StrC, StrLen, StrIndex, FieldGet, StructLit, EnumLit,
    Print, Exit,
};

enum class HandleKind { Value, Fail, Member };

struct Local;
struct Member;
struct Arm;
struct Reason;
struct Block;

struct Expr {
    ExprKind kind = ExprKind::Int;
    Pos pos;
    std::string text;               // Name, Field: the name; Str: the bytes; Unary/Binary/Logic: the op
    uint64_t value = 0;             // Int: the magnitude
    bool negative = false;          // Int, Float: a folded unary minus
    std::vector<Expr*> kids;        // operands, arguments; Call and Bracket: kids[0] is the base
    std::vector<std::string> ops;   // Compare: the operators between kids
    std::vector<std::string> names; // Bracket: a field name per item ("" for a plain item)
    std::vector<Pos> name_pos;      // Bracket: where each field name is

    // set by the checker
    Type* type = nullptr;
    Local* local = nullptr;   // LocalRef
    Item* item = nullptr;     // FnCall, MethodCall, StructLit, EnumLit
    int index = 0;            // FieldGet: field; EnumLit: case
    bool handled = false;     // a call or conversion that can fail, under a ??
    bool can_fail = false;    // Conv: a narrowing that may not fit; FnCall: the fn lists reasons
    std::vector<Reason*> reasons;      // what a call or conversion can fail with
    std::vector<std::string> pieces;   // Print: the text around each {}

    // Handle
    HandleKind handle = HandleKind::Value;
    Member* member = nullptr;
    std::vector<Local*> member_params;   // Member: the member's parameters, set from kids[2...]
    std::vector<Arm*> arms;              // Member: the checked arms, one per reason, in reason order
};

// ---- Statements ----

enum class StmtKind { Let, Var, Const, Assign, Expr, If, ForEver, ForWhile, ForRange, ForEach, Match, Ret, Fail, Stop, Next };

struct Stmt {
    StmtKind kind = StmtKind::Expr;
    Pos pos;
    std::string name;            // Let/Var/Const: the name; Fail: the reason; ForRange/ForEach: the loop name
    Pos name_pos;
    std::string name2;           // ForEach: for <name>, <name2> in ...
    Pos name2_pos;
    TypeRef* tref = nullptr;     // Let/Var/Const: a written type
    Expr* value = nullptr;       // Let/Var/Const/Assign: the value; Expr: the call; Ret/Fail: the value;
                                 // ForEach: what is walked; Match: what is matched
    Expr* target = nullptr;      // Assign: what is changed
    std::vector<Expr*> conds;    // If: one per branch; ForWhile: [cond]; ForRange: [from, to]
    std::vector<Block*> blocks;  // If: one per branch, plus else; For: [body]
    Block* else_block = nullptr; // If, Match
    std::vector<Arm*> arms;      // Match

    // set by the checker
    Local* local = nullptr;      // Let/Var/Const, ForRange, ForEach: the name
    Local* local2 = nullptr;     // ForEach: the second name
    Reason* reason = nullptr;    // Fail
    int reason_index = 0;        // Fail
    Type* type = nullptr;        // Match: the enum
};

struct Block {
    Pos pos;
    std::vector<Stmt*> stmts;
};

// A case arm of a match or of a handler member: case <name> [<binding>] { ... }
struct Arm {
    Pos pos;
    std::string name;
    std::string binding;  // "" when the data is not named
    Pos binding_pos;
    Block* body = nullptr;
    // set by the checker
    int index = 0;           // the enum case or reason it covers
    Local* local = nullptr;  // the binding
};

// ---- Declarations ----

enum class LocalKind { Let, Var, Const, Param, Loop, Binding, Self };

struct Local {
    std::string name;
    Pos pos;
    LocalKind kind = LocalKind::Let;
    bool is_var = false;   // may change
    bool changed = false;  // a var: changed somewhere in its scope
    Type* type = nullptr;
    Expr* const_value = nullptr;  // Const
    llvm::Value* slot = nullptr;  // its alloca, in gen
};

struct Param {
    Pos pos;
    std::string name;
    bool is_var = false;
    TypeRef* tref = nullptr;  // null for self
    Local* local = nullptr;   // set by the checker, per function
};

struct Reason {
    Pos pos;
    std::string name;
    TypeRef* tref = nullptr;  // null when it carries nothing
    Type* type = nullptr;
};

struct Field {
    Pos pos;
    std::string name;
    bool is_var = false;
    TypeRef* tref = nullptr;
    Type* type = nullptr;
    bool changed = false;  // a var field: changed somewhere in the program
};

struct Case {
    Pos pos;
    std::string name;
    TypeRef* tref = nullptr;  // null when it carries nothing
    Type* type = nullptr;
};

struct Member {
    Pos pos;
    std::string name;
    std::vector<Param> params;
    std::vector<Arm*> arms;
};

struct FileAst;

struct Coll {
    FileAst* file = nullptr;
    Pos pos;
    std::string name;
    std::vector<Member*> members;
};

struct Package;

enum class ItemKind { Fn, Struct, Enum, Const, ExternFn, ExternType };

struct Item {
    ItemKind kind = ItemKind::Fn;
    Pos pos;
    std::string name;
    bool is_pub = false;
    Package* pkg = nullptr;
    FileAst* file = nullptr;

    // Fn, ExternFn
    std::string recv_name;      // a method: the type's name, as in fn Point.dist
    Pos recv_pos;
    Item* recv = nullptr;       // a method: the type's item
    bool self_var = false;      // a method: [var self]
    std::vector<Param> params;  // a method's self is params[0]
    TypeRef* ret_ref = nullptr;
    Type* ret = nullptr;        // t_void when it returns nothing
    std::vector<Reason*> fails;
    bool has_fail_list = false; // -> T or fail[...] / -> fail[...]
    Block* body = nullptr;
    llvm::Function* llvm_fn = nullptr;

    // Struct, Enum, ExternType
    std::vector<Field> fields;
    std::vector<Case> cases;
    std::map<std::string, Item*> methods;
    Type* type = nullptr;
    int state = 0;  // checking a type for containing itself: 0 not yet, 1 in progress, 2 done

    // Const
    TypeRef* const_ref = nullptr;
    Expr* const_value = nullptr;
    Local* const_local = nullptr;
};

struct Import {
    Pos pos;
    std::string path;
    std::string name;
    Package* pkg = nullptr;  // null for fmt and os
};

struct FileAst {
    int file = 0;
    std::string pkg_name;
    Pos pkg_pos;
    std::vector<Import> imports;
    std::vector<Item*> items;
    std::vector<Coll*> colls;
};

struct Package {
    std::string name;  // "main" at the root
    std::string dir;   // the folder
    std::vector<FileAst*> files;
    std::map<std::string, Item*> items;   // functions, types, consts
    std::map<std::string, Coll*> colls;   // coll.handler collections, a namespace of their own
    std::vector<Item*> methods;           // fn Type.name, attached to their types by the checker
};

struct Program {
    std::vector<Package*> packages;  // packages[0] is main
    std::string name;                // from project.go2, or the file's name
};

// The reasons built-in failures give: a narrowing that does not fit, text that is not a number.
extern Reason* r_out_of_range;
extern Reason* r_not_a_number;

// ---- Stages ----

FileAst* parse_file(int file);
Expr* parse_project_file(int file);  // project.go2: one expression and a ;
Block* clone_block(Block* block);
void check_program(Program* program);
// Writes an object file; returns false with a message on failure.
bool gen_program(Program* program, bool release, const std::string& obj_path, std::string& err);

// Helpers shared by the stages.
std::string type_name(Type* type);
bool is_int(Type* type);
bool is_float(Type* type);
bool is_number(Type* type);
