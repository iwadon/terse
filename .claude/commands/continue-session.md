---
description: Save session state and generate continuation prompt
---

Save current session state and generate prompt for next session.

## CRITICAL: Verify Before Saving

**Before writing session-state.md, you MUST verify the actual state:**

1. **Check git status and recent commits**:
   ```bash
   git log -3 --oneline
   git log -1 --stat
   ```

2. **For each "Next Step" you plan to write**:
   - Search the codebase to confirm it's NOT already implemented
   - Use `grep` or `Glob` to find existing implementations
   - If found, move it to "Completed Work" instead

3. **Run tests** to confirm current state:
   ```bash
   ctest --test-dir build --output-on-failure 2>&1 | tail -10
   ```

This prevents stale/incorrect session state that wastes time in the next session.

## What It Does

1. Save to `.claude/session-state.md`:
   - Current task summary
   - Completed work (verified against git commits)
   - Next tasks to execute (verified NOT already done)
   - Important context (file paths, error messages, etc.)
   - Running subagent tasks (if any)

2. Generate continuation prompt:
   - Concise and clear (200-500 chars)
   - Ready to resume work immediately
   - Contains all necessary context

3. Display to user:
   ```
   ## Session State Saved

   **Continuation prompt for next session:**
   [Copy-pasteable prompt]

   **Steps:**
   1. Run `/clear`
   2. Paste the above prompt
   ```

## When to Use

- When context is running low
- At good breakpoints in large tasks
- When stuck on errors (save state and resume)

## Tip

Use subagent strategy (`/implement-with-agent`) to significantly reduce context consumption
and minimize need for this command.
