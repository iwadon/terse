# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Terse is a C library that unifies rendering, input, and terminal capability detection for POSIX text UI environments. It abstracts terminal differences with graceful degradation, supporting colors, mouse, images, and other extended features through a profile-based system (P0-P3). The library automatically detects terminal capabilities and adapts features accordingly. See `docs/progress-overview.md` for currently supported terminals.

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
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DTERSE_USE_SYSTEM_ICONV=OFF
```
Or for Ninja with MSVC:
```cmd
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTERSE_USE_SYSTEM_ICONV=OFF
```

Note: Windows typically does not have iconv available, so `-DTERSE_USE_SYSTEM_ICONV=OFF` uses the built-in mini iconv implementation.

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
- `-DTERSE_USE_SYSTEM_ICONV=ON` (default): Use system iconv for charset conversions
- `-DTERSE_USE_SYSTEM_ICONV=OFF`: Use built-in mini iconv (Shift_JIS ↔ UTF-8 only)
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

### Running Tests Directly

All tests are compiled into a unified binary. Run it directly for verbose output:

Unix-like systems:
```sh
./build/c/tests/terse_unit_test
```

Windows:
```cmd
build\c\tests\Debug\terse_unit_test.exe
```

### Building Samples

Samples are built automatically by default when building as a top-level project. They will be located in `build/samples/` directory.

To manually compile a sample after building the library:

Unix-like systems:
```sh
cc -I./c/include -L./build/c -lterse samples/p0_demo.c -o p0_demo
./p0_demo
```

Windows (using MSVC compiler):
```cmd
cl /I.\c\include samples\p0_demo.c /link /LIBPATH:.\build\c\Debug terse.lib
p0_demo.exe
```

Control build components with CMake options:
- `-DTERSE_BUILD_TESTING=OFF`: Skip tests
- `-DTERSE_BUILD_TOOLS=OFF`: Skip tools
- `-DTERSE_BUILD_SAMPLES=OFF`: Skip samples

## Code Architecture

### Profile System (P0-P3)
The library organizes terminal features into hierarchical profiles:
- **P0**: Basic output (cursor movement, screen/line clearing, text output, size detection)
- **P1**: Colors and text decoration (SGR styles, 16/256/TrueColor support)
- **P2**: Advanced input/output (mouse tracking, bracketed paste, window title, hyperlinks)
- **P3**: Extended features (clipboard, inline images, cursor shapes, notifications)

`TERSE_PROFILE_AUTO` enables automatic detection and graceful degradation based on terminal capabilities. See `docs/terse-specs.md` for detailed profile specifications.

### Core Handle and State Management
- **terse_handle_t**: Opaque session handle created by `terse_open()` and destroyed by `terse_close()`
- **State management**: `terse_capture_state/terse_restore_state` and `terse_push_state/terse_pop_state` for safely preserving cursor position, visibility, and styles
- **Error handling**: Functions return `terse_error_t` enum codes; use `terse_get_last_error()` to retrieve the last error code
- **Coordinate system**: 0-based coordinates (0, 0) = top-left; internally converts to 1-based for terminal escape sequences

### Platform Abstraction (c/src/terse_platform.h)
The library provides platform abstraction through `c/src/terse_platform.h` with implementations for POSIX (macOS/Linux), Windows, Human68k, and a stub for unsupported platforms. Platform selection is automatic via CMake. See `docs/terse-platform-porting.md` for porting details.

### Terminal Detection
Environment variable inspection (`TERM_PROGRAM`, `TERM`, `VTE_VERSION`, etc.) combined with Secondary Device Attributes (DA) sequences to identify specific terminals and their feature sets. On Windows, uses console mode and VT sequence support detection.

### Codec Support (c/src/terse.c and c/src/mini_iconv.c)
- Multibyte character codec support with East Asian Width-based cell width estimation
- Built-in mini iconv when system iconv unavailable. See `docs/mini-iconv-plan.md` for supported charsets and design rationale

### Input Normalization (terse_read_event)
Parses terminal input into `terse_event_t` structures covering keyboard input, mouse events, resize events, and paste sequences. Unrecognized sequences fall through as `TERSE_EVENT_RAW_SEQUENCE`. See `docs/progress-overview.md` for current input handling status.

### Image Display (P3)
`terse_display_image()` automatically selects the best available image protocol based on terminal capability detection. See `docs/graphics-roadmap.md` for supported protocols and the image feature roadmap.

## Key Implementation Files

- `c/include/terse.h`: Public API surface (all `terse_*` symbols)
- `c/src/terse.c`: Core library implementation (platform-agnostic)
- `c/src/terse_posix.c`: POSIX platform layer (macOS/Linux - termios, poll, ioctl)
- `c/src/terse_windows.c`: Windows platform layer (Win32 Console API, ReadConsoleInput)
- `c/src/terse_human68k.c`: Human68k platform layer (X68000 system)
- `c/src/terse_platform_stub.c`: Stub for unsupported platforms
- `c/src/mini_iconv.c`: Fallback charset converter for Shift_JIS
- `c/src/terse_platform.h`: Platform abstraction interface

## Testing Strategy

All unit tests located in `c/tests/unit/`:
- Tests are compiled into a unified `terse_unit_test` executable using the attest framework
- Tests are registered via `add_test()` in `c/tests/CMakeLists.txt`
- Run all tests: `ctest --test-dir build --output-on-failure`
- Run unified test binary directly: `./build/c/tests/terse_unit_test` (Unix) or `build\c\tests\Debug\terse_unit_test.exe` (Windows)
- Test coverage spans core API and feature areas. See `c/tests/CMakeLists.txt` for the complete list

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

See `docs/README.md` for the full documentation index.

### User Documentation
- `docs/terse-api-user.md`: Application developer API guide
- `docs/terse-specs.md`: Profile specifications and degradation rules (large file, read with offset/limit as needed)
- `docs/progress-overview.md`: Implementation status summary

### Feature & Platform Documentation
- `docs/graphics-roadmap.md`: Image features roadmap
- `docs/mini-iconv-plan.md`: mini iconv implementation notes
- `docs/terse-platform-porting.md`: Porting guide for additional platforms
- `docs/human68k-keyboard.md`: Human68k keyboard mapping reference

### Internal Research Notes
- `docs/research/`: Terminal inspection results and detection research (for contributors)

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
