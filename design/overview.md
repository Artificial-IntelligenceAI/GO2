# GO2 — overview

GO2 as it stands, rules only. Each rule is the owner's; the reasoning, what else was offered, and every reversal are in
[`DECISIONS.md`](../DECISIONS.md). Nothing here is built yet. Where this page and the log disagree, the log wins and
this page is wrong.

```
pkg main;
import "fmt";

enum Expr { number: i64; add: Add; }
struct Add { let left: Expr; let right: Expr; }

fn eval[let e: Expr] -> i64 {
    match e {
        case number n { ret n; }
        case add a { ret eval(a.left) + eval(a.right); }
    }
}

fn main[] {
    let e = Expr.add[Add[left = Expr.number[2], right = Expr.number[3]]];
    fmt.print("{}\n", eval(e)); // 5
}
```

## Toolchain

- Source files end `.go2`.
- **G2RS** (`g2rs/`, command `g2rs`) is the bootstrap compiler, in Rust. **GoTo** (`goto/`, command `goto`) is the
  self-hosted one, in GO2, and replaces G2RS once it builds itself. Both take `build` and `run`.
- Both compile through LLVM, linked as a library: G2RS through `llvm-sys` (Homebrew LLVM 23), with C++ shims where
  LLVM's C API falls short.
- Builds are debug or release; they differ in arithmetic overflow (below). How one is asked for is open.

## Projects and packages

- A project's root holds `project.go2`, a GO2 struct literal: `Project[name = "goto"]`. The compiler walks up from
  where it is to find it.
- A folder is a package. Every `.go2` file in it starts `pkg <name>;` and sees the others' names.
- `import "lexer";` names a folder, from the project root.
- `pub` makes a thing visible to other packages: `pub fn`, `pub struct`, `pub enum`. A `pub` type shows everything in
  it (fields, methods, cases); without `pub`, nothing leaves the package.

## Text of a program

- Every statement ends with `;`.
- Blocks are `{ … }`. Indentation and line breaks mean nothing to the compiler; `{` may sit anywhere.
- Comments are `//` to the end of the line, on their own line or after code. There is no block comment.
- **`( )` runs something** (a call, a conversion); **`[ ]` describes or holds something** (parameters, type
  arguments, literals, building, indexing, failure lists). The one exception: `( )` also groups maths, `a * (b + c)`.

## Variables and constants

```
let name = "GO2";        // never changes
var count = 0;           // may change, and must
count = count + 1;
const max = 10;          // known when compiling, baked in
let x: i32 = 5;          // a written type follows the name after :
```

- Leaving the type out infers it from the value, at the declaration and nowhere else.
- A `var` that is never changed in its scope is refused, naming `let`.
- A change is `name = value;`. There is no `+=` or other compound form.

## Types

| Type | Notes |
|---|---|
| `i8` `i16` `i32` `i64`, `u8` `u16` `u32` `u64` | No `int`; every integer type says its size |
| `bool` | `true`, `false` |
| `str` | UTF-8 bytes; never changes in place |
| `ls[T]` | A growable list: `[1, 2, 3]` |
| `map[K, V]` | `["ann" = 30, "bo" = 25]`, walked in insertion order |
| `struct` | Declared types with named fields |
| `enum` | One of several cases, each optionally carrying data |

**Values.** Lists and structs (and so all of the above) are values: assigning or passing one gives an independent copy,
made cheap by copy-on-write. So a `let` list or struct never changes, inside or out. A type that contains itself
(`Expr` holding `Add` holding `Expr`) just works; the compiler stores the cycle in GC memory.

**Numbers.** A bare number takes the type of the variable it meets (`n + 1` with `n: i32` makes `1` an `i32`;
`let x: i8 = 5;`). With nothing around it, it is an `i64`. Conversion calls the type: `i64(x)`. A narrowing that does
not fit follows the overflow rule, unless handled: `i8(x) ?? 0`. Text converts the same way and must be handled:
`i64(text) ?? 0` (fails `not_a_number` or `out_of_range`).

### Structs

```
struct Token {
    let kind: Kind;      // set only when built
    var line: i64;       // must be changed somewhere in the program
}

let t = Token[kind = Kind.plus, line = 1];   // every field, by name, any order
let n = t.line;                               // read a field
```

There are no zero values: building a struct without every field is refused, naming the missing one.

### Enums and `match`

```
enum Token { plus; name: str; number: i64; }

let t = Token.number[42];

match t {
    case plus { … }
    case number v { … }   // v is the case's data, a let
    else { … }            // every case not named above
}
```

A `match` without `else` must name every case, or it is refused.

### Strings

- `"…"` with C-style escapes: `\n`, `\t`, `\"`, `\\`.
- `s[i]` is the i-th byte, a `u8`, from 1. `s.len()` counts bytes.
- `concat(a, b, "!")` joins any number of strings; `+` on strings is refused.

### Lists

```
var xs: ls[i64] = [];
xs.append(4);                 // var lists only
let last = xs.rm_last() ?? 0; // gives the item, or fails empty
xs[1] = 7;                    // var lists only
for i in 1..xs.len() { … }
```

- An index counts from 1. `xs[i]` outside `1..xs.len()` stops the program, naming the line, in any build, unless the
  read is handled with `??`: `xs[i] ?? 0`.

### Maps

```
var ages: map[str, i64] = ["ann" = 30];
ages["bo"] = 25;              // adds or replaces; var maps only
let a = ages["cy"] ?? 0;      // a missing key fails missing, and must be handled
ages.rm("ann") ?? fail;       // var maps only; fails missing
ages.len()
for k, v in ages { … }        // insertion order
```

## Operators

| | |
|---|---|
| Arithmetic | `+` `-` `*` `/` `%` |
| Comparison | `==` `!=` `<` `<=` `>` `>=` |
| Logic, stopping early | `&&` `\|\|` (in `a && b`, `b` runs only if `a` is true) |
| Logic, both sides always | `and` `or` |
| Not | `!` (there is no `not`) |

- Both sides of an operator have the same type; `i32 + i64` is refused.
- Integer `/` drops the fraction: `7 / 2` is `3`.
- A result too big for its type stops a debug build, naming the line, and wraps in a release build.
- Dividing by zero stops the program, in any build.
- A condition must be a `bool`: `if count {` is refused, naming `count != 0`.

## Functions and methods

```
fn add[let a: i32, var b: i32] -> i32 {
    b = b + 1;
    ret a + b;
}

fn Point.dist[let self, let q: Point] -> i64 { … }   // a method
fn Counter.bump[var self] { self.n = self.n + 1; }  // called only on a var

fn main[] { … }                                      // returns nothing; exits 0
```

- Every parameter says `let` or `var`; a `var` one must be changed in the body.
- `-> T` gives the return type; `ret` returns, and `ret;` leaves a function that returns nothing. `main` has none
  and exits 0 at its end; `os.exit(3);` exits with a code.
- A method is declared outside its type as `fn Type.name[let self, …]` or `[var self, …]`, and called `v.name(…)`.

## Control flow

```
if a > 0 { … } else if a < 0 { … } else { … }

for { … }                 // forever
for n < 10 { … }          // while it holds
for x in xs { … }
for i, x in xs { … }      // with its position
for i in 1..10 { … }      // 1 to 10, both included
stop;                     // leave the innermost for
next;                     // go to its next turn
```

## Failures

A function that can fail lists its reasons by name; a caller must say what happens, or it is refused. There are no
exceptions, no `?`, no `otherwise`.

```
fn parse[let s: str] -> i64 or fail[empty, bad_char: u8] {
    if s.len() == 0 { fail empty; }
    …
    fail bad_char[c];
}

let a = parse(t) ?? 0;              // a value, whatever the reason
let b = parse(t) ?? fail;           // pass it up: my fail[…] must list each reason
let c = parse(t) ?? parse.skip;     // a named handler (skip uses next, so inside a for)
save(f) ?? fail;                    // fn save[…] -> fail[disk_full]: returns nothing, can fail
let d = parse(t) ?? parse.or_ret(0); // a handler with arguments: returns 0 from the enclosing fn
```

Named handlers are grouped in a `coll.handler`, and used as `<collection>.<member>`:

```
coll.handler parse {
    member skip {
        case empty { next; }
        case bad_char c { next; }
    }
    member or_ret[let fallback: i64] {
        case empty { ret fallback; }
        case bad_char c { ret fallback; }
    }
}
```

- A member covers exactly the reasons of the call it is used at; a missing or extra arm is refused there.
- An arm never gives the `??` a value; that is what `?? value` is for. It leaves, from where its handler is used,
  with `ret`, `stop`, `next` or `fail`. On a call that gives a value every arm must leave; on a call that gives
  nothing, an arm may also just finish, and the program carries on.
- A call that can fail stands alone, as a statement or a declaration's whole value: never `parse(a) + parse(b)`.
- Two failures may go unhandled: a list index out of range, which then stops the program, and a narrowing
  conversion that does not fit, which then follows the overflow rule. Both fail `out_of_range` when handled, and
  a handled one stands alone (`let x = xs[i] ?? 0;`) while an unhandled one may sit anywhere (`xs[i] + xs[j]`).

## Printing

`fmt.print` is formatted print. Each `{}` takes the next value, of any type; `{{` writes a brace. It adds no newline.

```
fmt.print("{}:{}: {}\n", line, col, msg);
```

## Calling C

```
pub extern.c type LLVMModuleRef;          // an opaque C pointer type; mixups are refused
pub extern.c type LLVMTypeRef;
pub extern.c fn LLVMModuleCreateWithName[let name: cstr] -> LLVMModuleRef;
pub extern.c fn LLVMFunctionType[let ret: LLVMTypeRef, let params: ls[LLVMTypeRef], let count: u32,
                                 let variadic: i32] -> LLVMTypeRef;

let m = LLVMModuleCreateWithName(cstr("main"));
let f = LLVMFunctionType(ret, params, u32(params.len()), 0);
```

- A C function is declared one at a time, `extern.c fn`, with its C name and no body.
- C's integers are GO2's sized types: `int` is `i32`, `unsigned` is `u32`, `long` and `size_t` are 64-bit.
- `cstr` is a NUL-ended C string: `cstr(s)` makes one from a `str`, `str(c)` copies one back.
- A C array parameter takes a GO2 list (a pointer to its items); the count is passed apart.

## Memory

Garbage collected, by a collector written for GO2.

## Open

Not decided yet, roughly in the order the compiler will need them:

- What "C++ when needed" covers; what C may do with a list it is given.
- Generics: none for now (only `ls` and `map`), until GoTo builds itself; their spelling then.
- Which text counts as a number.
- Walking a string's characters; whether invalid UTF-8 can exist.
- Leaving an outer loop; whether loop names are `let`.
- What a `const` may hold; whether a parameter can be `const`.
- `[a, b: i32]` sharing a type; a range that stops before its end.
- Which way negative integer division rounds; what can be compared with what.
- What a single `&` or `|` means.
- Whether a `match` gives a value; an `else` that covers nothing.
- Whether `coll` groups anything but handlers.
- Whether a folder's name must match its `pkg` line; what else `project.go2` holds; methods on another package's types.
- The garbage collector's kind, and what it is written in.
- How a debug or release build is asked for; a formatter.
- What GO2 is, in one sentence.
