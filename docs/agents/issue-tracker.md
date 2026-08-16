# Issue tracker: GitHub

Issues and specs for this repository live as GitHub issues. Use the `gh` CLI
for tracker operations.

## Conventions

- Create an issue: `gh issue create --title "..." --body "..."`.
- Read an issue: `gh issue view <number> --comments` and fetch its labels.
- List issues: `gh issue list --state open --json number,title,body,labels,comments`.
- Comment: `gh issue comment <number> --body "..."`.
- Apply or remove labels: `gh issue edit <number> --add-label "..."` or
  `--remove-label "..."`.
- Close: `gh issue close <number> --comment "..."`.

Infer the repository from the GitHub remote. GitHub shares one number space
across issues and pull requests, so resolve an ambiguous number with
`gh pr view <number>` and fall back to `gh issue view <number>`.

## Pull requests as a triage surface

**PRs as a request surface: no.** External pull requests do not enter the issue
triage queue automatically.

## Skill routing

- When a skill says to publish to the issue tracker, create a GitHub issue.
- When a skill says to fetch a ticket, read the issue and its comments.

## Wayfinding operations

The Wayfinder map is one issue labelled `wayfinder:map`. Decision tickets are
child issues labelled `wayfinder:research`, `wayfinder:prototype`,
`wayfinder:grilling`, or `wayfinder:task`.

- Link tickets with GitHub sub-issues. If sub-issues are unavailable, list them
  in the map and put `Part of #<map>` at the top of each ticket.
- Use GitHub's native issue dependencies for blocking. If dependencies are
  unavailable, put `Blocked by: #<number>` at the top of the blocked ticket.
- The frontier is the map's open, unblocked, unassigned children in map order.
- Claim a ticket before work with `gh issue edit <number> --add-assignee @me`.
- Resolve a ticket by posting its answer, closing it, and appending a linked
  one-line gist to the map's Decisions-so-far section.

For a native dependency edge, send the blocker's numeric database `id` to
`repos/<owner>/<repo>/issues/<child>/dependencies/blocked_by`; do not use its
issue number or GraphQL node ID.
