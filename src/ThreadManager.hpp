// src/ThreadManager.hpp
#pragma once
#include <jvmti.h>
#include <atomic>
#include <string>
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

    private:
        ThreadManager() : next_id(1) {}
        std::atomic<int> next_id;
};