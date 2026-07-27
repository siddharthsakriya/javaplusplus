cmake_minimum_required(VERSION 3.10)
project(JAVAPLUSPLUS CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(JNI REQUIRED)

add_library(javaplusplus SHARED 
    src/core/Agent.cpp
    src/utils/Logger.cpp
    src/profiling/SymbolCache.cpp
    src/profiling/ThreadManager.cpp
    src/profiling/Reporter.cpp
    src/profiling/Sampler.cpp
)

target_include_directories(javaplusplus PRIVATE 
    ${JNI_INCLUDE_DIRS} 
    src
    src/core
    src/utils
    src/profiling
)

set_target_properties(javaplusplus PROPERTIES PREFIX "")