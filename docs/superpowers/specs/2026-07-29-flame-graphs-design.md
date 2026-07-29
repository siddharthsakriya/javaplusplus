# Flame Graph Output (Sections 7 + 8) — Design

## Goal

Ship flame-graph output for the profiler. This requires finishing Section 7
(real per-sample CPU-time attribution across the *full* captured stack —
today only `frames[0]` is recorded, with inclusive/exclusive both hardcoded
to 0) before Section 8 (call tree + folded-stack output) can produce
meaningful data.

Scope for this pass: Sections 7 and 8 only. No allocation profiling, thread
state buckets, or monitor contention in this round.

## Part A — Real per-sample timing (Section 7)

### Capability

`Agent_OnLoad` currently requests zero JVMTI capabilities. Add
`AddCapabilities` with `can_get_thread_cpu_time` before the sampler starts.
Without it, `GetThreadCpuTime` fails with
`JVMTI_ERROR_MUST_POSSESS_CAPABILITY`.

### CPU-time baseline

Add `int64_t last_cpu_time_ns = -1;` to `ThreadState`. Each sample, for each
runnable thread, the sampler calls `GetThreadCpuTime(curr_thread, &now)`,
computes:

```
delta = (baseline < 0) ? 0 : now - baseline
baseline = now
```

`baseline < 0` (i.e. first observation of this thread) yields `delta == 0`
rather than a bogus large value.

### Correctness fix: ThreadManager use-after-free risk

Today `Sampler` never touches `ThreadState`, so there is no cross-thread
access to it — `ThreadManager::on_thread_start`/`on_thread_end` run on the
owning thread, and `Reporter` only reads thread state after `Sampler::stop()`
has joined. Making the sampler read/write `last_cpu_time_ns` on a live
`ThreadState*` introduces a real race: `ThreadEnd` can fire on the dying
thread and `delete state` at the same moment the sampler thread is mid-read
of that same object via `GetThreadLocalStorage`.

Fix, bundled into this work:
- Add a mutex to `ThreadManager` guarding the TLS get/delete sequence.
- Add `ThreadManager::with_state(jvmti, thread, fn)`, which locks, fetches
  the `ThreadState*`, and invokes `fn(state)` only if non-null, all under
  the lock. The sampler uses this instead of a raw `get_state` call for
  anything that mutates thread state.

### Full-stack attribution

The sampler already calls `GetStackTrace(thread, 0, 64, frames,
&frame_count)` — the full stack is already captured, only `frames[0]` was
ever *used*. For every captured frame `i` (0 = innermost/current frame),
call:

```cpp
MethodStatsRegistry::getInstance().add_stats(
    frames[i].method,
    /* inclusive */ delta,
    /* exclusive */ (i == 0) ? delta : 0
);
```

Inclusive time (and `call_count`) is attributed to every frame on the
sampled stack; exclusive time only to the top (leaf) frame — matching the
existing roadmap note in `readme.md` Section 7.

## Part B — Call tree + flame graph output (Section 8)

### Data structure

New files `src/profiling/CallTree.hpp` / `CallTree.cpp`, a singleton
`CallTreeRegistry` following the existing `MethodStatsRegistry` /
`SymbolCache` singleton pattern (mutex-guarded map, `getInstance()`):

```cpp
struct CallTreeNode {
    jmethodID method_id;
    int64_t inclusive_time_ns = 0;
    int64_t self_time_ns = 0;
    int sample_count = 0;   // times this node was anywhere on a sampled stack
    int self_count = 0;     // times this node was the top-of-stack (leaf)
    std::unordered_map<jmethodID, std::unique_ptr<CallTreeNode>> children;
};

class CallTreeRegistry {
public:
    static CallTreeRegistry& getInstance();
    void record_stack(const jvmtiFrameInfo* frames, jint frame_count, int64_t delta_ns);
    const CallTreeNode& root() const;
private:
    CallTreeNode root_node;  // synthetic root, method_id unused
    std::mutex tree_mutex;
};
```

`record_stack` walks the JVMTI frame array **root→leaf** — i.e. in reverse,
since `frames[0]` is the innermost/current frame and
`frames[frame_count-1]` is the outermost. For each frame, descend into (or
create) the matching child, incrementing `inclusive_time_ns`/`sample_count`;
the final node in the walk (the original `frames[0]`) additionally gets
`self_time_ns`/`self_count` incremented.

The `Sampler` calls `record_stack` once per thread per sample, alongside the
`MethodStatsRegistry::add_stats` calls from Part A, reusing the same
`delta_ns`.

### Output

Added to `Reporter`:

- `dump_call_tree(jvmti)` — indented text call tree logged to
  stderr/logfile (count, inclusive %, exclusive % per node), alongside the
  existing top-20 methods list. Percentages are relative to the root's
  total inclusive time.
- `dump_folded_stacks(jvmti, path)` — depth-first walk of the tree; at every
  node with `self_count > 0`, resolve the root→node path to
  `ClassName::methodName;ClassName::methodName;...` via `SymbolCache` and
  write one line `"<path> <self_count>"` to `path`. This is directly
  consumable by `flamegraph.pl` / speedscope's collapsed-stack format.

### New agent option

`flamepath=` — parsed in `Agent.cpp::parse_options` the same way as the
existing `jsonpath=`. If set, `cbVMDeath` calls `Reporter::dump_folded_stacks`
after the sampler has stopped, alongside the existing `dump_json` call.

## Part C — Verification & docs

### Test harness

`test/Main.java`'s current workload (two loops of 1000 `Math.sqrt` calls)
completes in well under one 10ms sample interval, so the sampler would
likely catch zero real application work today. Extend it with a deeper,
longer-running nested call chain (e.g. a loop that runs for a few hundred
milliseconds, calling through 2-3 levels of methods) so the resulting flame
graph has visible, verifiable structure. This is an addition to the
existing smoke-test file, not a rewrite.

### Manual verification

1. `./build.sh`
2. `javac test/Main.java`
3. `java -agentpath:./build/javaplusplus.dylib=logpath=/tmp/profiler.log,jsonpath=/tmp/profiler.json,flamepath=/tmp/profiler.folded -cp test Main`
4. Confirm `/tmp/profiler.folded` contains plausible
   `Class::method;Class::method;...  <count>` lines reflecting the test's
   call structure.
5. Confirm the log's indented call tree and top-20 methods list look
   sane (non-zero times, matches the folded-stack data).
6. If `flamegraph.pl` is available locally, feed it the folded file and
   eyeball the resulting SVG; otherwise this step is optional.

### Documentation

Update `readme.md`:
- Check off Section 7 and Section 8.
- Replace their bodies with notes on what shipped (mirroring how Sections
  4/5 document supersession/rework), including the `can_get_thread_cpu_time`
  capability addition and the `ThreadManager` UAF fix as notable details.
- Update the run instructions' option list to mention `flamepath=`.

## Out of scope

- Allocation profiling (Section 9)
- Thread state buckets (Section 10)
- Monitor contention (Section 11)
- GC events, heap census, control plane, cross-platform polish (Sections
  12-15)
