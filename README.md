# FileTracer

![FileTracer Demo](doc/demo.gif)

A thread-aware file access visualization tool that traces and visualizes file operations across processes and threads using ptrace.

Fully generated using Kavia.AI

## Features

- Complete process hierarchy tracking (fork, vfork, clone)
- Thread-aware operation monitoring with process context
- Temporal sequence visualization of file accesses
- Interactive HTML visualization with real-time search
- Collapsible directory and process trees
- Detailed thread/process relationship tracking

## Requirements

- Linux operating system
- GCC 8+ or Clang 7+
- CMake 3.15 or higher
- C++17 support

## Installation

```bash
mkdir build
cd build
cmake ..
make
