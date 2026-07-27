// src/ThreadManager.hpp
#pragma once
#include <jvmti.h>
#include <atomic>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "ThreadState.hpp"

class ThreadManager {
    public:
        static ThreadManager& getInstance() {
            static ThreadManager tm;
            return tm;
        }

        void on_thread_start(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
        void on_thread_end(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
        static ThreadState* get_state(jvmtiEnv* jvmti, jthread thread);

        static std::vector<ThreadSummary> get_live_thread_summaries(jvmtiEnv* jvmti, JNIEnv* jni);

        const std::unordered_map<int, ThreadSummary>& get_thread_summaries() const {
            return thread_summary_map;
        }

    private:
        ThreadManager() : next_id(1) {}
        std::atomic<int> next_id;
        std::unordered_map<int, ThreadSummary> thread_summary_map;
        mutable std::mutex summary_mutex;
};