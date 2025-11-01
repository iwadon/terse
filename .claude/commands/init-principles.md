---
description: Update CLAUDE.md with a principles-focused approach that avoids time-sensitive details
---

Analyze this codebase and update CLAUDE.md with a principles-focused approach.

## Core Philosophy

CLAUDE.md should contain **timeless principles and guidelines**, NOT current implementation snapshots.

**Key Question:** "Will this information still be accurate in 6 months even if we add 20 new features?"
- If **YES** → Include it
- If **NO** → Either generalize it or reference the appropriate doc file

## What to Include (✅)

### Commands & Workflows (Concrete is OK)
- Build commands (CMake, Ninja, etc.)
- Test execution commands
- CLI tool usage
- Code formatting commands
- Development workflow steps

These rarely change, so concrete examples are valuable.

### Architecture & Design Decisions (Principles)
- **Red-Green Tree architecture**: Why it matters, key concepts (immutability, structural sharing)
- **Pratt parsing**: The approach, how precedence works (but NOT the specific enum values)
- **GreenNodeBuilder**: The backward wrapping technique and why it's critical
- **Trivia preservation**: The asymmetric collection strategy
- **Error recovery**: Non-throwing approach, synchronization points

Focus on the "why" and "how it works conceptually", not the "what exactly is implemented now".

### Testing (Organization, Not Counts)
- Framework used (GoogleTest)
- How tests are organized (e.g., "split by feature area")
- How to run tests (commands)
- Shared infrastructure (e.g., test base classes)
- **NEVER**: Specific test counts, exhaustive file lists

### Code Style (Conventions)
- Style guide (WebKit)
- File extensions (.hh, .cc)
- Formatting requirements
- Namespace conventions

### Documentation References
- What docs exist and when to consult them
- Position them as "authoritative sources" for current details

## What to Avoid (❌)

### Time-Sensitive Numbers
- ❌ Test counts (273 tests, 53 tests, etc.)
- ❌ File counts (12 test files)
- ❌ Precedence level counts (12 levels)
- ❌ Feature counts (50+ features)

### Implementation Details Better Suited for docs/
- ❌ Complete list of implemented language features
- ❌ Specific enum values or operator lists
- ❌ Syntax examples for every feature
- ❌ Detailed parsing rules

**Instead:** Provide a high-level summary and reference `docs/syntax.md`, `docs/parser.md`, etc.

### Duplicating Existing Documentation
- ❌ Copying content from README.md
- ❌ Repeating what's in docs/syntax.md
- ❌ Listing every file in the codebase

**Instead:** Reference where to find this information.

### Generic Development Advice
- ❌ "Write good tests"
- ❌ "Provide helpful error messages"
- ❌ "Common Development Tasks" sections with obvious tips

## Recommended Structure

1. **Project Overview**
   - High-level description
   - Current stage (lexer + parser implemented)
   - Future goals
   - Keep it brief (3-5 bullet points)

2. **Build Commands**
   - Concrete commands are good here
   - Setup, build, test, clean

3. **Testing**
   - Framework and organization principles
   - How to run tests (specific commands OK)
   - Requirements (all tests must pass)
   - NO test counts or exhaustive file lists

4. **Code Architecture**
   - Red-Green Tree (the design pattern and why)
   - Pratt Parsing (the approach, reference src/parser.hh for details)
   - GreenNodeBuilder (the backward wrapping technique)
   - Error Recovery (the approach)
   - Language Features (high-level categories, reference docs/syntax.md for current details)
   - Trivia Preservation (the strategy)
   - Component Flow (the pipeline)
   - CLI Tools (what they do)
   - Key Design Decisions (the principles)

5. **Code Style**
   - Style guide, conventions, formatting commands

6. **Development Workflow**
   - Step-by-step process

7. **Documentation**
   - What docs exist, what they cover, when to use them
   - Position as authoritative sources

8. **Important Notes**
   - Setup requirements, gotchas, context

## Example Transformations

### ❌ Time-Sensitive (Bad)
```
- Twelve precedence levels: None (0) < LogicalOr (1) < ... < Postfix (12)
- 273 total tests across 12 files
- Currently implements: arrays, tuples, match expressions, ... (50+ features)
```

### ✅ Principles-Focused (Good)
```
- Multiple precedence levels (logical, bitwise, comparison, arithmetic, cast, unary, postfix)
- Precedence hierarchy defined in `src/parser.hh` as `enum class Precedence`
- Tests organized by feature area in `tests/` directory
- Language supports Rust-like syntax. For current features, see `docs/syntax.md`
```

## Implementation Approach

1. **Read** the current CLAUDE.md
2. **Identify** sections with time-sensitive details
3. **Refactor** each section:
   - Extract the underlying principle
   - Remove specific numbers/counts
   - Add references to authoritative docs where appropriate
4. **Verify** each statement passes the "6-month test"
5. **Ensure** essential guidance remains clear and actionable

## Success Criteria

After this update, CLAUDE.md should:
- ✅ Help new Claude instances understand the architecture quickly
- ✅ Remain accurate even as features are added
- ✅ Require updates only when design principles change (rarely)
- ✅ Direct readers to appropriate docs for current details
- ✅ Avoid becoming a maintenance burden

Now analyze the codebase and update CLAUDE.md following these principles.
