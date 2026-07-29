# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A native Java profiling agent written in C++17 against JVMTI (JVM Tool Interface), built as a shared library (`.dylib`/`.so`) that attaches to a JVM via `-agentpath`. It is being built incrementally per the roadmap in `readme.md` — check that file for what's implemented vs. planned before assuming a feature exists.

## Build

```bash
./build.sh
# or manually:
mkdir -p build && cd build && cmake .. && cmake --build .
```

`build.sh` sets `JAVA_HOME` via `/usr/libexec/java_home` (macOS) before configuring. There is no separate lint step or automated test suite — `test/Main.java` is a manual smoke-test harness, not a unit test.

The CMake target is named `javaplusplus`, so the build output is `build/javaplusplus.dylib` (macOS) or `build/javaplusplus.so` (Linux) — **not** `profiler.so`/`profiler.dylib` as `readme.md`'s run instructions say. Use the actual built filename when constructing `-agentpath` commands.

## Running / manual verification

```bash
javac test/Main.java
java -agentpath:./build/javaplusplus.dylib=logpath=/tmp/profiler.log,jsonpath=/tmp/profiler.json -cp test Main
```

Agent options are comma-separated `key=value` pairs parsed in `Agent.cpp::parse_options`: `logpath=` (file the `Logger` singleton appends to, in addition to stderr) and `jsonpath=` (where `Reporter::dump_json` writes on `VMDeath`). Both are optional.

## Architecture

The agent's lifecycle is driven entirely by JVMTI callbacks registered in `src/core/Agent.cpp::Agent_OnLoad`: `VMInit`, `VMDeath`, `ThreadStart`, `ThreadEnd`. There is currently no bytecode-level instrumentation (no `MethodEntry`/`MethodExit` callbacks) — profiling is purely statistical/sampling-based. This is a deliberate pivot recorded in `readme.md`'s roadmap (Section 4/5 notes): entry/exit instrumentation was built and then ripped out in favor of a sampling thread, and the old `Clock.hpp` abstraction was deleted as dead code once that happened. Don't reintroduce method-entry/exit event handling or a `Clock` class without checking those notes for why they were removed.

Data flow on a running JVM:

1. **`Sampler`** (`src/profiling/Sampler.{hpp,cpp}`) — started in `cbVMInit`, stopped in `cbVMDeath`. Runs a dedicated native thread attached to the JVM as a daemon (`AttachCurrentThreadAsDaemon`). Every 10ms it calls `GetAllThreads`, filters to threads in `JVMTI_THREAD_STATE_RUNNABLE`, and takes a `GetStackTrace` sample of each — deliberately *without* `SuspendThread`/`ResumeThread` (JVMTI permits sampling live threads; suspension was dropped to cut overhead). Currently only `frames[0]` (the top frame) is recorded, with inclusive/exclusive time both hardcoded to 0 — real per-sample CPU-time attribution (via `GetThreadCpuTime` deltas against a per-thread baseline, across the *full* captured stack) is in-progress work (roadmap Section 7).
2. **`MethodStatsRegistry`** (`src/profiling/MethodStats.hpp`) — thread-safe singleton keyed by `jmethodID`, aggregating `call_count`, `total_time_ns` (inclusive), `self_time_ns` (exclusive), and min/max self time. Fed by the sampler.
3. **`SymbolCache`** (`src/profiling/SymbolCache.{hpp,cpp}`) — thread-safe singleton resolving a `jmethodID` to a `MethodInfo` (class name, method name, signature) via JVMTI, using `std::shared_mutex` (many concurrent readers, one writer) and a double-checked-lock pattern to avoid resolving the same method twice. Responsible for freeing the JVMTI-allocated strings it consumes.
4. **`ThreadManager`** (`src/profiling/ThreadManager.{hpp,cpp}`) — tracks thread lifecycle. On `ThreadStart` it allocates a `ThreadState` (id, name, start time) and attaches it to the thread via JVMTI thread-local storage (`SetThreadLocalStorage`/`GetThreadLocalStorage`); IDs are assigned sequentially via an atomic counter. On `ThreadEnd` the `ThreadState` is converted into a `ThreadSummary` (adds end time / time alive) stored in a map keyed by thread id, and the TLS entry is freed. `get_live_thread_summaries` walks currently-running threads (via `GetAllThreads`) to report on threads still alive at dump time.
5. **`Reporter`** (`src/profiling/Reporter.{hpp,cpp}`) — called once, at `VMDeath`, after the sampler is stopped. `dump_report` logs the top 20 methods by total time; `dump_thread_summaries` logs both ended and still-running threads; `dump_json` (only if `jsonpath=` was supplied) writes both method stats and thread summaries as hand-rolled JSON (no JSON library dependency — see `escape_json` in `Reporter.cpp` for the escaping logic if extending the schema).

Cross-cutting utilities in `src/utils/`:
- **`Logger`** — thread-safe singleton, always writes to stderr and optionally appends to a file (`logpath=`). Use the `LOG_INFO`/`LOG_WARN`/`LOG_ERROR` macros rather than calling `Logger::getInstance().log(...)` directly.
- **`JvmtiHelper`** — the `CHECK_JVMTI(jvmti, err, msg)` macro is the standard way to check a `jvmtiError` return; it resolves the error to a name via `GetErrorName` and logs it, freeing the JVMTI-allocated name string. Use it after every non-trivial JVMTI call rather than checking `err != JVMTI_ERROR_NONE` inline.

## Working in this codebase

- Singletons (`Logger`, `SymbolCache`, `MethodStatsRegistry`, `ThreadManager`, `Sampler`) are the established pattern for shared agent-wide state — follow it rather than introducing globals or passing state through callback signatures.
- Any pointer/buffer JVMTI hands back via an out-parameter (method names, signatures, thread names, thread lists, error name strings) is heap-allocated by the JVM and must be freed with `jvmti->Deallocate(...)`; check existing call sites (e.g. `SymbolCache::get_or_resolve`, `ThreadManager::on_thread_start`) for the pattern before adding a new JVMTI call.
- `readme.md` doubles as the design doc/roadmap (checkboxed sections with notes on what was reworked or superseded). Update it when a section's status changes or a documented design decision is revisited — it's the authoritative record of *why* the code looks the way it does, not just a task list.
