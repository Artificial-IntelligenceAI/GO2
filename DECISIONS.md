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
definition `fn f[…]`, and `( )` otherwise only groups maths, as in `a + (b * 2)`** (spelling of names and operators not decided). Where else `[ ]` goes (indexing, list
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

## Open

- What GO2 is, in one sentence. Design claude proposed a small Go remix (Go's simplicity plus sum types and
  pattern matching, little enough to self-host early); not decided.
