#include <jvmti.h>
#include <iostream>
#include <cstring>
#include <string>
#include <chrono>

#include "Logger.hpp"
#include "JvmtiHelper.hpp"
#include "SymbolCache.hpp"
#include "ThreadManager.hpp"
#include "MethodStats.hpp"

int64_t get_current_time_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

static void JNICALL cbVMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    LOG_INFO("VM initialized.");
    ThreadManager::getInstance().on_thread_start(jvmti, jni, thread);
}

static void JNICALL cbVMDeath(jvmtiEnv* jvmti, JNIEnv* jni) {
    LOG_INFO("VM shutting down.");
}

static void JNICALL cbThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    ThreadManager::getInstance().on_thread_start(jvmti, jni, thread);
}

static void JNICALL cbThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    ThreadManager::getInstance().on_thread_end(jvmti, jni, thread);
}

static void JNICALL cbMethodEntry(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jmethodID method) {
    ThreadState* state = ThreadManager::get_state(jvmti, thread);
    if (state == nullptr) return;

    Frame frame;
    frame.method_id = method;
    frame.start_time_ns = get_current_time_ns();
    frame.child_time_ns = 0;
    state->call_stack.push_back(frame);
}

static void JNICALL cbMethodExit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value) {
    ThreadState* state = ThreadManager::get_state(jvmti, thread);
    if (state == nullptr) return;

    if (state->call_stack.empty()) return;

    Frame frame = state->call_stack.back();
    state->call_stack.pop_back();

    int64_t end_time_ns = get_current_time_ns();
    int64_t inclusive_time = end_time_ns - frame.start_time_ns;
    int64_t exclusive_time = inclusive_time - frame.child_time_ns;

    if (!state->call_stack.empty()) {
        state->call_stack.back().child_time_ns += inclusive_time;
    }

    MethodStatsRegistry::getInstance().add_stats(method, inclusive_time, exclusive_time);
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
    capabilities.can_generate_method_entry_events = 1;
    capabilities.can_generate_method_exit_events = 1;
    
    jvmtiError err = jvmti->AddCapabilities(&capabilities);
    CHECK_JVMTI(jvmti, err, "AddCapabilities");

    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.VMInit = &cbVMInit;
    callbacks.VMDeath = &cbVMDeath;
    callbacks.ThreadStart = &cbThreadStart;
    callbacks.ThreadEnd = &cbThreadEnd;
    callbacks.MethodEntry = &cbMethodEntry;
    callbacks.MethodExit = &cbMethodExit;

    err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    CHECK_JVMTI(jvmti, err, "SetEventCallbacks");

    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT, nullptr);

    LOG_INFO("Successfully loaded and registered callbacks.");
    return JNI_OK;
}