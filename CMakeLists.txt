cmake_minimum_required(VERSION 3.10)
project(JavaProfiler CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find JNI (this includes jvmti.h paths)
find_package(JNI REQUIRED)

# Create the shared library
add_library(javaplusplus SHARED src/Agent.cpp)

# Include JNI headers
target_include_directories(javaplusplus PRIVATE ${JNI_INCLUDE_DIRS})

# Force the output name to be 'libjavaplusplus.so' or 'javaplusplus.dll' 
# without the 'lib' prefix on Windows.
set_target_properties(javaplusplus PROPERTIES PREFIX "")