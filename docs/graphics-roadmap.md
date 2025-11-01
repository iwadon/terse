# Graphics Output Roadmap (Sixel / kitty)

## Current State
- API: `terse_display_image` (request-based) + compatibility wrapper `terse_display_image_inline`
- Capability reporting: `TERSE_IMAGE_ITERM_INLINE` / `TERSE_IMAGE_SIXEL` / `TERSE_IMAGE_KITTY` depending on detected terminal
- Detection: iTerm2 / WezTerm / kitty / Sixel環境を自動識別し、tmux/screen では保守的に `TERSE_IMAGE_NONE` にフォールバック
- Degrade path: 未対応環境ではノーオペ成功、`TERSE_IMAGE_FLAG_ALLOW_DEGRADE` をオフにすると `-ENOTSUP`

## Goals
1. Support **Sixel** output (DEC-compatible terminals, some tmux passthroughs)
2. Support **kitty graphics protocol** (direct binary chunks with compression)
3. Allow application to query which protocols are available and pick best option
4. Keep API surface minimal (reuse existing function where possible) while allowing future extension (streamed uploads, rectangles)

## Design Considerations
### Capabilities
- Extend `terse_image_support_t` enum:
  - `TERSE_IMAGE_NONE`
  - `TERSE_IMAGE_ITERM_INLINE`
  - `TERSE_IMAGE_SIXEL`
  - `TERSE_IMAGE_KITTY`
- Add bitfield in `terse_capabilities_t` if simultaneous protocols can be available (e.g., some terminals support kitty + sixel). Two approaches:
  1. Keep enum but allow chained detection: treat kitty preference > iterm > sixel. Document deterministic priority order.
  2. Change to bitmask (breaking ABI). To avoid breakage, choose option 1 for now; add helper API later if multi-protocol selection required.
- Introduce runtime enable/disable flags for image transports? (Probably not necessary initially; rely on detection + degrade.)

### Environment Detection
- **Sixel**: `$TERM` に `sixel` を含むもの (`xterm-sixel`, `mlterm`, `contour`, `yaft-sixel` など) を優先。Secondary DA の `;4;` ビットなど追加検知は今後検討。
- **kitty**: `$TERM` == `xterm-kitty`, `KITTY_PID` 環境変数、もしくは Secondary DA `ESC[>1;4000;...c`。
- **WezTerm**: `$TERM_PROGRAM` や `WEZTERM_EXECUTABLE`、Secondary DA `ESC[>1;277;...c` を手掛かりに `TERSE_IMAGE_KITTY` を広告。
- **tmux/screen**: `TMUX` / `TERM` に `tmux` / `screen` が含まれる場合は `TERSE_IMAGE_NONE` に縮退（安全側）。

### API Shape
- Adopt new entry point `terse_display_image(handle, const terse_image_request_t *request)` while keeping `terse_display_image_inline` as a backwards-compatibility shim.
- `terse_image_request_t` (tentative fields):
  - `const unsigned char *data;`
  - `size_t size;`
  - `terse_image_format_t format;` (`TERSE_IMAGE_FORMAT_AUTO`, `PNG`, `JPEG`, `SIXEL`, etc.)
  - Optional width/height hints (pixels) for Sixel scaling.
  - Optional `const char *name;` (used where supported, e.g., iTerm/kitty).
  - `unsigned int flags;` (`TERSE_IMAGE_FLAG_INLINE`, `TERSE_IMAGE_FLAG_ALLOW_DEGRADE`, ...)
- `terse_display_image_inline` will internally build a request with `format=AUTO`, `flags=INLINE|ALLOW_DEGRADE` and call the new API。現状は iTerm inline / Sixel / kitty graphics を自動判別し、互換ルートからでも新 API の経路を通る。
- Implementation phases (完了済み):
  1. Capability detection更新（Sixel / kitty / iTerm inline の広告）
  2. 送信ヘルパー実装（Sixel チャンク出力 / kitty OSC_G ベース64）
  3. `terse_display_image_inline` を新 API に委譲し、互換性維持

### Encoding / Transmission
- **Sixel**: Terminal expects `ESC P q ... ESC \`. Need to chunk output, optionally apply RLE. Strategy: accept already-encoded Sixel, and supply helper `terse_image_encode_sixel_rgba` later. For now support basic streaming with `write_sequence`.
- **kitty**: Use `OSC_G` frames (`ESC_G` ... `ESC\`) with parameters such as `a=T` (base64), `f=100` (PNG), `m=1` (final chunk). Current implementation emits a single base64 chunk per image; future work may add compression and streaming refinements.

### Error Handling / Fallback
- If chosen protocol fails (transport error), set `TERSE_ERROR_TRANSPORT`.
- If data format unsupported (e.g., request asks for kitty but capability is Sixel), return `-ENOTSUP` or degrade to available protocol if `TERSE_IMAGE_FLAG_ALLOW_DEGRADE` set.
- Logging/test harness to confirm fallback (unit tests with pipe verifying sequences) + golden tests.

### Testing Strategy
- Extend unit tests in `c/tests/unit/terse_image_test.c` to validate capability checks and emitted sequences using fake FDs.
- **完了**: `samples/` にデモを追加済み（`p3_sixel_demo.c`, `p3_kitty_graphics_demo.c`, `p3_image_protocol_fallback_demo.c`）。利用可能なプロトコルの出力・フォールバック挙動を可視化。
- ドキュメント更新（API/仕様）は完了済み。

## Proposed Implementation Steps
1. Update enums and capability structure; adjust detection for kitty/sixel; add degrade priority logic.
2. Introduce internal send helpers:
   - `static int send_iterm_inline(...)` (reuse existing logic)
   - `static int send_sixel(...)`
   - `static int send_kitty_graphics(...)`
3. Add new public API (if chosen) or extend existing function; ensure backward compatibility.
4. Update documentation + samples.
5. Add tests verifying sequences for each capability.

## Open Questions / Input Needed
- Do we want automatic PNG/JPEG -> Sixel encoding inside library? (Large scope; might defer.)
- Should kitty chunks handle compression (`z=` parameter)? (Optional for MVP.)
- How to expose partial support (e.g., kitty with read-back disabled)? Possibly via future capability flags.

