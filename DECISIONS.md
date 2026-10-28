# GO2 — Design decisions

Running log. Each entry: date, the decision, who proposed it, and who chose it.
Nothing here is final until the owner says so; open questions live at the bottom.

## Decided

### 2026-10-28 — Commits follow `Soclets/CyborgPL`'s form (owner)

The owner: "Do GH commits like Soclets/CyborgPL". So **subjects are `kind.folder.narrowing: sentence (who chose)`**, one
decision per commit, authored as Tankun Sriket, with the trailer `Co-Authored-By: design claude <noreply@anthropic.com>`.
`CLAUDE.md` has the full form.

### 2026-10-28 — `( )` runs something; `[ ]` describes or holds something; maths grouping the one exception (owner)

The owner's first sketch had `fn main[] -> i32:` beside `fmt.print("Hello, World!\n");`. Design claude asked whether
`[ ]` for parameters was on purpose, recommending `( )` for both. The owner: "`[]` describes or holds, `()` means...
Uh... Ask CyborgPL design claude". CyborgPL's design claude answered with CyborgPL's rule (its DECISIONS.md,
2026-10-22): "`( )` runs something; `[ ]` describes or holds something", so a definition's parameters are in `[ ]`
(a definition runs nothing) and only a call uses `( )`; then a correction, maths grouping is the one exception (its
DECISIONS.md, 2026-10-28). Design claude changed to recommending the rule. The owner: "yes". So **a call is `f(…)`, a
definition `fn f[…]:`, and `( )` otherwise only groups maths, as in `a + (b * 2)`** (spelling of names and operators not decided). Where else `[ ]` goes (indexing, list
literals, building a struct, a type filled with types) is not decided yet. Not built.

### 2026-10-28 — A block closes with `;` and its kind, then its name when it has one: `;fn main`, `;if` (owner: (a), with `;fn main`)

The owner's sketch closed `fn main` with `;main`. Design claude offered (a) `;name` for a named block and `;kind` for
the rest (`;main`, `;if`), (b) always `;kind`, plus the name when there is one (`;fn main`, `;if`), (c) only a
function closes by name, inner blocks with a bare `;`; and recommended (a). The owner: "(a), but it's gotta be `;fn
main`". So **a closer is `;`, the block's kind, and its name if it has one: `;fn main`, `;if`, `;for`**; the sketch's
`;main` is now `;fn main`. Since the kind is always written, a name never clashes with a keyword in a closer. Not built.

### 2026-10-28 — The first compiler is written in Rust (owner)

Asked what the first compiler outputs, the owner answered that and what it is written in: "Hosted in Rust, uses the
LLVM backend, and C++ when needed". So **the bootstrap compiler is Rust**; the self-hosted one, in GO2, replaces it.
Not built.

### 2026-10-28 — The compiler's backend is LLVM, with C++ when needed (owner)

Design claude offered (a) C source through clang, (b) LLVM IR, (c) arm64 assembly directly, (d) a bytecode VM, and
recommended (a). The owner: "uses the LLVM backend, and C++ when needed". So **GO2 compiles through LLVM**; C++ is
used where it is needed (when, and for what, the owner has not said yet). Installed here: Homebrew LLVM 23.1.2
(also 21 and 22), Apple clang 21. Not built.

### 2026-10-28 — The compiler links LLVM as a library, with C++ shims where its C API falls short (owner: (b))

Design claude offered (a) writing LLVM IR as text for `clang` (the self-hosted compiler then needs no link to LLVM),
(b) linking LLVM from Rust (`inkwell` or `llvm-sys`), with C++ shims where the C API falls short, (c) (a) first, (b)
once GO2 compiles itself; and recommended (c). The owner: "(b)". So **the Rust compiler builds IR through LLVM's
library**, and the self-hosted one will call LLVM (and the shims) from GO2 too. Which crate is not decided. Not built.

### 2026-10-28 — GO2 is garbage collected, by its own collector (owner: (a), custom)

Design claude offered (a) garbage collection as Go has it (Boehm GC to start, GO2's own later), (b) manual
`alloc`/`free`, (c) arenas first, (d) ownership and borrowing; and recommended (c) with plain `alloc`/`free` beside it.
The owner: "(a), custom". So **memory is garbage collected, by a collector written for GO2**, not Boehm. Its kind
(tracing or counting, precise or conservative, moving or not) and what it is written in are not decided. Not built.

### 2026-10-28 — `main` returns nothing; a program exits 0 unless it says otherwise (owner: (a))

The owner's sketch had `fn main[] -> i32:` ending in `ret 0;`. Design claude flagged it as a difference from Go and,
asked "What should it return, then?", offered (a) nothing, as in Go, another code by a call such as `os.exit(3);`, (b)
an exit code (better `u8` than `i32`, since Unix keeps only its low 8 bits), (c) success or failure, a failure printed
and exit 1 (waits on how GO2 reports errors), (d) nothing or a code, as `main` declares; and recommended (a), with (c)
possible later. The owner: "(a)". So **`fn main[]:` has no return type, and reaching its end exits with 0**. The
call's exact name follows the package decisions; letting `main` fail is not decided. Not built.

### 2026-10-28 — A file names its package first, `pkg main;` (owner)

From the owner's hello-world sketch; design claude listed its parts to confirm and recommended all. The owner: "indentation is required, confirm (and btw, `fmt.print` is formatted print)". So **a file starts `pkg <name>;`**. Not built.

### 2026-10-28 — An import names a package by its path in quotes, `import "fmt";` (owner)

From the owner's hello-world sketch; design claude listed its parts to confirm and recommended all. The owner: "indentation is required, confirm (and btw, `fmt.print` is formatted print)". So **`import "<path>";`**; how paths resolve is not decided. Not built.

### 2026-10-28 — Every statement ends with `;`, required (owner)

From the owner's hello-world sketch; design claude listed its parts to confirm and recommended all. The owner: "indentation is required, confirm (and btw, `fmt.print` is formatted print)". So **a statement ends with `;`**, never left out; a statement never starts with `;`, which is how a closer is told apart. Not built.

### 2026-10-28 — A function is declared with `fn` (owner)

From the owner's hello-world sketch; design claude listed its parts to confirm and recommended all. The owner: "indentation is required, confirm (and btw, `fmt.print` is formatted print)". So **`fn <name>[<parameters>]:`**. Not built.

### 2026-10-28 — A return type follows `->`, `fn f[] -> i32:` (owner)

From the owner's hello-world sketch; design claude listed its parts to confirm and recommended all. The owner: "indentation is required, confirm (and btw, `fmt.print` is formatted print)". So **`-> <type>` after the parameters**; a function returning nothing has none. Not built.

### 2026-10-28 — `:` opens a block (owner)

From the owner's hello-world sketch; design claude listed its parts to confirm and recommended all. The owner: "indentation is required, confirm (and btw, `fmt.print` is formatted print)". So **a block's header ends with `:`**, and its closer (`;fn main`) ends it. Not built.

### 2026-10-28 — Return is spelled `ret` (owner)

From the owner's hello-world sketch; design claude listed its parts to confirm and recommended all. The owner: "indentation is required, confirm (and btw, `fmt.print` is formatted print)". So **`ret <value>;`**; whether a function returning nothing can `ret` early, and how, is not decided. Not built.

### 2026-10-28 — Integer types are named by sign and width, `i8` to `i64`, `u8` to `u64` (owner)

From the owner's hello-world sketch; design claude listed its parts to confirm and recommended all. The owner: "indentation is required, confirm (and btw, `fmt.print` is formatted print)". So **`i8 i16 i32 i64 u8 u16 u32 u64`**; a pointer-sized integer, and whether there are others, is not decided. Not built.

### 2026-10-28 — `fmt.print` is formatted print, and adds no newline (owner)

From the owner's hello-world sketch; design claude listed its parts to confirm and recommended all. The owner: "indentation is required, confirm (and btw, `fmt.print` is formatted print)". Design claude had listed it as printing that adds no newline; the owner added that it is **formatted print**, a format string with its values, as Go's `Printf`. So **`fmt.print("…\n", …)`** formats and prints, ending no line itself. The format's spelling is not decided. Not built.

### 2026-10-28 — A string literal is in double quotes, with C-style escapes (owner)

From the owner's hello-world sketch; design claude listed its parts to confirm and recommended all. The owner: "indentation is required, confirm (and btw, `fmt.print` is formatted print)". So **`"Hello, World!\n"`**, with `\n`, `\t`, `\\`, `\"` and the rest; which escapes exactly is not decided. Not built.

### 2026-10-28 — Indentation is the writer's; the closer alone marks where a block ends (owner: (b))

From the owner's hello-world sketch; design claude listed "indentation is cosmetic, since the closers mark where blocks end" among its parts to confirm. The owner: "indentation is required", first recorded as leading whitespace being required. Asked next how wide, the owner answered of the `:` ("`fn main[]:` is correct, `fn main[]` isn't"), then: "Oh, I thought you meant `:` is indention". Design claude offered (a) indentation required, checked beside the closer, (b) optional, the compiler ignoring leading whitespace, a formatter keeping code tidy as gofmt does; and recommended (b). The owner: "(b)". So **leading whitespace means nothing to the compiler; `:` opens a block and its closer ends it**. A formatter is not decided. Not built.

### 2026-10-28 — A block's header must end with `:`; `fn main[]:`, never `fn main[]` (owner)

Asked how indentation is checked, the owner: "`fn main[]:` is correct, `fn main[]` isn't". So **the `:` that opens a block is required: a header without it is refused**, naming the `:`. Two entries above that showed a definition as `fn f[…]` now show its `:`. Whether a function can be declared without a body is not decided. Not built.

### 2026-10-28 — The compiler binds LLVM through `llvm-sys`, its C API as it is (owner: (b))

Design claude offered (a) `inkwell`, a safe typed Rust layer, (b) `llvm-sys`, the raw C API under `unsafe`; and recommended (b): the self-hosted compiler will call the same C API, so the Rust one is a guide for it, and C++ shims sit beside it alike. Both support LLVM 23 (`inkwell` 0.11.0, `llvm-sys` 231.0.1). The owner: "(b)". So **the Rust compiler uses `llvm-sys` against Homebrew LLVM 23**. Not built.

### 2026-10-28 — GO2 source files end `.go2` (owner: (a))

Design claude offered (a) `.go2`, (b) `.g2`, (c) one of the owner's own, as CyborgPL has `.cy`; and recommended (a): it says what it is, and not being `.go` keeps Go's tools and editors off it. The owner: "(a)". So **a GO2 file is `<name>.go2`**. Not built.

### 2026-10-28 — The compiler is GoTo (owner)

Design claude offered (a) `go2`, the language's name, the Rust one in `boot/`, (b) a name of its own, as CyborgPL has `kayo`; and recommended (a). The owner: "GoTo". So **the compiler is named GoTo**. How the command is spelled, its folder, and whether the Rust compiler and the self-hosted one both carry the name, are not decided. Not built.

### 2026-10-28 — G2RS is the Rust compiler; GoTo is the self-hosted one, written in GO2 (owner)

Design claude proposed the command `goto`, the Rust compiler being GoTo too (folder `goto/`, crate `goto`), the self-hosted one taking the name over. The owner: "G2RS". Asked whether that names the Rust compiler, GoTo being the self-hosted one, the owner: "yes". So **G2RS is the bootstrap compiler, in Rust; GoTo is GO2's own, written in GO2**, which replaces G2RS once it builds itself. How each is spelled as a command and folder is not decided. Not built.

### 2026-10-28 — On disk, `g2rs` and `goto`: each its own command and folder (owner)

Design claude proposed G2RS as command `g2rs`, folder `g2rs/`, Rust crate `g2rs` (`g2rs build hello.go2`, `g2rs run hello.go2`), and GoTo as command `goto`, folder `goto/`, taking the same subcommands so it drops in for G2RS. The owner: "yes". So **`g2rs/` holds the Rust compiler and `goto/` the self-hosted one; both take `build` and `run`**. Not built.

### 2026-10-28 — A variable is `let` if it never changes, `var` if it may; a constant is `const` (owner)

The owner, asked how parameters are written in `[ ]`: "Let's do vars first, inferrence is the absence of the type / var immut = let / var mut = var / const = const". So **`let` declares an immutable variable, `var` a mutable one, `const` a constant**. What sets `const` apart from `let` (a value known when compiling, or something else), and whether `let` reaches inside what it holds, are not decided. Not built.

### 2026-10-28 — Leaving out a variable's type asks for it to be inferred (owner)

The owner, asked how parameters are written in `[ ]`: "Let's do vars first, inferrence is the absence of the type / var immut = let / var mut = var / const = const". So **a declaration without a type takes the type of its value**; there is no inference keyword or mark. Where a written type goes (`let x i32 = 5;` or `let x: i32 = 5;`) waits on how parameters are written. Not built.

### 2026-10-28 — A `var` must change: a `var` never changed in its scope is refused, naming `let` (owner: (c))

The owner: "Should `let` & `var` be a promise? Like, `var` must change?" Design claude: `let` already is one; offered (a) no promise, (b) a warning, "never changes; use `let`" (GO2 has no warnings yet), (c) a refusal; and recommended (c), calling it Go's spirit. The owner: "(c), but your options says otherwise for Go". Design claude: both were true but read as a contradiction. Go has no `let`, every variable may change, so on change Go is (a); what it refuses is a variable never *used*, a different check; (c) borrows only that strictness. So **a `var` with no assignment to it in its scope is refused, naming `let`**; `let` and `var` each say something true. Not built.

### 2026-10-28 — A `var` is changed by `x = …;`, as in Go (owner: (a))

Design claude offered (a) `x = x + 1;`, as Go and C, (b) a keyword, `set x = x + 1;`, (c) either, plus compound forms such as `x += 1;` (dropped by CyborgPL the same day); and recommended (a) without `+=`. The owner: "(a)". So **a change is `<name> = <value>;`**, told apart from a declaration by having no `let`, `var` or `const` in front; compound forms are not part of it. Not built.

### 2026-10-28 — A written type follows its name after `:`, `let x: i32 = 5;`, `fn add[a: i32, b: i32] -> i32:` (owner: (b))

Set aside earlier for variables first, then asked for both: design claude offered (a) `name type`, as Go, (b) `name: type`, as Rust, TypeScript and CyborgPL, (c) `type name`, as C; and recommended (a), leaving `:` one job. The owner: "(b)". So **a variable or parameter is `<name>: <type>`**; `:` both labels a type and opens a block. Leaving the type out of a variable, `let x = 5;`, infers it. Whether `[a, b: i32]` shares a type is not decided. Not built.

### 2026-10-28 — A `const` is baked in: its value is known when compiling (owner: (a))

Design claude offered (a) `const` known when compiling, as Go (it can size arrays, stand outside functions, cost nothing at run time), `let` fixed once set but worked out while running, (b) `const` deep, nothing inside its value changing, (c) both; and recommended (a), keeping whether `let` reaches inside its value for when lists and structs come. The owner: "`const` must be baked in". So **a `const`'s value is worked out by the compiler and baked into the program; `let` is set once, while running**. What a `const` may hold, and whether `let` goes deep, are not decided. Not built.

### 2026-10-28 — Every parameter says `let` or `var`, `fn add[let a: i32, var b: i32]` (owner: (b))

With `fn add[a: i32, b: i32]` as the example, the owner: "Though, it should be `fn add[var a: i32, var b: i32]` or something, idk". Design claude offered (a) a bare parameter a `let`, `var` making it changeable, (b) every parameter saying which, (c) parameters never changing, copied into a `var` to change; and recommended (a). The owner: "(b)". So **a parameter is `let <name>: <type>` or `var <name>: <type>`**, as a variable is declared; a bare one is refused, and a `var` one must be changed in the body, as any `var`. Whether a parameter may be `const` is not decided. Not built.

### 2026-10-28 — An `if` chain closes once, `;if` after its last branch; `else if` is two words (owner: (a))

Design claude offered (a) one closer for the whole chain, each `else if …:` or `else:` line starting the next branch, (b) each branch closing itself (`;if`, then `else:` … `;else`), (c) as (a) but `elif`; and recommended (a). The owner: "Ask CyborgPL dclaude". CyborgPL's design claude: CyborgPL has (a) with `else if` (its DECISIONS.md, 2026-10-27), having had (b) for one day; a reviewer's point reversed it, that a named closer should mark the real end, and a `;if` with an `else` after it does not; the `else` line itself shows where a branch ends. The owner: "yes". So **`if c:` … `else if d:` … `else:` … `;if`**, and a `;if` right before an `else` is refused, as it would claim an end that is not one. `match` and loops are not decided. Not built.

### 2026-10-28 — `for` is the only loop: `for:` forever, `for c:` while `c` holds, closed by `;for` (owner: (a))

Design claude offered (a) only `for`, as Go: `for:` forever, `for n < 10:` while a condition holds, `for item in items:` over a collection or range; (b) `for` and `while`, as CyborgPL; (c) (b) and `loop:`; and recommended (a), adding that with no `x += 1` a C-style three-part `for` would not fit. The owner: "(a)". So **every loop is `for`, closed by `;for`; `for:` runs forever and `for <condition>:` runs while it holds**. How a loop walks over things (`in`, ranges) is not decided, nor are `break` and `continue`. Not built.

### 2026-10-28 — A `for` walks with `in`: `for x in xs:`, `for i, x in xs:`, `for i in 0..10:` (owner: (a))

Design claude offered (a) `for x in xs:`, with a position `for i, x in xs:`, over numbers `for i in 0..10:`; (b) Go's `range`, `for i, x in range xs:`; and as (c) a "CyborgPL-style" `for i in i32[min: 0, max: 9]:`. The owner: "What in the curse shit did I just see". CyborgPL's design claude then corrected it: that loop is not CyborgPL (`i32[min: 0, max: 9]` is a bounded type, and its oracle refuses the loop); CyborgPL counts with `for $i [from: 0, to: 9, by: 1]:` and walks with `for $x in lend $xs:`. Design claude re-offered (c) as that, and recommended (a). The owner: "(a)". So **`for <name> in <things>:`, `for <pos>, <name> in <things>:`, and `<a>..<b>` for a run of numbers**. Whether `0..10` includes 10, and whether the loop's names are `let`, are not decided. Not built.

### 2026-10-28 — An index counts from 1: `xs[1]` is the first item (owner)

Asked whether `0..10` includes 10, the owner: "`xs[0]` is the position before 1, but what position even is that?" Design claude: 0-based counts an offset from the start (C, Go, Rust, Python), 1-based a position (Lua, Julia, Fortran); each pairs with a range rule, 0 with ranges stopping before the end, 1 with ranges including it; and recommended 0, LLVM's C API counting from 0. The owner: "I mean, look at GO2. It doesn't have manual memory". Design claude agreed the offset reading only matters where memory shows, which GO2 hides; the one cost left is where GO2 meets C and LLVM, paid once in a thin wrapper; and changed to recommending 1, with ranges including the end. The owner: "yes". So **the first item is `xs[1]` and the last `xs[n]`**, written in `[ ]` as it holds; a 0 is out of range. How a length is asked for is not decided. Not built.

### 2026-10-28 — `a..b` includes `b`: `for i in 1..n:` visits 1 to n (owner)

Asked whether `0..10` includes 10, the owner: "`xs[0]` is the position before 1, but what position even is that?" Design claude: 0-based counts an offset from the start (C, Go, Rust, Python), 1-based a position (Lua, Julia, Fortran); each pairs with a range rule, 0 with ranges stopping before the end, 1 with ranges including it; and recommended 0, LLVM's C API counting from 0. The owner: "I mean, look at GO2. It doesn't have manual memory". Design claude agreed the offset reading only matters where memory shows, which GO2 hides; the one cost left is where GO2 meets C and LLVM, paid once in a thin wrapper; and changed to recommending 1, with ranges including the end. The owner: "yes". So **`1..10` is the ten numbers 1 to 10**, and a loop over positions is `for i in 1..n:`. Whether there is a form stopping before the end is not decided. Not built.

### 2026-10-28 — A comment is `//` and takes its own line; one after code is refused (owner)

Design claude offered (a) `//` to the end of the line, as Go, (b) `#`, (c) `--`, and asked whether a block comment (`/* … */`) comes too; and recommended (a), line comments only. The owner answered with an example: "`//correct` / `KWFHGugUJFWhuF //wrong`". So **a comment is a line starting `//`** (after any whitespace, which means nothing); a `//` after code on the same line is refused. Whether there is a block comment is not decided. Not built.

### 2026-10-28 — No block comment; a longer note is several `//` lines (owner: (a))

Design claude offered (a) only `//` lines, (b) `/* … */` on lines of its own; and recommended (a), editors commenting a chunk out line by line. The owner: "(a), and btw no comments on a line with code", the rule of the entry before. So **`//` lines are the only comments**. Not built.

### 2026-10-28 — "And", "or" and "not" are `&&`, `||`, `!`, as Go (owner: (b))

Design claude offered (a) words, `and`, `or`, `not`, (b) Go's symbols, `&&`, `||`, `!`, (c) both as two spellings; and recommended (a). The owner: "(b)". So **`if x > 0 && !done:`**. Whether `&&` and `||` skip their right side when the left decides, as Go's do, and what single `&`, `|` mean, are not decided. Not built.

### 2026-10-28 — `&&` and `||` stop early: in `a && b`, `b` runs only if `a` is true (owner)

Design claude listed, among how booleans work, "`&&` and `||` stop early", as Go's do. The owner: "`and`, `or`, `not` don't short circuit". Design claude read it as both sets existing, the symbols stopping early and the words always running both sides (as Java and C# split `&&` from `&`), and asked what `not` is for beside `!`. The owner: "yes, ignore `not`". So **`&&` and `||` short-circuit**: `i <= n && xs[i] > 0` never reads past the end. Not built.

### 2026-10-28 — `and` and `or` always run both sides; there is no `not` (owner)

Design claude listed, among how booleans work, "`&&` and `||` stop early", as Go's do. The owner: "`and`, `or`, `not` don't short circuit". Design claude read it as both sets existing, the symbols stopping early and the words always running both sides (as Java and C# split `&&` from `&`), and asked what `not` is for beside `!`. The owner: "yes, ignore `not`". So **`a and b` and `a or b` run `a`, then `b`, whatever `a` gave**, for when both sides do something; negation is `!` alone, and `not` is no keyword. This adds the words beside the symbols chosen earlier, each with its own meaning. Not built.

### 2026-10-28 — The boolean type is `bool`, its values `true` and `false` (owner)

Design claude listed how booleans work, as in Go, and recommended confirming each. The owner: "confirm". So **`let done: bool = false;`**. Not built.

### 2026-10-28 — A condition must be a `bool`; `if count:` is refused, naming `count != 0` (owner)

Design claude listed how booleans work, as in Go, and recommended confirming each. The owner: "confirm". So **`if`, `else if` and `for <condition>:` take only a `bool`**; there is no truthiness. Not built.

### 2026-10-28 — Comparisons are `==`, `!=`, `<`, `<=`, `>`, `>=` (owner)

Design claude listed how booleans work, as in Go, and recommended confirming each. The owner: "confirm". So **`a == b`**, each giving a `bool`. What can be compared with what is not decided. Not built.

### 2026-10-28 — Arithmetic is `+`, `-`, `*`, `/`, `%`, as Go (owner)

Design claude listed how arithmetic works and recommended confirming each, and for a result that does not fit its type offered (a) wrapping, as Go, silently, (b) stopping with an error naming the line; recommending (b). The owner: "correct, but (b) on debug, and (a) on release". So **`%` is the remainder**. Not built.

### 2026-10-28 — Integer `/` drops the fraction: `7 / 2` is `3` (owner)

Design claude listed how arithmetic works and recommended confirming each, and for a result that does not fit its type offered (a) wrapping, as Go, silently, (b) stopping with an error naming the line; recommending (b). The owner: "correct, but (b) on debug, and (a) on release". So **`/` on integers gives an integer**, the fraction dropped. Which way a negative result rounds is not decided. Not built.

### 2026-10-28 — Both sides of an operator have the same type; `a_i32 + b_i64` is refused (owner)

Design claude listed how arithmetic works and recommended confirming each, and for a result that does not fit its type offered (a) wrapping, as Go, silently, (b) stopping with an error naming the line; recommending (b). The owner: "correct, but (b) on debug, and (a) on release". So **one side is converted first**, as in Go. How a conversion is written, and the type of a bare `5`, are not decided. Not built.

### 2026-10-28 — A result too big for its type stops the program in a debug build, and wraps in a release build (owner)

Design claude listed how arithmetic works and recommended confirming each, and for a result that does not fit its type offered (a) wrapping, as Go, silently, (b) stopping with an error naming the line; recommending (b). The owner: "correct, but (b) on debug, and (a) on release". So **an `i8` at 127 plus 1 stops with an error naming the line in a debug build, and gives -128 in a release build**. This makes debug and release builds part of GO2; how one is asked for is not decided. Not built.

### 2026-10-28 — Dividing by zero stops the program with an error naming the line (owner)

Design claude listed how arithmetic works and recommended confirming each, and for a result that does not fit its type offered (a) wrapping, as Go, silently, (b) stopping with an error naming the line; recommending (b). The owner: "correct, but (b) on debug, and (a) on release". So **`x / 0` and `x % 0` stop the program**, in debug and release builds alike. Not built.

### 2026-10-28 — A failure is a value the caller must handle; forgetting is refused (owner: (b))

Asked how a conversion is written, the owner: "Let's do err handling first". Design claude offered (a) Go's, the error an extra return value, checked by convention (bringing `nil` and several return values), (b) a failure a value the caller must handle, as Rust, Zig and CyborgPL: the signature says it can fail, and the result cannot be used until the caller says what happens (`?` passes it up, `??` gives a fallback, or each reason handled), forgetting a compile error, (c) exceptions; and recommended (b). The owner: "(b), and a way to make named reusable handlers (but not with a `coll`)". So **a function that can fail says so in its signature, and a call to it must say what happens on failure before its result is used**; there are no exceptions and no unchecked error values. How a signature says it, whether `?` and `??` are the spellings, and how each reason is handled, are not decided. Not built.

### 2026-10-28 — A handler can be named and reused, though not as CyborgPL's `coll.handler` (owner)

Asked how a conversion is written, the owner: "Let's do err handling first". Design claude offered (a) Go's, the error an extra return value, checked by convention (bringing `nil` and several return values), (b) a failure a value the caller must handle, as Rust, Zig and CyborgPL: the signature says it can fail, and the result cannot be used until the caller says what happens (`?` passes it up, `??` gives a fallback, or each reason handled), forgetting a compile error, (c) exceptions; and recommended (b). The owner: "(b), and a way to make named reusable handlers (but not with a `coll`)". So **a way of handling failures can be written once, named, and used at many calls**; it is not a `coll` (CyborgPL's `coll.handler` collections). Its spelling is not decided. Not built.

Clarified the same day, with the next question: "by 'named reusable handlers', I mean CyborgPL's kind but not with a `coll`". CyborgPL's kind (its `examples/collection_handlers.cy`, `handler_arguments.cy`): a named set of arms, one per reason the call can fail, used at a call as if written there, so an arm may `ret`, `break`, `continue` or `fail` from where it is used; one missing a reason the call can fail is refused there; it may take arguments. CyborgPL groups them as members of a `coll.handler`; **in GO2 a handler is that, without the collection around it**.

### 2026-10-28 — A function lists the ways it can fail by name, `-> i32 or fail[empty, not_a_number]` (owner: (a))

Design claude offered (a) named reasons in the signature, as CyborgPL and Zig, `fn parse[let s: str] -> i32 or fail[empty, not_a_number]:`, each able to carry data, adding one breaking every caller on purpose; (b) one error type per function, as Rust (needing sum types first); (c) one universal error value, as Go's `error`; and recommended (a): it needs no other feature first, makes handlers exact, and `fail[…]` fits `[ ]` holding. The owner: "(a)". So **`-> <type> or fail[<reason>, …]`**; a reason with data is `<reason>: <type>`, as a parameter is. How a function fails (a `fail` statement or other), and the spelling for one that returns nothing but can fail, are not decided. Not built.

### 2026-10-28 — A function fails with `fail <reason>;`, data in `[ ]`: `fail bad_char[c];` (owner)

Design claude listed, after CyborgPL: 1. `fail empty;` / `fail bad_char[c];` leaves with that reason, 2. `f(x)?` passes it up, 3. `f(x) ?? 0` a fallback value, 4. arms for each reason after `otherwise:` at the call, 5. `f(x) otherwise name` a named handler, 6. a call that can fail stands alone; recommending all. The owner: "Or what about 1-3, but named handlers only, no 4?" Design claude agreed (call sites one line, arms in one place, arguments carrying what a call has; CyborgPL itself has 98 `?? value` against 7 named handlers) and listed 1, 2, 3, 5, 6. The owner: "yes, but no `otherwise` anymore, just `??`, and no `?` too". So **`fail` leaves the function with one of the reasons its signature lists**; a reason not listed is refused. Not built.

### 2026-10-28 — `??` is the only way to handle a failure: a value, or a named handler (owner)

Design claude listed, after CyborgPL: 1. `fail empty;` / `fail bad_char[c];` leaves with that reason, 2. `f(x)?` passes it up, 3. `f(x) ?? 0` a fallback value, 4. arms for each reason after `otherwise:` at the call, 5. `f(x) otherwise name` a named handler, 6. a call that can fail stands alone; recommending all. The owner: "Or what about 1-3, but named handlers only, no 4?" Design claude agreed (call sites one line, arms in one place, arguments carrying what a call has; CyborgPL itself has 98 `?? value` against 7 named handlers) and listed 1, 2, 3, 5, 6. The owner: "yes, but no `otherwise` anymore, just `??`, and no `?` too". So **`f(x) ?? 0` uses a value whatever the reason, and `f(x) ?? name` hands each reason to a named handler**; there are no arms at the call and no `otherwise`. How `?? name` is told from a value named `name` is not decided. Not built.

### 2026-10-28 — There is no `?`; a failure is not passed up by a mark (owner)

Design claude listed, after CyborgPL: 1. `fail empty;` / `fail bad_char[c];` leaves with that reason, 2. `f(x)?` passes it up, 3. `f(x) ?? 0` a fallback value, 4. arms for each reason after `otherwise:` at the call, 5. `f(x) otherwise name` a named handler, 6. a call that can fail stands alone; recommending all. The owner: "Or what about 1-3, but named handlers only, no 4?" Design claude agreed (call sites one line, arms in one place, arguments carrying what a call has; CyborgPL itself has 98 `?? value` against 7 named handlers) and listed 1, 2, 3, 5, 6. The owner: "yes, but no `otherwise` anymore, just `??`, and no `?` too". So **`f(x)?` is not GO2**. How a failure reaches the caller's caller, if at all, is not decided. Not built.

### 2026-10-28 — A call that can fail stands alone, as a statement or a declaration's whole value (owner)

Design claude listed, after CyborgPL: 1. `fail empty;` / `fail bad_char[c];` leaves with that reason, 2. `f(x)?` passes it up, 3. `f(x) ?? 0` a fallback value, 4. arms for each reason after `otherwise:` at the call, 5. `f(x) otherwise name` a named handler, 6. a call that can fail stands alone; recommending all. The owner: "Or what about 1-3, but named handlers only, no 4?" Design claude agreed (call sites one line, arms in one place, arguments carrying what a call has; CyborgPL itself has 98 `?? value` against 7 named handlers) and listed 1, 2, 3, 5, 6. The owner: "yes, but no `otherwise` anymore, just `??`, and no `?` too". So **`let n = parse(a) ?? 0;`, never `parse(a) + parse(b)`**. Not built.

### 2026-10-28 — `?? fail` passes a failure up: the same reason, with its data, to my caller (owner: (b))

With `?` gone, design claude offered (a) a named handler whose arms `fail` again, (b) `?? fail`, a bare `fail` after `??` sending the same reason up, (c) not at all, each caller handling every failure; and recommended (b): short, spelled with words GO2 has, `??` staying the one way to handle. The owner: "(b)". So **`let n = parse(t) ?? fail;`**, and my own `fail[…]` must list every reason that can come through, or it is refused. Not built.

### 2026-10-28 — Handlers live in a `coll.handler`, used as `?? parse.parse_failed` (owner)

Asked how a named handler is defined, design claude proposed a standalone `handler parse_failed[…]:` … `;handler parse_failed`, CyborgPL's member without the collection, as the owner had asked. The owner: "Actually, let's do `coll.handler`, so it could be organized well. It becomes say `parse.parse_failed` at call". So **handlers are grouped in a named `coll.handler`, and a call names the collection and the handler: `parse(t) ?? parse.parse_failed`**. This reverses "not with a `coll`" (the two `decision.failures.named` entries above). How a handler is written inside the collection, and whether `coll` groups anything but handlers, are not decided. Not built.

### 2026-10-28 — Inside a `coll.handler`, a handler is `member <name>[…]:` … `;member <name>`, the full name only at the call (owner: (b))

Design claude offered (a) CyborgPL's, the full name everywhere, `member parse.parse_failed[…]:`, (b) the short name inside, `member parse_failed[…]:` … `;member parse_failed`, (c) as (b) with `handler` for `member`; and recommended (b), the `parse.` prefix only repeating the block it is in. The owner: "(b)". So:

```
coll.handler parse:
    member parse_failed[let fallback: i32]:
        case empty:
            ret fallback;
        ;case empty
    ;member parse_failed
;coll.handler parse
```

used as **`parse(t) ?? parse.parse_failed(0)`**, arguments in `( )` as the use runs it. An arm is `case <reason>:` … `;case <reason>`, as proposed; how an arm names a reason's data, and whether a handler without parameters is used bare, are not decided. Not built.

### 2026-10-28 — An arm names a reason's data after it, `case bad_char c:`, a `let` inside the arm (owner)

Design claude listed three handler rules, after CyborgPL's, and recommended confirming each. The owner: "yes". So **`case <reason> <name>:` binds the data**; it never changes. Not built.

### 2026-10-28 — A member without parameters is used bare, `?? parse.skip` (owner)

Design claude listed three handler rules, after CyborgPL's, and recommended confirming each. The owner: "yes". So **no `()` on a member that takes nothing**. Not built.

### 2026-10-28 — A member must cover exactly the reasons the call can fail with (owner)

Design claude listed three handler rules, after CyborgPL's, and recommended confirming each. The owner: "yes". So **a missing reason is refused at the call, naming it, and so is an arm for a reason the call cannot give**; a function gaining a reason breaks every handler used with it until it is covered. Not built.

### 2026-10-28 — An arm only leaves, from where its handler is used: `ret`, `break`, `continue`, `fail` (owner: (a))

Design claude asked what `ret fallback;` in an arm does, offering (a) it only leaves, as in CyborgPL: `ret 0;` returns from the function the handler is used in, and `break`, `continue`, `fail` act there too, a value reaching the declaration only by a plain `?? 0`; (b) an arm gives the `??` its value, and cannot leave; (c) both, `ret` leaving and a new word (`use 0;`) giving a value; and recommended (a). The owner: "Before I answer this, if we choose (a), would there be an alt for things that (c) covers?" Design claude: a small function around the call, its arms' `ret` becoming the value (`fn parse_or_code[…] -> i32:` with `?? parse.as_code`, `ret 0;` / `ret -1;`), as CyborgPL's `half_or_zero` does. The owner: "(a)". So **an arm leaves the function, loop or failure it is used in; it never gives the `??` a value**. How the call shows its handler can leave is not decided. Not built.

### 2026-10-28 — No mark at a call for a handler that can leave; on a call giving a value, every arm must leave (owner: (a))

Design claude noted that, an arm only leaving, a handler on a call that gives a value (`let n = parse(t) ?? parse.x;`) always leaves, and only one on a call with no value (`save(f) ?? save.report;`) may carry on; offered (a) no mark, `?? coll.member` already saying a handler decides, an arm on a value-giving call that does not end by leaving refused, one on a call with no value free to carry on; (b) CyborgPL's `*` at the use, `?? parse.*as_code`, required where the handler can leave; and recommended (a), `*` landing on nearly every use. The owner: "(a)". So **`?? parse.as_code` is written bare; an arm used where a value is needed ends by leaving, or is refused**. Not built.

### 2026-10-28 — Blocks are `{ … }`, as Go; no `:` opener and no `;kind name` closer (owner: (a))

After the failure-handling example (a `coll.handler` thick with `;case …` closers), the owner: "Should we change to `{}`?" Design claude took it to mean blocks and listed what `{ }` costs (the closer no longer says what it closes; the closer, `:`-opener, `;if` and `member`/`case` closer entries reversed; CyborgPL went the other way), placed the noise in each `case` closing itself, and offered (a) `{ }` everywhere, as Go, (b) closers kept, arms closing as the `if` chain does, (c) as it is; recommending (b). The owner: "(a), and allow trailing `//`". So **`fn main[] { … }`, `if c { … } else if d { … } else { … }`, `for x in xs { … }`**; `:` only labels a type, and `;` only ends a statement. **This reverses** the block closer (`;fn main`, `;if`), the required `:` opener, the `if` chain's single `;if` (the chain is now Go's `} else if … {`), the loops' `;for`, and the `coll.handler` / `member` closers, which take braces too. Where `{` must sit, and how a handler's arms are written, are not decided. Not built.

### 2026-10-28 — A `//` comment may follow code on its line (owner)

After the failure-handling example (a `coll.handler` thick with `;case …` closers), the owner: "Should we change to `{}`?" Design claude took it to mean blocks and listed what `{ }` costs (the closer no longer says what it closes; the closer, `:`-opener, `;if` and `member`/`case` closer entries reversed; CyborgPL went the other way), placed the noise in each `case` closing itself, and offered (a) `{ }` everywhere, as Go, (b) closers kept, arms closing as the `if` chain does, (c) as it is; recommending (b). The owner: "(a), and allow trailing `//`". So **`let n = 5; // why`** is allowed; a comment still runs to the end of its line, and there is still no block comment. **This reverses** "a comment takes its own line; one after code is refused". Not built.

### 2026-10-28 — A handler's arm is braced like any block, `case empty { continue; }` (owner: (b))

With blocks now `{ }`, design claude offered (a) Go's `switch` form, `case empty:` running to the next `case`, (b) each arm in its own braces; and recommended (a). The owner: "How does (b) look?" Shown both, and (b) on one line (`case empty { continue; }`), design claude changed to leaning (b): as compact, `:` keeping its one job, braces the only block. The owner: "(b)". So **`case <reason> { … }` and `case <reason> <name> { … }`**, inside `member <name> { … }`, inside `coll.handler <name> { … }`. Not built.

### 2026-10-28 — `{` may sit anywhere; line breaks, like indentation, mean nothing to the compiler (owner: (b))

Design claude offered (a) `{` on the header's line and `} else {` on one line, as Go (which needs it for its semicolon insertion), (b) anywhere, GO2 writing every `;` itself; and recommended (b), as indentation is already the writer's, a formatter giving one look later. The owner: "(b)". So **`fn main[]\n{` and `fn main[] {` are the same**; a line break matters only as the end of a `//` comment, and so are `}\nelse {` and `} else {`. A formatter is not decided. Not built.

### 2026-10-28 — A conversion calls the type, `i64(x)`, as Go (owner: (a))

Design claude offered (a) `i64(x)`, as Go, (b) `x as i64`, as Rust, (c) `to.i64(x)`, as CyborgPL; and recommended (a), Go's and shortest, and running, so in `( )`. First asked before failures existed; the owner: "Actually, `i64[]` is for bounds in CyPL, and it uses `otherwise`, `?`, and `??` err handling with `coll.handler` collections", correcting design claude's account of (c), then "Let's do err handling first"; asked again after. The owner: "(a)". So **`i64(x)` converts `x` to `i64`**; whether a narrowing that does not fit follows the overflow rule (stopping a debug build, wrapping in a release one), as design claude assumed in offering it, is not confirmed. Whether text converts this way (`i32("42") ?? 0`, failing `not_a_number`) is not decided. Not built.

### 2026-10-28 — A bare number takes the type of the variable it meets: `n + 1`, `let x: i8 = 5;` (owner)

Asked what type a bare number has when nothing says it, design claude offered a fixed 64-bit `int` (as Go); the owner: "Is `int` a bigint?" and, told no, "Then, it isn't math-accurate, nor says its size". Design claude agreed and offered (a) no `int`, a bare number defaulting to `i64`, (b) `int` a real bigint. The owner: "What even is the question for?" Shown where it comes up (`let n = 5;`, `let m: i32 = n + 1;`), the owner: "We don't have Precog. so lazy inference it is. But what width? Btw, bare numbers in a variable will assume to be the same type as the variable". So **in `n + 1` with `n: i32`, the `1` is an `i32`**, and a bare number given to a typed variable or parameter is that type. Not built.

### 2026-10-28 — Lazy inference: a variable takes its type from its value when declared, never from later use (owner)

Asked what type a bare number has when nothing says it, design claude offered a fixed 64-bit `int` (as Go); the owner: "Is `int` a bigint?" and, told no, "Then, it isn't math-accurate, nor says its size". Design claude agreed and offered (a) no `int`, a bare number defaulting to `i64`, (b) `int` a real bigint. The owner: "What even is the question for?" Shown where it comes up (`let n = 5;`, `let m: i32 = n + 1;`), the owner: "We don't have Precog. so lazy inference it is. But what width? Btw, bare numbers in a variable will assume to be the same type as the variable". Design claude first read it as `n` in `let n = 5;` taking its type from a later `let m: i32 = n + 1;`; the owner: "Wrong, I meant `1` becomes `i32`" (the entry above). So **`let n = 5;` settles `n`'s type there and then**, the inference stopping at the declaration; with no type around a bare number, it takes a default width, not decided. Not built.

### 2026-10-28 — A bare number with nothing around it is an `i64` (owner: (a))

Design claude offered (a) `i64`, (b) `i32`, (c) none, `let n = 5;` refused; and recommended (a). The owner: "Pros and cons of (a) & (b)". For `i64`: overflow practically never (millisecond times, files over 2 GB), no cost per operation on a 64-bit machine, a loop counter matching an `i64` length; against, twice the memory in large number lists, conversions where C and LLVM take 32-bit numbers. For `i32`: half the memory, matching C's `int` and LLVM's C API, Rust's default; against, overflow at 2,147,483,647, which real data reaches, and lengths either capped or needing a conversion in every counting loop. The owner: "(a)". So **`let n = 5;` makes `n` an `i64`**. The type of a length is not decided. Not built.

### 2026-10-28 — A struct is declared `struct <Name> { … }`, a field `<name>: <type>;` (owner: (a))

Design claude offered (a) `struct Point { x: i64; y: i64; }`, as Rust with `;` after each field, (b) `type Point struct { … }`, as Go, (c) as (a) with `,` between fields; and recommended (a). The owner: "(a), but where's `let` & `var`?" So **`struct Point { … }`, fields ending in `;`**. Whether a field says `let` or `var`, as a parameter does, is asked next. Not built.

### 2026-10-28 — Every field says `let` or `var`; a `var` field never changed anywhere in the program is refused (owner: (a))

From "where's `let` & `var`?" on fields, design claude offered (a) every field saying which, a `var` field required to change somewhere in the program (the compiler building the whole program at once), (b) every field saying which, fields exempt from the must-change rule, (c) a bare field `let`, `var` marking one; and recommended (a), as for parameters. The owner: "(a)". So **`struct Token { let kind: TokenKind; var line: i64; }`**; a `let` field is set only when the struct is built, and a `var` field with no change to it anywhere is refused, naming `let`. If separate compilation ever comes, this is to be revisited. Not built.

### 2026-10-28 — A struct is built in `[ ]`, `Point[x = 1, y = 2]`; a field is read `p.x` (owner: (a))

Design claude offered (a) `Point[x = 1, y = 2]`, `[ ]` holding and `=` setting a field as it sets a variable, as CyborgPL builds, (b) `Point{x: 1, y: 2}`, as Go and Rust (braces now blocks, `if p == Point{x: 1} {` ambiguous), (c) `Point(x = 1, y = 2)` (building runs nothing, so not `( )`); and recommended (a). The owner: "(a)". So **`let p = Point[x = 1, y = 2];`, and `p.x` reads a field**. Whether every field must be given, and in what order, is not decided. Not built.

### 2026-10-28 — Building a struct gives every field, by name, in any order (owner: (a))

Design claude offered (a) every field, by name, any order, a missing one refused, (b) a missing field a zero value, as Go (every type then needing a zero, Go's `nil` troubles), (c) a field able to declare a default and be left out; and recommended (a), defaults addable later without breaking code. The owner: "(a)". So **`Point[y = 2, x = 1]` is fine; `Point[x = 1]` is refused, naming `y`**; there are no zero values. Not built.

### 2026-10-28 — An `enum`'s cases can carry data, shaped as failure reasons: `plus;`, `number: i64;` (owner: (a))

Design claude offered (a) one `enum` whose cases are a bare name or `name: type`, as a failure reason, built `Token.number[42]` as `fail bad_char[c]`; (b) `enum` for plain names and `union` for cases with data; (c) nothing new, as Go (constants and interfaces); and recommended (a), the shape failure reasons already have, and what makes a compiler pleasant to write. The owner: "(a)". So **`enum Token { plus; name: str; number: i64; }`, a value built `Token.plus` or `Token.number[42]`**. Whether a case says `let` or `var`, and how code tells cases apart, are not decided. Not built.

### 2026-10-28 — `match` tells an enum's cases apart, arms as a handler's, every case covered (owner: (a))

Design claude offered (a) `match tok { case plus { … } case number v { … } }`, arms shaped as handler arms, a missing case refused and named, (b) as (a) under Go's `switch` (which checks no coverage), (c) only `if tok is number v { … }`; and recommended (a). The owner: "(a)". So **`match <value> { case <case> { … } case <case> <name> { … } }`**, the data a `let` inside the arm, every case covered. A way to say "every other case", and whether a `match` gives a value, are not decided. Not built.

### 2026-10-28 — A `match`'s last arm may be `else { … }`, every case not named above (owner: (a))

Design claude offered (a) `else { … }` as the last arm, as in an `if` chain, (b) `case _ { … }`, (c) no catch-all; and recommended (a), noting a `match` with `else` does not break when a case is added. The owner: "(a)". So **`match tok { case plus { … } else { … } }`**; an `else` covering nothing (every case already named) is not decided. Not built.

### 2026-10-28 — The string type is `str` (owner: (a))

Design claude offered (a) `str`, as Rust and Python, short as `fn`, `ret`, `pkg`, (b) `string`, as Go, (c) `text`; and recommended (a). The owner: "(a)". So **`let name: str = "GO2";`**. How a `str` behaves (encoding, changing, joining, indexing) is not decided. Not built.

### 2026-10-28 — A `str` holds UTF-8 bytes (owner)

Design claude listed how a `str` behaves: 1. UTF-8 bytes, 2. never changing in place, 3. `s[i]` the i-th byte, 4. `a + b` joining, as Go (CyborgPL having moved to the word `concat`); and recommended all. The owner: "yeah, but `concat". So **a `str` is a run of UTF-8 bytes**. Whether one that is not valid UTF-8 can exist is not decided. Not built.

### 2026-10-28 — A `str` never changes in place; changing a `var` gives it a new one (owner)

Design claude listed how a `str` behaves: 1. UTF-8 bytes, 2. never changing in place, 3. `s[i]` the i-th byte, 4. `a + b` joining, as Go (CyborgPL having moved to the word `concat`); and recommended all. The owner: "yeah, but `concat". So **joining onto a `var` string builds a new string** and sets the `var` to it; a string is shared safely. Not built.

### 2026-10-28 — `s[i]` is the i-th byte of a `str`, a `u8`, counting from 1 (owner)

Design claude listed how a `str` behaves: 1. UTF-8 bytes, 2. never changing in place, 3. `s[i]` the i-th byte, 4. `a + b` joining, as Go (CyborgPL having moved to the word `concat`); and recommended all. The owner: "yeah, but `concat". So **`s[1]` is the first byte**; walking over characters is a separate way, not decided. Not built.

### 2026-10-28 — Strings are joined with `concat`, not `+` (owner)

Design claude listed how a `str` behaves: 1. UTF-8 bytes, 2. never changing in place, 3. `s[i]` the i-th byte, 4. `a + b` joining, as Go (CyborgPL having moved to the word `concat`); and recommended all. The owner: "yeah, but `concat". So **joining is spelled `concat`**, and `+` on two `str` is refused. Whether it is a word between them (`a concat b`) or a call (`concat(a, b)`) is not decided. Not built.

### 2026-10-28 — `concat` is a call joining any number of strings, `concat(a, b, "!")` (owner: (b))

Design claude offered (a) a word between strings, `a concat b concat "!"`, as `and` / `or`, (b) a call, `concat(a, b, "!")`; and recommended (b): any number of pieces in one new string, no precedence, and running is `( )`. The owner: "(b)". So **`let s = concat(a, b, "!");`**. Whether `concat` is built in or comes from a package is not decided. Not built.

### 2026-10-28 — A list type is `list[i64]`; a literal is `[1, 2, 3]`; an item is `xs[i]` (owner: (a))

Design claude offered (a) `list[i64]`, the type described in `[ ]`, (b) Go's `[]i64`, (c) Swift's `[i64]`, a literal `[1, 2, 3]` and `xs[i]` (from 1) in all three; and recommended (a), "`[ ]` describes", setting the pattern for other containers (`map[str, i64]`). The owner: "(a)". So **`let xs: list[i64] = [1, 2, 3];` and `xs[1]` is `1`**. Whether a list grows, how one is added to, and its length, are not decided. Not built.

### 2026-10-28 — Lists and structs are values: assigning or passing one gives an independent copy (owner: (a))

With `let xs = [1, 2, 3]; var ys = xs; ys[1] = 9;`, design claude offered (a) values, as Swift, `ys` its own list, copied only when one side changes (copy-on-write); (b) shared, as Go, Java, Python, `let` a shallow promise; and recommended (a), only values keeping `let` true, the cost landing in the compiler and runtime. The owner: "(a)". So **`xs` stays `[1, 2, 3]`; a `let` list or struct never changes, inside or out**. Copy-on-write is how it is made cheap, as proposed; whether anything is ever shared by choice (a reference type) is not decided. Not built.

### 2026-10-28 — GO2 has methods, belonging to a type, as Go: `xs.len()`, `p.dist(q)` (owner: (b))

Asked how a list gives its length and takes an item, design claude offered (a) functions only, `len(xs)`, `push(xs, 4)`, (b) methods belonging to a type, as Go, `xs.len()`, `xs.push(4)`, the receiver saying `let` or `var`, (c) any function also callable with a dot, as D and Nim; and recommended (b). The owner: "(b)". So **a type can have methods, called `<value>.<method>(…)`**; one whose receiver is `var` is called only on a `var`. How a method is declared, and the names of the list methods, are not decided. Not built.

### 2026-10-28 — A method is declared `fn Point.dist[let self, …]`, outside the type, the receiver `let self` or `var self` (owner: (c))

Design claude offered (a) Go's, the receiver before the name, `fn [let p: Point] dist[let q: Point]`, (b) inside the struct's block, `fn dist[let self, …]`, (c) outside, named by type and method, `fn Point.dist[let self, let q: Point] -> i64 { … }`; and recommended (c), the declaration mirroring the call. The owner: "(c)". So **`fn <Type>.<method>[let self, …]` or `[var self, …]`**, declared anywhere; `self` has no written type, being the method's own. Whether a method may be declared for a type from another package is not decided. Not built.

### 2026-10-28 — The list type is `ls`: `ls[i64]` (owner)

Design claude listed how lists work (`xs.push(4)`, `xs.pop()` among them); the owner: "Any alt words for `push` and `pop`?" Offered `push`/`pop`, `append`/`pop`, `add`/`remove_last`, `append`/`remove_last`, `put`/`take`, design claude recommended `append` and `remove_last`. The owner: "Can we abbrev `remove`? Maybe `rem`?" Design claude: it fits `pkg`, `fn`, `ret`, `str`, though `rem` is the usual short name for remainder elsewhere; offered `rem_last`, `del_last`, `rm_last`, `drop_last`. The owner: "`ls` for `list`, and `rm_last`. I didn't approve the others yet". So **`let xs: ls[i64] = [1, 2, 3];`**; this renames `list[i64]` from the entry choosing the list type. Not built.

### 2026-10-28 — Removing a list's last item is `xs.rm_last()` (owner)

Design claude listed how lists work (`xs.push(4)`, `xs.pop()` among them); the owner: "Any alt words for `push` and `pop`?" Offered `push`/`pop`, `append`/`pop`, `add`/`remove_last`, `append`/`remove_last`, `put`/`take`, design claude recommended `append` and `remove_last`. The owner: "Can we abbrev `remove`? Maybe `rem`?" Design claude: it fits `pkg`, `fn`, `ret`, `str`, though `rem` is the usual short name for remainder elsewhere; offered `rem_last`, `del_last`, `rm_last`, `drop_last`. The owner: "`ls` for `list`, and `rm_last`. I didn't approve the others yet". So **`rm_last`**. What it gives back, and what it does on an empty list, are not decided; neither is the rest of the list (growing, length, adding, an index out of range, replacing an item). Not built.

## Open

- What GO2 is, in one sentence. Design claude proposed a small Go remix (Go's simplicity plus sum types and
  pattern matching, little enough to self-host early); not decided.
