#pragma once
#include <jvmti.h>

class Reporter {
    public:
        static void dump_report(jvmtiEnv* jvmti);
};