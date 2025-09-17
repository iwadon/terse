# P0 Demo Application

`samples/p0_demo.c` demonstrates the minimal P0 surface:

- opens a handle with text-styles enabled via `enabled_caps`
- moves the cursor, writes text, and toggles styles with `terse_set_style`
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
- call `terse_set_style` with `TERSE_STYLE_*` flags
- capture/restore style state together with cursor position

## Line Editing Demo

`line_edit_demo.c` provides a minimal readline-style editor using only P0 APIs:

- terminal raw mode handled locally with `termios`
- cursor moves with `terse_move_to` and `terse_move_by` (indirectly via re-render)
- text styles toggled via `terse_set_style`
- state captured/restored with `terse_capture_state` / `terse_restore_state`
- input normalized by `terse_read_event`

Build:

```sh
cc -I../c/include -L../build/c -lterse line_edit_demo.c -o line_edit_demo
./line_edit_demo
```
