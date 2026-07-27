// src/ThreadState.hpp
#pragma once
#include <jvmti.h>
#include <string>

struct ThreadState {
    int id;
    std::string name;
    int64_t start_time_ns;
};