# C++ JVMTI Java Profiler

A custom Java profiler built from scratch in C++ using the JVM Tool Interface (JVMTI). This project is built incrementally, focusing on low-overhead, thread-safe native profiling.

## Prerequisites
* C++17 compatible compiler (Clang/GCC/MSVC)
* CMake (>= 3.10)
* Java Development Kit (JDK 8+)
* `JAVA_HOME` environment variable set.

## Build Instructions
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

## Run Instructions
```bash
# Compile your test Java code
javac test/Main.java

# Run with the agent attached (macOS/Linux)
java -agentpath:./build/libprofiler.so=logpath=/tmp/profiler.log -cp test Main

# (On macOS, the file might be profiler.dylib depending on CMake config)
java -agentpath:./build/profiler.dylib=logpath=/tmp/profiler.log -cp test Main
```

---

## Project Roadmap

### Core Infrastructure
- [x] **Section 0 — Skeleton agent + build system**
  - Set up CMake build system with `find_package(JNI)`.
  - Create `Agent.cpp` with `extern "C" Agent_OnLoad` entry point.
  - Fetch `jvmtiEnv*` and register basic `VMInit` and `VMDeath` callbacks.
  - Verified JVM loads the `.dylib`/`.so` successfully.
- [x] **Section 1 — Logging + error helpers**
  - Thread-safe `Logger` singleton writing to stderr and a configurable file (`logpath=...`).
  - Timestamps and log levels (`INFO`, `WARN`, `ERROR`).
  - `JvmtiHelper` namespace and `CHECK_JVMTI` macro for translating and logging `jvmtiError` codes.
- [x] **Section 2 — Symbol cache**
  - Thread-safe `SymbolCache` singleton using `std::shared_mutex`.
  - Resolves `jmethodID` to `MethodInfo` (class name, method name, signature).
  - Manages JVMTI memory allocation/deallocation safely.
- [x] **Section 3 — Thread-local agent state**
  - `ThreadManager` to track thread lifecycles.
  - `ThreadState` struct attached via JVMTI Thread Local Storage (`SetThreadLocalStorage`).
  - Assign sequential IDs and capture thread names.

### Instrumentation & Timing
- [x] **Section 4 — Method entry/exit instrumentation**
  - Enable `MethodEntry` and `MethodExit` events.
  - Maintain per-thread call stacks in `ThreadState`.
  - Compute inclusive and exclusive execution times.
- [ ] **Section 5 — Time sources**
  - Abstract `Clock` interface for monotonic wall-clock time.
  - Per-thread CPU time tracking via `GetCurrentThreadCpuTime`.
- [ ] **Section 6 — First output: text + JSON dump**
  - Aggregate method statistics (`MethodStats`: count, total, self, min, max).
  - Dump top methods and per-thread summaries on `VMDeath`.
  - Implement basic JSON export.

### Statistical Profiling
- [ ] **Section 7 — CPU sampling thread**
  - Native background thread for statistical sampling.
  - Use `SuspendThread`/`GetStackTrace`/`ResumeThread` (or `GetAllStackTraces`).
  - Aggregate method frequency to find hot methods.
- [ ] **Section 8 — Call tree + flame graph output**
  - Build a Trie structure keyed on `methodId` sequences.
  - Output indented call trees (count, inclusive %, exclusive %).
  - Output folded stacks for `flamegraph.pl`.

### Memory & Advanced Profiling
- [ ] **Section 9 — Allocation profiling**
  - `SampledObjectAlloc` events (Java 9+) or `VMObjectAlloc`.
  - Aggregate by class (count + bytes) and allocation call site.
- [ ] **Section 10 — Thread state + per-thread CPU**
  - Poll `GetThreadState` and `GetThreadCpuTime` via the sampler thread.
  - Bucket samples into `RUNNABLE`, `BLOCKED`, `WAITING`, etc.
- [ ] **Section 11 — Monitor contention**
  - `MonitorContendedEnter`/`Entered` and `MonitorWait`/`Waited` events.
  - Track blocked and waiting durations per lock/thread.
- [ ] **Section 12 — GC events**
  - `GarbageCollectionStart`/`Finish` pairing for pause times.
  - `GetMemoryUsage` before/after GC.
- [ ] **Section 13 — Heap census + object tagging**
  - `can_tag_objects` capability.
  - On-demand `IterateThroughHeap` to count live objects by class.

### Polish & Productionizing
- [ ] **Section 14 — Control plane**
  - `Agent_OnAttach` support for dynamic attachment.
  - Signal handlers (`SIGUSR1`) or Unix socket to trigger dumps/resets on demand.
- [ ] **Section 15 — Polish**
  - Cross-platform testing (Linux/Windows).
  - Configurable feature flags via agent options.
  - pprof output format support.
  - Overhead documentation.


### Additional Notes
- Using GLM to help scope and plan this out.
- Some of the CPP might be questionable lol