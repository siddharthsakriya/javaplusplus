#pragma once
#include <chrono>
#include <jvmti.h>

struct Clock {

    static int64_t get_wall_time_ns() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    static int64_t get_current_thread_cpu_time_ns(jvmtiEnv* jvmti) {
        jvmtiError err;
        jlong cpu_time_ns;

        err = jvmti->GetCurrentThreadCpuTime(&cpu_time_ns);
        if (err != JVMTI_ERROR_NONE) {
            return 0;
        }

        return static_cast<int64_t>(cpu_time_ns);
    }
};