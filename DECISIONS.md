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

## Open

- What GO2 is, in one sentence. Design claude proposed a small Go remix (Go's simplicity plus sum types and
  pattern matching, little enough to self-host early); not decided.
