---
description: Save session state and generate continuation prompt
---

Save current session state and generate prompt for next session.

## What It Does

1. Save to `.claude/session-state.md`:
   - Current task summary
   - Completed work (from TodoList)
   - Next tasks to execute
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
