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

- **Host**: macOS 26.0 (25A354)
- **Term Identifiers**: `TERM=xterm-256color`, `TERM_PROGRAM=Apple_Terminal`, `TERM_PROGRAM_VERSION=464`, `COLORTERM=truecolor`
- **TTY**: stdin/stdout report `isatty: yes`
- **Device Attributes**: Primary `\x1B[?1;2c`, Secondary `\x1B[>1;95;0c`
- **Focus Tracking**: No response to `\x1B[?1004$p`
- **Notes**: Behavior matches Ventura/Sonoma Terminal.app—truecolor advertised, focus query silent, DA signature unchanged.

## iTerm2 3.5.14

- **Host**: macOS 26.0 (25A354)
- **Term Identifiers**: `TERM=xterm-256color`, `TERM_PROGRAM=iTerm.app`, `TERM_PROGRAM_VERSION=3.5.14`, `LC_TERMINAL=iTerm2`
- **TTY**: stdin/stdout report `isatty: yes`
- **Device Attributes**: Primary `\x1B[?64;1;2;4;6;17;18;21;22c`, Secondary `\x1B[>64;2500;0c`
- **Focus Tracking**: Responds `\x1B[?1004;2$y`
- **Notes**: Secondary DA prefix `>64` distinguishes iTerm2; bracketed paste and mouse enabled by default.

## WezTerm 20240203-110809-5046fc22

- **Host**: macOS 26.0 (25A354)
- **Term Identifiers**: `TERM=xterm-256color`, `TERM_PROGRAM=WezTerm`, `TERM_PROGRAM_VERSION=20240203-110809-5046fc22`, `WEZTERM_EXECUTABLE=/Applications/WezTerm.app/Contents/MacOS/wezterm-gui`
- **TTY**: stdin/stdout report `isatty: yes`
- **Device Attributes**: Primary `\x1B[?65;4;6;18;22c`, Secondary `\x1B[>1;277;0c`
- **Focus Tracking**: Responds `\x1B[?1004;2$y`
- **Notes**: Secondary DA encodes Wez low prefix (`>1;277`). Env variables expose `WEZTERM_EXECUTABLE` for detection.

## kitty 0.42.2

- **Host**: macOS 26.0 (25A354)
- **Term Identifiers**: `TERM=xterm-kitty`, `TERMINFO=/Applications/kitty.app/Contents/Resources/kitty/terminfo`, `KITTY_PID` present
- **TTY**: stdin/stdout report `isatty: yes`
- **Device Attributes**: Primary `\x1B[?62;52;c`, Secondary `\x1B[>1;4000;42c`
- **Focus Tracking**: Responds `\x1B[?1004;2$y`
- **Notes**: kitty-specific `TERM` and `KITTY_PID` provide reliable env markers; DA advertises vendor code `4000`.

## Ghostty 1.2.0

- **Host**: macOS 26.0 (25A354)
- **Term Identifiers**: `TERM=xterm-ghostty`, `TERM_PROGRAM=ghostty`, `TERM_PROGRAM_VERSION=1.2.0`, `TERMINFO=/Applications/Ghostty.app/Contents/Resources/terminfo`
- **TTY**: stdin/stdout report `isatty: yes`
- **Device Attributes**: Primary `\x1B[?62;22;52c`, Secondary `\x1B[>1;10;0c`
- **Focus Tracking**: Responds `\x1B[?1004;2$y`
- **Notes**: Ghostty uses unique `TERM` and DA vendor code `>1;10;0` for discrimination.

## Warp 2025.09.10

- **Host**: macOS 26.0 (25A354)
- **Term Identifiers**: `TERM=xterm-256color`, `TERM_PROGRAM=WarpTerminal`, `TERM_PROGRAM_VERSION=v0.2025.09.10.08.11.stable_01`
- **TTY**: stdin/stdout report `isatty: yes`
- **Device Attributes**: Primary `\x1B[?62c`, Secondary `\x1B[>0;95;0c`
- **Focus Tracking**: No response to `\x1B[?1004$p`
- **Notes**: Warp shares the `>0;95;0` vendor code with Apple Terminal but sets `TERM_PROGRAM=WarpTerminal`; focus events disabled by default.

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

- Apple Terminal advertises `TERM_PROGRAM=Apple_Terminal` and a consistent secondary DA `>1;95;0`; no focus tracking response in either captured version.
- VTE-based terminals (GNOME Terminal) expose `COLORTERM=truecolor`, reply to focus queries, and carry `>61` / `>65` vendor IDs in secondary DA.
- iTerm2 and WezTerm both answer focus tracking, provide `>64` / `>1;277` secondary IDs, and surface clipboard/image-friendly capabilities via env vars.
- kitty and Ghostty use distinctive `TERM` values (`xterm-kitty`, `xterm-ghostty`) plus vendor-specific DA codes (`>1;4000;42`, `>1;10;0`) that simplify detection.

## Next Steps

- Capture Warp, Alacritty, Windows Terminal, and tmux-within-terminal scenarios to round out the capability matrix.
- Validate kitty graphic protocol vs. iTerm inline images so capability flags can distinguish image transports.
- Monitor future OS releases for Terminal.app/ghostty DA changes.
