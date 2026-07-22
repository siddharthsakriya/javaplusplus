// src/JvmtiHelper.hpp
#pragma once
#include <jvmti.h>
#include <string>
#include "Logger.hpp"

namespace JvmtiHelper {

    inline void check_jvmti_error (jvmtiEnv* jvmti, jvmtiError error, std::string_view msg) {
        if (error == JVMTI_ERROR_NONE) return;
        
        char* err_name_c = nullptr;
        
        // resolves error name given number, hands back pointer to the string 
        jvmtiError name_err = jvmti->GetErrorName(error, &err_name_c); 

        std::string err_name = (name_err == JVMTI_ERROR_NONE && err_name_c != nullptr) 
                                ? std::string(err_name_c) 
                                : "Unknown error";
        
        // free up mem so we dont have a leak lol 
        if (err_name_c != nullptr) {
            jvmti->Deallocate(reinterpret_cast<unsigned char*>(err_name_c));
        }        

        LOG_ERROR(std::string(msg) + " failed: " + err_name + " (" + std::to_string(error) + ")");

    }

    #define CHECK_JVMTI(jvmti, err, msg) JvmtiHelper::check_jvmti_error(jvmti, err, msg)

} // namespace JvmtiHelper