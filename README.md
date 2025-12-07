# Terse

Terse is a C library that unifies rendering, input, and terminal capability detection for POSIX text UI environments. It abstracts terminal differences with graceful degradation, supporting colors, mouse, images, and other extended features through a profile-based system.

[日本語版 README はこちら](README.ja.md)

## Profiles

Terse manages terminal capabilities through 4 profile levels, progressively enabling features:

| Profile | Features |
|---------|----------|
| **P0** | Cursor movement, screen clearing, text output, size detection, input events |
| **P1** | P0 + 16/256/TrueColor, text decoration (bold, italic, underline, etc.) |
| **P2** | P1 + mouse tracking, bracketed paste, window title, hyperlinks |
| **P3** | P2 + clipboard, inline images, cursor shapes, desktop notifications |

Use `TERSE_PROFILE_AUTO` for automatic capability detection and appropriate profile selection.

## Features

- **Profile control**: `TERSE_PROFILE_AUTO` with P0-P3 profiles provides automatic degradation and capability clipping based on terminal support.
- **Terminal detection**: Identifies Apple Terminal, GNOME Terminal (VTE), iTerm2, WezTerm, kitty, Ghostty, Warp, and estimates available feature sets.
- **Rendering API**: Cursor movement, screen/line clearing, text/style/color output, inline image display, and notifications.
- **Input normalization**: `terse_read_event` abstracts keys, mouse, resize, and bracketed paste events with consistent modifier key handling.
- **Encoding support**: UTF-8 by default, with Shift_JIS conversion and mini iconv fallback for multibyte I/O.
- **State management**: `terse_capture_state` / `terse_restore_state` and push/pop APIs safely preserve and restore cursor and styles.

## Minimal Example

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

## Quick Start

### Requirements
- C11 compatible compiler (clang, GCC, etc.)
- CMake 3.20 or later with Ninja
- POSIX compatible environment (tested on macOS and Linux)

### Configure and Build
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja -C build              # Build all targets
# or
ninja -C build terse        # Build library only
```

### Test
```sh
ctest --test-dir build --output-on-failure
```

### Try the Samples
The `samples/` directory contains profile-specific demos. After building the library, compile a sample like this:
```sh
cc -I./c/include -L./build/c -lterse samples/p0_demo.c -o p0_demo
./p0_demo
```

Other demos:
- `p1_style_demo.c`: Text decoration sample
- `p1_color_demo.c`: 16/256/TrueColor color grid
- `p2_features_demo.c`: Mouse, bracketed paste, title, hyperlinks
- `p3_notifications_demo.c`: Bell, visual, and desktop notifications
- `p3_image_demo.c`: Inline images for iTerm2/kitty
- `line_edit_demo.c`: Simple line editor using only P0 API

## Directory Structure
```
c/include        Public headers (terse.h, etc.)
c/src            Library implementation
c/tests/unit     Unit test suite
cmake/           CMake modules and toolchains
samples/         Feature-specific demo programs
docs/            Specifications, design notes, user guides
build/           CMake/Ninja build artifacts (generated)
```

## Documentation
- `docs/terse-api-user.md`: API guide for application developers
- `docs/terse-specs.md`: Profile specifications and degradation rules
- `docs/progress-overview.md`: Implementation status summary
- `docs/graphics-roadmap.md`: Graphics features roadmap
- `docs/terse-platform-porting.md`: Platform porting guide
- `docs/mini-iconv-plan.md`: Mini iconv implementation notes

See the above documents for detailed samples and usage notes.

## Development Workflow
- Build with CMake/Ninja. Run `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` once, then use `ninja -C build` for incremental builds.
- After making changes, always run `ctest --test-dir build --output-on-failure` and check stdout/stderr for failures.
- Coding style: K&R braces, tab indentation, one declaration per line. Use `clang-format` for larger diffs.
- Public API goes in `c/include` with `terse_` prefix. Keep shared structures internal (`c/src`) until needed.
- Commit messages follow Conventional Commits (`feat:`, `fix:`, etc.). Include verification details for smoother reviews.

## License
This project is licensed under the MIT No Attribution (MIT-0) license. See `LICENSE` for details.

## Support and Feedback
Bug reports and improvement suggestions are welcome via Issues or Pull Requests. For terminal-specific behavior or feature requests, please include reproduction steps and environment information (terminal type, requested profile, I/O encoding, etc.).
