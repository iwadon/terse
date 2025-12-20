# macOS Profile Detection Research

## Goals

- Detect the effective TERSE profile (P0–P3) automatically on macOS without requiring manual capability flags.
- Populate `terse_capabilities` fields with sensible defaults for common terminal emulators (Terminal.app, iTerm2, kitty, Warp, WezTerm, etc.).
- Provide fallbacks when the environment is unknown, erring on P0 safety but allowing opt-in overrides.

## Observed Environment Signals

### Environment Variables (per `env` in iTerm2)

| Variable | Notes |
| --- | --- |
| `TERM` | Usually `xterm-256color` for Terminal.app/iTerm2; kitty advertises `xterm-kitty`; Warp uses `xterm-256color`. |
| `TERM_PROGRAM` / `TERM_PROGRAM_VERSION` | `Apple_Terminal` vs `iTerm.app` vs `WezTerm` vs `Hyper`. Useful primary discriminator. |
| `LC_TERMINAL` / `LC_TERMINAL_VERSION` | Terminal.app=`Apple_Terminal`; iTerm2 exposes `iTerm2`. |
| `COLORTERM` | `truecolor` is a good signal for P1 color availability. |
| `ITERM_PROFILE`, `ITERM_SESSION_ID`, `WEZTERM_EXECUTABLE` | Vendor-specific hints.

### Terminfo

- `infocmp $TERM` provides declarative capabilities (e.g., `flash` entry for visual bell, `smcup`, `kmous`).
- `xterm-256color` terminfo claims mouse/flash support even if Terminal.app gates them; must cross-reference with terminal program ID.

### Runtime Probes

- **Device Attributes**: `CSI > 0 c` reply differs by terminal.
- **Operating System Command responses**: e.g., OSC 0 query (title), OSC 1337 (iTerm2 inline images) only answered/supported on some terminals.
- **Secondary DA** (`CSI > Pp ; Pv ; Pc c`) codes: xterm-based terminals send vendor/version numbers (iTerm2: `>0;95;0`).

## Candidate Terminal Profiles

| Terminal | Expected Profile | Known Capabilities |
| --- | --- | --- |
| Terminal.app (macOS 14+) | P1 (colors/styles), partial P2 (title) | No mouse by default, no OSC 52 unless pref enabled, no OSC 9, no visual bell. TrueColor advertised via `COLORTERM=truecolor` starting macOS 14. |
| Terminal.app (macOS 15 / 26 Tahiti) | P1 (colors/styles), partial P2 (title) | Primary DA `ESC[?1;2c`, secondary DA `ESC[>1;95;0c`; no response to focus query `CSI ?1004$p`. `COLORTERM=truecolor` is present. |
| iTerm2 3.5.x | P3 (images, clipboard, notifications) with opt-ins | Primary DA `ESC[?64;…c`, secondary DA `ESC[>64;2500;0c`, focus query echoes `?1004`, `?2004`, etc. Mouse (SGR), bracketed paste, OSC 52, OSC 1337, OSC 9, focus events. Notifications gated by prefs. |
| kitty | P3 (images, keyboard, notifications) | kitty graphics, keyboard protocol, focus events, clipboard. |
| Warp | P2? | Claims mouse, bracketed paste, but restricts raw control sequences. Needs validation. |
| WezTerm | P3 | kitty keyboard protocol, notifications, clipboard, images. |

## Suggested Detection Flow

1. **Gather Env Hints**
   - Read `TERM_PROGRAM`, `LC_TERMINAL`, `TERM`.
   - Normalize to lowercase to match known terminals.
2. **Identify Baseline Profile**
   - Map known terminals to default profiles:
     - Terminal.app → P1 (colors, styles).
     - iTerm2 / WezTerm / kitty → P3.
     - Unknown → conservative P0.
3. **Apply Feature Overrides**
   - For Terminal.app: disable bracketed paste, mouse, clipboard, images, notifications.
   - For iTerm2: enable bracketed paste, mouse, clipboard (write), notifications (bell + desktop if `TERM_PROGRAM_VERSION ≥ 3.4`), images (OSC 1337).
   - For kitty: enable images (kitty protocol), keyboard reporting (kitty), notification desktop.
4. **Probe When Needed**
   - If terminal not recognized, run lightweight queries:
     - Request DA (`CSI > 0 c`) and parse responses for `iTerm2`, `WezTerm`, `Apple_Terminal` signatures.
     - Optionally test for bracketed paste (enter mode, see if responses end up in input).
5. **Respect User Overrides**
   - Keep current `enabled_caps` / `disabled_caps` to allow apps to override detection.

## Open Questions

- How to detect when Terminal.app has OSC 52 allowed (disabled by default, toggle in Settings → Profiles → Terminal → “Allow clipboard access”)? Possibly send a short probe and expect pasteboard change—may be too invasive.
- Warp’s control sequence allowances are undocumented; need manual testing to avoid sending unsupported OSC 1337/9.
- Visual bell capability: terminfo claims support but Terminal.app ignores `CSI ?5h`. Need vendor-specific override list.
- Keyboard reporting (modifyOtherKeys/kitty protocol) enabling requires explicit negotiation; detection must avoid spurious enabling.

## Next Steps

1. Build a detection utility that prints gathered env vars and DA responses for different terminals to populate a capability matrix.
   - `tools/terse_inspect_terminal` provides this functionality; collect output across terminals to expand the matrix.
2. Implement a helper in `terse_open` (macOS path) that fills `terse_capabilities` using the mapping above.
3. Provide documentation/guidance for terminals requiring user opt-in (clipboard/notifications in iTerm2, etc.).
