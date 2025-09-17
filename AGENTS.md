# AGENTS.md

## Project overview

## Build and test commands
Configure: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
Build: `ninja -C build`
Test: `ctest --test-dir build --output-on-failure`

## Code style guidelines

## Testing instructions

### Unit test naming guidelines
- File name: `<function>_test.c` (e.g., `terse_open_test.c`)
- Suite name: function name in PascalCase (e.g., `TerseOpen`)
- Test name format: Behavior_Condition, present tense, short words
  - Patterns: `ReturnsXxx_OnYyy`, `ShouldXxx_WhenYyy` (optional: `GivenYyy_WhenXxx_ThenZzz`)
  - Examples:
    - `TEST(TerseOpen, ReturnsNonNull_OnP1)`
    - `TEST(TerseOpen, ReturnsNull_OnInvalidProfile)`
    - `TEST(TerseClose, IsIdempotent_WhenCalledTwice)`
- Rules:
  - One test = one behavior; <= 60 chars; use `_` as word separator
  - Prefer fixed small vocabulary: `ReturnsNonNull`, `ReturnsNull`, `Succeeds`, `Fails`, `IsIdempotent`, `On`, `When`, `InvalidProfile`, `AlreadyOpen`, `Timeout`, `PermDenied`
  - Optionally use `// Arrange`, `// Act`, `// Assert` comments inside tests

## Security considerations
