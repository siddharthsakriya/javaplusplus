// src/Agent.cpp
#include <jvmti.h>
#include <iostream>
#include <cstring>
#include <string>

#include "Logger.hpp"
#include "JvmtiHelper.hpp"
#include "SymbolCache.hpp"

// JVMTI callbacks

static void JNICALL cbVMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    LOG_INFO("VM initialized.");

    jclass mainClass = jni->FindClass("Main");
    if (mainClass == nullptr) {
        LOG_ERROR("Could not find Main class to test symbol cache.");
        return;
    }

    jmethodID mainMethod = jni->GetStaticMethodID(mainClass, "main", "([Ljava/lang/String;)V");
    if (mainMethod == nullptr) {
        LOG_ERROR("Could not find Main.main method.");
        return;
    }

    const MethodInfo& info = SymbolCache::instance().get_or_resolve(jvmti, mainMethod);
    
    LOG_INFO("Symbol Cache Test -> Resolved method: " + info.class_name + " :: " + info.method_name + " " + info.signature);
}

static void JNICALL cbVMDeath(jvmtiEnv* jvmti, JNIEnv* jni) {
    LOG_INFO("VM shutting down.");
}

// additional methods 

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
    
    jvmtiError err = jvmti->AddCapabilities(&capabilities);
    CHECK_JVMTI(jvmti, err, "AddCapabilities");

    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    
    callbacks.VMInit = &cbVMInit;
    callbacks.VMDeath = &cbVMDeath;

    err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    CHECK_JVMTI(jvmti, err, "SetEventCallbacks");

    err = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
    CHECK_JVMTI(jvmti, err, "SetEventNotificationMode(VM_INIT)");

    err = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr);
    CHECK_JVMTI(jvmti, err, "SetEventNotificationMode(VM_DEATH)");

    LOG_INFO("Successfully loaded and registered callbacks.");
    return JNI_OK;
}