# clang-analyzer-etl

A standalone Clang Static Analyzer (CSA) plugin designed to enforce checked memory access for `etl::optional` and `etl::expected` from the [Embedded Template Library (ETL)](https://github.com/ETLCPP/etl).

## Why does this exist?
Clang-Tidy provides an excellent `bugprone-unchecked-optional-access` check, but it strictly hardcodes support for `std::optional`, `absl::optional`, and `base::Optional`. It does not support custom types or `expected` paradigms natively. 

Instead of maintaining a custom fork of `clang-tools-extra`, this project implements a standalone CSA plugin. It uses **symbolic execution** to trace execution paths and ensure that `.value()`, `.error()`, `operator*`, and `operator->` are only called *after* a successful state check.

## Features

### Currently Supported
Because this plugin hooks directly into Clang's symbolic execution engine, it natively understands complex control flow. It correctly tracks state through:
* **Basic Checks:** `if (opt.has_value())` and `if (opt)` branches.
* **Expected Types:** Checks both `.value()` and `.error()` accesses for `etl::expected`.
* **Discarded expected Returns:** Warns when a function returns `etl::expected` and the result is ignored.
* **Discarded optional Returns:** Warns when a function returns `etl::optional` and the result is ignored.
* **Short-Circuiting:** Safely evaluates `if (opt && opt.value() > 0)` without false positives.
* **Boolean Casting:** Tracks state correctly through explicit variables (e.g., `bool is_safe = static_cast<bool>(opt); if (is_safe) { ... }`).
* **Loops & Branching:** Understands `while`, `for`, and ternary operators.
* **State Clearing:** Correctly marks regions as unsafe after `opt.reset()` or `opt.clear()` calls.
* **State Mutation Tracking:** Recognizes `optional` constructors, `emplace`, assignment, and `swap` state transfer.
* **Early Exits:** Respects standard early `return` paths.

### Work in Progress (Roadmap)
The following state-mutating operations are currently being implemented:
- [ ] **Constructors & Emplace:** Recognizing that `etl::optional<int> opt(42);` and `opt.emplace(42);` immediately make the state safe.
- [ ] **Assignments:** Recognizing that `opt = 42;` makes the state safe.
- [ ] **State Transfers:** Swapping states between two regions using `opt1.swap(opt2);`.

## Performance Considerations
This is a path-sensitive static analyzer, not a simple AST linter.  It builds a Control Flow Graph (CFG) and explores every possible branching execution path mathematically. 

* **Accuracy:** Yields near-zero false positives for dataflow tracking.
* **Speed:** It is significantly slower than standard compilation or Clang-Tidy. 
* **Deployment:** It is highly recommended to run this tool in CI/CD pipelines (e.g., GitHub Actions incrementally on Pull Requests or as a nightly full-codebase scan) rather than as an on-save IDE linter.

## Building
*Note: This requires LLVM/Clang development headers to be installed on your system (e.g., `llvm-dev` and `libclang-dev`).*

```bash
cmake --preset default
cmake --build --preset default
```

## Testing

This project uses Clang's `-verify` flag to ensure the plugin emits warnings exactly where expected. Run the test suite using:

```bash
clang++ -cc1 -load build_clang/libraries/EtlCheckerlib/libEtlChecker.so -analyze -analyzer-checker=custom.EtlAccessChecker,custom.EtlDiscardedExpectedChecker,custom.EtlDiscardedOptionalChecker libraries/EtlCheckerlib/tests/test_etl_access.cxx -verify
```

## Usage

Load the compiled shared library dynamically into Clang during analysis:

```bash
clang++ -cc1 -load build_clang/libraries/EtlCheckerlib/libEtlChecker.so -analyze -analyzer-checker=custom.EtlAccessChecker,custom.EtlDiscardedExpectedChecker,custom.EtlDiscardedOptionalChecker libraries/EtlCheckerlib/tests/test_etl_access.cxx
```

To run only the discarded-return checker:

```bash
clang++ -cc1 -load build_clang/libraries/EtlCheckerlib/libEtlChecker.so -analyze -analyzer-checker=custom.EtlDiscardedExpectedChecker libraries/EtlCheckerlib/tests/test_etl_access.cxx
```

To run only the discarded-optional checker:

```bash
clang++ -cc1 -load build_clang/libraries/EtlCheckerlib/libEtlChecker.so -analyze -analyzer-checker=custom.EtlDiscardedOptionalChecker libraries/EtlCheckerlib/tests/test_etl_access.cxx
```
