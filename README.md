# clang-analyzer-etl

> **🚧 Work in Progress:** This plugin is currently under active development. The path-sensitive dataflow analysis (splitting states on `.has_value()` and `operator bool()`) is actively being implemented. 

A standalone Clang Static Analyzer (CSA) plugin designed to catch unchecked accesses to `etl::optional` and `etl::expected` from the [Embedded Template Library (ETL)](https://github.com/ETLCPP/etl).

## Why does this exist?
Clang-Tidy provides an excellent `bugprone-unchecked-optional-access` check, but it strictly hardcodes support for `std::optional`, `absl::optional`, and `base::Optional`. It does not support custom types or `expected` paradigms natively. 

Instead of maintaining a massive custom fork of `clang-tools-extra` to add ETL support, this project implements a standalone CSA plugin. It uses symbolic execution to trace execution paths and ensure that `.value()`, `.error()`, `operator*`, and `operator->` are only called *after* a successful state check.

## Features (Planned)
- [ ] Warns on unchecked `.value()` and `operator*` calls for `etl::optional`.
- [ ] Warns on unchecked `.value()` and `.error()` calls for `etl::expected`.
- [ ] Path-sensitive analysis: understands `if (opt.has_value())` and `if (opt)` to suppress false positives in safe branches.
- [ ] Tracks state across function boundaries and assignments.

## Building (WIP)
*Note: This requires LLVM/Clang development headers to be installed on your system.*
