cmake_minimum_required(VERSION 3.10)
project(JAVAPLUSPLUS CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(JNI REQUIRED)

add_library(JavaPlusPlus SHARED 
    src/Agent.cpp
    src/Logger.cpp
    src/SymbolCache.cpp
    src/ThreadManager.cpp
)

target_include_directories(JavaPlusPlus PRIVATE ${JNI_INCLUDE_DIRS})
