---
description: Implement feature using subagent to minimize context consumption
argument-hint: <implementation request>
---

## User Request

$ARGUMENTS

---

**IMPORTANT**: If the user request is empty:
- Ask the user: "What feature would you like to implement? Please provide specific requirements."
- Confirm implementation details (target files, feature overview, etc.) before starting
- Do NOT proceed to task analysis or execution at this point

---

You act as an **Architect** and delegate implementation details to subagents:

## Execution Steps

1. **Task Analysis** (Parent Claude):
   - Understand the user request above
   - Decide high-level approach
   - Identify required files/functions
   - Split implementation into independent subtasks

2. **Delegate to Subagents** (Context isolation):
   - **Select appropriate subagent** based on subtask nature:
     - **Refactoring** (multi-file, function/type rename) → `refactoring` (Sonnet)
     - **New features** (Phase implementation, complex algorithms) → `implementation` (Sonnet)
     - **Small fixes** (add comments, error messages, 1-3 files) → `quick-fix` (Haiku)
     - **Code research** (understand existing code, pattern search) → `research` (Haiku)
     - **Other** → `general-purpose`
   - Provide detailed implementation instructions to each subagent
   - Subagents work autonomously (read files, search, implement)
   - Execute subtasks sequentially (or in parallel if explicitly needed)

3. **Review Results** (Parent Claude):
   - Receive concise summaries from each subagent
   - Provide additional instructions if needed
   - Integrate and report to user

## Delegation Method

For each subtask, **select the appropriate subagent** based on task nature, then invoke using this format:

### Task Classification Guide

1. **Use `refactoring` agent (Sonnet)** when:
   - Renaming functions/types/variables across multiple files
   - Wide-reaching changes including test files
   - Structural reorganization (file splits, function moves, etc.)
   - Updating all call sites after API changes

2. **Use `implementation` agent (Sonnet)** when:
   - Adding new features (Phase implementations, etc.)
   - Implementing complex algorithms
   - Integrating multiple components
   - Implementation with test creation

3. **Use `quick-fix` agent (Haiku)** when:
   - Adding comments or documentation
   - Improving error messages
   - Small fixes within 1-3 files
   - Fixing typos or formatting

4. **Use `research` agent (Haiku)** when:
   - Investigating/understanding existing implementation
   - Searching for patterns or usage examples
   - Analyzing codebase structure
   - Gathering information before implementation

5. **Use `general-purpose` agent** when:
   - Task doesn't fit the above categories

### Delegation Format

```
Use the `[agent-name]` subagent to execute the following task:

**Original user request**: [Summarize user request]

**Subtask objective**: [Specific implementation details]

**Requirements**:
- [Requirement 1]
- [Requirement 2]

**Implementation location**: [File paths, function names, etc.]

**Reference info**: [Similar existing code, related files]

**Expected results**:
- Implementation summary (max 500 chars)
- List of modified files
- Test results (pass/fail)
- Issues or notes

**IMPORTANT**: Return only summary, not detailed code or full file contents.
```

## Benefits

- ✅ Minimize parent session context consumption
- ✅ Each subagent works in independent context window
- ✅ Session persists even with large implementations
- ✅ Execute multiple independent tasks incrementally
- ✅ Retry failed tasks without affecting parent session

## Important Notes

- These subagents must be defined in `.claude/agents/`:
  - `research.md` - Code investigation (Haiku)
  - `quick-fix.md` - Small fixes (Haiku)
  - `implementation.md` - New features (Sonnet)
  - `refactoring.md` - Large-scale refactoring (Sonnet)
  - `general-purpose.md` - General tasks
- Command fails if subagents don't exist
- **Proper agent selection is critical** for cost/quality balance:
  - Haiku (`research`, `quick-fix`): Fast, low-cost, ideal for simple tasks
  - Sonnet (`implementation`, `refactoring`): High-quality, high-cost, required for complex tasks

## Usage Examples

### Example 1: New Feature (implementation agent)

```
/implement-with-agent Implement Phase 7: Add Ctrl+Y (yank), Ctrl+T (transpose)

Parent Claude: Using implementation agent based on task analysis.

Use the `implementation` subagent to execute the following task:

**Subtask objective**: Phase 7 - yank/transpose functionality

**Requirements**:
- Ctrl+Y (yank): Insert last deleted text
- Ctrl+T (transpose): Swap characters before/after cursor
- Follow existing key binding patterns
- Add test cases

**Implementation location**: src/tprompt.c, tests/test_keybindings.c

[Subagent implements, tests, reports]
```

### Example 2: Refactoring (refactoring agent)

```
/implement-with-agent Rename tprompt_cursor_move_foo to tprompt_cursor_foo

Parent Claude: Using refactoring agent for multi-file changes.

Use the `refactoring` subagent to execute the following task:

**Subtask objective**: Improve function name consistency

**Requirements**:
- Update declaration in src/tprompt_internal.h
- Update definition and all call sites in src/tprompt.c
- Update all test files in tests/
- Verify build and all tests pass

[Subagent searches thoroughly, updates, verifies]
```

### Example 3: Research + Quick Fix (research + quick-fix)

```
/implement-with-agent Find all TODO comments, prioritize, and fix simple ones

Parent Claude: Splitting into 2 phases.

**Step 1**: Use `research` subagent to investigate TODOs
**Step 2**: Use `quick-fix` subagent to fix simple ones

[Each agent executes sequentially]
```

## Agent Selection Cheat Sheet

| Task Type | Agent | Model | Reason |
|-----------|-------|-------|--------|
| Function rename (multi-file) | `refactoring` | Sonnet | Thorough search & update needed |
| New Phase implementation | `implementation` | Sonnet | Complex integration & testing |
| Add comments | `quick-fix` | Haiku | Simple & fast |
| Improve error messages | `quick-fix` | Haiku | Low risk, fast |
| TODO investigation | `research` | Haiku | Info gathering only, low cost |
| Understand codebase | `research` | Haiku | Specialized for exploration |
| Complex algorithm | `implementation` | Sonnet | Careful design needed |
| API migration | `refactoring` | Sonnet | Update all call sites |
