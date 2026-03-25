# SolveSpace — Agent Guidelines

## Project Overview

SolveSpace is a parametric 2D/3D CAD application written in C++11. It consists of a
**portable core** (everything outside `src/platform/`) and **platform-specific UI** code
(`src/platform/`). Licensed under GPLv3+.

## Building

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Run tests:
```sh
make test_solvespace
# or directly:
./build/test/solvespace-testsuite
```

The test suite links against `solvespace-headless` (no GUI). Tests live in `test/`
organized by category (constraint, request, group, core, analysis).

## Architecture

| Directory | Purpose |
|-----------|---------|
| `src/` | Portable core: geometric kernel, constraint solver, file I/O, UI logic |
| `src/platform/` | Platform abstraction layer (`gui.h`, `platform.h`) and backends |
| `src/platform/gui*.cpp` | GUI backends: `guigtk.cpp` (GTK 3), `guiqt.cpp` (Qt), `guiwin.cpp` (Win32), `guimac.mm` (Cocoa), `guihtml.cpp` (Emscripten), `guinone.cpp` (headless) |
| `src/render/` | Rendering backends (OpenGL 1/3, Cairo, 2D export) |
| `src/slvs/` | C API for the constraint solver library |
| `src/srf/` | Surface/NURBS code |
| `include/` | Public header for the solver library (`slvs.h`) |
| `extlib/` | Vendored third-party libraries (built as CMake subprojects) |
| `res/` | Resources: icons, locales, shaders, fonts |
| `test/` | Test suite (branch-coverage focused) |
| `exposed/` | Additional language bindings |

## Coding Conventions

### Style
- **4-space indent**, no tabs, no trailing whitespace, 100-column wrap.
- Braces on same line as declaration/control flow (`Attach` style).
- Braces required on all control flow bodies, even single-statement.
- No space after `if`, `while`, `for` — write `if(cond)` not `if (cond)`.
- camelCase variables (`exampleVar`), PascalCase functions (`ExampleFunction`).
- Apply `.clang-format` via `git clang-format` before committing.

### C++ Usage
- **C++11 only** — do not use C++14/17/20 features.
- **No exceptions** — they interfere with branch-coverage measurement.
- **No operator overloading** — use named methods (e.g. `Plus`, `Minus`).
- **All members are public** — no access specifiers for implementation hiding.
- **No constructors for initialization** — use aggregate init `Foo foo = {};`.
- Output parameters are passed by pointer (`Vector *out`), never by reference.
- Use `ssassert(cond, "message")` instead of `assert()` — it is always enabled.
- Use `ssprintf()` for string formatting (remember `.c_str()` for std::string args).
- Use `ssfopen()` / `ssremove()` for filesystem access (UTF-8 safe).
- Strings are always UTF-8 internally.

### Platform Code
- Everything outside `src/platform/` must be portable standard C++11.
- Platform interaction goes through `src/platform/platform.h` and `src/platform/gui.h`.
- When adding a new platform backend, implement the abstract interfaces in `gui.h`
  (`Window`, `Menu`, `MenuBar`, `MenuItem`, `Timer`, `Settings`, `MessageDialog`,
  `FileDialog`) plus the free functions (`InitGui`, `RunGui`, `ExitGui`, etc.).

### Libraries
- External libraries must be portable, includable as CMake subprojects, and use a
  license less restrictive than GPL (BSD/MIT, Apache2, MPL).
- STL I/O streams are **not used** — SolveSpace has its own file I/O.
- Check `extlib/` and `CMakeLists.txt` for available dependencies before adding new ones.

## Testing

- Tests use the headless build (`solvespace-headless`).
- Test harness is in `test/harness.h` / `test/harness.cpp`.
- Tests compare against reference `.slvs` files and expected output.
- Branch coverage is tracked — avoid patterns that add untestable branches.
- `ssassert` and `switch` are excluded from coverage metrics.

## Plans

Migration/implementation plans are stored in `plans/`. Read them before starting
related work.
