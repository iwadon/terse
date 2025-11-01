---
description: Validate that CLAUDE.md follows principles-focused guidelines and identify time-sensitive content
---

Review CLAUDE.md and check if it follows principles-focused guidelines.

## Validation Criteria

### Check for Time-Sensitive Content (❌ Red Flags)

Look for these patterns that indicate content will become outdated:

1. **Specific Counts/Numbers**
   - Test counts (e.g., "273 tests", "53 tests")
   - File counts (e.g., "12 test files")
   - Precedence level counts (e.g., "twelve precedence levels")
   - Feature counts (e.g., "50+ features")
   - Line counts, function counts, etc.

2. **Exhaustive Lists**
   - Complete enumeration of test files with descriptions
   - Full list of implemented language features
   - All operators or syntax elements
   - All precedence levels with specific values
   - Every file in a directory

3. **Implementation Snapshots**
   - "Currently implements: X, Y, Z..." (long lists)
   - Detailed syntax examples for every feature
   - Specific enum values or constants
   - Code snippets that duplicate docs/

4. **Duplicated Documentation**
   - Content that's already in docs/syntax.md
   - Content that's already in docs/parser.md
   - Content that's already in docs/lexer.md
   - Content that's in README.md

### Check for Good Principles-Focused Content (✅ Green Flags)

Look for these patterns that indicate timeless content:

1. **Architectural Decisions**
   - Red-Green Tree design and why it matters
   - Pratt parsing approach (not specific precedence values)
   - GreenNodeBuilder's backward wrapping technique
   - Trivia collection strategy
   - Error recovery approach

2. **Generalized Descriptions**
   - "Tests organized by feature area" (not listing all files)
   - "Multiple precedence levels" with reference to source code
   - "Rust-like syntax including..." (categories, not full lists)
   - "See docs/syntax.md for current features"

3. **Concrete Commands**
   - Build commands (these rarely change)
   - Test execution commands
   - Formatting commands
   - CLI tool usage

4. **References to Authoritative Sources**
   - "Defined in src/parser.hh as enum class Precedence"
   - "For current features, see docs/syntax.md"
   - "Refer to docs/parser.md for implementation details"

## Validation Process

1. **Read CLAUDE.md** completely
2. **Scan each section** for red flags and green flags
3. **For each red flag found**, report:
   - The problematic content (quote it)
   - Why it's time-sensitive
   - How to fix it (generalize or reference docs)
   - Suggested replacement text

4. **Summary Report**:
   - List all issues found
   - Rate overall adherence to principles (High/Medium/Low)
   - Provide actionable recommendations

## Report Format

For each issue, use this format:

```
### Issue: [Section Name]

**Problem:**
> [Quote the problematic text]

**Why it's time-sensitive:**
[Explanation of why this will become outdated]

**Recommendation:**
[How to fix: generalize, remove, or reference docs]

**Suggested replacement:**
> [Proposed new text]
```

## Example Issues

### ❌ Example Issue

**Problem:**
> "Test files (273 total tests): lexer_test.cc (53 tests), parser_call_index_member_test.cc (35 tests)..."

**Why it's time-sensitive:**
Every new test added will make these numbers incorrect.

**Recommendation:**
Remove specific counts, keep organizational principle.

**Suggested replacement:**
> "Tests organized by feature area into separate files in tests/ directory"

### ✅ Example Good Content

**Good:**
> "Pratt parsing with operator precedence. Precedence hierarchy defined in src/parser.hh as enum class Precedence"

**Why it's good:**
References the source of truth instead of duplicating specific values.

## Final Checklist

After validation, confirm:
- [ ] No specific test/file/feature counts remain
- [ ] No exhaustive lists of current features
- [ ] Architectural principles are clearly explained
- [ ] References to authoritative docs are present
- [ ] Commands and workflows are concrete (this is OK)
- [ ] The "6-month test" passes: content will be accurate in 6 months

Now validate CLAUDE.md and provide a detailed report.
