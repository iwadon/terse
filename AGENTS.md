# Repository Guidelines

## Project Structure & Module Organization
Core C sources live under `c/src` with headers in `c/include`; exported APIs belong in headers and shared structs stay internal unless needed externally. Unit tests sit in `c/tests/unit`, grouped by function. Add supporting fixtures beside the test that depends on them. Docs and design notes go under `docs`. Generated build artifacts remain inside `build/` so the workspace stays clean.

## Build, Test, and Development Commands
Configure the project once per checkout with `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`. Build C targets via `ninja -C build`; rebuilds are incremental. Run the suite with `ctest --test-dir build --output-on-failure` to surface stdout/stderr on failures. Use `ninja -C build terse` while iterating on the main library to limit the build scope.

## Coding Style & Naming Conventions
Follow the existing C style: tabs for indentation, K&R braces, and one declaration per line. Keep public identifiers prefixed with `terse_`; prefer `snake_case` for variables and functions, and `TERSE_UPPER` for enums or macros. Keep comments English-friendly even if code includes localized strings. Run `clang-format` if you touch more than a few lines; the repository uses its default LLVM style unless a local `.clang-format` overrides it.

## Testing Guidelines
Unit tests rely on the minimal harness in `c/tests/unit/test.h`. Name files `<function>_test.c`, suites as PascalCase (e.g., `TerseOpen`), and tests `ReturnsXxx_OnYyy` form, capped at one behavior each. Organize test bodies with optional `// Arrange`, `// Act`, `// Assert` markers. Always call `ctest --test-dir build --output-on-failure` before submitting. New functionality needs either new tests or an explanation for omissions.

## Commit & Pull Request Guidelines
Base your commits on the existing Conventional Commit style (`feat:`, `fix:`, etc.); short descriptions can be English or Japanese but keep the imperative mood. Group related changes together and avoid mixing refactors with behavior changes. Pull requests should summarize the impact, list testing done, and link issues when relevant. Attach screenshots or logs if a change affects user-visible behavior. Request review once CI is green and the branch has no TODOs.

## Security & Configuration Tips
Do not commit secrets or generated keys; keep configuration in environment variables or local CMake cache entries. For optional capabilities, gate experimental flags behind CMake options so downstream consumers can opt in explicitly.
