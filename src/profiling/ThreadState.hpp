// src/ThreadState.hpp
#pragma once
#include <jvmti.h>
#include <string>

struct ThreadState {
    int id;
    std::string name;
    int64_t start_time_ns;
    int64_t last_cpu_time_ns = -1; // baseline for per-sample CPU-time deltas; -1 = not yet observed
};

struct ThreadSummary {
    int id;
    std::string name;
    int64_t start_time_ns;
    int64_t end_time_ns;
    int64_t time_alive_ns;
};