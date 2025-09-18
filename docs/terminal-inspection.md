# Terminal Inspection Results

## Scope

- Capture reference outputs from `tools/terse_inspect_terminal` on the current target platforms.
- Record OS and kernel metadata alongside environment hints and probe responses.
- Summarize similarities and differences that matter for capability detection in TERSE.

## macOS 15.6 Terminal.app

- **Host**: macOS 15.6 (24G84) on Apple Terminal
- **Term Identifiers**: `TERM=xterm-256color`, `TERM_PROGRAM=Apple_Terminal`, `TERM_PROGRAM_VERSION=455.1`
- **TTY**: stdin/stdout report `isatty: yes`
- **Device Attributes**: Primary `\x1B[?1;2c`, Secondary `\x1B[>1;95;0c`
- **Focus Tracking**: No response to `\x1B[?1004$p`
- **Notes**: No vendor-specific env vars exposed; useful signature is the `TERM_PROGRAM` pair and low secondary DA version.

## macOS 26.0 Terminal.app

- **Host**: macOS 26.0 (25A354) on Apple Terminal
- **Term Identifiers**: _pending inspection_
- **TTY**: _pending inspection_
- **Device Attributes**: _pending inspection_
- **Focus Tracking**: _pending inspection_
- **Notes**: sw_vers output captured; rerun `tools/terse_inspect_terminal` once hardware is available to confirm DA and focus behavior.

## Debian 12 GNOME Terminal

- **Host**: Debian GNU/Linux 12 (bookworm), kernel `6.1.0-37-arm64 #1 SMP Debian 6.1.140-1 (2025-05-22)`
- **Term Identifiers**: `TERM=xterm-256color`, `COLORTERM=truecolor`
- **TTY**: stdin/stdout report `isatty: yes`
- **Device Attributes**: Primary `\x1B[?65;1;9c`, Secondary `\x1B[>65;7006;1c`
- **Focus Tracking**: Responds `\x1B[?1004;2$y`
- **Notes**: Signatures match VTE 65 (GNOME Terminal); `COLORTERM` advertises truecolor support.

## Ubuntu 24.10 GNOME Terminal

- **Host**: Ubuntu 24.10 (oracular), kernel `6.11.0-28-generic #28-Ubuntu SMP PREEMPT_DYNAMIC (2025-05-19)`
- **Term Identifiers**: `TERM=xterm-256color`, `COLORTERM=truecolor`
- **TTY**: stdin/stdout report `isatty: yes`
- **Device Attributes**: Primary `\x1B[?61;1;21;22;28c`, Secondary `\x1B[>61;7800;1c`
- **Focus Tracking**: Responds `\x1B[?1004;2$y`
- **Notes**: Secondary DA reports VTE build 7800; focus tracking enabled by default similar to Debian.

## Comparison Highlights

- All three terminals advertise `TERM=xterm-256color`; rely on `TERM_PROGRAM` where available or DA codes (`>1;95;0` vs `>65;7006;1` vs `>61;7800;1`) to differentiate.
- GNOME Terminal builds respond to focus tracking queries and expose `COLORTERM=truecolor`; Terminal.app does not reply to focus probes and omits `COLORTERM`.
- Primary DA codes differ: Terminal.app `?1;2c`, Debian GNOME `?65;1;9c`, Ubuntu GNOME `?61;1;21;22;28c`, giving a deterministic discriminator.

## Next Steps

- Add automated parsing in capability detection to map `TERM_PROGRAM=Apple_Terminal` or secondary DA `>1;95;0` to the Terminal.app profile.
- Treat VTE signatures (`>61;7800;1`, `>65;7006;1`) as GNOME Terminal variants and enable focus-tracking dependent features when detected.
- Track additional terminals (iTerm2, WezTerm, kitty, Warp) using the same recording process to extend the capability matrix.
