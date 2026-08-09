#!/usr/bin/env python3
"""Mirror GitHub Issues into a local markdown file.

Once open work items live on GitHub rather than in `docs/notes/wishlist.md`,
a session that starts offline — or an agent session that would rather read a
file than shell out per query — has no view of the backlog. This pulls the
issues down and writes them in the shape the wishlist used, so the reading
habit survives the move.

**One-way, GitHub -> file.** The generated file is gitignored and is never a
source of truth: edit issues on GitHub, re-run this, and the file catches up.
Nothing here writes back, so a stale mirror can only ever be out of date, never
wrong in a way that propagates.

Usage:
    ./scripts/gh-issues.py                  # refresh docs/notes/issues.local.md
    ./scripts/gh-issues.py --stdout         # print instead of writing
    ./scripts/gh-issues.py -o path.md       # write somewhere else
    ./scripts/gh-issues.py --closed-days 60 # widen the "recently closed" window

Requires the `gh` CLI, authenticated (`gh auth status`).
"""

import argparse
import datetime as dt
import json
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUT = REPO_ROOT / "docs" / "notes" / "issues.local.md"

# Issues carrying no scheduling label sort under "unscheduled", mirroring the
# wishlist's "Active (unscheduled)" section.
SCHEDULED_LABEL = "status:scheduled"
FIELDS = "number,title,body,labels,milestone,state,createdAt,closedAt,url,assignees"


def run_gh(args):
    try:
        out = subprocess.run(
            ["gh", *args], capture_output=True, text=True, check=True, cwd=REPO_ROOT
        )
    except subprocess.CalledProcessError as e:
        sys.exit(f"gh failed: {' '.join(args)}\n{e.stderr.strip()}")
    return json.loads(out.stdout)


def fetch(state, limit):
    return run_gh(
        ["issue", "list", "--state", state, "--limit", str(limit), "--json", FIELDS]
    )


def label_names(issue):
    return [lbl["name"] for lbl in issue.get("labels") or []]


def milestone_title(issue):
    ms = issue.get("milestone")
    return ms["title"] if ms else None


def indent_body(body, prefix="  "):
    """Re-indent an issue body so it nests under its bullet, wishlist-style."""
    if not body:
        return ""
    lines = body.replace("\r\n", "\n").rstrip().split("\n")
    return "\n".join(prefix + ln if ln.strip() else "" for ln in lines)


def render_issue(issue, *, include_body=True):
    labels = [ln for ln in label_names(issue) if not ln.startswith("status:")]
    bits = [f"[#{issue['number']}]({issue['url']})"]
    ms = milestone_title(issue)
    if ms:
        bits.append(f"**{ms}**")
    if labels:
        bits.append(" ".join(f"`{ln}`" for ln in labels))
    assignees = [a["login"] for a in issue.get("assignees") or []]
    if assignees:
        bits.append("@" + ", @".join(assignees))
    meta = " · ".join(bits)

    out = [f"- **{issue['title']}** — {meta}"]
    if include_body and issue.get("body", "").strip():
        out.append("")
        out.append(indent_body(issue["body"]))
    return "\n".join(out)


def section(title, issues, *, include_body=True, empty_note=None):
    out = [f"## {title}", ""]
    if not issues:
        out.append(empty_note or "_Nothing here._")
        out.append("")
        return out
    for issue in issues:
        out.append(render_issue(issue, include_body=include_body))
        out.append("")
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("-o", "--output", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--stdout", action="store_true", help="print instead of writing")
    ap.add_argument("--limit", type=int, default=300)
    ap.add_argument(
        "--closed-days",
        type=int,
        default=30,
        help="how far back to list closed issues (default 30)",
    )
    args = ap.parse_args()

    if shutil.which("gh") is None:
        sys.exit("gh CLI not found — see https://cli.github.com/")

    open_issues = fetch("open", args.limit)
    closed_issues = fetch("closed", args.limit)

    cutoff = dt.datetime.now(dt.timezone.utc) - dt.timedelta(days=args.closed_days)
    recent_closed = []
    for issue in closed_issues:
        closed_at = issue.get("closedAt")
        if not closed_at:
            continue
        when = dt.datetime.fromisoformat(closed_at.replace("Z", "+00:00"))
        if when >= cutoff:
            recent_closed.append(issue)
    recent_closed.sort(key=lambda i: i["closedAt"], reverse=True)

    # Scheduled = has a milestone, or carries the scheduling label. A milestone
    # is a phase, so this reproduces the wishlist's Active/Graduated split.
    scheduled, unscheduled = [], []
    for issue in open_issues:
        if milestone_title(issue) or SCHEDULED_LABEL in label_names(issue):
            scheduled.append(issue)
        else:
            unscheduled.append(issue)

    scheduled.sort(key=lambda i: (milestone_title(i) or "~", i["number"]))
    unscheduled.sort(key=lambda i: i["number"])

    now = dt.datetime.now().astimezone().strftime("%Y-%m-%d %H:%M %Z")
    lines = [
        "# Open issues (generated mirror — do not edit)",
        "",
        f"Generated {now} by `scripts/gh-issues.py` from GitHub Issues.",
        "This file is gitignored and one-way: **edit issues on GitHub**, then",
        "re-run the script. See `docs/notes/issue-tracking.md` for what lives",
        "here versus what stays in the repo.",
        "",
        f"{len(open_issues)} open · {len(recent_closed)} closed in the last "
        f"{args.closed_days} days.",
        "",
    ]

    lines += section(
        "Open — scheduled into a phase",
        scheduled,
        empty_note="_Nothing scheduled. Assign a milestone to schedule an issue._",
    )

    lines += section(
        "Open — unscheduled",
        unscheduled,
        empty_note="_Nothing unscheduled._",
    )
    lines += section(
        f"Closed in the last {args.closed_days} days",
        recent_closed,
        include_body=False,
        empty_note="_None._",
    )

    text = "\n".join(lines).rstrip() + "\n"

    if args.stdout:
        sys.stdout.write(text)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
        print(
            f"Wrote {args.output.relative_to(REPO_ROOT)} "
            f"({len(open_issues)} open, {len(recent_closed)} recently closed)"
        )


if __name__ == "__main__":
    main()
