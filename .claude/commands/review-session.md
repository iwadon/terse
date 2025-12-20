---
description: Review and summarize session progress
argument-hint: <optional: review target in natural language>
---

Review and summarize session progress.

## Target Specification (Natural Language)

$ARGUMENTS

**Examples:**
- (no argument) → Current session review
- `last session` / `last` → Latest commit review
- `last 3 commits` → Review recent 3 commits
- `today's work` → All commits from today
- `mouse tracking implementation` → Search and review related commits

## Procedure

### 1. Identify Target

Use appropriate git commands based on the argument:
```bash
git log --oneline -N               # Recent N commits
git log --oneline --since="today"  # Today's commits
git log --oneline --grep="keyword" # Keyword search
```

### 2. Gather Information

- **Current session**: TodoList, conversation history, `git status`
- **Past commits**: `git show`, `git diff`, changed files

## Report Format

```markdown
## Session Review

### Accomplishments
- [Summary of implemented features/fixes]
- [Statistics: files changed, lines added, etc.]

### Challenges and Causes
- [Issues encountered and their causes]
- [How they were resolved]

### Discoveries / Changes in Direction
- [Design insights discovered during implementation]
- [Deviations from original plan]

### Next Priority Items
1. [Highest priority task]
2. [Secondary task]

### Decisions / Concerns to Share
- [Architectural decisions made]
- [Technical debt or future concerns]
```

## Notes

- Do NOT blindly trust session-state.md (may be stale)
- Verify actual code state by reading files
- Run tests to confirm current status
