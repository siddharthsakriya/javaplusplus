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

static std::string g_json_path;
static std::string g_flame_path;

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
    Reporter::dump_thread_summaries(jvmti, jni);
    Reporter::dump_call_tree(jvmti);

    if (!g_json_path.empty()) {
        Reporter::dump_json(jvmti, jni, g_json_path);
    }

    if (!g_flame_path.empty()) {
        Reporter::dump_folded_stacks(jvmti, g_flame_path);
    }
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

    std::string log_key = "logpath=";
    size_t pos = opts.find(log_key);
    if (pos != std::string::npos) {
        size_t start = pos + log_key.length();
        size_t end = opts.find(',', start);
        if (end == std::string::npos) end = opts.length();
        std::string path = opts.substr(start, end - start);
        Logger::getInstance().init(path);
        LOG_INFO("Logger initialized to file: " + path);
    }

    std::string json_key = "jsonpath=";
    pos = opts.find(json_key);
    if (pos != std::string::npos) {
        size_t start = pos + json_key.length();
        size_t end = opts.find(',', start);
        if (end == std::string::npos) end = opts.length();
        g_json_path = opts.substr(start, end - start);
        LOG_INFO("JSON report will be written to: " + g_json_path);
    }

    std::string flame_key = "flamepath=";
    pos = opts.find(flame_key);
    if (pos != std::string::npos) {
        size_t start = pos + flame_key.length();
        size_t end = opts.find(',', start);
        if (end == std::string::npos) end = opts.length();
        g_flame_path = opts.substr(start, end - start);
        LOG_INFO("Folded-stack report will be written to: " + g_flame_path);
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
    capabilities.can_get_thread_cpu_time = 1;
    jvmtiError cap_err = jvmti->AddCapabilities(&capabilities);
    CHECK_JVMTI(jvmti, cap_err, "AddCapabilities(can_get_thread_cpu_time)");

    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.VMInit = &cbVMInit;
    callbacks.VMDeath = &cbVMDeath;
    callbacks.ThreadStart = &cbThreadStart;
    callbacks.ThreadEnd = &cbThreadEnd;

    jvmtiError err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    CHECK_JVMTI(jvmti, err, "SetEventCallbacks");

    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, nullptr);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, nullptr);
    // We do NOT enable METHOD_ENTRY or METHOD_EXIT events anymore!

    LOG_INFO("Successfully loaded and registered callbacks.");
    return JNI_OK;
}