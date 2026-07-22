// src/SymbolCache.cpp
#include "SymbolCache.hpp"
#include "Logger.hpp"
#include "JvmtiHelper.hpp"

const MethodInfo& SymbolCache::get_or_resolve(jvmtiEnv* jvmti, jmethodID method_id) {

    {
        std::shared_lock<std::shared_mutex> lock(cache_mutex);
        auto it = cache_map.find(method_id);
        if (it != cache_map.end()) {
            return it->second; 
        }
    }

    {
        std::unique_lock<std::shared_mutex> lock(cache_mutex);
        
        // another thread could have resolved while we waited on the lock
        auto it = cache_map.find(method_id);
        if (it != cache_map.end()) {
            return it->second;
        }

        MethodInfo info;
        char* method_name_c = nullptr;
        char* signature_c = nullptr;
        jclass dec_class = nullptr;
        char* class_signature_c = nullptr;

        // try get meth name and signature
        jvmtiError err = jvmti->GetMethodName(method_id, &method_name_c, &signature_c, nullptr);
        if (err != JVMTI_ERROR_NONE) {
            CHECK_JVMTI(jvmti, err, "GetMethodName");
            info.method_name = "<unknown>";
            info.signature = "<unknown>";
        } else {
            info.method_name = method_name_c;
            info.signature = signature_c;
        }

        // try get class declaring this method 
        err = jvmti->GetMethodDeclaringClass(method_id, &dec_class);
        if (err != JVMTI_ERROR_NONE) {
            CHECK_JVMTI(jvmti, err, "GetMethodDeclaringClass");
            info.class_name = "<unknown>";
        } else {
            // 5. Get the class signature (e.g., "Ljava/lang/String;")
            err = jvmti->GetClassSignature(dec_class, &class_signature_c, nullptr);
            if (err != JVMTI_ERROR_NONE) {
                CHECK_JVMTI(jvmti, err, "GetClassSignature");
                info.class_name = "<unknown>";
            } else {
                info.class_name = class_signature_c;
            }
        }
        
        // free up memory
        if (method_name_c) jvmti->Deallocate(reinterpret_cast<unsigned char*>(method_name_c));
        if (signature_c) jvmti->Deallocate(reinterpret_cast<unsigned char*>(signature_c));
        if (class_signature_c) jvmti->Deallocate(reinterpret_cast<unsigned char*>(class_signature_c));

        auto [new_it, success] = cache_map.emplace(method_id, std::move(info));
        return new_it->second;
    }

}
