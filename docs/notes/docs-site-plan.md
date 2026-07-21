# Docs site plan — GitHub Pages + TI-style workbooks

**Date**: 2026-07-21
**Status**: Plan only. No scaffolding created this session (by choice —
this was a stock-taking/reflection session). Pick this up as a Phase 6
(app framework/polish) task, or opportunistically earlier since it doesn't
block firmware work and can be written incrementally from what already
exists in `docs/`.

## Goal

TI ships every 83/84+ with a PDF guidebook: a chapter per feature area,
worked examples, keystroke-by-keystroke walkthroughs. We have no
equivalent user-facing document — `docs/` today is entirely developer/spec
material (phase contracts, architecture, decisions log) plus a terse
"Using the calculator" section in the README. The goal is a public,
browsable site that plays the guidebook role: task-oriented workbooks
("how do I graph a parametric curve," "how do I run a 2-sample t-test"),
not implementation detail.

## Two audiences, two doc trees

Keep these separate rather than blending them:

- **User docs** (new, public-facing): "how do I use this calculator."
  Written for someone who picked up a flashed PicoCalc, not someone
  reading source. This is what goes on the published site.
- **Developer docs** (existing `docs/`): phase specs, architecture,
  decisions log, hardware notes. Stays as source-tree markdown, read by
  contributors/agents via the repo, not necessarily republished on the
  site (or republished under a clearly separate "Internals" nav section
  if there's appetite for it later — not required for v1).

This split also means the user docs can stabilize and read well even
while the developer docs keep accumulating session-by-session churn
(worklog, next-session handoff, decisions log) that isn't reader-friendly
material.

## Static site generator: recommend MkDocs Material

Reasoning, not just preference:

- The project's dev tooling is already Python (`scripts/validate_md.py`,
  `requirements-dev.txt`, `.venv` convention per project policy) — MkDocs
  fits the existing toolchain instead of adding a Node/JS dependency for
  docs alone (Docusaurus would be the main alternative, but it's a
  heavier addition for a C++/Python project with no other JS surface).
- Docs already use LaTeX-style math (`$...$`, `$$...$$` throughout
  `docs/phases/*.md`) — MkDocs Material supports this directly via the
  `pymdownx.arithmatex` extension + MathJax, matching what's already
  written rather than requiring a rewrite.
- `scripts/validate_md.py` already validates "math mode, links, code
  blocks" per AGENTS.md — a MkDocs build is a natural second consumer of
  the same markdown conventions, not a parallel format to maintain.
- Zero-JS-build-step deploy: `mkdocs build` + `mkdocs gh-deploy` (or a
  GitHub Actions equivalent) is a single Python-tooling step, consistent
  with how `scripts/*.sh`/`scripts/*.py` already work in this repo.

## Proposed structure

```
docs-site/                      # new top-level dir, or docs/site/ — TBD
├── mkdocs.yml
├── index.md                    # landing page: what this is, quick links
├── getting-started/
│   ├── build-and-flash.md      # adapted from README quick start
│   └── first-steps.md          # home screen, ANS, store, history
├── guide/                      # the TI-guidebook-equivalent chapters
│   ├── 01-basic-calculations.md
│   ├── 02-function-graphing.md
│   ├── 03-parametric-polar-graphing.md
│   ├── 04-tables.md
│   ├── 05-split-screen.md
│   ├── 06-lists-and-stats.md
│   ├── 07-regression.md
│   ├── 08-distributions.md
│   ├── 09-inference.md
│   ├── 10-stat-plots.md
│   ├── 11-matrices.md
│   ├── 12-complex-numbers.md
│   ├── 13-calc-menu-graph-analysis.md
│   ├── 14-cas.md               # stub until Phase 5 ships
│   └── 15-programming.md       # stub until Phase 4E/MicroPython ships
├── reference/
│   ├── function-catalog.md     # generated or hand-mirrored from math/catalog.cpp
│   ├── key-reference.md        # per-screen softkey map (mirrors F5 HELP content)
│   ├── error-messages.md
│   └── keyboard-map.md
└── about/
    ├── hardware.md              # adapted from docs/hardware.md, reader-facing cut
    └── license.md                # adapted from NOTICE.md
```

Each `guide/NN-*.md` chapter follows a TI-guidebook shape: what it's for,
a worked example with keystrokes, a screenshot (once available — see
below), then a compact reference table for the screen's softkeys/syntax.
The built-in `F5` HELP screen's catalog/key-reference/syntax-notes content
is the natural seed — it's already curated and correct, just not
illustrated or example-driven the way a workbook chapter should be.

## Content sourcing — reuse before writing new

- `README.md` "Using the calculator" section → seeds `getting-started/`
  and the chapter reference tables.
- The `F5` HELP screen's catalog table (`math/catalog.cpp`) → seeds
  `reference/function-catalog.md`; consider a small script that emits
  this table as markdown so it can't drift from the actual registered
  catalog (the same "single source of truth" principle the in-app help
  browser already follows — README: *"function catalog driven by the
  same table the parser registers from"*).
- `docs/phases/*-spec.md` acceptance-criteria/example sections already
  contain worked expression examples (e.g. phase4-spec §5.4's complex
  function examples) that can seed chapter worked examples directly.
- Screenshots: none exist yet. Either photograph the physical device
  during a hardware session, or (bigger lift) build a desktop emulator
  target — which the wishlist already flags as wanted for font
  antialiasing testing (D31) and would double as a screenshot rig. Not
  blocking v1 — text-only chapters with keystroke sequences are still
  useful without images, TI's own guidebook predates having screenshots.

## Hosting

- **GitHub Pages via GitHub Actions**, consistent with the existing
  `.github/workflows/build.yml` CI matrix (same repo, same Actions
  infrastructure, no new external service).
- New workflow, e.g. `.github/workflows/docs.yml`: on push to `main`
  touching `docs-site/**`, run `mkdocs build` then deploy via
  `actions/deploy-pages` (or `mkdocs gh-deploy` driving a `gh-pages`
  branch — either is fine; `deploy-pages` is the more current GitHub-
  native path and avoids a branch that needs its own gitignore/history
  hygiene).
- Repo settings: Pages source = GitHub Actions (one-time manual toggle,
  not something a workflow file can do itself).
- No custom domain needed initially; the default
  `<user>.github.io/picocalc_gc/` is fine for a personal project.

## What NOT to do in v1

- Don't try to keep the site perfectly in sync with in-progress phases.
  Stub the CAS and programming chapters ("coming in Phase 5" /
  "coming — phase unscheduled") rather than blocking the whole site on
  those phases shipping.
- Don't republish the developer docs (phase specs, decisions log,
  worklog) verbatim on the public site — they're written for an AI
  agent/future-session audience (dense, cross-referenced, D-numbered),
  not a calculator user. If a developer-facing "Internals" section is
  wanted later, that's a deliberate second pass, not a byproduct of
  standing up the user site.
- Don't block this on a desktop emulator / screenshots. Ship text-first,
  add images opportunistically from hardware sessions.

## Suggested sequencing

Natural home is **Phase 6 (app framework, polish, release)** — a
public-facing docs site is exactly "polish, release" territory, and by
then Phase 5 (CAS) content would exist to document rather than stub.
Nothing here blocks earlier phases, though, and the `getting-started/` +
early `guide/` chapters (chapters 1–13, everything already shipped) could
reasonably be drafted earlier as a low-priority background task if there's
appetite — there's no dependency forcing it to wait.
