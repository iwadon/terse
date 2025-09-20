# GEMINI.md

## Project Overview

This is the `terse` project, a C library for building rich terminal user interfaces. It provides a comprehensive API for terminal manipulation, styling, event handling, and advanced features like hyperlink and image support. The project is built with CMake and includes a suite of unit tests.

The core library is located in the `c/` directory, with the public API defined in `c/include/terse.h`. The `samples/` directory contains several demo programs that showcase the library's features.

## Building and Running

The project uses CMake for building and Ninja for compiling.

**1. Configure the project:**

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

**2. Build the project:**

```sh
ninja -C build
```

To build only the main library:

```sh
ninja -C build terse
```

**3. Run the tests:**

```sh
ctest --test-dir build --output-on-failure
```

**4. Build and run a sample:**

For example, to run the `p0_demo`:

```sh
cc -I../c/include -L../build/c -lterse samples/p0_demo.c -o p0_demo
./p0_demo
```

## Development Conventions

### Coding Style

*   **Indentation:** Tabs
*   **Braces:** K&R style
*   **Declarations:** One per line
*   **Naming:**
    *   Public identifiers: `terse_` prefix
    *   Variables and functions: `snake_case`
    *   Enums and macros: `TERSE_UPPER`
*   **Formatting:** The project uses `clang-format` with the LLVM style.

### Testing

*   Unit tests are located in `c/tests/unit`.
*   Tests are written using the minimal harness in `c/tests/unit/test.h`.
*   Test files are named `<function>_test.c`.
*   Test suites are named in PascalCase (e.g., `TerseOpen`).
*   Test cases are named in the form `ReturnsXxx_OnYyy`.
*   Run tests with `ctest --test-dir build --output-on-failure`.

### Commits and Pull Requests

*   **Commit Messages:** Follow the Conventional Commits specification (e.g., `feat:`, `fix:`).
*   **Pull Requests:** Summarize the impact of the changes, list the testing that was done, and link to any relevant issues.
