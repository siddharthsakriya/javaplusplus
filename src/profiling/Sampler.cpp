#include "Sampler.hpp"
#include "Logger.hpp"
#include "JvmtiHelper.hpp"
#include "MethodStats.hpp"
#include "CallTree.hpp"
#include "ThreadManager.hpp"
#include "ThreadState.hpp"
#include <chrono>

void Sampler::start(jvmtiEnv* jvmti_env, JNIEnv* jni) {
    jvmti = jvmti_env;
    if (jni->GetJavaVM(&java_vm) != JNI_OK) {
        LOG_ERROR("Sampler failed to get JavaVM; not starting.");
        return;
    }
    running.store(true);
    sampler_thread = std::thread(&Sampler::sampler_loop, this);
}

void Sampler::stop() {
    running = false;
    if (sampler_thread.joinable()) {
        sampler_thread.join();
    }
}

void Sampler::sampler_loop() {
    JNIEnv* jni = nullptr;
    JavaVMAttachArgs args{};
    args.version = JNI_VERSION_1_8;
    args.name    = const_cast<char*>("JVM++ Sampler");
    args.group   = nullptr;

    if (java_vm->AttachCurrentThreadAsDaemon(
            reinterpret_cast<void**>(&jni), &args) != JNI_OK) {
        return;
    }

    while (running.load()) {
        jint count = 0;
        jthread* threads = nullptr;
        jvmtiError err = jvmti->GetAllThreads(&count, &threads);
        
        if (err == JVMTI_ERROR_WRONG_PHASE) break;   

        if (err == JVMTI_ERROR_NONE) {
            for (jint i = 0; i < count; ++i) {
                jthread curr_thread = threads[i];
                jint thread_state = 0;
                jvmtiError state_err = jvmti->GetThreadState(curr_thread, &thread_state);
            
                if (state_err == JVMTI_ERROR_NONE && (thread_state & JVMTI_THREAD_STATE_RUNNABLE) != 0) {
                    jvmtiFrameInfo frames[64];
                    jint frame_count = 0;
                    jvmtiError trace_err = jvmti -> GetStackTrace(curr_thread, 0, 64, frames, &frame_count);

                    if (trace_err == JVMTI_ERROR_NONE && frame_count > 0) {
                        jlong current_cpu_time = 0;
                        jvmtiError cpu_err = jvmti->GetThreadCpuTime(curr_thread, &current_cpu_time);

                        if (cpu_err == JVMTI_ERROR_NONE) {
                            int64_t delta_ns = 0;

                            ThreadManager::getInstance().with_state(jvmti, curr_thread, [&](ThreadState* state) {
                                if (state->last_cpu_time_ns >= 0) {
                                    delta_ns = current_cpu_time - state->last_cpu_time_ns;
                                }
                                state->last_cpu_time_ns = current_cpu_time;
                            });

                            for (jint frame_idx = 0; frame_idx < frame_count; ++frame_idx) {
                                MethodStatsRegistry::getInstance().add_stats(
                                    frames[frame_idx].method,
                                    delta_ns,
                                    frame_idx == 0 ? delta_ns : 0
                                );
                            }
                            CallTreeRegistry::getInstance().record_stack(frames, frame_count, delta_ns);
                        }
                    }
                }
                jni->DeleteLocalRef(threads[i]);
            }
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(threads));   
        } else {
            CHECK_JVMTI(jvmti, err, "GetAllThreads");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    java_vm->DetachCurrentThread();
}