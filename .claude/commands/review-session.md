---
description: Review recent work and identify next priorities
argument-hint: <optional: review target in natural language>
---

Review recent work and identify next priorities.

## Target Specification (Natural Language)

$ARGUMENTS

**Examples:**
- (no argument) → Review the most recent unit of work (related commit group)
- `last 3 commits` → Review recent 3 commits
- `today's work` → All commits from today
- `smart pointer implementation` → Search and review related commits

## Procedure

### 1. Identify Target

**When no argument is provided:**
- Run `git log --oneline -10` to see recent commits
- Identify the most recent "unit of work" — a group of related commits (e.g., a feature, a fix, a refactoring)
- Use the commit messages and timestamps to determine where the current work begins

**When argument is provided:**
Use appropriate git commands based on the argument:
```bash
git log --oneline -N               # Recent N commits
git log --oneline --since="today"  # Today's commits
git log --oneline --grep="keyword" # Keyword search
```

### 2. Gather Information

- **Commits**: `git show`, `git diff`, changed files for the identified range
- **Current state**: `git status`, conversation context
- **Code**: Read relevant files to understand current implementation state

## Report Format

```markdown
## Recap

### Accomplishments
- [Summary of implemented features/fixes]
- [Statistics: files changed, lines added, etc.]

### Notes
- [Challenges encountered and how they were resolved]
- [Design insights discovered during implementation]
- [Deviations from original plan]

(Omit this section if nothing notable)

### Next Priority Items
1. [Highest priority task — check progress-overview.md, graphics-roadmap.md]
2. [Secondary task]

### Decisions / Concerns
- [Architectural decisions made]
- [Technical debt or future concerns]

(Omit this section if nothing notable)
```

## Notes

- Verify actual code state by reading files
- Run tests to confirm current status
- Focus on the identified work unit, not the entire session
