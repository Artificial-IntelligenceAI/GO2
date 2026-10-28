# GO2

Design repository for **GO2**, the owner's experimental hobby language, meant to be self-hosted fairly early.

## Ground rules

- **Design authority is the owner's.** Sessions advise and recommend; none decide syntax, naming, semantics or
  architecture, and none fill in a default for an open decision. Ask in plain text and wait.
- **One question at a time** (owner, 2026-10-28), each with a recommendation.
- **Never use the AskUserQuestion tool** in this project ("no pop ups", owner, 2026-10-28), nor a question with
  buttons in the end-of-turn box (owner: "That's a popup"). Questions go in the message text only.
- **Building waits for the owner's go** (owner, 2026-10-28: "Woah, I didn't say go yet (but bug fixing is auto)").
  A settled design is not a go. Fixing a bug in what is already built needs no go.
- Record decisions in `DECISIONS.md` as they are made, noting what the owner chose versus what was proposed.
- **GO2 is a hobby language.** No timeline; do not push for schedules or scope.

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
- One decision, one commit.
- Subjects are `kind.folder.narrowing: sentence`. Kinds: `decision`, `design`, `feature`, `fix`, `test`, `house`. The
  second word is the folder, or the topic for `decision`/`design`/`house`. The third word is optional; there is never
  a fourth. A parenthesis at the end says who chose it: `(owner)`, `(owner: (a))` for an offered option, plus
  `not built` when the decision isn't implemented yet. An undecided idea starts `exploring, …`.
  E.g. `decision.ops.andor: and and or keep their words; &, &&, |, || dropped (owner)`.
