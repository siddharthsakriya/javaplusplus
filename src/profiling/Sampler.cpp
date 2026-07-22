// src/profiling/Sampler.cpp
#include "Sampler.hpp"
#include "Logger.hpp"
#include "JvmtiHelper.hpp"
#include "MethodStats.hpp"
#include <chrono>

void Sampler::start(jvmtiEnv* env, JNIEnv* jni) {
    jvmti = env;
    running = true;
    // bg cpp thread 
    sampler_thread = std::thread(&Sampler::sampler_loop, this);
}

void Sampler::stop() {
    running = false;
    if (sampler_thread.joinable()) {
        sampler_thread.join();
    }
}

void Sampler::sampler_loop() {
    LOG_INFO("Sampler thread running...");
    
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        jint thread_count = 0;
        jthread* threads = nullptr;

        jvmtiError err = jvmti->GetAllThreads(&thread_count, &threads);
        CHECK_JVMTI(jvmti, err, "GetAllThreads");

        for (int i = 0; i < thread_count; i++) {
            jthread thread = threads[i];

            jint state = 0;
            err = jvmti->GetThreadState(thread, &state);

            if (err != JVMTI_ERROR_NONE) continue;

            if ((state & JVMTI_THREAD_STATE_RUNNABLE) != 0) {

                jint frame_count = 0;
                jvmtiFrameInfo* frames = nullptr;
                
                err = jvmti->GetStackTrace(thread, 0, 64, frames, &frame_count);
                if (err != JVMTI_ERROR_NONE) continue;

                if (frame_count > 0) {
                    MethodStatsRegistry::getInstance().add_stats(frames[0].method, 0, 0);
                }

                if (frames) jvmti->Deallocate(reinterpret_cast<unsigned char*>(frames));

            }
        }
        if (threads) jvmti->Deallocate(reinterpret_cast<unsigned char*>(threads));        
    }
    LOG_INFO("Sampler thread stopped...");
}