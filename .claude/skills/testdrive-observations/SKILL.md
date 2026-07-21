---
name: testdrive-observations
description: Interviews the user for testdrive feedback after they've spent hands-on time with the PicoCalc calculator on real hardware, logs the verbatim responses to a new docs/notes/ file, and commits it. Use when the user says things like "interview me", "let's log testdrive feedback", or "I just tried it out, take my feedback" after a hardware test session. Do NOT use mid-task, to fix bugs, to write application code, or for general session wrap-up (worklog/next-session.md updates) — that's the session-wrapup agent's job.
---

# Testdrive observations interview

Run this interview yourself, directly in this conversation — do not delegate it to a subagent. `AskUserQuestion` only produces a real interactive prompt when called by the agent the user is actually talking to; a spawned subagent asking it just produces text the user never sees as a live prompt. So: gather context, then ask the questions yourself, one `AskUserQuestion` call at a time.

You collect and log. You do not investigate, diagnose, or fix — that's future work for someone else.

## 1. Build the question list first

Before asking anything, find out what's actually worth asking about:

- Read `docs/notes/next-session.md` — the "on-device evals" / HW-PENDING checklist (code-complete features not yet verified on hardware), and which firmware is actually flashed on each board right now (Pico 1 and Pico 2 are frequently on different, older sessions — don't ask about code that isn't even flashed yet).
- Skim the tail of `docs/notes/worklog.md` for the most recent session's details if `next-session.md` references something not fully spelled out.
- Note the open "watch-items" under "Open design threads" in `next-session.md` — things explicitly waiting on a judgment call from real use.
- If a broader search across `docs/notes/` would help (e.g. the user mentions something not covered by the two files above), use the `session-notes` agent for that lookup rather than reading everything yourself.

Build a prioritized list: newest code-complete-but-unverified items first, then older open watch-items. Cap it — this is an interview, not an audit; the open-ended pass in step 3 catches anything you didn't get to.

## 2. Interview

Use `AskUserQuestion`, batched up to 4 questions per call. Phrase each question concretely (name the feature, screen, or key) with options like "Works as expected" / "Found a problem" / "Didn't test this" so a quick answer is easy — but expect the real content to land in the free-text "Other" field, that's normal and desired, not a fallback.

Order:
1. First call: which board(s) and build the user actually exercised (this frames everything else — compare against what `next-session.md` says is flashed).
2. Next calls: walk the prioritized checklist from step 1, newest items first.
3. Finish with one or more open questions inviting anything not covered: bugs, UI/UX friction, feature requests, anything that felt off. After an answer, ask again whether there's more ("Yes, more to add" / "No, that's everything") and loop until the user signals they're done.

Do not answer your own questions, speculate about root causes, or suggest fixes mid-interview.

## 3. Log it

Write a new file under `docs/notes/`. Naming:
- Default: `docs/notes/testdrive-<YYYY-MM-DD>-observations.md` (today's date), matching the existing `testdrive-phase2-observations.md` pattern.
- If the interview turned out to be squarely about one numbered session's on-device eval (e.g. the user confirms they're evaluating "Session 18"), use `docs/notes/session<N>-observations-verbatim.md` instead, matching `session14-observations-verbatim.md`.

Structure, following the project's existing verbatim convention (see `session14-observations-verbatim.md` and `phase3A-3B-observations-verbatim.md`):
- A one-line header: date, and which board/build was tested (from question 1).
- A short line noting this is raw feedback, not yet investigated or fixed.
- The responses themselves, grouped naturally if they split cleanly (e.g. "Usage notes", "UI notes", "Bugs", "Good to haves") — otherwise a single numbered "Observations (verbatim)" list is fine. Preserve the user's own wording; do not paraphrase away specifics, do not add your own analysis or fix suggestions.

Do not edit `next-session.md`, `worklog.md`, or any other file — only create the new observations file.

## 4. Commit

- `git status` first — stage only the new file you just created (`git add <path>`), nothing else.
- Commit message: `docs: log testdrive observations (<YYYY-MM-DD>)`, imperative, matching the repo's `docs:` prefix convention (check `git log --oneline` if unsure).
- Never `--amend`, never `--no-verify`, never push without explicit instruction.
- After committing, `git status` to confirm a clean tree, and report the file path and commit hash.

## What not to do

- Don't investigate root causes or propose fixes — log what was said, nothing more.
- Don't touch `docs/phases/*-spec.md` files.
- Don't write or modify application source code.
- Don't skip step 1 — generic questions instead of ones grounded in the actual current checklist defeats the point of this skill.
