# Terse

**English** | [日本語](README.ja.md)

Terse is a C library that unifies rendering, input, and terminal capability detection for text UI environments. It absorbs differences between terminals with safe degradation, and is designed to handle extended features such as colors, mouse, and images in a progressive manner.

## Features
- Profile control: `TERSE_PROFILE_AUTO` together with the P0-P3 profiles provides automatic degradation and capability clipping based on terminal support.
- Terminal detection: Identifies Apple Terminal / GNOME Terminal (VTE) / iTerm2 / WezTerm / kitty / Ghostty / Warp and estimates the available feature set.
- Rendering API: Covers cursor movement, screen/line clearing, text/style/color output, inline image display, and notifications.
- Input normalization: `terse_read_event` abstracts keys, mouse, resize, and bracketed paste events, with consistent handling of modifier keys.
- Encoding support: UTF-8 by default, with Shift_JIS conversion and a mini iconv fallback for multibyte I/O.
- Consistent state management: `terse_capture_state` / `terse_restore_state` and the push/pop API safely save and restore cursor and styles.

## Profiles

Terse manages terminal capabilities through four profile levels, progressively enabling features.

| Profile | Features |
|---------|----------|
| **P0** | Cursor movement, screen clearing, text output, size detection, input events |
| **P1** | P0 + 16/256/TrueColor, text decoration (bold, italic, underline, etc.) |
| **P2** | P1 + mouse tracking, bracketed paste, window title, hyperlinks |
| **P3** | P2 + clipboard, inline images, cursor shapes, desktop notifications |

Specifying `TERSE_PROFILE_AUTO` enables automatic capability detection and selection of an appropriate profile.

## Quick Start

### Requirements

- C99 compatible compiler (clang / GCC on POSIX systems, MSVC on Windows, m68k-xelf-gcc on Human68k)
- CMake 3.14 or later
- A CMake-compatible build tool (Ninja / Make / Visual Studio, etc.)

### Build

POSIX systems (macOS / Linux):

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Windows (MSVC):

```cmd
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Human68k (cross compile):

```sh
cmake -S . -B build-human68k -DCMAKE_TOOLCHAIN_FILE=cmake/human68k.cmake
cmake --build build-human68k
```

On Windows, iconv is not available by default, so the bundled mini iconv implementation is selected automatically (supports Shift_JIS ↔ UTF-8 only).

### Test

```sh
ctest --test-dir build --output-on-failure
```

On Windows, specify the configuration with `-C Debug`. Tests are not built for the Human68k target.

### Minimal Example

```c
#include "terse.h"
#include <unistd.h>

int main(void)
{
    terse_options_t options = {
        .input_fd = STDIN_FILENO,
        .output_fd = STDOUT_FILENO,
        .codec_name = "UTF-8"
    };

    terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
    if (!handle) return 1;

    terse_clear_screen(handle, TERSE_CLEAR_ALL);
    terse_move_to(handle, 0, 0);
    terse_write_text(handle, "Hello, Terse!");

    terse_close(handle);
    return 0;
}
```

## Samples

The `samples/` directory contains demos that exercise the features of each profile. They are built together with the library and placed under `build/samples/`. If you do not need the samples, pass `-DTERSE_BUILD_SAMPLES=OFF` to cmake.

P0:
- `p0_demo`: Basic cursor movement, screen clearing, and text output
- `line_edit_demo`: Simple line editor implemented using only the P0 API
- `cursor_position_test`: Verifies cursor position retrieval

P1:
- `p1_style_demo`: Text decorations such as bold, italic, and underline
- `p1_color_demo`: 16/256/TrueColor color grids

P2:
- `p2_features_demo`: Mouse tracking, bracketed paste, window title, and hyperlinks
- `mouse_click_demo`: Mouse click event capture
- `input_complete_demo`: Comprehensive log of input events including keys, mouse, and modifiers

P3:
- `p3_notifications_demo`: Bell, visual, and desktop notifications
- `p3_image_demo`: Inline image display selected from the terminal's capabilities
- `p3_sixel_demo`: Image display via the Sixel protocol
- `p3_kitty_graphics_demo`: Image display via the kitty graphics protocol
- `p3_image_protocol_fallback_demo`: Automatic image protocol selection and degradation

Other:
- `event_logger_demo`: Logs the contents of input events

## Verified Environments

- macOS 26.5 (arm64)
- Ubuntu 24.04.4 LTS (x86_64 WSL2)
- Ubuntu 24.04 LTS (aarch64)
- Debian 13 trixie (aarch64)
- Windows 11 25H2 (x86_64)
- Human68k (X68000)

## Documentation
- `docs/terse-api-user.md`: API guide for application developers
- `docs/terse-specs.md`: Profile specifications and degradation rules
- `docs/progress-overview.md`: Implementation status summary
- `docs/graphics-roadmap.md`: Roadmap for image-related features
- `docs/terse-platform-porting.md`: Guide for porting to additional platforms
- `docs/mini-iconv-plan.md`: mini iconv implementation notes

See the documents above for detailed samples and operational notes.

## License
MIT No Attribution (MIT-0). See `LICENSE` for details.
