#include <jvmti.h>
#include <iostream>
#include <cstring>

// --- JVMTI Callbacks ---
// These must have C linkage so the JVM can find them.

static void JNICALL cbVMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    std::cerr << "[Agent] VM initialized.\n";
}

static void JNICALL cbVMDeath(jvmtiEnv* jvmti, JNIEnv* jni) {
    std::cerr << "[Agent] VM shutting down.\n";
}

extern "C" JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    std::cerr << "[Agent] Agent_OnLoad called.\n";

    jvmtiEnv* jvmti = nullptr;
    
    jint rc = vm->GetEnv(reinterpret_cast<void**>(&jvmti), JVMTI_VERSION_1_0);
    if (rc != JNI_OK || jvmti == nullptr) {
        std::cerr << "[Agent] Error: Failed to get JVMTI environment.\n";
        return JNI_ERR;
    }

    jvmtiCapabilities capabilities;
    memset(&capabilities, 0, sizeof(capabilities));
    
    jvmtiError err = jvmti->AddCapabilities(&capabilities);
    if (err != JVMTI_ERROR_NONE) {
        std::cerr << "[Agent] Error: Failed to add capabilities.\n";
        return JNI_ERR;
    }

    jvmtiEventCallbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    
    callbacks.VMInit = &cbVMInit;
    callbacks.VMDeath = &cbVMDeath;

    err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    if (err != JVMTI_ERROR_NONE) {
        std::cerr << "[Agent] Error: Failed to set callbacks.\n";
        return JNI_ERR;
    }

    err = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, nullptr);
    if (err != JVMTI_ERROR_NONE) return JNI_ERR;

    err = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, nullptr);
    if (err != JVMTI_ERROR_NONE) return JNI_ERR;

    std::cerr << "[Agent] Successfully loaded and registered callbacks.\n";
    return JNI_OK;
}