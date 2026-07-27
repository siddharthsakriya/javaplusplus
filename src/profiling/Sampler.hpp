#pragma once
#include <jvmti.h>
#include <jni.h>
#include <atomic>
#include <thread>

class Sampler {
    public: 
        static Sampler& getInstance() {
            static Sampler s;
            return s;
        }

        void start(jvmtiEnv* jvmti, JNIEnv* jni);
        void stop();

    private:
        Sampler() = default;
        JavaVM* java_vm = nullptr;
        jvmtiEnv* jvmti = nullptr;
        std::thread sampler_thread;
        std::atomic<bool> running{false};

        void sampler_loop();
};