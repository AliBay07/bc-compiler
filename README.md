# b-compiler (bcc v0.2.0)

A simple compiler for a basic language, written in C.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Directory Structure](#directory-structure)
- [Installation & Build](#installation--build)
- [Usage](#usage)
- [Testing](#testing)
- [Versioning](#versioning)

---

## Project Overview

`b-compiler` is a minimal educational compiler for a simple language. It takes `.bc` source files, parses them, and generates ARM assembly (`.s`). The project includes a test suite to verify correctness.

---

## Directory Structure

- `src/` — Source code for the compiler (lexer, parser, codegen, etc.)
- `include/` — Header files
- `tests/`
    - `test_files/` — Test input files (`.bc`)
    - `expected_results/` — Expected output for each test
    - `failed_assemblies/` — Stores `.s` files for failed tests
- `lib/` — Library files (e.g., `stdio.s`)
- `scripts/` — Helper scripts (`run_tests.sh`, `generate_executable.sh`)
- `Makefile` — Build and test automation
- `.gitignore` — Git ignore rules

---

## Installation & Build

### Prerequisites

- GCC or Clang (C compiler)
- Make
- CMake (for CLion or manual builds)
- ARM toolchain (if you want to run generated assembly)

### Build Instructions

**Using Make:**
```bash
make
```

The compiler binary (`bcc`) will be in `build/` or `cmake-build-debug/`.

---

## Usage

Compile a `.bc` file to ARM assembly:

```bash
./build/bcc path/to/source.bc
```

### Command-Line Options

The compiler supports the following options:

- `-h`, `--help`  
  Show help message. 

- `-v`, `--version`  
  Show version information.

- `-t`, `--tokens`  
  Display the token stream after lexing.

- `-a`, `--ast`  
  Display the abstract syntax tree (AST) after parsing.

- `-g`, `--show-registers`  
  Show register allocation details.

- `-r <arch>`, `--arch=<arch>`  
  Specify target architecture. Currently, only `ARM` is supported.

- `-s`, `--save-assembly`  
  Save the generated assembly file (`.s`). By default, the assembly file is deleted after linking.

- `-o <output>`  
  Specify the name of the output executable.

- `<input-file>`  
  Path to the input `.bc` source file (required).

## Testing

Tests are located in `tests/test_files/` with expected outputs in `tests/expected_results/`.

**Test Workflow:**
1. Each `.bc` file is compiled.
2. The resulting program is run.
3. Output is compared to the expected result.
4. On failure, the generated `.s` file is moved to `tests/failed_assemblies/` for inspection.

**To run tests:**

```bash
make test
```

or

```bash
./scripts/run_tests.sh
```

## Versioning
The version is defined by MAJOR.MINOR.PATCH :
```plaintext
* Major is incremented for the release of a stable version.
* Minor is incremented for new features or improvements.
* Patch is incremented for bug fixes or minor changes.
```

## To-Do List
- [ ] Implement more complex language features (e.g., loops, conditionals).
- [ ] Add more comprehensive tests.
- [ ] Add optimizations after parsing and code generation (e.g., dead code elimination, constant folding).


