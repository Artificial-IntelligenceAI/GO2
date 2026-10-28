# GO2

Design repository for **GO2**, the owner's experimental hobby language, meant to be self-hosted fairly early.

## Ground rules

- **Design authority is the owner's.** Sessions advise and recommend; none decide syntax, naming, semantics or
  architecture, and none fill in a default for an open decision. Ask in plain text and wait.
- **One question at a time** (owner, 2026-10-28), each with a recommendation.
- **Never use the AskUserQuestion tool** in this project ("no pop ups", owner, 2026-10-28), nor a question with
  buttons in the end-of-turn box (owner: "That's a popup"). Questions go in the message text only.
- **Building waits for the owner's go** (owner, 2026-10-28: "Woah, I didn't say go yet (but bug fixing is auto)").
  A settled design is not a go, and neither is naming who will build it ("let another session make it"). Fixing a bug
  in what is already built needs no go.
- **Prompts for other sessions follow Anthropic's prompting guide** (owner: "you didn't even read Anthropic's
  prompting guide"): platform.claude.com/docs/en/build-with-claude/prompt-engineering/claude-prompting-best-practices.
  After a context compaction the guide may be only half-remembered, so a session that knows it was compacted writes
  its messages to other sessions plainly instead (owner: "if you know you got compacted, just msg normally").
- Record decisions in `DECISIONS.md` as they are made, noting what the owner chose versus what was proposed.
- **GO2 is a hobby language.** No timeline; do not push for schedules or scope.

## Sessions

- **Design session** ("design claude", this repo's default): decisions, `design/`, `DECISIONS.md`. Commits carry
  `Co-Authored-By: design claude <noreply@anthropic.com>`.
- **Compiler session** ("compiler claude", not started; a first start on 2026-10-28 was stopped by the owner): G2C2 in `g2c2/` and the runtime in `runtime/`, everything
  that does not need a GC (owner: "let another session (compiler claude) make it, everything unless it needs a GC").
  Its brief is `design/compiler-session.md`. It works on its own branch, never edits `DECISIONS.md` or `design/`, and
  asks the owner through design claude. Commits carry `Co-Authored-By: compiler claude <noreply@anthropic.com>`.

## Layout

- `README.md` — what GO2 is, and its licences (Apache-2.0 WITH LLVM-exception for code, CC BY 4.0 for writing).
- `DECISIONS.md` — the running log of settled design decisions.
- `design/` — design documents, one topic per file, as they are written.

## Git

Public at `Artificial-IntelligenceAI/GO2` (owner, 2026-10-28: "the AI account public, and with same licenses as CyPL").

Commits follow `Soclets/CyborgPL`'s form (owner, 2026-10-28: "Do GH commits like Soclets/CyborgPL").

- Branch: `main`. Author: `Tankun Sriket <tankunsriket63741ecff056.invalid>` (repo-local config, already set).
- Every commit ends with `Co-Authored-By: design claude <noreply@anthropic.com>`. The house name says which session
  wrote it and stays put when the model changes.
- One decision, one commit, pushed right after (owner, 2026-10-28: "auto push, bro").
- Subjects are `kind.folder.narrowing: sentence`. Kinds: `decision`, `design`, `feature`, `fix`, `test`, `house`. The
  second word is the folder, or the topic for `decision`/`design`/`house`. The third word is optional; there is never
  a fourth. A parenthesis at the end says who chose it: `(owner)`, `(owner: (a))` for an offered option, plus
  `not built` when the decision isn't implemented yet. An undecided idea starts `exploring, …`.
  E.g. `decision.ops.andor: and and or keep their words; &, &&, |, || dropped (owner)`.
