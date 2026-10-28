# Compiler session brief

For **compiler claude**, the session that builds G2C2. Started 2026-10-28; the owner: "let another session (compiler
claude) make it, everything unless it needs a GC".

## The job

Build **G2C2**, GO2's bootstrap compiler, and the C++ runtime it links, from [`overview.md`](overview.md) and
[`../DECISIONS.md`](../DECISIONS.md). Where they disagree, `DECISIONS.md` wins (the last entry on a topic; earlier ones
may be reversed).

- `g2c2/`: the compiler, C++23, building IR through LLVM's C++ API (Homebrew LLVM 23 at `/opt/homebrew/opt/llvm`).
- `runtime/`: the C++23 runtime every program links.
- One CMake project for both (`find_package(LLVM)`), compiled with Homebrew clang 23
  (`/opt/homebrew/opt/llvm/bin/clang++`) and its libc++.
- `g2c2 build <file or project>` and `g2c2 run …`, `--release` on both; every build at LLVM `-O1`.
- `examples/`: GO2 programs using settled rules only, each with its expected `.out` (and `.exit` if not 0), run by the
  tests. Programs G2C2 must refuse go in `g2c2/tests/reject/`, each with the expected error's text in `.err`.

## Scope: everything that does not need a GC

In scope, as decided: packages, `project.go2`, `pkg`/`import`/`pub`, comments, every statement and block form,
`let`/`var`/`const` with their promises (a `var` that never changes is refused), integer and float types, literals,
precedence (maths, else parentheses), the same-type rule, conversions, debug and release overflow, division by zero,
`bool` and conditions, `if`/`else if`/`else`, `for` in all forms over ranges, `stop`/`next`, functions, parameters,
`ret`, `main`, `os.exit`, structs (built in `[ ]`, every field given), enums and `match`, methods, failures (`fail`,
`?? value`, `?? fail`, `coll.handler` members and their coverage rules), `fmt.print` with `{}` marks, string literals
and their escapes, and calling C (`extern.c`, `str.c`, opaque types).

**Out of scope until the collector exists:** anything that must allocate memory that outlives its creator. That is
`ls` and `map`, `concat`, `str(bytes)`, `str(c)`, building strings at run time, types that contain themselves, copy-on-write and its shared flag, the collector itself and its
stack maps. If something in scope turns out to need GC memory, stop and say so rather than adding a leak or a stand-in.

## Rules

- **Design authority is the owner's.** You do not decide syntax, naming, semantics or architecture, and you never fill
  in a default for an open question. When the design is silent or unclear, ask: send the question to design claude
  (the session that started you), which asks the owner and records the answer in `DECISIONS.md`. Meanwhile build
  around it.
- Questions to the owner, if you ask directly, go in plain text, one at a time, with a recommendation. **Never** use
  the AskUserQuestion tool, and never put a question with buttons in the end-of-turn box: the owner calls both
  popups.
- **Never edit** `DECISIONS.md` or `design/`; they are design claude's. Report a mistake you find in them instead.
- Fixing a bug in what you have built needs no one's go.

## Git

The form is in `CLAUDE.md`. Yours, in short:

- Author `Tankun Sriket <tankunsriket63741ecff056.invalid>` (repo-local config). Every commit ends with
  `Co-Authored-By: compiler claude <noreply@anthropic.com>`.
- Subjects `kind.folder.narrowing: sentence`: `feature.g2c2.lexer: …`, `fix.runtime.print: …`, `test.examples.match: …`.
- Small commits, one feature or fix each.
- Work on your own branch. Do not push, and do not merge into `main`: the owner decides when your branch lands.
