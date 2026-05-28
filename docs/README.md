# Terse Documentation

This directory contains documentation for the Terse terminal abstraction library.

## User Documentation

| Document | Description |
|----------|-------------|
| [terse-api-user.md](terse-api-user.md) | API guide for application developers |
| [terse-api-user.ja.md](terse-api-user.ja.md) | API guide (Japanese) |
| [terse-specs.md](terse-specs.md) | Profile specifications and degradation rules |
| [progress-overview.md](progress-overview.md) | Implementation status summary |

## Feature Documentation

| Document | Description |
|----------|-------------|
| [graphics-roadmap.md](graphics-roadmap.md) | Graphics features roadmap (Sixel, kitty, iTerm2) |
| [mini-iconv-plan.md](mini-iconv-plan.md) | Built-in charset converter design |

## Platform Documentation

| Document | Description |
|----------|-------------|
| [terse-platform-porting.md](terse-platform-porting.md) | Porting guide for additional platforms |
| [human68k-keyboard.md](human68k-keyboard.md) | Human68k (X68000) keyboard mapping reference |

## Contributor Documentation

| Document | Description |
|----------|-------------|
| [testing-terminal-detection.md](testing-terminal-detection.md) | How to write unit tests that depend on terminal detection (env-var hygiene) |
| [redesign-proposal.md](redesign-proposal.md) | (Draft) Redesign proposal: layered architecture, capability-centric API, buffered rendering |

## Internal Research Notes

The `research/` subdirectory contains internal development notes and investigation logs:

- [research/terminal-inspection.md](research/terminal-inspection.md) - Terminal DA/capability inspection results
- [research/macos-profile-detection.md](research/macos-profile-detection.md) - macOS terminal detection research

These notes are for contributors and may be incomplete or outdated.
