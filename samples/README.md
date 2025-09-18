# P0 Demo Application

`samples/p0_demo.c` demonstrates the minimal P0 surface:

- opens a handle with text-styles enabled via `enabled_caps`
- moves the cursor, writes text, and toggles styles by building `terse_style_t` values for `terse_set_style`
- resets styling with `terse_reset_style(handle, TERSE_RESET_ALL)` when returning to defaults
- captures and restores state (cursor position/visibility + styles)
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

- enable text styles via `TERSE_CAP_ENABLE_TEXT_STYLES`
- construct `terse_style_t` (set `effects`) and pass it to `terse_set_style`
- call `terse_reset_style` with an appropriate scope when restoring defaults
- capture/restore style state together with cursor position

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
