#pragma once
#include <jvmti.h>
#include <string>
#include <unordered_map>
#include <shared_mutex>

struct MethodInfo {
    std::string class_name;   
    std::string method_name;  
    std::string signature;    
};

class SymbolCache {
    public:
        static SymbolCache& instance() {
            static SymbolCache cache;
            return cache;
        }

        const MethodInfo& get_or_resolve(jvmtiEnv* jvmti, jmethodID method_id);

    private:
        std::unordered_map<jmethodID, MethodInfo> cache_map;
        // means we can have multiple readers, one writer
        mutable std::shared_mutex cache_mutex;
};