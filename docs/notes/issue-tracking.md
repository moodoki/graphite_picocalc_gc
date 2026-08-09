# Issue tracking — GitHub Issues, and what stays in the repo

**Date**: 2026-08-10
**Status**: Proposal. The tooling (`scripts/gh-issues.py`) is written and
working; the migration itself has not been done. Nothing here takes effect
until issues are actually created.

## Why change anything

Open work has accumulated in four places, and none of them is a queue:

| File | What it holds | Problem once the repo is public |
|------|---------------|---------------------------------|
| `wishlist.md` | 10 active items, each an essay with real analysis | Invisible to anyone outside the repo; no way for a reader to add one |
| `next-session.md` | "Decisions left to soak", "Open design threads" | 1,207 lines of mixed session state and open questions; the open items are buried |
| `next-bench-session.md` | Deferred hardware work (D14) | Fine, but disconnected from everything else |
| `decisions.md` | D-entries, some with open follow-ups (D53's root cause) | Correct place for the *finding*; wrong place to track that it is still open |

The wishlist's own header says it is "not a backlog" — which is honest, and
also the problem: there is no backlog. Items get remembered because a session
happens to re-read the file.

## The rule: record versus queue

Move things that are **unfinished work** to GitHub Issues. Keep things that are
a **finished record** in the repo, versioned with the code they describe.

**Goes to GitHub Issues**

- `wishlist.md` → the 10 items under **Active (unscheduled)**
- `next-session.md` → the "Open design threads" section, and any soak-table row
  still open
- `next-bench-session.md` → D14 rail settle, as one issue
- Open follow-ups named in decision entries — D53's root cause is the live
  example, and the pattern is: the D-entry keeps the analysis, an issue tracks
  that it is unresolved
- Anything a reader reports once the repo is public

**Stays in the repo, unchanged**

- `decisions.md` — the decision log. These are *finished thinking*: what was
  decided, what was rejected, and why. They must version with the code they
  justify and be readable from a checkout at any commit. An ADR is not a task.
- `worklog.md` — dated history. Append-only, never actionable.
- `docs/phases/*-spec.md` — design contracts, agreed before the work. A phase
  spec is not a milestone-worth-of-issues; it is the document the issues would
  be derived *from*.
- `next-session.md` — keeps its handoff role: where the working tree is, what
  is half-done, what to read first. That is session state, not a backlog, and
  it is genuinely useful precisely because it is not one.
- Retros, testdrive observations, measurement datasets — evidence.

The test to apply: **could someone close this?** If not, it is a record.

## Labels

The default set gets replaced with something the project actually sorts by.

**Type** — exactly one per issue.

| Label | Meaning |
|-------|---------|
| `type:feature` | New user-visible capability. Most of the wishlist. |
| `type:bug` | Wrong behaviour in shipped firmware. |
| `type:chore` | Internal change with no user-visible effect — refactors, re-vendoring. |
| `type:docs` | Documentation only. |
| `type:question` | An open design thread with no decided answer yet. Closes into a D-entry. |

**Area** — where the work lands; mirrors `src/`.

`area:math`, `area:cas`, `area:graph`, `area:ui`, `area:render`,
`area:platform`, `area:drivers`, `area:tooling`, `area:docs`

**Project-specific, and the ones that earn their keep:**

| Label | Meaning |
|-------|---------|
| `hw-pending` | Cannot be verified without a board on the desk. The project has repeatedly shipped things that were correct on the host and wrong on hardware (D48, D53) — this label is the difference between "done" and "done as far as we can tell". |
| `board:pico1` / `board:pico2` | Reproduces on one board only. Both are supported targets and they differ in core, RAM and FPU. |
| `blocked:upstream` | Waiting on a third party — e.g. the tinyexpr re-vendor. |
| `good first issue` | Kept from the defaults; meaningful now that the repo is public. |

**Milestones are phases.** `Phase 6A`, `Phase 6B`, and so on. Assigning a
milestone is exactly what `wishlist.md` called *graduating* an item, and it
replaces the Graduated section outright. `scripts/gh-issues.py` splits its
output on milestone-or-not for the same reason.

## Provenance must survive the move

The wishlist's real value is not the list — it is the sentence attached to each
item saying **when it was raised and what raised it** ("raised 2026-08-09 while
re-evaluating the change register's preserved rows"). That context is why an
item is still worth doing a year later, and it is the first thing a bulk
migration would drop.

So: migrate by hand, ten items, body-first. Each issue opens with the raised
date and the circumstance, then the existing analysis verbatim. This is an
hour of work and it is the whole point of doing the migration at all.

Then `wishlist.md` gets a header saying it is closed to new entries and keeps
its **Graduated** and **Completed / Closed** sections as the historical record
— those are provenance for things that already shipped, and moving them to
GitHub would gain nothing and lose their D-number cross-references.

## The local mirror

`scripts/gh-issues.py` writes `docs/notes/issues.local.md`: every open issue
with its full body, grouped into scheduled and unscheduled, in the same bullet
shape the wishlist used.

```bash
./scripts/gh-issues.py              # refresh the mirror
./scripts/gh-issues.py --stdout     # print it instead
```

Three properties, each deliberate:

- **One-way.** GitHub is the source of truth; the script only reads. A stale
  mirror can be out of date, but it can never push a wrong edit back. Any
  two-way sync would have to resolve conflicts, and there is no version of that
  which is worth the failure mode.
- **Gitignored** (`*.local.md`). It never appears in a diff, never conflicts on
  merge, and never has to be kept current in review. It is a cache.
- **Full bodies included.** The point is that a session with no network — or an
  agent that would rather read a file than shell out per query — sees the same
  backlog as GitHub, not just its titles.

Run it at the start of a session, in the same breath as reading
`next-session.md`.

## Migration steps, when this is picked up

1. Create the labels above; delete the unused defaults (`duplicate`,
   `invalid`, `wontfix`, `help wanted` — none maps to how this project works).
2. Create milestones `Phase 6A` and `Phase 6B`.
3. Hand-migrate the 10 `wishlist.md` **Active** items, provenance line first.
   The two 5.2 items (collapse `kMatrix`/`kList`; re-vendor tinyexpr) already
   have full analyses — those go across verbatim.
4. Open issues for: D53's root cause (`type:bug`, `hw-pending`,
   `board:pico1`), the D14 rail settle (`hw-pending`), the matrix-nesting
   ~104 B/level unattributed stack cost, and 6B's `calc` bindings needing
   re-verification against the unified evaluator.
5. Add the header note to `wishlist.md`; strike the migrated sections from
   `next-session.md` and `next-bench-session.md`, leaving a pointer.
6. Add `./scripts/gh-issues.py` to the session-start habit in `AGENTS.md`.
7. Enable issue templates (`.github/ISSUE_TEMPLATE/`) — bug reports from users
   with a real device should ask for board, firmware version and the expression
   that reproduced it, since those three are what every past hardware bug
   needed.

## What this does not solve

Issues are a poor fit for the thing this project does most: **long-form
reasoning about a decision**. `decisions.md` entries run to hundreds of lines
with rejected alternatives and measured numbers, and that belongs in the repo,
next to the code, versioned with it. The migration deliberately moves only the
queue and leaves the reasoning where it is. If an issue's discussion turns into
a decision, it closes with a link to the new D-entry — the issue tracks that
the question was open, the log records the answer.
