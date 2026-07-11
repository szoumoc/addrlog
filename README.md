# addrlog / memtrace

A **memory access tracer** built in two complementary modes:

1. **Standalone logger** (`main.cpp`) — explicit instrumentation via C++ template wrappers. Spawns threads that run sequential and random access patterns and reports stride statistics.
2. **LLVM pass** (`memtrace/`) — automatic, compiler-level instrumentation. An LLVM plugin rewrites every load/store in any C/C++ program at compile time, then links a lightweight runtime library that collects accesses and prints stride analysis on exit.

---

## Project structure

```
.
├── main.cpp                        # Standalone demo (explicit instrumentation)
├── logger.cpp                      # (reserved)
├── memtrace/
│   ├── CMakeLists.txt              # Build system for the LLVM plugin + runtime
│   ├── instrument.sh               # One-shot script: compile → instrument → run
│   ├── pass/
│   │   ├── MemTracePass.cpp        # LLVM function pass (injects logAccess calls)
│   │   └── memtrace_rt.h           # Header declaring the runtime ABI
│   ├── runtime/
│   │   ├── memtrace_rt.cpp         # Runtime: Logger class, logAccess, analyzeAndPrint
│   │   └── memtrace_rt.h           # Header for the runtime
│   └── test/
│       ├── test.cpp                # Test program: sequential + random array access
│       └── test2.cpp               # Test program: LCS dynamic programming
```

---

## How it works

### Standalone mode (`main.cpp`)

`main.cpp` defines a thread-local `Logger` that records every `loggedRead` / `loggedWrite` call. Two `std::jthread`s run concurrently:

- **Sequential test** — walks an array `data[0..N-1]` in order.
- **Random test** — walks `data2` through a shuffled `indices` permutation.

After both threads finish, each prints stride statistics (total, avg, min, max, zero-stride %, most-common strides, and cache-line change rate).

### LLVM pass mode (`memtrace/`)

```
source.cpp  ──clang──►  LLVM IR (.ll)
                             │
                         ──opt──►  instrumented IR (.ll)   ← MemTracePass inserts
                             │                                logAccess() before
                         ──clang++──►  binary               every load/store
                             │
                         links libmemtrace_rt.a
                             │
                         runs → prints stride report on exit
```

**`MemTracePass`** (in `pass/MemTracePass.cpp`):
- Iterates every `BasicBlock` and every `Instruction` inside each function.
- For each `LoadInst` or `StoreInst`, inserts a call to `logAccess(ptr, size, type)` immediately before the instruction.
- Skips its own runtime functions (`logAccess`, `analyzeAndPrint`) to avoid infinite recursion.
- In `main()`, inserts a call to `analyzeAndPrint()` before every `ReturnInst`.

**Runtime** (in `runtime/memtrace_rt.cpp`):
- `thread_local Logger logger` — one lock-free ring-buffer per thread, capacity 1 000 000 entries.
- `logAccess(void*, size_t, int)` — called by injected code; appends to the buffer atomically by slot.
- `analyzeAndPrint()` — separates reads and writes, computes consecutive-address strides, prints full statistics including cache-line boundary crossing rate.

---

## Prerequisites

| Tool | Version | Install (macOS) |
|------|---------|-----------------|
| Clang / LLVM | 18 | `brew install llvm@18` |
| CMake | ≥ 3.20 | `brew install cmake` |
| C++20 compiler | any modern | ships with LLVM |

> The build system hard-codes `/opt/homebrew/opt/llvm@18`. If your LLVM is installed elsewhere, update the `PATHS` in `memtrace/CMakeLists.txt` and the variables at the top of `instrument.sh`.

---

## Part 1 — Build and run the standalone logger

```bash
# from the repo root
clang++ -std=c++20 -O2 main.cpp -o main_logger
./main_logger
```

Expected output (abbreviated):

```
Thread <id>: Starting SEQUENTIAL test (N=10000)
Thread <id>: Starting RANDOM test (N=10000)

=== OPTION B: Type-Separated Strides ===
Total log entries: 20000
READ strides:
  Total: 9999
  Average: 4 bytes
  ...
  READ cache line changes: 78 (0.78%)

WRITE strides:
  ...
  WRITE cache line changes: 9921 (99.22%)
```

---

## Part 2 — Build the LLVM plugin and runtime

```bash
cd memtrace
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
```

After a successful build you will have:

```
memtrace/build/pass/libMemTracePass.dylib   ← the LLVM plugin
memtrace/build/runtime/libmemtrace_rt.a     ← the runtime static library
```

---

## Part 3 — Instrument and run a program

### Using the convenience script

`instrument.sh` does the full pipeline in one command:

```bash
cd memtrace
./instrument.sh test/test.cpp
```

What it does internally:

```bash
# 1. Compile to LLVM IR (no instrumentation yet)
clang -S -emit-llvm -O1 -I<runtime_dir> test/test.cpp -o /tmp/_mt.ll

# 2. Run the LLVM pass to inject logAccess calls
opt -load-pass-plugin build/pass/libMemTracePass.dylib \
    -passes=memtrace -S -o /tmp/_mt_inst.ll /tmp/_mt.ll

# 3. Compile instrumented IR + link runtime, then execute
clang++ /tmp/_mt_inst.ll build/runtime/libmemtrace_rt.a -o /tmp/test_bin
/tmp/test_bin
```

### Manual step-by-step

```bash
CLANG=/opt/homebrew/opt/llvm@18/bin/clang
OPT=/opt/homebrew/opt/llvm@18/bin/opt
PASS=memtrace/build/pass/libMemTracePass.dylib
RT=memtrace/build/runtime/libmemtrace_rt.a

# Compile your program to LLVM IR
$CLANG -S -emit-llvm -O1 -Imemtrace/runtime your_program.cpp -o /tmp/_mt.ll

# Instrument with the MemTrace pass
$OPT -load-pass-plugin $PASS -passes=memtrace -S -o /tmp/_mt_inst.ll /tmp/_mt.ll

# Link with runtime and run
clang++ /tmp/_mt_inst.ll $RT -o /tmp/test_bin && /tmp/test_bin
```

### Instrument a different test file

```bash
./instrument.sh test/test2.cpp   # instruments the LCS benchmark
```

---

## Sample output (LLVM pass mode)

```
=== MEMTRACE: Type-Separated Strides ===
Total log entries: 59998

READ strides:
  Total: 19999
  Sum: 79996 bytes
  Average: 4 bytes
  Min: 4 bytes
  Max: 4 bytes
  Zero strides: 0 (0%)
  Most common strides:
    4 bytes: 9999 (49.998%)
    ...
  READ cache line changes: 156 (0.78%)

WRITE strides:
  Total: 19999
  ...
  WRITE cache line changes: 19843 (99.22%)

Checksum: 140732920535192
```

---

## Reading the stride report

| Field | Meaning |
|-------|---------|
| **Total** | Number of consecutive-address differences recorded |
| **Sum / Average / Min / Max** | Basic statistics of the stride distribution in bytes |
| **Zero strides** | How often the same address was accessed back-to-back (temporal locality) |
| **Most common strides** | Top-5 stride values by frequency |
| **Cache line changes** | How often consecutive accesses crossed a 128-byte cache-line boundary (spatial locality proxy) |
| **Checksum** | Reproducibility guard — sum of all logged addresses, sizes, and types |

Low cache-line change rate on READs → good spatial locality (sequential pattern).  
High cache-line change rate on WRITEs → poor spatial locality (random pattern).

---

## Troubleshooting

**`opt: error: unable to load plugin`**  
Make sure the build succeeded and the `.dylib` path in `instrument.sh` / your command matches the actual output path.

**Linker warnings about macOS version**  
These come from LLVM's pre-built static libraries being compiled for a newer SDK. They are harmless and don't affect functionality.

**`cmake` can't find LLVM**  
Verify LLVM 18 is installed: `brew list llvm@18`. Then confirm the path `/opt/homebrew/opt/llvm@18/lib/cmake/llvm` exists and update `CMakeLists.txt` accordingly.

**Infinite recursion / crash**  
The pass guards against instrumenting `logAccess` and `analyzeAndPrint` themselves. If you add functions to the runtime, make sure they don't call `logAccess` or add their names to the guard check in `MemTracePass.cpp`.
