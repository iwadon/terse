# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Terse is a C library that unifies rendering, input, and terminal capability detection for POSIX text UI environments. It abstracts terminal differences with graceful degradation, supporting colors, mouse, images, and other extended features through a profile-based system (P0-P3). The library automatically detects terminal capabilities (Apple Terminal, GNOME Terminal/VTE, iTerm2, WezTerm, kitty, Ghostty, Warp) and adapts features accordingly.

## Build Commands

### Unix-like Systems (macOS/Linux with Ninja)

#### Initial Configuration
```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

#### Build Targets
```sh
ninja -C build              # Build all targets
ninja -C build terse        # Build library only
```

### Windows (MSVC)

#### Initial Configuration
```cmd
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DTERSE_ENABLE_ICONV=OFF
```
Or for Ninja with MSVC:
```cmd
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTERSE_ENABLE_ICONV=OFF
```

Note: Windows typically does not have iconv available, so `-DTERSE_ENABLE_ICONV=OFF` uses the built-in mini iconv implementation.

#### Build Targets
With Visual Studio generator:
```cmd
cmake --build build --config Debug
cmake --build build --config Release
```

With Ninja generator:
```cmd
ninja -C build              # Build all targets
ninja -C build terse        # Build library only
```

### Build Options
- `-DTERSE_ENABLE_ICONV=ON` (default): Use system iconv for charset conversions
- `-DTERSE_ENABLE_ICONV=OFF`: Use built-in mini iconv (Shift_JIS ↔ UTF-8 only)
- `-DTERSE_ENABLE_TEST_MODE=ON`: Enable test mode with API recording and mocking capabilities

### Testing

Unix-like systems:
```sh
ctest --test-dir build --output-on-failure
```

Windows:
```cmd
ctest --test-dir build --output-on-failure -C Debug
```

### Running Individual Tests

Unix-like systems:
```sh
./build/c/tests/terse_open_test
./build/c/tests/terse_style_test
```

Windows:
```cmd
build\c\tests\Debug\terse_open_test.exe
build\c\tests\Debug\terse_style_test.exe
```

### Building Samples

Unix-like systems (after building the library):
```sh
cc -I./c/include -L./build/c -lterse samples/p0_demo.c -o p0_demo
./p0_demo
```

Windows (using MSVC compiler):
```cmd
cl /I.\c\include samples\p0_demo.c /link /LIBPATH:.\build\c\Debug terse.lib
p0_demo.exe
```

## Code Architecture

### Profile System (P0-P3)
The library organizes terminal features into hierarchical profiles:
- **P0**: Basic output (cursor movement, screen/line clearing, text output, size detection)
- **P1**: Colors and text decoration (SGR styles, 16/256/TrueColor support)
- **P2**: Advanced input/output (mouse tracking, bracketed paste, window title, hyperlinks)
- **P3**: Extended features (clipboard, inline images, cursor shapes, notifications)

`TERSE_PROFILE_AUTO` enables automatic detection and graceful degradation based on terminal capabilities.

### Core Handle and State Management
- **terse_handle_t**: Opaque session handle created by `terse_open()` and destroyed by `terse_close()`
- **State management**: `terse_capture_state/terse_restore_state` and `terse_push_state/terse_pop_state` for safely preserving cursor position, visibility, and styles
- **Error handling**: Functions return `terse_error_t` enum codes; use `terse_get_last_error()` to retrieve the last error code
- **Coordinate system**: 0-based coordinates (0, 0) = top-left; internally converts to 1-based for terminal escape sequences

### Terminal Detection (c/src/terse_posix.c)
Environment variable inspection (`TERM_PROGRAM`, `TERM`, `VTE_VERSION`, etc.) combined with Secondary Device Attributes (DA) sequences to identify specific terminals and their feature sets.

### Codec Support (c/src/terse.c and c/src/mini_iconv.c)
- UTF-8 and Shift_JIS support with multibyte decoding/encoding
- East Asian Width-based cell width estimation (combining characters=0, full-width=2)
- Built-in mini iconv when system iconv unavailable (design in `docs/mini-iconv-plan.md`)

### Input Normalization (terse_read_event)
Parses terminal input into `terse_event_t` structures covering:
- ASCII, control keys, arrows (with modifiers)
- Resize events, bracketed paste sequences
- SGR mouse events (button down/up/move/scroll with modifiers)
- Raw unrecognized sequences fall through as `TERSE_EVENT_RAW_SEQUENCE`

Currently limited: function keys and complex grapheme clusters need expansion.

### Image Display (P3)
`terse_display_image()` automatically selects the best available protocol:
- iTerm2 inline images (OSC 1337)
- Sixel graphics
- kitty graphics protocol

Legacy `terse_display_image_inline()` wraps the new API for compatibility.

## Key Implementation Files

- `c/include/terse.h`: Public API surface (all `terse_*` symbols)
- `c/src/terse.c`: Core library implementation
- `c/src/terse_posix.c`: POSIX platform layer (terminal I/O, detection, raw mode)
- `c/src/terse_platform_stub.c`: Stub for non-UNIX platforms
- `c/src/mini_iconv.c`: Fallback charset converter for Shift_JIS

## Testing Strategy

All unit tests located in `c/tests/unit/`:
- Each test is a standalone executable linked against the terse library
- Tests are registered via `add_test()` in `c/tests/CMakeLists.txt`
- Run all tests: `ctest --test-dir build --output-on-failure`
- Test coverage includes: open/close, output primitives, input parsing, styles, mouse, clipboard, images, notifications, keyboard features

### Test Mode
Build with `-DTERSE_ENABLE_TEST_MODE=ON` to enable test mode features:
- **API Call Recording**: Records all rendering and control API calls (move_to, write_text, set_style, etc.) for verification in automated tests
- **Capability Mocking**: Override terminal capabilities to test graceful degradation (colors, mouse, images)
- **Size Mocking**: Inject custom terminal dimensions to test UI layout at different sizes
- **Event Mocking**: Inject synthetic input events (keyboard, mouse, resize) for automated UI testing

Example usage in `samples/test_mode_demo.c` demonstrates recording API calls and mocking capabilities/size/events for regression testing and automated verification.

## Coding Style

- K&R brace style, tab indentation, one declaration per line
- Public API uses `terse_` prefix; internal symbols should remain in `c/src`
- Use `clang-format` for large diffs
- Error returns: `terse_error_t` enum codes; success returns `TERSE_OK` (0)

## Documentation Structure

- `docs/terse-api-user.md`: Application developer API guide
- `docs/terse-specs.md`: Profile specifications and degradation rules (large file, read with offset/limit as needed)
- `docs/progress-overview.md`: Implementation status summary
- `docs/graphics-roadmap.md`: Image features roadmap
- `docs/terse-platform-porting.md`: Porting guide for additional platforms
- `docs/mini-iconv-plan.md`: mini iconv implementation notes

## Adding New Features

When extending the library:
1. Determine the appropriate profile level (P0-P3)
2. Add capability flags to `terse_capabilities_t` if needed
3. Implement feature with graceful no-op fallback when unsupported
4. Add unit test in `c/tests/unit/` and register in `c/tests/CMakeLists.txt`
5. Update `docs/progress-overview.md` to reflect implementation status
6. Consider adding a sample in `samples/` if the feature is user-facing

## Common Patterns

### Capability Checking
```c
terse_capabilities_t caps = terse_get_capabilities(handle);
if (caps.colors >= TERSE_COLOR_TRUECOLOR) {
    // Use TrueColor
}
```

### Runtime Capability Override
```c
terse_capabilities_enable(handle, TERSE_CAP_ENABLE_TRUECOLOR);
terse_capabilities_disable(handle, TERSE_CAP_DISABLE_MOUSE);
terse_capabilities_reset_overrides(handle);  // Restore auto-detection
```

### Safe State Preservation
```c
terse_push_state(handle);
// Modify cursor/style
terse_pop_state(handle);  // Restore previous state
```
