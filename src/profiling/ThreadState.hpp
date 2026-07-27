// src/ThreadState.hpp
#pragma once
#include <jvmti.h>
#include <string>

struct ThreadState {
    int id;
    std::string name;
    int64_t start_time_ns;
};

struct ThreadSummary {
    int id;
    std::string name;
    int64_t start_time_ns;
    int64_t end_time_ns;
    int64_t time_alive_ns;
};