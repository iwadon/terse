# Terse Sample Programs

This directory contains demonstration programs showcasing the features of the Terse library across all profile levels (P0-P3).

## Building the Samples

### Prerequisites
- CMake 3.10 or later
- Ninja (or your preferred CMake generator)
- C compiler (GCC, Clang, etc.)

### Build Instructions

1. **Configure the build**:
   ```sh
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
   ```

2. **Build all samples**:
   ```sh
   ninja -C build
   ```

   The compiled samples will be located in `build/samples/`.

3. **Run a sample**:
   ```sh
   ./build/samples/p0_demo
   ./build/samples/p1_color_demo
   # etc.
   ```

## Samples by Category

### Profile P0: Basic Output

#### `p0_demo`
**Purpose**: Demonstrates fundamental terminal operations available at P0 profile level.

**Features**:
- Cursor movement with `terse_move_to()`
- Text output with `terse_write_text()`
- Screen clearing with `terse_clear_screen()`
- State capture/restore for cursor position
- Basic event reading

**Usage**:
```sh
./build/samples/p0_demo
```

**What to expect**: The demo displays text at various positions, captures and restores cursor state, then enters an interactive mode where you can press keys (press 'q' to quit).

---

### Profile P1: Colors and Styles

#### `p1_color_demo`
**Purpose**: Showcases comprehensive color support including 16-color, 256-color palette, and TrueColor.

**Features**:
- Basic 16 colors (normal and bright variants)
- Full 256-color palette display
- TrueColor (24-bit RGB) gradients
- Foreground and background color combinations

**Usage**:
```sh
./build/samples/p1_color_demo
```

**What to expect**: A colorful display showing:
- 16x16 grid of basic color combinations
- 256-color palette cube (indices 16-231)
- Smooth TrueColor gradient

**Requirements**: Terminal with color support (most modern terminals).

#### `p1_style_demo`
**Purpose**: Demonstrates text styling effects.

**Features**:
- Bold, Faint, Italic
- Underline, Strike-through
- Inverse (reverse video)
- Blink
- Combined effects

**Usage**:
```sh
./build/samples/p1_style_demo
```

**What to expect**: Various text styling effects displayed sequentially with brief pauses between each.

---

### Profile P2: Advanced Input/Output

#### `p2_features_demo`
**Purpose**: Shows P2-level features including mouse tracking, bracketed paste, hyperlinks, and window title.

**Features**:
- Mouse event tracking (clicks, movements, scrolling)
- Bracketed paste mode
- Window title setting
- Hyperlink support (OSC 8)

**Usage**:
```sh
./build/samples/p2_features_demo
```

**What to expect**: Interactive demo that reports mouse events, paste operations, and displays a hyperlink. Press 'q' to quit.

**Note**: Requires raw terminal mode (handled by the demo).

---

### Profile P3: Extended Features

#### `p3_notifications_demo`
**Purpose**: Demonstrates various notification mechanisms.

**Features**:
- Desktop notifications (OSC 9 or OSC 777)
- Visual bell
- Audible bell (BEL)

**Usage**:
```sh
./build/samples/p3_notifications_demo
```

**What to expect**: Interactive shell where you can trigger notifications:
- `b <message>` - Send desktop notification
- `v` - Visual bell
- `g` - Audible bell
- `q` - Quit

**Requirements**: Terminal with notification support (iTerm2, WezTerm, etc.).

#### `p3_image_demo`
**Purpose**: Displays images using iTerm2 inline image protocol (OSC 1337).

**Features**:
- iTerm2 inline image display
- File loading and rendering

**Usage**:
```sh
./build/samples/p3_image_demo <image-file>
```

**What to expect**: The specified image file is displayed inline in the terminal.

**Requirements**: iTerm2 or WezTerm with inline image support.

#### `p3_sixel_demo`
**Purpose**: Demonstrates Sixel graphics protocol with auto-generated test patterns.

**Features**:
- Sixel graphics rendering
- RGB gradient generation
- Graceful degradation with `TERSE_IMAGE_FLAG_ALLOW_DEGRADE`
- Auto-format detection

**Usage**:
```sh
./build/samples/p3_sixel_demo
```

**What to expect**: Three test images displayed using different rendering strategies:
1. Strict Sixel-only mode
2. Sixel with degradation allowed
3. Auto-format selection

**Requirements**: Terminal with Sixel support (xterm, mlterm, WezTerm, etc.).

#### `p3_kitty_graphics_demo`
**Purpose**: Demonstrates kitty graphics protocol.

**Features**:
- Kitty graphics protocol rendering
- RGB test pattern generation
- Strict vs. degraded rendering modes
- Auto-protocol detection

**Usage**:
```sh
./build/samples/p3_kitty_graphics_demo
```

**What to expect**: Multiple test renders showing protocol behavior in different modes.

**Requirements**: kitty terminal or compatible terminal.

#### `p3_image_protocol_fallback_demo`
**Purpose**: Comprehensive demonstration of image protocol priority and fallback behavior.

**Features**:
- Protocol priority testing (Kitty > iTerm2 > Sixel)
- Graceful degradation with `TERSE_IMAGE_FLAG_ALLOW_DEGRADE`
- Auto-format selection
- Terminal capability detection and reporting

**Usage**:
```sh
./build/samples/p3_image_protocol_fallback_demo
```

**What to expect**: Detailed testing of each protocol individually, followed by auto-detection and degradation tests. Includes capability reporting.

---

### Input Handling

#### `event_logger_demo`
**Purpose**: Comprehensive event logger that displays all input events with detailed information.

**Features**:
- Keyboard event logging (characters, modifiers, function keys)
- Mouse event logging (clicks, moves, scrolls)
- Raw sequence capture
- Window resize events
- Enhanced keyboard protocol support (modifyOtherKeys, kitty protocol)

**Usage**:
```sh
./build/samples/event_logger_demo
```

**What to expect**: Every keyboard press, mouse action, and terminal event is logged with full details. Press Ctrl+C to exit.

**Note**: Raw terminal mode is automatically configured.

#### `input_complete_demo`
**Purpose**: Demonstrates comprehensive input event handling with user-friendly display.

**Features**:
- All keyboard event types
- Function keys (F1-F12)
- Navigation keys (Home, End, PageUp, PageDown, Insert, Delete)
- Arrow keys with modifiers
- Enhanced keyboard protocol support

**Usage**:
```sh
./build/samples/input_complete_demo
```

**What to expect**: Interactive demo showing formatted event information. Press ESC to exit.

#### `mouse_click_demo`
**Purpose**: Interactive mouse tracking demonstration with visual feedback.

**Features**:
- Mouse click tracking and visualization
- Different marks for left (●), right (○), and middle (◆) buttons
- Scroll wheel detection
- Window resize handling
- Up to 100 marks displayed

**Usage**:
```sh
./build/samples/mouse_click_demo
```

**What to expect**: Click anywhere in the terminal to place marks. Press 'c' to clear, 'q' to quit.

**Requirements**: Terminal with mouse support.

#### `line_edit_demo`
**Purpose**: Full-featured line editing with Unicode support.

**Features**:
- Character insertion and deletion
- Cursor movement (arrows, Home, End)
- Combining character support
- Emacs-style keybindings:
  - Ctrl+A: Beginning of line
  - Ctrl+E: End of line
  - Ctrl+U: Delete to beginning
  - Ctrl+K: Delete to end
  - Ctrl+W: Delete previous word
- Bracketed paste mode
- Colored prompt
- Bar cursor shape

**Usage**:
```sh
./build/samples/line_edit_demo [--shift-jis|--sjis]
```

**What to expect**: Interactive line editor with full editing capabilities. Press Enter to submit, Ctrl+C to cancel.

**Optional**: Pass `--shift-jis` to use Shift_JIS codec instead of UTF-8.

---

## Common Features Across Demos

Most demos share these characteristics:

### Raw Terminal Mode
Demos requiring input handling (event loggers, line editor, mouse demo) automatically configure raw terminal mode and restore the original terminal state on exit.

### Error Handling
All demos include comprehensive error checking and report detailed error information using `terse_get_last_error()`.

### Capability Detection
Advanced demos (P2/P3) check terminal capabilities before enabling features and gracefully degrade when features are unavailable.

### Signal Handling
Interactive demos handle SIGINT (Ctrl+C) gracefully, ensuring proper cleanup.

---

## Troubleshooting

### No Color Display
- Verify your terminal supports colors: `echo $TERM`
- Try setting `TERM=xterm-256color` or `TERM=xterm-truecolor`

### Mouse Events Not Working
- Ensure your terminal supports mouse tracking (most modern terminals do)
- Check if another program is capturing mouse events

### Images Not Displaying
- Check terminal compatibility:
  - iTerm2: `p3_image_demo`
  - Sixel: `p3_sixel_demo` (xterm, mlterm, WezTerm)
  - Kitty: `p3_kitty_graphics_demo` (kitty terminal)
- Use `p3_image_protocol_fallback_demo` to test protocol detection

### Function Keys Not Recognized
- Some terminals require enhanced keyboard protocol support
- Try `input_complete_demo` which enables enhanced protocols when available

---

## Further Reading

- **API Documentation**: `docs/terse-api-user.md`
- **Profile Specifications**: `docs/terse-specs.md`
- **Implementation Status**: `docs/progress-overview.md`
- **Graphics Features**: `docs/graphics-roadmap.md`

---

## Contributing

When adding new samples:

1. Add the source file to this directory
2. Register it in `samples/CMakeLists.txt`
3. Update this README with a description
4. Include usage examples and requirements
5. Test on multiple terminal emulators
