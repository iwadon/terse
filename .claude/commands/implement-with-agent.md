---
description: Implement feature using subagent to minimize context consumption
argument-hint: <implementation request>
---

## User Request

$ARGUMENTS

---

**IMPORTANT**: If the user request is empty, ask the user for specific requirements before proceeding.

---

You act as an **Architect** and delegate implementation to subagents:

## Prerequisites Check

Before starting implementation:

1. **Identify dependencies**: List all features/APIs required for the task
2. **Check availability**: Verify each dependency is already implemented in terse
3. **Report blockers**: If critical features are missing, STOP and report:
   - What feature is needed
   - Why it's needed for this task
   - Recommended implementation order

**Do NOT create workarounds** for missing features without explicit user approval.

## Workflow

1. **Analyze** the user request and split into subtasks
2. **Select appropriate subagent** for each subtask:
   - `refactoring` (Sonnet) - Multi-file renames, structural changes
   - `implementation` (Sonnet) - New features, complex algorithms
   - `quick-fix` (Haiku) - Comments, error messages, small fixes (1-3 files)
   - `research` (Haiku) - Code investigation, pattern search
3. **Delegate** with detailed instructions
4. **Review** results and integrate

### Delegation Format

```
Use the `[agent-name]` subagent to execute the following task:

**Original user request**: [Summary]

**Subtask objective**: [Specific details]

**Requirements**:
- [Requirement 1]
- [Requirement 2]

**Implementation location**: [Files/functions]

**Reference info**: [Similar code/related files]

**CRITICAL**: Apply changes using Edit/Write tools directly. NO patch files or scripts.

**Expected results**:
- Summary (max 500 chars)
- Modified files (with "✅ Applied via Edit/Write")
- Test results
- Verification: "✅ Direct edits completed"
```

## Verification

After each subagent completes, check for "✅ Direct edits completed". If patch files/scripts were created instead, reject and re-delegate with stronger emphasis on direct editing.

## Example Usage

**New feature:**
```
/implement-with-agent Add P2 mouse tracking support

→ Use `implementation` agent
→ Provide requirements, target files, reference code
→ Agent implements, tests, reports with "✅ Direct edits completed"
```

**Multi-file refactoring:**
```
/implement-with-agent Rename terse_cursor_move_foo to terse_cursor_foo

→ Use `refactoring` agent
→ Agent searches all files, updates declarations/call sites
→ Verifies build and tests pass
```

**Research + Fix:**
```
/implement-with-agent Find and fix simple TODOs

→ Step 1: Use `research` agent to find/categorize TODOs
→ Step 2: Use `quick-fix` agent to fix simple ones
→ Execute sequentially
```
