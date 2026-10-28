# G2C2 progress

Where G2C2 and the runtime stand, kept current by compiler claude. With `git log`, it is enough to pick up where
work stopped.

## Building and testing

```
cmake -S . -B build -G Ninja
cmake --build build
cmake --build build --target check
```

`check` runs every program in `examples/` (in a debug and a release build) and every program in `g2c2/tests/reject/`.
An example `x.go2` (or a project folder `x/` holding `project.go2`) has its expected output in `x.out`, its exit code in
`x.exit` when it is not 0, and what it writes to stderr in `x.err` when it writes anything. A release build expects
`x.release.out` / `x.release.exit` / `x.release.err` where those differ. A reject case `x.go2` has G2C2's error in
`x.err`.

## How G2C2 is laid out

- `g2c2/g2c2.h`: every struct and stage, in one header.
- `g2c2/lex.cpp`: text to tokens.
- `g2c2/parse.cpp`: tokens to a syntax tree per file.
- `g2c2/check.cpp`: names, types and every rule the compiler enforces; resolves the tree in place.
- `g2c2/gen.cpp`: the checked tree to LLVM IR, optimised at `-O1`, written as an object file.
- `g2c2/main.cpp`: `g2c2 build` and `g2c2 run`, finding `project.go2`, linking with clang.
- `runtime/runtime.cpp`: what compiled programs call: printing, stopping, text to number.

Where the design is still open, G2C2 refuses the program with an error ending "is not decided yet", rather than
picking an answer.

## Works

- Hello world, end to end: `fmt.print` of a string literal; the test runner.
- The checker and generator cover the whole GC-free language; each feature below is listed once an example proves it.
- `let`, `var` (must change), `const` (a literal), written types, lazy inference; `_` and hex literals.
- Integer types, literals taking the type they meet, `+ - * / %`, division toward zero, precedence and the
  parenthesis rules (`/` then `*`, `%` beside arithmetic, mixed logic, one-way comparison chains), the same-type rule.
- `bool`, `if`/`else if`/`else`, `for` forever / while / over `a..b` (both ends worked out once, see question 18),
  `stop`, `next`; `&&`/`||` stopping early, `and`/`or` running both sides, comparison chains; `str` comparisons.
- Functions: `let`/`var` parameters (a `var` must change), `-> T`, `ret`, `ret;`, recursion, Go's rule for a
  reachable end (question 16), calls as statements.
- Structs: `let`/`var` fields (a `var` field must change somewhere in the program), building with every field by
  name, field reads, values copied, `==` field by field. Methods `fn T.m[let self, …]` / `[var self, …]`, a
  `var self` method only on a `var` and counting as its change.
- Enums with and without data, `Token.plus`, `Token.number[42]`, `match` with every case or `else` (an `else`
  covering nothing refused), `==` by case and data. A type containing itself is refused (it needs the collector).
- Failures: `-> T or fail[…]`, `-> fail[…]`, `fail r;` / `fail r[data];`, `?? value` (worked out only on failure,
  question 24), `?? fail` (each reason listed, same data), `coll.handler` members with and without arguments, each
  covering exactly the call's reasons, arms leaving by `ret`/`fail`/`stop`/`next` from where they are used, and
  finishing on a call that gives nothing. A failing call stands alone. A member's arms are checked afresh at each
  use, since their data's types come from the call; a member never used is not checked.
- Conversions between integer types (a narrowing handled with `??` fails `out_of_range`), text to an integer
  (`not_a_number`, `out_of_range`, must be handled).
- Strings: literals (checked as UTF-8) with `\n`, `\t`, `\"`, `\\`; `s.len()`; `s[i]` (an `i64` index; out of range
  stops the program); `for c in s.chars()`; `==`, `<` and the rest byte by byte. `fmt.print` with `{}` and `{{`,
  printing integers and strings.

## Next

- Then structs, enums, failures, strings, packages, calling C.

## Open questions

Sent to design claude on 2026-10-28, most blocking first. G2C2 refuses what depends on an unanswered one (its
error says "not decided yet"), except where noted. Each has compiler claude's recommendation.

1. **Where `g2c2 build` writes the program, and its name.** Rec: the current folder, named after the project's `name`
   (a lone file: its name without `.go2`), as `go build` does. Until then `build` checks and compiles, then says so.
2. **What a stopping program prints, and its exit code** (overflow in a debug build, dividing by zero, a narrowing
   that does not fit). Rec: to stderr, `<file>:<line>: <what>` (`main.go2:7: i8 overflow`), exit code 2, as Go's
   panics.
3. **`os.exit`'s parameter type.** Rec: `u8`, as Unix keeps 8 bits; `os.exit(256)` is then refused, not exit 0.
4. **How `{}` prints `bool`, `f32`/`f64`, structs and enums.** Rec: `true`/`false`; a float as the shortest decimal
   that reads back to the same value, written as a GO2 float literal (`2.0`, `0.1`, `6.02e23`), and `inf`, `-inf`,
   `nan`; a struct as its literal, `Point[x = 1, y = 2]`; an enum as `Token.number[42]` or `Token.plus`; `str.c` and
   `extern.c` types refused.
5. **A literal that does not fit the type it takes** (`let x: i8 = 300;`, `let y: u8 = -1;`, a bare
   `9223372036854775808`). Rec: refused when compiling, naming the type's range.
6. **An integer literal meeting a float** (`let x: f64 = 5;`, `y * 2` with `y: f64`). Rec: allowed, `5` becoming
   `5.0`, as "a bare number takes the type of the variable it meets".
7. **`const` outside a function, `pub const`, and a `const` worked out from others** (`const b = a * 2;`). Rec: all
   three allowed.
8. **Shadowing**: may an inner block declare a name an outer block or the package already has, and may one block
   declare a name twice? Rec: neither.
9. **`??` in an assignment and a `ret`**: `x = parse(t) ?? 0;`, `ret parse(t) ?? 0;`. Rec: both allowed, each the
   statement's whole value.
10. **`fmt.print`'s format**: must it be a string literal (or a `str` const)? Is a count of `{}` that does not match
    the values refused? A `{` followed by anything but `{` or `}`? Rec: a literal or `str` const; a mismatch refused;
    such a `{` refused; a lone `}` plain text.
11. **A float to an integer that does not fit, unhandled, in a release build**: wrapping means nothing for infinity
    or NaN. Rec: it stops the program in every build. Also: `f32(x)` of a too-big `f64` gives infinity (G2C2 reads
    "float arithmetic follows IEEE" as covering it); confirm.
12. **Strings**: is `s[i]` out of range handled as a list's (`s[i] ?? 0`, failing `out_of_range`)? Is an index only
    an `i64`? What is `i` in `for i, c in s.chars()`? Rec: as a list's; only `i64`; `for i, c` refused for now.
13. **An arm for a case with data that does not name it**: `case number { … }`. Rec: allowed.
14. **`pub coll.handler`**, used from another package as `?? lexer.parse.skip`. Rec: allowed, showing every member.
15. **After `??`, `a.b` where `a` is both a `coll.handler` and a value in scope.** Rec: refused as ambiguous.
16. **When a function's end cannot be reached** (so `-> T` needs no final `ret`). Rec: Go's rule: it ends with `ret`,
    `fail`, a `for` with no condition and no `stop`, or an `if`/`else` or `match` whose every branch ends so;
    `os.exit` does not count.
17. **`g2c2 build x.go2` where `x.go2` sits inside a project**: the file alone, or its project? Rec: the file alone;
    a folder (or none, the current one) builds the project. G2C2 does this now.
18. **`for i in a..b`: are `a` and `b` worked out once, before the first turn?** Rec: once. G2C2 reads it so (a range
    of numbers is walked); say if not.
19. **`type`, in `extern.c type`, is not a reserved word.** Rec: reserve it. G2C2 reads it as a word only after
    `extern.c`.
20. **A mistake in the overview**: its Calling C example names a parameter `ret` (`let ret: LLVMTypeRef`), a reserved
    word.
21. **A bare `{ … }` block as a statement.** Rec: not for now.
22. **Must imports come right after `pkg`?** Rec: yes.
23. **`??` on something that cannot fail** (a function with no `fail[…]`, a widening conversion). Rec: refused, as
    an `else` covering nothing is.
24. **The value after `??`, and a member's arguments: worked out only when the call fails?** Rec: only then.
25. **Escapes beyond `\n`, `\t`, `\"`, `\\`** (`\0`, `\r`, `\x41`, `\u{…}`), and a line break inside a string
    literal. Rec: no more for now; a line break refused.
26. **`str.c(s)` "is a copy"**, but a copy that outlives the call needs the collector. Every `str` is now a literal
    G2C2 stores with a NUL after it, so `str.c(s)` hands C the literal itself. Rec: keep that until the collector;
    then a copy.
27. **`extern.c` with `f32`/`f64` (C's `float`/`double`) and `bool`.** Rec: floats allowed, `bool` refused.
28. **A NULL from C** (an opaque type or `str.c`): GO2 has no null. Rec: leave it until GoTo needs it.
29. **Import cycles between packages.** Rec: refused, as Go.
30. **`==` on `str.c`.** Rec: refused.
31. **`bool` to and from integers, and text to a float** (`f64("2.5")`). Rec: neither for now.
32. **A range of floats**, `for x in 0.5..2.5`. Rec: refused.
