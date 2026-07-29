#pragma once
#include <jvmti.h>
#include <string>

class Reporter {
    public:
        static void dump_report(jvmtiEnv* jvmti);
        static void dump_thread_summaries(jvmtiEnv* jvmti, JNIEnv* jni);
        static void dump_json(jvmtiEnv* jvmti, JNIEnv* jni, const std::string& path);
        static void dump_call_tree(jvmtiEnv* jvmti);
        static void dump_folded_stacks(jvmtiEnv* jvmti, const std::string& path);
};