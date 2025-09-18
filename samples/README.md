# P0 Demo Application

`samples/p0_demo.c` demonstrates the minimal P0 surface:

- opens a handle with default capabilities (no optional styles/colors)
- moves the cursor, writes text, and uses `terse_clear_*` calls
- captures and restores cursor state with `terse_capture_state` / `terse_restore_state`
- reads events in a simple loop, illustrating `TERSE_EVENT_NONE` and character events
- shows how `terse_get_last_error` can be used to log transport/config errors

Build and run:

```sh
cc -I../c/include -L../build/c -lterse p0_demo.c -o p0_demo
./p0_demo
```

(Adjust include/library paths based on your build layout.)

## Style Demonstration

`p0_demo.c` shows how to:

- use the basic cursor/output APIs at P0
- capture/restore cursor position and visibility
- handle `terse_read_event` timeouts and EOFs

## P1 Style Demo

`p1_style_demo.c` focuses on text effects (P1 capabilities):

- enables text styles via `TERSE_CAP_ENABLE_TEXT_STYLES`
- renders bold, faint, italic, underline, inverse, blink, and strike examples
- uses `terse_reset_style` between samples to restore defaults

Build and run:

```sh
cc -I../c/include -L../build/c -lterse p1_style_demo.c -o p1_style_demo
./p1_style_demo
```

## P1 Color Demo

`p1_color_demo.c` focuses on color output (P1 capabilities):

- enables SGR basic/extended/truecolor via `enabled_caps`
- renders a basic16 foreground/background grid with `terse_color_basic`
- shows the 6×6×6 palette cube and a truecolor gradient using the helper constructors
- uses `terse_reset_style` between sections to keep the terminal tidy

Build and run:

```sh
cc -I../c/include -L../build/c -lterse p1_color_demo.c -o p1_color_demo
./p1_color_demo
```

## P2 Features Demo

`p2_features_demo.c` showcases the new P2 APIs:

- enables SGR mouse tracking (`terse_enable_mouse(…, TERSE_MOUSE_SGR)`)
- enables bracketed paste notifications
- updates terminal title and emits OSC 8 hyperlinks
- prints a running log of mouse / paste / key events

Build and run:

```sh
cc -I../c/include -L../build/c -lterse p2_features_demo.c -o p2_features_demo
./p2_features_demo
```

## Line Editing Demo

`line_edit_demo.c` provides a minimal readline-style editor using only P0 APIs:

- terminal raw mode handled locally with `termios`
- cursor moves with `terse_move_to` and `terse_move_by` (indirectly via re-render)
- text styles toggled via `terse_set_style` with `terse_style_t`, and cleared via `terse_reset_style`
- state captured/restored with `terse_capture_state` / `terse_restore_state`
- input normalized by `terse_read_event`

Build:

```sh
cc -I../c/include -L../build/c -lterse line_edit_demo.c -o line_edit_demo
./line_edit_demo
```
