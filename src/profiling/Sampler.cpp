#include "Sampler.hpp"
#include "Logger.hpp"
#include "JvmtiHelper.hpp"
#include "MethodStats.hpp"
#include <chrono>

void Sampler::start(jvmtiEnv* jvmti_env, JNIEnv* jni) {
    jvmti = jvmti_env;
    jni->GetJavaVM(&java_vm);          
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

    // AsDaemon so this thread never blocks VM shutdown
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
                jni->DeleteLocalRef(threads[i]);
            }
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(threads));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    java_vm->DetachCurrentThread();
}