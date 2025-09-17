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
