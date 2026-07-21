// src/ThreadManager.cpp
#include "ThreadManager.hpp"
#include "Logger.hpp"
#include "JvmtiHelper.hpp"
#include <chrono>

void ThreadManager::on_thread_start(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    
    ThreadState* state = new ThreadState();
    state->id = next_id.fetch_add(1);

    jvmtiThreadInfo info;
    jvmtiError err = jvmti->GetThreadInfo(thread, &info);
    if (err == JVMTI_ERROR_NONE) {
        state->name = info.name;
        // we only need to clean up name cos jvm allocates memory on heap for it
        jvmti->Deallocate(reinterpret_cast<unsigned char*>(info.name));
    } else {
        state->name = "<unknown>";
    }

    state->start_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    err = jvmti->SetThreadLocalStorage(thread, reinterpret_cast<void*>(state));
    CHECK_JVMTI(jvmti, err, "SetThreadLocalStorage");

    LOG_INFO("Thread started: ID=" + std::to_string(state->id) + ", Name=" + state->name);
}

void ThreadManager::on_thread_end(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    ThreadState* state = get_state(jvmti, thread);
    if (state==nullptr) {
        return;
    }

    LOG_INFO("Thread ended: ID=" + std::to_string(state->id) + ", Name=" + state->name);

    delete state;

    jvmti->SetThreadLocalStorage(thread, nullptr);
}

ThreadState* ThreadManager::get_state(jvmtiEnv* jvmti, jthread thread) {
    void* raw_ptr = nullptr;
    jvmtiError err = jvmti->GetThreadLocalStorage(thread, &raw_ptr);
    if (err != JVMTI_ERROR_NONE || raw_ptr == nullptr) {
        return nullptr;
    }    
    return reinterpret_cast<ThreadState*>(raw_ptr);
}