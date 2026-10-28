<context>
I'm Tankun Sriket, and GO2 is my hobby programming language: "a Go-style simple programming language, that has fast
compiles, and okay runtime performance." I make every design call. The design was settled with my design session
("design claude") one decision at a time, and it lives in three files I'd like you to read before writing any code:

- `CLAUDE.md`: house rules and the commit form.
- `design/overview.md`: the language as it stands, rules only.
- `DECISIONS.md`: every decision, with my words. Some topics were decided more than once; the latest entry on a topic
  is the one that holds. If the overview and `DECISIONS.md` disagree, `DECISIONS.md` wins, and I'd like to hear about
  the disagreement.

Two compilers are planned. G2C2, which you're building, is the bootstrap compiler, written in C++23 over LLVM's C++
API. GoTo, written later in GO2 itself, replaces it. So please keep G2C2 plain and readable: it will be ported to GO2.
You're "compiler claude".
</context>

<task>
Build G2C2 and the C++ runtime it links, covering every part of GO2 that doesn't need a garbage collector.

- `g2c2/` holds the compiler; `runtime/` holds the runtime every compiled program links. Both are one CMake project
  using `find_package(LLVM)`, built with Homebrew clang 23 (`/opt/homebrew/opt/llvm/bin/clang++`) and its libc++,
  against LLVM 23 at `/opt/homebrew/opt/llvm`, in C++23.
- `g2c2 build <file or project>` and `g2c2 run …` should work, each with `--release`. Every build optimises at LLVM
  `-O1`; debug and release differ only in overflow checks.
- `examples/` holds one GO2 program per feature with its expected output in a `.out` file (and a `.exit` file when
  the exit code isn't 0). `g2c2/tests/reject/` holds programs G2C2 must refuse, each with the expected error text in
  a `.err` file. One command runs them all.
</task>

<scope>
In scope is everything in the overview that works without memory outliving the code that made it: packages and
`project.go2`, `pub`, comments, statements and blocks, `let`/`var`/`const` and their promises, integer and float types,
literals, precedence, the same-type rule, conversions, overflow and division rules, `bool`, `if`, every `for`,
`stop`/`next`, functions and methods, `main` and `os.exit`, structs, enums and `match`, failures (`fail`, `??` with a
value, `?? fail`, `coll.handler` members and their coverage rules), `fmt.print` with `{}` marks, string literals and
escapes, and calling C (`extern.c`, `str.c`, opaque types).

Out of scope for now: `ls`, `map`, `concat`, `str(bytes)`, `str(c)`, strings built at run time, types that contain
themselves, copy-on-write, and the collector with its stack maps. The collector is designed (tracing, generational,
moving, with precise LLVM stack maps) but I haven't started it, and these features all depend on it. A stand-in such
as `malloc` without freeing would hide where memory really goes and have to be torn out again, so if something you
thought was in scope turns out to need that kind of memory, leave it out and tell me in your report.
</scope>

<how_to_work>
1. Read the three files, then start with hello world end to end: lexer, parser, checker, LLVM output, the runtime's
   `fmt.print`, and the test runner. One program through every stage first shows the pipeline works before it grows.
2. Then add features one at a time, each with its example (and reject cases where the rules refuse something), and
   commit each one when its tests pass.
3. Keep `g2c2/PROGRESS.md` current: what works, what's next, and open questions. With the git log, it lets you or a
   later session pick up exactly where work stopped, including after your context is compacted.
4. Write general code that implements the rule, not code that only makes a given example pass. If an example or a
   documented rule looks wrong, tell me instead of working around it.
5. Keep the code as simple as the job allows, with no abstractions or options beyond what the language needs, since
   GoTo will be a port of it.
</how_to_work>

<design_questions>
GO2 is my language, so I decide its syntax, naming, semantics and architecture. When the documents don't answer
something, write the question in `g2c2/PROGRESS.md`, send it to design claude by message (it asks me and records my
answer in `DECISIONS.md`), and carry on with work that doesn't depend on it. Please don't pick an answer yourself, even
a provisional one: provisional picks tend to become the answer by default.

`DECISIONS.md` and `design/` belong to design claude; if you find a mistake in them, tell me or design claude rather
than editing them.

If you ask me something directly, ask in plain text, one question at a time, with your recommendation. I find the
AskUserQuestion tool and question buttons in the end-of-turn box disruptive (they're popups to me), so keep questions
in your message.
</design_questions>

<git>
Commit in the house form from `CLAUDE.md`: author `Tankun Sriket <tankunsriket63741ecff056.invalid>`, subjects such as
`feature.g2c2.lexer: …` or `fix.runtime.print: …`, and every commit ending with
`Co-Authored-By: compiler claude <noreply@anthropic.com>`. Work on a branch named `g2c2` (create it if you're not on
one), and push it after each commit: I like commits on GitHub as they're made. Merging into `main` is my call, so
leave that to me.
</git>

<when_to_stop>
Keep going while there's in-scope work that doesn't depend on an unanswered question; a long turn or a finished
feature isn't a reason to stop and report. Stop when everything in scope is done, or when everything left is blocked on
a question. Then tell me what works (with the test command and its result), what's left, and every open question.
</when_to_stop>

Go ahead.
