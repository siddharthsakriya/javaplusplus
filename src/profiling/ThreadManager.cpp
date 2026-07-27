// src/ThreadManager.cpp
#include "ThreadManager.hpp"
#include "Logger.hpp"
#include "JvmtiHelper.hpp"
#include <chrono>

void ThreadManager::on_thread_start(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {

    if (get_state(jvmti, thread) != nullptr) {
        return;
    }

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

    std::lock_guard<std::mutex> lock(summary_mutex);
    ThreadSummary& thread_summary = thread_summary_map[state->id];
    thread_summary.id = state->id;
    thread_summary.name = state->name;
    thread_summary.start_time_ns = state->start_time_ns;
    thread_summary.end_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    thread_summary.time_alive_ns = thread_summary.end_time_ns - thread_summary.start_time_ns;

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

std::vector<ThreadSummary> ThreadManager::get_live_thread_summaries(jvmtiEnv* jvmti, JNIEnv* jni) {
    std::vector<ThreadSummary> live_summaries;

    jint count = 0;
    jthread* threads = nullptr;
    jvmtiError err = jvmti->GetAllThreads(&count, &threads);
    if (err != JVMTI_ERROR_NONE) {
        CHECK_JVMTI(jvmti, err, "GetAllThreads (live thread summaries)");
        return live_summaries;
    }

    int64_t now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    for (jint i = 0; i < count; ++i) {
        ThreadState* state = get_state(jvmti, threads[i]);
        if (state != nullptr) {
            ThreadSummary summary;
            summary.id = state->id;
            summary.name = state->name;
            summary.start_time_ns = state->start_time_ns;
            summary.end_time_ns = -1;
            summary.time_alive_ns = now_ns - state->start_time_ns;
            live_summaries.push_back(summary);
        }
        jni->DeleteLocalRef(threads[i]);
    }

    jvmti->Deallocate(reinterpret_cast<unsigned char*>(threads));
    return live_summaries;
}