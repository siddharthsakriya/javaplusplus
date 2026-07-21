// src/ThreadState.hpp
#pragma once
#include <jvmti.h>
#include <string>
#include <vector>

struct Frame {
    jmethodID method_id;
    int64_t start_time_ns;
    int64_t child_time_ns;
};

struct ThreadState {
    int id;
    std::string name;
    int64_t start_time_ns;
    
    std::vector<Frame> call_stack;
};