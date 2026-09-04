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

# Run with the agent attached (macOS/Linux); build output is javaplusplus.dylib/.so
java -agentpath:./build/javaplusplus.dylib=logpath=/tmp/profiler.log,jsonpath=/tmp/profiler.json,flamepath=/tmp/profiler.folded -cp test Main
```

Agent options (comma-separated `key=value`, all optional):
- `logpath=` — file the `Logger` appends to, in addition to stderr.
- `jsonpath=` — where `Reporter::dump_json` writes method stats + thread summaries on `VMDeath`.
- `flamepath=` — where `Reporter::dump_folded_stacks` writes a collapsed-stack file (`Class::method;Class::method;... count` per line) directly consumable by `flamegraph.pl` / speedscope.

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
- [x] **Section 4 — Method entry/exit instrumentation** *(superseded — see note)*
  - Enable `MethodEntry` and `MethodExit` events.
  - Maintain per-thread call stacks in `ThreadState`.
  - Compute inclusive and exclusive execution times.
  - **Note:** this approach — and the capabilities/callbacks it needed — was later removed from `Agent.cpp` in favor of statistical sampling (Section 7). Kept checked as a historical record of what was built; it does not reflect current agent behavior.
- [x] **Section 5 — Time sources** *(partially reworked — see note)*
  - Abstract `Clock` interface for monotonic wall-clock time.
  - Per-thread CPU time tracking via `GetCurrentThreadCpuTime`.
  - **Note:** `GetCurrentThreadCpuTime` was the *right* call for Section 4's design, since `MethodEntry`/`MethodExit` callbacks run on the thread that triggered them — "current thread" was the target thread. It does **not** work from the sampler thread, which observes *other* threads: that needs `GetThreadCpuTime(thread, ...)` plus the `can_get_thread_cpu_time` capability (different from `can_get_current_thread_cpu_time`). `Clock.hpp` was removed as dead code once Section 4 was ripped out; a per-thread CPU-time baseline is being rebuilt directly in `ThreadState`/`Sampler` as part of Section 7.
- [x] **Section 6 — First output: text + JSON dump**
  - Aggregate method statistics (`MethodStats`: count, total, self, min, max).
  - Dump top methods and per-thread summaries on `VMDeath`. (maybe think about adding isDaemon as a field)
  - Implement basic JSON export.

### Statistical Profiling
- [x] **Section 7 — CPU sampling thread**
  - Native background thread for statistical sampling — using `GetAllThreads` + `GetThreadState` + `GetStackTrace` on runnable threads directly, without `SuspendThread`/`ResumeThread`: JVMTI permits `GetStackTrace` on a live thread, so explicit suspension was dropped to avoid the extra overhead/complexity.
  - Aggregate method frequency to find hot methods (`frames[0]` per sample, plus every other frame on the stack — see below).
  - **Pulled forward from Section 10:** real per-sample time attribution via `GetThreadCpuTime` deltas against a per-thread baseline, walking the *full* captured stack — inclusive time to every frame, exclusive time to the top frame only.
  - Requires the `can_get_thread_cpu_time` capability, requested via `AddCapabilities` in `Agent_OnLoad` (the agent previously requested zero capabilities).
  - The per-thread CPU-time baseline lives in `ThreadState::last_cpu_time_ns`, updated by the sampler each sample. Since this is the first time the sampler thread reads/writes a `ThreadState` owned by another (possibly-dying) thread, `ThreadManager` gained a mutex guarding `SetThreadLocalStorage`/`delete` in `on_thread_end` against concurrent access, exposed via `ThreadManager::with_state(jvmti, thread, fn)` for safe cross-thread reads. Plain `ThreadManager::get_state` remains for call sites that only ever run on the owning thread or after the sampler has stopped.
- [x] **Section 8 — Call tree + flame graph output**
  - `CallTreeRegistry` (`src/profiling/CallTree.hpp`) — a Trie keyed on `jmethodID` sequences, built by `Sampler` alongside `MethodStatsRegistry` on every sample. Each node tracks `inclusive_time_ns`/`sample_count` (accrued at every node on the sampled stack) and `self_time_ns`/`self_count` (accrued only at the top-of-stack node).
  - `Reporter::dump_call_tree` — indented text call tree (count, inclusive %, exclusive %) logged alongside the existing top-20 methods list.
  - `Reporter::dump_folded_stacks` — writes a collapsed-stack file, one line per unique full stack (`Class::method;Class::method;... count`), directly consumable by `flamegraph.pl`/speedscope. Wired to the new `flamepath=` agent option.
  - `SymbolCache`'s `class_name` is the raw JVMTI class signature (e.g. `LMain;`), which itself contains a `;` — colliding with the `;` frame separator in the folded-stack format. `Reporter`'s internal `resolve_name`/`normalize_class_name` strip the `L`/`;` wrapper and convert `/` to `.` (`LMain;` → `Main`, `Ljava/lang/String;` → `java.lang.String`) before formatting any call-tree/folded-stack output. This only affects the new dump functions — `dump_report`/`dump_json` still show the raw signature.

### Additional Notes
- Using GLM to help scope and plan this out.
- Some of the CPP might be questionable lol
