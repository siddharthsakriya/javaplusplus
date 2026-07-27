#include <jvmti.h>
#include <iostream>
#include <cstring>
#include <string>

#include "utils/Logger.hpp"
#include "utils/JvmtiHelper.hpp"
#include "profiling/SymbolCache.hpp"
#include "profiling/ThreadManager.hpp"
#include "profiling/MethodStats.hpp"
#include "profiling/Reporter.hpp"
#include "profiling/Sampler.hpp"

static void JNICALL cbVMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    LOG_INFO("VM initialized.");
    ThreadManager::getInstance().on_thread_start(jvmti, jni, thread);
    
    // Start the statistical sampler!
    Sampler::getInstance().start(jvmti, jni);
}

static void JNICALL cbVMDeath(jvmtiEnv* jvmti, JNIEnv* jni) {
    LOG_INFO("VM shutting down.");
    
    // Stop the sampler before we generate the report
    Sampler::getInstance().stop();
    Reporter::dump_report(jvmti);
}

static void JNICALL cbThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    ThreadManager::getInstance().on_thread_start(jvmti, jni, thread);
}

static void JNICALL cbThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    ThreadManager::getInstance().on_thread_end(jvmti, jni, thread);
}

void parse_options(const char* options) {
    if (options == nullptr) return;
    std::string opts(options);
    std::string key = "logpath=";
    size_t pos = opts.find(key);
    if (pos != std::string::npos) {
        size_t start = pos + key.length();
        size_t end = opts.find(',', start);
        if (end == std::string::npos) end = opts.length();
        std::string path = opts.substr(start, end - start);
        Logger::getInstance().init(path);
        LOG_INFO("Logger initialized to file: " + path);
    }
}

extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    LOG_INFO("Agent_OnLoad called.");
    parse_options(options);

    jvmtiEnv* jvmti = nullptr;
    jint rc = vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_0);
    if (rc != JNI_OK || jvmti == nullptr) {
        LOG_ERROR("Failed to get JVMTI environment.");
        return JNI_ERR;
    }

    jvmtiCapabilities capabilities;
    memset(&capabilities, 0, sizeof(capabilities));
    // We do NOT request method entry/exit events anymore!
    
    jvmtiError err = jvmti->AddCapabilities(&capabilities);
    CHECK_JVMTI(jvmti, err, "AddCapabilities");

    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.VMInit = &cbVMInit;
    callbacks.VMDeath = &cbVMDeath;
    callbacks.ThreadStart = &cbThreadStart;
    callbacks.ThreadEnd = &cbThreadEnd;
    // We do NOT register MethodEntry or MethodExit callbacks anymore!

    err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    CHECK_JVMTI(jvmti, err, "SetEventCallbacks");

    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, nullptr);
    // We do NOT enable METHOD_ENTRY or METHOD_EXIT events anymore!

    LOG_INFO("Successfully loaded and registered callbacks.");
    return JNI_OK;
}