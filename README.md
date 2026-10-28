# GO2

> GO2 is a Go-style simple programming language, that has fast compiles, and okay runtime performance.

GO2 is a hobby language, meant to compile itself early. It is being designed now; nothing is built yet, and anything
may change.

```
pkg main;
import "fmt";

fn main[] {
    fmt.print("Hello, World!\n");
}
```

- [`design/overview.md`](design/overview.md): GO2 as it stands, rules only.
- [`DECISIONS.md`](DECISIONS.md): every decision, why, and what else was offered.

**G2C2** (C++23, over LLVM) is the bootstrap compiler; **GoTo**, written in GO2, replaces it once it builds itself.

## Licences

- [`LICENSE`](LICENSE): Apache-2.0 WITH LLVM-exception, for the code and everything else. Programs you compile owe
  nothing.
- [`LICENSE-CC-BY-4.0`](LICENSE-CC-BY-4.0): CC BY 4.0, for this repo's text (its Markdown files: this README,
  `DECISIONS.md`, `design/`). Credit it as `GO2 GitHub repo text: https://github.com/Artificial-IntelligenceAI/GO2`,
  say it is under CC BY 4.0, and say whether you changed it.

GO2 is designed by **Tankun Sriket**, who makes every decision in `DECISIONS.md`. **Claude**, by
[Anthropic](https://www.anthropic.com), wrote the documents here under the owner's direction, as a
[Claude Code](https://claude.com/claude-code) session, *design claude*.
